//! RESEARCH-ONLY. The panel's bus tap (ADR 0033, issue I0080): one subscriber of the event bus
//! (`Bus` in src/panel.rs) that hands every event, in bus order, to two sinks:
//!
//! - a ZeroMQ PUB socket on tcp://127.0.0.1:<PRISMATIC_BUS_PORT or an ephemeral port>, two frames per
//!   message: the topic (the event kind, so a SUB client filters by prefix) and the envelope, JSON of
//!   schema prismatic-bus/v1. The pure-Rust `zeromq` crate runs on a small tokio runtime of its own
//!   thread, so no system libzmq is needed;
//! - a DuckDB file per instance ($PRISMATIC_STORE_DIR/<instance>.duckdb, by default
//!   ~/.local/state/prismatic-mcp/store/) with the tables runs, events and samples, committed in
//!   batches at least twice a second (cargo feature `store`, linked against the system libduckdb).
//!
//! One producer was added for it: the overview's daily sample is published as game.sample (src/panel.rs
//! `record_sample`), because the history lived in its own ring and never reached the bus; every other
//! producer is unchanged. The tap is the one place to add a further sink. The read
//! surfaces (GET /api/bus, GET /api/db/tables, GET|POST /api/db/query) are rows of the panel's route
//! table, so they reach OpenAPI, Swagger UI and Rhai (`api_dbQuery(#{sql: "..."})`) like every other.

use std::collections::BTreeMap;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{mpsc, Arc, Mutex, OnceLock};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde_json::{json, Value};

use crate::panel::{Game, Input};

/// The envelope's schema; also the answer of GET /api/bus.
pub const SCHEMA: &str = "prismatic-bus/v1";
/// The most rows one /api/db/query answers; more are cut and the answer says `capped`.
pub const QUERY_CAP: usize = 5000;
/// A batch is committed once it is this old: with the tap waking at least every 250 ms, every event
/// reaches the store well within a second.
const COMMIT_EVERY: Duration = Duration::from_millis(400);

/// What the tap reports and the routes read: the bus endpoint, the store file, what went out.
pub struct Tap {
    pub instance: String,
    pub session: Value,
    pub endpoint: Option<String>,
    pub bus_error: Option<String>,
    pub store: Option<PathBuf>,
    pub store_error: Option<String>,
    published: AtomicU64,
    topics: Mutex<BTreeMap<String, u64>>,
    #[cfg(feature = "store")]
    reader: Option<Mutex<db::Store>>,
    stopping: AtomicBool,
    done: Mutex<Option<mpsc::Receiver<()>>>,
}

static TAP: OnceLock<Arc<Tap>> = OnceLock::new();

/// The running tap, once `start` has run (never in unit tests of other modules).
pub fn tap() -> Option<&'static Arc<Tap>> {
    TAP.get()
}

fn now_ms() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_millis() as i64).unwrap_or(0)
}

/// Who this run belongs to: the Claude Code session that started it, else the Majordomus task named
/// by PRISMATIC_MCP_SUPERVISOR (mcp-e2e sets it), else null.
pub fn session_of() -> Value {
    if let Some(s) = std::env::var("CLAUDE_CODE_SESSION_ID").ok().filter(|s| !s.is_empty()) {
        return json!(s);
    }
    crate::panel::supervisor().and_then(|s| s.get("task").cloned()).filter(|t| !t.is_null()).unwrap_or(Value::Null)
}

/// The store's directory: $PRISMATIC_STORE_DIR, else ~/.local/state/prismatic-mcp/store.
pub fn store_dir() -> PathBuf {
    match std::env::var_os("PRISMATIC_STORE_DIR").filter(|v| !v.is_empty()) {
        Some(d) => PathBuf::from(d),
        None => PathBuf::from(std::env::var("HOME").unwrap_or_default()).join(".local/state/prismatic-mcp/store"),
    }
}

/// The envelope of one bus event (`{seq, time, kind, data}`), schema prismatic-bus/v1. game_tick and
/// game_date are the event's own when it carries them (game.month knows its date before the overview
/// sample of that day is taken; game.sample carries `t` and `date`), else the latest overview
/// sample's, else null.
pub fn envelope(e: &Value, instance: &str, session: &Value, seed: i64, ts_ms: i64, state: Option<&Value>) -> Value {
    let data = e.get("data").cloned().unwrap_or(Value::Null);
    let pick = |own: &str, sample: &str| {
        data.get(own).or_else(|| data.get(sample)).filter(|v| !v.is_null()).cloned()
            .or_else(|| state.and_then(|s| s.get(sample)).filter(|v| !v.is_null()).cloned())
            .unwrap_or(Value::Null)
    };
    json!({"schema": SCHEMA, "seq": e.get("seq").cloned().unwrap_or(Value::Null), "ts_ms": ts_ms,
           "instance": instance, "session": session, "seed": seed,
           "game_tick": pick("tick", "t"), "game_date": pick("date", "date"),
           "kind": e.get("kind").cloned().unwrap_or(Value::Null), "data": data})
}

/// One `samples` row per company of an overview sample (`sample_of` in src/panel.rs).
#[derive(Clone, Debug, PartialEq)]
pub struct SampleRow {
    pub ts_ms: i64,
    pub game_tick: Option<i64>,
    pub game_date: Option<String>,
    pub company: Option<i64>,
    pub name: Option<String>,
    pub money: f64,
    pub loan: f64,
    pub value: f64,
    pub net: f64,
    pub vehicles: Option<i64>,
}

fn as_i64(v: Option<&Value>) -> Option<i64> {
    v.and_then(|v| v.as_i64().or_else(|| v.as_f64().map(|f| f as i64)))
}

pub fn sample_rows(sample: &Value, ts_ms: i64) -> Vec<SampleRow> {
    let n = |c: &Value, k: &str| c.get(k).and_then(Value::as_f64).unwrap_or(0.0);
    sample["companies"].as_array().into_iter().flatten().map(|c| SampleRow {
        ts_ms, game_tick: as_i64(sample.get("t")), game_date: sample.get("date").and_then(Value::as_str).map(str::to_string),
        company: as_i64(c.get("id")), name: c.get("name").and_then(Value::as_str).map(str::to_string),
        money: n(c, "money"), loan: n(c, "loan"), value: n(c, "value"), net: n(c, "net"), vehicles: as_i64(c.get("fleet")),
    }).collect()
}

// ---------------------------------------------------------------------- the read-only guard

/// The statements /api/db/query runs: reads only.
pub const READ_ONLY: &[&str] = &["SELECT", "WITH", "DESCRIBE", "SUMMARIZE", "SHOW"];

/// `sql` from `at` with whitespace and comments (`-- ...` to the line's end, `/* ... */`) skipped.
fn skip_blank(sql: &[u8], mut at: usize) -> usize {
    loop {
        while at < sql.len() && sql[at].is_ascii_whitespace() { at += 1; }
        if sql[at..].starts_with(b"--") {
            while at < sql.len() && sql[at] != b'\n' { at += 1; }
        } else if sql[at..].starts_with(b"/*") {
            at = sql[at + 2..].windows(2).position(|w| w == b"*/").map_or(sql.len(), |p| at + 2 + p + 2);
        } else {
            return at;
        }
    }
}

/// Is `sql` one read-only statement? Its first word (after whitespace and comments) must be one of
/// `READ_ONLY`, and no `;` outside strings, quoted names and comments may be followed by anything but
/// whitespace and comments. An `E'...'` string honours backslash escapes, as DuckDB reads it, so a
/// `\'` cannot hide a second statement. The store's connection runs without external access and
/// prepares one statement only, so this is the first of three locks, not the only one.
pub fn read_only_sql(sql: &str) -> Result<(), String> {
    let b = sql.as_bytes();
    let start = skip_blank(b, 0);
    let word: String = sql[start..].chars().take_while(|c| c.is_ascii_alphabetic()).collect::<String>().to_ascii_uppercase();
    if word.is_empty() {
        return Err("empty statement".into());
    }
    if !READ_ONLY.contains(&word.as_str()) {
        return Err(format!("read-only: {word} is refused; only {} run here", READ_ONLY.join(", ")));
    }
    let mut i = start;
    while i < b.len() {
        match b[i] {
            q @ (b'\'' | b'"') => {
                let escapes = q == b'\'' && i > 0 && matches!(b[i - 1], b'E' | b'e')
                    && (i < 2 || !(b[i - 2].is_ascii_alphanumeric() || b[i - 2] == b'_'));
                i += 1;
                while i < b.len() {
                    if escapes && b[i] == b'\\' { i += 2; continue; }
                    if b[i] == q {
                        // A doubled quote is one quote character inside the string.
                        if i + 1 < b.len() && b[i + 1] == q { i += 2; continue; }
                        break;
                    }
                    i += 1;
                }
                i += 1;
            }
            b'-' if b[i..].starts_with(b"--") => i = skip_blank(b, i),
            b'/' if b[i..].starts_with(b"/*") => i = skip_blank(b, i),
            b';' => {
                if skip_blank(b, i + 1) < b.len() {
                    return Err("read-only: one statement only (a second statement follows ';')".into());
                }
                return Ok(());
            }
            _ => i += 1,
        }
    }
    Ok(())
}

// ---------------------------------------------------------------------- the DuckDB sink

#[cfg(feature = "store")]
pub mod db {
    use super::{as_i64, SampleRow, QUERY_CAP};
    use duckdb::types::Value as Dv;
    use duckdb::{params, Config, Connection};
    use serde_json::{json, Value};
    use std::path::Path;

    /// The tables of ADR 0033, created when missing: one file holds every run of its instance.
    pub const TABLES: &str = "
        CREATE TABLE IF NOT EXISTS runs(instance VARCHAR, session VARCHAR, seed BIGINT, started_ms BIGINT,
                                        panel_port INTEGER, mcp_port INTEGER, game_port INTEGER);
        CREATE TABLE IF NOT EXISTS events(instance VARCHAR, seq BIGINT, ts_ms BIGINT, kind VARCHAR,
                                          game_tick BIGINT, game_date VARCHAR, data JSON);
        CREATE TABLE IF NOT EXISTS samples(instance VARCHAR, ts_ms BIGINT, game_tick BIGINT, game_date VARCHAR,
                                           company BIGINT, name VARCHAR, money DOUBLE, loan DOUBLE, value DOUBLE,
                                           net DOUBLE, vehicles BIGINT);";

    /// One connection to the instance's database: the tap writes through one, the query routes read
    /// through a clone of it (same database, its own transactions).
    pub struct Store {
        conn: Connection,
        instance: String,
    }

    fn e(err: duckdb::Error) -> String {
        err.to_string()
    }

    /// No external access (no files read or written by a query, no ATTACH, no extension loading) and
    /// the configuration locked, so a statement that passes the guard still cannot leave the store.
    fn config() -> Result<Config, String> {
        Config::default().enable_external_access(false).map_err(e)?.with("lock_configuration", "true").map_err(e)
    }

    impl Store {
        pub fn open(path: &Path, instance: &str) -> Result<Store, String> {
            if let Some(dir) = path.parent() {
                std::fs::create_dir_all(dir).map_err(|err| format!("{}: {err}", dir.display()))?;
            }
            let conn = Connection::open_with_flags(path, config()?).map_err(|err| format!("{}: {err}", path.display()))?;
            Store::init(conn, instance)
        }

        /// A store in memory, for tests.
        #[cfg(test)]
        pub fn open_in_memory(instance: &str) -> Result<Store, String> {
            Store::init(Connection::open_in_memory_with_flags(config()?).map_err(e)?, instance)
        }

        fn init(conn: Connection, instance: &str) -> Result<Store, String> {
            conn.execute_batch(TABLES).map_err(e)?;
            Ok(Store { conn, instance: instance.to_string() })
        }

        /// Another connection to the same database, for the readers.
        pub fn reader(&self) -> Result<Store, String> {
            Ok(Store { conn: self.conn.try_clone().map_err(e)?, instance: self.instance.clone() })
        }

        pub fn insert_run(&self, session: &Value, seed: i64, started_ms: i64, ports: (u16, u16, u16)) -> Result<(), String> {
            self.conn.execute("INSERT INTO runs VALUES (?, ?, ?, ?, ?, ?, ?)",
                              params![self.instance, session.as_str(), seed, started_ms, ports.0, ports.1, ports.2])
                .map(|_| ()).map_err(e)
        }

        /// Envelopes (see `envelope`) as rows of `events`, in one transaction.
        pub fn insert_events(&mut self, envs: &[Value]) -> Result<usize, String> {
            let tx = self.conn.transaction().map_err(e)?;
            {
                let mut st = tx.prepare("INSERT INTO events VALUES (?, ?, ?, ?, ?, ?, ?)").map_err(e)?;
                for env in envs {
                    st.execute(params![self.instance, as_i64(env.get("seq")), as_i64(env.get("ts_ms")),
                                       env["kind"].as_str(), as_i64(env.get("game_tick")), env["game_date"].as_str(),
                                       serde_json::to_string(&env["data"]).unwrap_or_else(|_| "null".into())]).map_err(e)?;
                }
            }
            tx.commit().map_err(e)?;
            Ok(envs.len())
        }

        pub fn insert_samples(&mut self, rows: &[SampleRow]) -> Result<usize, String> {
            let tx = self.conn.transaction().map_err(e)?;
            {
                let mut st = tx.prepare("INSERT INTO samples VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)").map_err(e)?;
                for r in rows {
                    st.execute(params![self.instance, r.ts_ms, r.game_tick, r.game_date, r.company, r.name,
                                       r.money, r.loan, r.value, r.net, r.vehicles]).map_err(e)?;
                }
            }
            tx.commit().map_err(e)?;
            Ok(rows.len())
        }

        /// One statement (already passed `read_only_sql`): `{columns, rows, count, capped}`, each row
        /// a list in column order, at most `QUERY_CAP` rows.
        pub fn query(&self, sql: &str) -> Result<Value, String> {
            let mut st = self.conn.prepare(sql).map_err(e)?;
            let mut rows = st.query([]).map_err(e)?;
            let mut columns: Vec<String> = Vec::new();
            let mut out = Vec::new();
            let mut capped = false;
            while let Some(row) = rows.next().map_err(e)? {
                if columns.is_empty() {
                    columns = row.as_ref().column_names();
                }
                if out.len() == QUERY_CAP {
                    capped = true;
                    break;
                }
                let cells: Vec<Value> = (0..columns.len()).map(|i| row.get::<_, Dv>(i).map(to_json).unwrap_or(Value::Null)).collect();
                out.push(Value::Array(cells));
            }
            if columns.is_empty() {
                columns = rows.as_ref().map(|s| s.column_names()).unwrap_or_default();
            }
            Ok(json!({"columns": columns, "count": out.len(), "rows": out, "capped": capped}))
        }
    }

    /// Days since 1970-01-01 as YYYY-MM-DD (Howard Hinnant's civil_from_days).
    fn civil(days: i64) -> String {
        let z = days + 719_468;
        let era = z.div_euclid(146_097);
        let doe = z.rem_euclid(146_097);
        let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
        let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        let mp = (5 * doy + 2) / 153;
        let d = doy - (153 * mp + 2) / 5 + 1;
        let m = if mp < 10 { mp + 3 } else { mp - 9 };
        format!("{:04}-{:02}-{:02}", yoe + era * 400 + i64::from(m <= 2), m, d)
    }

    /// A DuckDB value as JSON: numbers as numbers (a chart reads them), dates and timestamps as ISO
    /// text, JSON columns as the text DuckDB holds, nested values nested.
    pub fn to_json(v: Dv) -> Value {
        match v {
            Dv::Null => Value::Null,
            Dv::Boolean(b) => json!(b),
            Dv::TinyInt(n) => json!(n),
            Dv::SmallInt(n) => json!(n),
            Dv::Int(n) => json!(n),
            Dv::BigInt(n) => json!(n),
            Dv::HugeInt(n) => i64::try_from(n).map(|n| json!(n)).unwrap_or_else(|_| json!(n.to_string())),
            Dv::UHugeInt(n) => u64::try_from(n).map(|n| json!(n)).unwrap_or_else(|_| json!(n.to_string())),
            Dv::UTinyInt(n) => json!(n),
            Dv::USmallInt(n) => json!(n),
            Dv::UInt(n) => json!(n),
            Dv::UBigInt(n) => json!(n),
            Dv::Float(f) => json!(f),
            Dv::Double(f) => json!(f),
            Dv::Decimal(d) => d.to_string().parse::<f64>().map(|f| json!(f)).unwrap_or_else(|_| json!(d.to_string())),
            Dv::Timestamp(unit, n) => {
                let us = unit.to_micros(n);
                let (days, rest) = (us.div_euclid(86_400_000_000), us.rem_euclid(86_400_000_000));
                let s = rest / 1_000_000;
                json!(format!("{}T{:02}:{:02}:{:02}.{:03}Z", civil(days), s / 3600, s / 60 % 60, s % 60, rest % 1_000_000 / 1000))
            }
            Dv::Date32(days) => json!(civil(i64::from(days))),
            Dv::Text(s) | Dv::Enum(s) => json!(s),
            Dv::List(xs) | Dv::Array(xs) => Value::Array(xs.into_iter().map(to_json).collect()),
            Dv::Struct(m) => Value::Object(m.iter().map(|(k, v)| (k.clone(), to_json(v.clone()))).collect()),
            Dv::Union(b) => to_json(*b),
            other => json!(format!("{other:?}")),
        }
    }
}

// ---------------------------------------------------------------------- the ZeroMQ sink

/// What the PUB thread is handed: (topic, envelope JSON).
type Outbox = tokio::sync::mpsc::UnboundedSender<(String, String)>;

/// Bind the PUB socket on its own thread and runtime; the endpoint it bound (the real port when 0 was
/// asked for) and the sender of the messages it publishes.
fn start_pub(port: u16) -> Result<(String, Outbox), String> {
    let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel::<(String, String)>();
    let (ready_tx, ready_rx) = mpsc::channel::<Result<String, String>>();
    thread::Builder::new().name("bus-pub".into()).spawn(move || {
        use zeromq::{Socket, SocketSend};
        let rt = match tokio::runtime::Builder::new_current_thread().enable_all().build() {
            Ok(rt) => rt,
            Err(err) => { let _ = ready_tx.send(Err(err.to_string())); return; }
        };
        rt.block_on(async move {
            let mut sock = zeromq::PubSocket::new();
            match sock.bind(&format!("tcp://127.0.0.1:{port}")).await {
                Ok(ep) => { let _ = ready_tx.send(Ok(ep.to_string())); }
                Err(err) => { let _ = ready_tx.send(Err(err.to_string())); return; }
            }
            while let Some((topic, body)) = rx.recv().await {
                if let Ok(msg) = zeromq::ZmqMessage::try_from(vec![bytes::Bytes::from(topic), bytes::Bytes::from(body)]) {
                    // A PUB socket drops what no subscriber takes; nothing to report here.
                    let _ = sock.send(msg).await;
                }
            }
        });
    }).map_err(|err| err.to_string())?;
    ready_rx.recv_timeout(Duration::from_secs(5)).map_err(|_| "the bus socket did not bind within 5 s".to_string())?.map(|ep| (ep, tx))
}

/// The port from PRISMATIC_BUS_PORT, else 0 (the OS picks one).
fn bus_port() -> u16 {
    std::env::var("PRISMATIC_BUS_PORT").ok().and_then(|p| p.trim().parse().ok()).unwrap_or(0)
}

// ---------------------------------------------------------------------- the tap

/// Start the tap for `game` as registry instance `instance`: bind the bus, open the store, record the
/// run, and follow the bus from its first event. Runs once per process; a sink that cannot start is
/// reported (`bus_error`, `store_error`) and the other still runs.
pub fn start(game: Arc<Game>, instance: &str) -> Arc<Tap> {
    let session = session_of();
    let port = bus_port();
    let bus = start_pub(port).or_else(|err| if port != 0 {
        println!("mcp-panel: bus port {port}: {err}; an ephemeral port instead");
        start_pub(0)
    } else { Err(err) });
    let (endpoint, outbox, bus_error) = match bus {
        Ok((ep, tx)) => (Some(ep), Some(tx), None),
        Err(err) => (None, None, Some(err)),
    };
    #[cfg(feature = "store")]
    let (store_path, writer, reader, store_error) = {
        let path = store_dir().join(format!("{instance}.duckdb"));
        let a = &game.args;
        let opened = db::Store::open(&path, instance).and_then(|w| {
            w.insert_run(&session, a.seed, now_ms(), (a.panel_port, a.mcp_port, a.game_port))?;
            let r = w.reader()?;
            Ok((w, r))
        });
        match opened {
            Ok((w, r)) => (Some(path), Some(w), Some(Mutex::new(r)), None),
            Err(err) => (None, None, None, Some(err)),
        }
    };
    #[cfg(not(feature = "store"))]
    let (store_path, store_error): (Option<PathBuf>, Option<String>) = (None, Some("built without the store feature".into()));
    let (done_tx, done_rx) = mpsc::channel();
    let tap = Arc::new(Tap {
        instance: instance.to_string(), session, endpoint, bus_error, store: store_path, store_error,
        published: AtomicU64::new(0), topics: Mutex::new(BTreeMap::new()),
        #[cfg(feature = "store")]
        reader,
        stopping: AtomicBool::new(false), done: Mutex::new(Some(done_rx)),
    });
    let _ = TAP.set(tap.clone());
    let t = tap.clone();
    #[cfg(feature = "store")]
    thread::spawn(move || { follow(&game, &t, outbox, writer); let _ = done_tx.send(()); });
    #[cfg(not(feature = "store"))]
    thread::spawn(move || { follow(&game, &t, outbox); let _ = done_tx.send(()); });
    tap
}

/// Commit what the tap holds and stop it; waits at most two seconds.
pub fn stop() {
    let Some(t) = tap() else { return };
    t.stopping.store(true, Ordering::SeqCst);
    if let Some(rx) = t.done.lock().unwrap().take() {
        let _ = rx.recv_timeout(Duration::from_secs(2));
    }
}

/// The tap's loop: every event after the cursor, in bus order, to both sinks; each game.sample
/// event (one per game day, published where the panel records its overview history) also to
/// `samples`, one row per company.
fn follow(game: &Game, tap: &Tap, outbox: Option<Outbox>, #[cfg(feature = "store")] mut writer: Option<db::Store>) {
    let mut cursor = 0u64;
    let mut events: Vec<Value> = Vec::new();
    let mut samples: Vec<SampleRow> = Vec::new();
    let mut last_commit = Instant::now();
    loop {
        let stopping = tap.stopping.load(Ordering::SeqCst);
        game.bus.wait_past(cursor, Duration::from_millis(250));
        let state = game.latest_sample();
        for e in game.bus.since(cursor, &[], 1000) {
            cursor = e["seq"].as_u64().unwrap_or(cursor);
            let env = envelope(&e, &tap.instance, &tap.session, game.args.seed, now_ms(), state.as_ref());
            let kind = env["kind"].as_str().unwrap_or("").to_string();
            if let Some(tx) = &outbox {
                let _ = tx.send((kind.clone(), serde_json::to_string(&env).unwrap_or_default()));
            }
            if kind == "game.sample" {
                samples.extend(sample_rows(&env["data"], env["ts_ms"].as_i64().unwrap_or(0)));
            }
            tap.published.fetch_add(1, Ordering::Relaxed);
            *tap.topics.lock().unwrap().entry(kind).or_insert(0) += 1;
            events.push(env);
        }
        if last_commit.elapsed() >= COMMIT_EVERY || stopping {
            #[cfg(feature = "store")]
            if let Some(w) = writer.as_mut() {
                if !events.is_empty() {
                    if let Err(err) = w.insert_events(&events) { eprintln!("mcp-panel: store: events: {err}"); }
                }
                if !samples.is_empty() {
                    if let Err(err) = w.insert_samples(&samples) { eprintln!("mcp-panel: store: samples: {err}"); }
                }
            }
            events.clear();
            samples.clear();
            last_commit = Instant::now();
        }
        if stopping {
            return;
        }
    }
}

// ---------------------------------------------------------------------- the routes

/// The bus endpoint, or null when the socket did not bind.
pub fn endpoint() -> Value {
    tap().and_then(|t| t.endpoint.clone()).map_or(Value::Null, Value::String)
}

/// How another process reads the store: DuckDB lets one process hold a database file open for
/// writing, and then no other process can open it, not even read-only (`duckdb -readonly` answers
/// "Conflicting lock is held"). So while the panel runs, readers go through the query routes or the
/// bus, and the file itself is for after the panel stopped.
pub const STORE_ACCESS: &str = "the panel holds the file's lock while it runs: read through GET|POST /api/db/query or the ZeroMQ bus; open the file directly only after the panel stopped";

/// `STORE_ACCESS` when a store is open, else null.
pub fn store_access() -> Value {
    tap().and_then(|t| t.store.as_ref()).map_or(Value::Null, |_| json!(STORE_ACCESS))
}

/// The store file, or "off" (built without the feature, or the file could not be opened).
pub fn store_status() -> Value {
    tap().and_then(|t| t.store.as_ref()).map_or(json!("off"), |p| json!(p.display().to_string()))
}

pub fn h_bus(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let Some(t) = tap() else { return Ok((503, json!({"error": "the bus tap is not running"}))) };
    let topics: serde_json::Map<String, Value> = t.topics.lock().unwrap().iter().map(|(k, n)| (k.clone(), json!(n))).collect();
    Ok((200, json!({"endpoint": t.endpoint, "schema": SCHEMA, "instance": t.instance, "session": t.session,
                    "topics_seen": topics, "published": t.published.load(Ordering::Relaxed), "error": t.bus_error,
                    "store": store_status(), "store_access": store_access(),
                    "frames": ["topic (the event kind)", "envelope (JSON, schema prismatic-bus/v1)"]})))
}

/// Run one read-only statement on the store: 400 for anything else, 503 without a store.
pub fn run_query(sql: &str) -> (u16, Value) {
    if let Err(err) = read_only_sql(sql) {
        return (400, json!({"error": err}));
    }
    #[cfg(feature = "store")]
    if let Some(r) = tap().and_then(|t| t.reader.as_ref()) {
        return match r.lock().unwrap().query(sql) {
            Ok(v) => (200, v),
            Err(err) => (400, json!({"error": err})),
        };
    }
    let why = tap().and_then(|t| t.store_error.clone()).unwrap_or_else(|| "the store is not open".into());
    (503, json!({"error": format!("store off: {why}")}))
}

pub fn h_db_tables(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let (code, out) = run_query("SELECT table_name, column_name, data_type FROM information_schema.columns \
                                 WHERE table_schema = 'main' ORDER BY table_name, ordinal_position");
    if code != 200 {
        return Ok((code, out));
    }
    let mut tables: BTreeMap<String, Vec<Value>> = BTreeMap::new();
    for row in out["rows"].as_array().into_iter().flatten() {
        let table = row[0].as_str().unwrap_or("").to_string();
        tables.entry(table).or_default().push(json!({"name": row[1], "type": row[2]}));
    }
    let tables: Vec<Value> = tables.into_iter().map(|(name, columns)| json!({"name": name, "columns": columns})).collect();
    Ok((200, json!({"store": store_status(), "tables": tables})))
}

/// What a GET without `?sql=` answers: the newest events, so the bare address shows what is there.
pub const DEFAULT_QUERY: &str = "SELECT seq, ts_ms, kind, game_tick, game_date, data FROM events ORDER BY ts_ms DESC, seq DESC LIMIT 20";

pub fn h_db_query(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let sql = i.body.get("sql").and_then(Value::as_str).or_else(|| i.query.get("sql")).unwrap_or(DEFAULT_QUERY);
    Ok(run_query(sql))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_one_read_only_statement_passes_the_guard() {
        for ok in ["SELECT 1", "  select count(*) from events", "WITH x AS (SELECT 1) SELECT * FROM x", "describe events",
                   "SUMMARIZE samples", "SHOW TABLES", "-- a note\nSELECT 1", "/* a\nnote */ SELECT 1;", "SELECT 1 ;  -- done",
                   "SELECT ';' AS semi", "SELECT \"a;b\" FROM t", "SELECT 'it''s; fine'", "SELECT E'a\\';b'"] {
            assert!(read_only_sql(ok).is_ok(), "{ok}: {:?}", read_only_sql(ok));
        }
        for bad in ["INSERT INTO events VALUES (1)", "drop table events", "ATTACH 'x.db'", "COPY events TO 'x.csv'",
                    "SELECT 1; DROP TABLE events", "SELECT 1;SELECT 2", "-- SELECT\nDELETE FROM events", "/* SELECT */ UPDATE x SET a=1",
                    "PRAGMA version", "SET threads=1", "", "   ", "-- only a comment", "SELECT E'\\''; DROP TABLE events",
                    "SELECT 1 /* ; */ ; INSTALL httpfs", "CALL checkpoint()"] {
            assert!(read_only_sql(bad).is_err(), "{bad} passed");
        }
    }

    #[test]
    fn an_envelope_is_prismatic_bus_v1_with_the_game_clock() {
        let e = json!({"seq": 7, "time": "2026-11-18T10:00:00+0100", "kind": "ai.log", "data": {"company": 1, "text": "hi"}});
        let state = json!({"t": 1234, "date": "1950-03-04"});
        let env = envelope(&e, "panel-9", &json!("s1"), 3, 99, Some(&state));
        assert_eq!(env, json!({"schema": "prismatic-bus/v1", "seq": 7, "ts_ms": 99, "instance": "panel-9", "session": "s1", "seed": 3,
                               "game_tick": 1234, "game_date": "1950-03-04", "kind": "ai.log", "data": {"company": 1, "text": "hi"}}));
        // The event's own date wins over the last sample's; no state at all is null.
        let month = json!({"seq": 8, "kind": "game.month", "data": {"date": "1950-04-01"}});
        assert_eq!(envelope(&month, "p", &Value::Null, 3, 1, Some(&state))["game_date"], "1950-04-01");
        let day = json!({"seq": 9, "kind": "game.sample", "data": {"t": 1300, "date": "1950-03-05", "companies": []}});
        let env = envelope(&day, "p", &Value::Null, 3, 1, Some(&state));
        assert_eq!((env["game_tick"].clone(), env["game_date"].clone()), (json!(1300), json!("1950-03-05")));
        let bare = envelope(&e, "p", &Value::Null, 3, 1, None);
        assert!(bare["game_tick"].is_null() && bare["game_date"].is_null() && bare["session"].is_null());
    }

    #[cfg(feature = "store")]
    #[test]
    fn events_and_samples_go_into_duckdb_and_come_back_by_query() {
        let mut st = db::Store::open_in_memory("panel-t").unwrap();
        st.insert_run(&json!("s1"), 3, 10, (1, 2, 3)).unwrap();
        let state = json!({"t": 50, "date": "1950-01-02"});
        let envs: Vec<Value> = (1..=3).map(|n| envelope(&json!({"seq": n, "kind": if n == 2 { "game.month" } else { "ai.log" },
                                                                  "data": {"text": format!("line {n}")}}),
                                                         "panel-t", &json!("s1"), 3, 100 + n, Some(&state))).collect();
        assert_eq!(st.insert_events(&envs).unwrap(), 3);
        let sample = json!({"t": 50, "date": "1950-01-02", "companies": [
            {"id": 0, "name": "A", "money": 10.0, "loan": 4.0, "value": 20.0, "net": 6.0, "fleet": 2},
            {"id": 1, "name": "B", "money": 1.5, "loan": 0.0, "value": 2.0, "net": 1.5, "fleet": null}]});
        assert_eq!(st.insert_samples(&sample_rows(&sample, 7)).unwrap(), 2);
        let r = st.reader().unwrap();
        let out = r.query("SELECT seq, kind, game_tick, game_date, data->>'text' AS text FROM events ORDER BY seq").unwrap();
        assert_eq!(out["columns"], json!(["seq", "kind", "game_tick", "game_date", "text"]));
        assert_eq!(out["count"], 3);
        assert_eq!(out["rows"][1], json!([2, "game.month", 50, "1950-01-02", "line 2"]));
        assert_eq!(out["capped"], false);
        let out = r.query("SELECT kind, count(*) AS n FROM events GROUP BY 1 ORDER BY 1").unwrap();
        assert_eq!(out["rows"], json!([["ai.log", 2], ["game.month", 1]]));
        let out = r.query("SELECT name, net, vehicles FROM samples ORDER BY company").unwrap();
        assert_eq!(out["rows"], json!([["A", 6.0, 2], ["B", 1.5, null]]));
        let out = r.query("SELECT session, seed, game_port FROM runs").unwrap();
        assert_eq!(out["rows"], json!([["s1", 3, 3]]));
        // An empty answer still names its columns; a large one is capped and says so.
        let out = r.query("SELECT kind FROM events WHERE false").unwrap();
        assert_eq!((out["columns"].clone(), out["count"].clone()), (json!(["kind"]), json!(0)));
        let out = r.query("SELECT * FROM range(6000)").unwrap();
        assert_eq!((out["count"].clone(), out["capped"].clone()), (json!(QUERY_CAP), json!(true)));
        // The store cannot reach outside itself, whatever passes the guard.
        assert!(r.query("SELECT * FROM read_csv('/etc/hosts')").is_err());
    }
}
