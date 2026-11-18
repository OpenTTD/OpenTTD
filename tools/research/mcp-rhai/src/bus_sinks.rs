//! RESEARCH-ONLY. The two sinks of an event bus (ADR 0033), shared by every publisher: the panel's
//! bus tap (src/panel_store.rs, I0080) and the hub's own events (src/hub.rs, I0084).
//!
//! - a ZeroMQ PUB socket on tcp://127.0.0.1:<port or an ephemeral one>, two frames per message: the
//!   topic (the event kind, so a SUB client filters by prefix) and the envelope, JSON of schema
//!   prismatic-bus/v1. The pure-Rust `zeromq` crate runs on a small tokio runtime of its own thread, so
//!   no system libzmq is needed;
//! - a DuckDB file per publisher ($PRISMATIC_STORE_DIR/<publisher>.duckdb, by default
//!   ~/.local/state/prismatic-mcp/store/) with the tables runs, events and samples, committed in
//!   batches at least twice a second (cargo feature `store`, linked against the system libduckdb).
//!
//! A publisher opens its `Sinks` once, hands each envelope to `send` and calls `commit` at least every
//! 250 ms. `events.instance` is the instance the event is about (the envelope's `instance`): for the
//! panel that is the panel itself, for the hub the game instance an audit or derived event names.

use std::collections::BTreeMap;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{mpsc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde_json::{json, Value};

/// The envelope's schema; also the answer of GET /api/bus.
pub const SCHEMA: &str = "prismatic-bus/v1";
/// The most rows one /api/db/query answers; more are cut and the answer says `capped`.
pub const QUERY_CAP: usize = 5000;
/// A batch is committed once it is this old: with `commit` called at least every 250 ms, every event
/// reaches the store well within a second.
pub const COMMIT_EVERY: Duration = Duration::from_millis(400);
/// How often a publisher calls `commit`.
pub const COMMIT_TICK: Duration = Duration::from_millis(250);

pub fn now_ms() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_millis() as i64).unwrap_or(0)
}

/// Who this run belongs to: the Claude Code session that started it, else the Majordomus task named
/// by PRISMATIC_MCP_SUPERVISOR (mcp-e2e sets it), else null.
pub fn session_of() -> Value {
    if let Some(s) = std::env::var("CLAUDE_CODE_SESSION_ID").ok().filter(|s| !s.is_empty()) {
        return json!(s);
    }
    std::env::var("PRISMATIC_MCP_SUPERVISOR").ok()
        .and_then(|s| serde_json::from_str::<Value>(&s).ok()).filter(Value::is_object)
        .and_then(|s| s.get("task").cloned()).filter(|t| !t.is_null()).unwrap_or(Value::Null)
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
/// sample of that day is taken; game.sample carries `t` and `date`), else the latest clock `state`
/// gives (`{t, date}`), else null. `seed` is null when the publisher does not know it (the hub).
pub fn envelope(e: &Value, instance: &str, session: &Value, seed: Option<i64>, ts_ms: i64, state: Option<&Value>) -> Value {
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

    /// The tables of ADR 0033, created when missing: one file holds every run of its publisher.
    pub const TABLES: &str = "
        CREATE TABLE IF NOT EXISTS runs(instance VARCHAR, session VARCHAR, seed BIGINT, started_ms BIGINT,
                                        panel_port INTEGER, mcp_port INTEGER, game_port INTEGER);
        CREATE TABLE IF NOT EXISTS events(instance VARCHAR, seq BIGINT, ts_ms BIGINT, kind VARCHAR,
                                          game_tick BIGINT, game_date VARCHAR, data JSON);
        CREATE TABLE IF NOT EXISTS samples(instance VARCHAR, ts_ms BIGINT, game_tick BIGINT, game_date VARCHAR,
                                           company BIGINT, name VARCHAR, money DOUBLE, loan DOUBLE, value DOUBLE,
                                           net DOUBLE, vehicles BIGINT);";

    /// One connection to the publisher's database: the writer holds one, the query routes read
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

        /// Another publisher's file, read-only: only when no process holds it open for writing (a
        /// running panel does, and is then read through its /api/db/query instead).
        pub fn open_read_only(path: &Path) -> Result<Store, String> {
            let cfg = config()?.access_mode(duckdb::AccessMode::ReadOnly).map_err(e)?;
            let conn = Connection::open_with_flags(path, cfg).map_err(|err| format!("{}: {err}", path.display()))?;
            let instance = path.file_stem().and_then(|s| s.to_str()).unwrap_or("").to_string();
            Ok(Store { conn, instance })
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

        /// One `runs` row for this publisher; a seed or port it does not have is null (the hub, an
        /// import with no game behind it).
        pub fn insert_run(&self, session: &Value, seed: Option<i64>, started_ms: i64,
                          ports: (Option<u16>, Option<u16>, Option<u16>)) -> Result<(), String> {
            self.conn.execute("INSERT INTO runs VALUES (?, ?, ?, ?, ?, ?, ?)",
                              params![self.instance, session.as_str(), seed, started_ms, ports.0, ports.1, ports.2])
                .map(|_| ()).map_err(e)
        }

        /// Envelopes (see `envelope`) as rows of `events`, in one transaction. The row's instance is
        /// the envelope's (the instance the event is about), else this store's.
        pub fn insert_events(&mut self, envs: &[Value]) -> Result<usize, String> {
            let tx = self.conn.transaction().map_err(e)?;
            {
                let mut st = tx.prepare("INSERT INTO events VALUES (?, ?, ?, ?, ?, ?, ?)").map_err(e)?;
                for env in envs {
                    st.execute(params![env["instance"].as_str().unwrap_or(&self.instance), as_i64(env.get("seq")),
                                       as_i64(env.get("ts_ms")), env["kind"].as_str(), as_i64(env.get("game_tick")),
                                       env["game_date"].as_str(),
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

// ---------------------------------------------------------------------- one publisher's sinks

/// The `runs` row a publisher records when its store opens.
pub struct Run {
    pub seed: Option<i64>,
    /// (panel_port, mcp_port, game_port); null for a port the publisher does not have.
    pub ports: (Option<u16>, Option<u16>, Option<u16>),
}

/// What is waiting for the next commit.
struct Pending {
    #[cfg(feature = "store")]
    writer: Option<db::Store>,
    events: Vec<Value>,
    samples: Vec<SampleRow>,
    last_commit: Instant,
}

/// One publisher's bus socket and store, with what went out.
pub struct Sinks {
    /// The publisher (`panel-8800`, `hub-8790`): it names the store file and the `runs` row.
    pub instance: String,
    pub session: Value,
    pub endpoint: Option<String>,
    pub bus_error: Option<String>,
    pub store: Option<PathBuf>,
    pub store_error: Option<String>,
    published: AtomicU64,
    topics: Mutex<BTreeMap<String, u64>>,
    outbox: Option<Outbox>,
    #[cfg(feature = "store")]
    reader: Option<Mutex<db::Store>>,
    pending: Mutex<Pending>,
}

/// How another process reads a store: DuckDB lets one process hold a database file open for
/// writing, and then no other process can open it, not even read-only (`duckdb -readonly` answers
/// "Conflicting lock is held"). So while the publisher runs, readers go through its query route or the
/// bus, and the file itself is for after it stopped.
pub const STORE_ACCESS: &str = "the process that writes the store holds the file's lock while it runs: read through GET|POST /api/db/query or the ZeroMQ bus; open the file directly only after it stopped";

/// What a GET without `?sql=` answers: the newest events, so the bare address shows what is there.
pub const DEFAULT_QUERY: &str = "SELECT seq, ts_ms, kind, game_tick, game_date, data FROM events ORDER BY ts_ms DESC, seq DESC LIMIT 20";

impl Sinks {
    /// Bind the bus on `bus_port` (an ephemeral port when that one is taken, or when 0) and open the
    /// store at `store` (none when None), recording `run`. A sink that cannot start is reported
    /// (`bus_error`, `store_error`) and the other still runs.
    pub fn open(instance: &str, session: Value, bus_port: u16, store: Option<PathBuf>, run: Run) -> Sinks {
        let bus = start_pub(bus_port).or_else(|err| if bus_port != 0 {
            println!("{instance}: bus port {bus_port}: {err}; an ephemeral port instead");
            start_pub(0)
        } else { Err(err) });
        let (endpoint, outbox, bus_error) = match bus {
            Ok((ep, tx)) => (Some(ep), Some(tx), None),
            Err(err) => (None, None, Some(err)),
        };
        #[cfg(feature = "store")]
        let (store_path, writer, reader, store_error) = match store {
            None => (None, None, None, Some("no store asked for".to_string())),
            Some(path) => {
                let opened = db::Store::open(&path, instance).and_then(|w| {
                    w.insert_run(&session, run.seed, now_ms(), run.ports)?;
                    let r = w.reader()?;
                    Ok((w, r))
                });
                match opened {
                    Ok((w, r)) => (Some(path), Some(w), Some(Mutex::new(r)), None),
                    Err(err) => (None, None, None, Some(err)),
                }
            }
        };
        #[cfg(not(feature = "store"))]
        let (store_path, store_error): (Option<PathBuf>, Option<String>) = {
            let _ = (store, run);
            (None, Some("built without the store feature".into()))
        };
        Sinks {
            instance: instance.to_string(), session, endpoint, bus_error, store: store_path, store_error,
            published: AtomicU64::new(0), topics: Mutex::new(BTreeMap::new()), outbox,
            #[cfg(feature = "store")]
            reader,
            pending: Mutex::new(Pending {
                #[cfg(feature = "store")]
                writer,
                events: Vec::new(), samples: Vec::new(), last_commit: Instant::now(),
            }),
        }
    }

    /// One envelope out: to ZeroMQ at once (topic = its kind), into the next store batch; a
    /// game.sample also as `samples` rows, one per company.
    pub fn send(&self, env: Value) {
        let kind = env["kind"].as_str().unwrap_or("").to_string();
        if let Some(tx) = &self.outbox {
            let _ = tx.send((kind.clone(), serde_json::to_string(&env).unwrap_or_default()));
        }
        self.published.fetch_add(1, Ordering::Relaxed);
        *self.topics.lock().unwrap().entry(kind.clone()).or_insert(0) += 1;
        if self.store.is_none() {
            return;
        }
        let mut p = self.pending.lock().unwrap();
        if kind == "game.sample" {
            p.samples.extend(sample_rows(&env["data"], env["ts_ms"].as_i64().unwrap_or(0)));
        }
        p.events.push(env);
    }

    /// Write the batch when it is `COMMIT_EVERY` old, or now when `force`.
    pub fn commit(&self, force: bool) {
        let mut p = self.pending.lock().unwrap();
        if !force && p.last_commit.elapsed() < COMMIT_EVERY {
            return;
        }
        let events = std::mem::take(&mut p.events);
        let samples = std::mem::take(&mut p.samples);
        #[cfg(feature = "store")]
        if let Some(w) = p.writer.as_mut() {
            if !events.is_empty() {
                if let Err(err) = w.insert_events(&events) { eprintln!("{}: store: events: {err}", self.instance); }
            }
            if !samples.is_empty() {
                if let Err(err) = w.insert_samples(&samples) { eprintln!("{}: store: samples: {err}", self.instance); }
            }
        }
        #[cfg(not(feature = "store"))]
        let _ = (events, samples);
        p.last_commit = Instant::now();
    }

    /// The store file, or "off" (built without the feature, none asked for, or it could not open).
    pub fn store_status(&self) -> Value {
        self.store.as_ref().map_or(json!("off"), |p| json!(p.display().to_string()))
    }

    /// `STORE_ACCESS` when a store is open, else null.
    pub fn store_access(&self) -> Value {
        self.store.as_ref().map_or(Value::Null, |_| json!(STORE_ACCESS))
    }

    /// The answer of GET /api/bus, without `kinds` (each publisher adds the kinds it declares).
    pub fn bus(&self) -> Value {
        let topics: serde_json::Map<String, Value> = self.topics.lock().unwrap().iter().map(|(k, n)| (k.clone(), json!(n))).collect();
        json!({"endpoint": self.endpoint, "schema": SCHEMA, "instance": self.instance, "session": self.session,
               "topics_seen": topics, "published": self.published.load(Ordering::Relaxed), "error": self.bus_error,
               "store": self.store_status(), "store_access": self.store_access(),
               "frames": ["topic (the event kind)", "envelope (JSON, schema prismatic-bus/v1)"]})
    }
}

/// The store files of `dir` other than `own`, by name.
pub fn other_stores(dir: &std::path::Path, own: &std::path::Path) -> Vec<PathBuf> {
    let mut files: Vec<PathBuf> = std::fs::read_dir(dir).map(|d| {
        d.filter_map(|e| e.ok().map(|e| e.path()))
            .filter(|p| p.extension().and_then(|x| x.to_str()) == Some("duckdb") && p.as_path() != own)
            .collect()
    }).unwrap_or_default();
    files.sort();
    files
}

/// Run one read-only statement on a publisher's store: 400 for anything else, 503 without a store.
pub fn query(sinks: Option<&Sinks>, sql: &str) -> (u16, Value) {
    if let Err(err) = read_only_sql(sql) {
        return (400, json!({"error": err}));
    }
    #[cfg(feature = "store")]
    if let Some(r) = sinks.and_then(|s| s.reader.as_ref()) {
        return match r.lock().unwrap().query(sql) {
            Ok(v) => (200, v),
            Err(err) => (400, json!({"error": err})),
        };
    }
    let why = sinks.and_then(|s| s.store_error.clone()).unwrap_or_else(|| "the store is not open".into());
    (503, json!({"error": format!("store off: {why}")}))
}

/// The store's schema: `{store, tables: [{name, columns: [{name, type}]}]}`.
pub fn tables(sinks: Option<&Sinks>) -> (u16, Value) {
    let (code, out) = query(sinks, "SELECT table_name, column_name, data_type FROM information_schema.columns \
                                    WHERE table_schema = 'main' ORDER BY table_name, ordinal_position");
    if code != 200 {
        return (code, out);
    }
    let mut tables: BTreeMap<String, Vec<Value>> = BTreeMap::new();
    for row in out["rows"].as_array().into_iter().flatten() {
        let table = row[0].as_str().unwrap_or("").to_string();
        tables.entry(table).or_default().push(json!({"name": row[1], "type": row[2]}));
    }
    let tables: Vec<Value> = tables.into_iter().map(|(name, columns)| json!({"name": name, "columns": columns})).collect();
    (200, json!({"store": sinks.map_or(json!("off"), Sinks::store_status), "tables": tables}))
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
        let env = envelope(&e, "panel-9", &json!("s1"), Some(3), 99, Some(&state));
        assert_eq!(env, json!({"schema": "prismatic-bus/v1", "seq": 7, "ts_ms": 99, "instance": "panel-9", "session": "s1", "seed": 3,
                               "game_tick": 1234, "game_date": "1950-03-04", "kind": "ai.log", "data": {"company": 1, "text": "hi"}}));
        // The event's own date wins over the last sample's; no state at all is null.
        let month = json!({"seq": 8, "kind": "game.month", "data": {"date": "1950-04-01"}});
        assert_eq!(envelope(&month, "p", &Value::Null, Some(3), 1, Some(&state))["game_date"], "1950-04-01");
        let day = json!({"seq": 9, "kind": "game.sample", "data": {"t": 1300, "date": "1950-03-05", "companies": []}});
        let env = envelope(&day, "p", &Value::Null, Some(3), 1, Some(&state));
        assert_eq!((env["game_tick"].clone(), env["game_date"].clone()), (json!(1300), json!("1950-03-05")));
        let bare = envelope(&e, "p", &Value::Null, None, 1, None);
        assert!(bare["game_tick"].is_null() && bare["game_date"].is_null() && bare["session"].is_null() && bare["seed"].is_null());
    }

    #[cfg(feature = "store")]
    #[test]
    fn events_and_samples_go_into_duckdb_and_come_back_by_query() {
        let mut st = db::Store::open_in_memory("panel-t").unwrap();
        st.insert_run(&json!("s1"), Some(3), 10, (Some(1), Some(2), Some(3))).unwrap();
        let state = json!({"t": 50, "date": "1950-01-02"});
        let envs: Vec<Value> = (1..=3).map(|n| envelope(&json!({"seq": n, "kind": if n == 2 { "game.month" } else { "ai.log" },
                                                                  "data": {"text": format!("line {n}")}}),
                                                         "panel-t", &json!("s1"), Some(3), 100 + n, Some(&state))).collect();
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

    #[cfg(feature = "store")]
    #[test]
    fn a_row_names_the_instance_its_envelope_is_about_and_a_publisher_without_ports_records_nulls() {
        let mut st = db::Store::open_in_memory("hub-1").unwrap();
        st.insert_run(&Value::Null, None, 10, (None, None, None)).unwrap();
        let about = envelope(&json!({"seq": 1, "kind": "audit.resolved", "data": {}}), "panel-8800", &Value::Null, None, 5, None);
        let bare = json!({"seq": 2, "kind": "x.y", "ts_ms": 6, "data": null});
        st.insert_events(&[about, bare]).unwrap();
        let out = st.query("SELECT instance, kind FROM events ORDER BY seq").unwrap();
        assert_eq!(out["rows"], json!([["panel-8800", "audit.resolved"], ["hub-1", "x.y"]]));
        let out = st.query("SELECT instance, seed, panel_port, mcp_port, game_port FROM runs").unwrap();
        assert_eq!(out["rows"], json!([["hub-1", null, null, null, null]]));
    }

    #[cfg(feature = "store")]
    #[test]
    fn sinks_batch_what_they_are_sent_and_commit_it() {
        let dir = std::env::temp_dir().join(format!("bus-sinks-{}-{}", std::process::id(), now_ms()));
        let s = Sinks::open("pub-t", json!("s"), 0, Some(dir.join("pub-t.duckdb")), Run { seed: Some(1), ports: (Some(9), None, None) });
        assert!(s.endpoint.as_deref().is_some_and(|e| e.starts_with("tcp://127.0.0.1:")), "{:?}", s.bus_error);
        assert!(s.store_error.is_none(), "{:?}", s.store_error);
        s.send(envelope(&json!({"seq": 1, "kind": "game.sample", "data": {"t": 1, "date": "1950-01-01",
                                 "companies": [{"id": 0, "name": "A", "money": 1.0}]}}), "pub-t", &json!("s"), Some(1), 1, None));
        assert_eq!(query(Some(&s), "SELECT count(*) FROM events").1["rows"], json!([[0]]), "not before the commit");
        s.commit(true);
        assert_eq!(query(Some(&s), "SELECT count(*) FROM events").1["rows"], json!([[1]]));
        assert_eq!(query(Some(&s), "SELECT name FROM samples").1["rows"], json!([["A"]]));
        assert_eq!(query(Some(&s), "DROP TABLE events").0, 400);
        assert_eq!(s.bus()["topics_seen"], json!({"game.sample": 1}));
        let (code, t) = tables(Some(&s));
        assert_eq!((code, t["tables"].as_array().map(Vec::len)), (200, Some(3)));
        assert_eq!(query(None, "SELECT 1").0, 503);
        let _ = std::fs::remove_dir_all(dir);
    }
}
