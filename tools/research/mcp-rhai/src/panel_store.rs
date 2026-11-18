//! RESEARCH-ONLY. The panel's bus tap (ADR 0033, issue I0080): one subscriber of the event bus
//! (`Bus` in src/panel.rs) that hands every event, in bus order, to the panel's two sinks
//! (src/bus_sinks.rs, which the hub's own events use too, I0084):
//!
//! - a ZeroMQ PUB socket on tcp://127.0.0.1:<PRISMATIC_BUS_PORT or an ephemeral port>, two frames per
//!   message: the topic (the event kind) and the envelope, JSON of schema prismatic-bus/v1;
//! - a DuckDB file per instance ($PRISMATIC_STORE_DIR/<instance>.duckdb, by default
//!   ~/.local/state/prismatic-mcp/store/) with the tables runs, events and samples, committed in
//!   batches at least twice a second (cargo feature `store`, linked against the system libduckdb).
//!
//! One producer was added for it: the overview's daily sample is published as game.sample (src/panel.rs
//! `record_sample`), because the history lived in its own ring and never reached the bus; every other
//! producer is unchanged. The tap is the one place to add a further sink. The read
//! surfaces (GET /api/bus, GET /api/db/tables, GET|POST /api/db/query) are rows of the panel's route
//! table, so they reach OpenAPI, Swagger UI and Rhai (`api_dbQuery(#{sql: "..."})`) like every other.

use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{mpsc, Arc, Mutex, OnceLock};
use std::thread;
use std::time::Duration;

use serde_json::{json, Value};

use crate::bus_sinks::{self, envelope, now_ms, other_stores, read_only_sql, session_of, store_dir, Run, Sinks, COMMIT_TICK};
use crate::panel::{Game, Input};

/// The running tap: the panel's sinks (src/bus_sinks.rs) and the thread that follows the bus.
pub struct Tap {
    pub sinks: Sinks,
    stopping: AtomicBool,
    done: Mutex<Option<mpsc::Receiver<()>>>,
}

static TAP: OnceLock<Arc<Tap>> = OnceLock::new();

/// The running tap, once `start` has run (never in unit tests of other modules).
pub fn tap() -> Option<&'static Arc<Tap>> {
    TAP.get()
}

fn sinks() -> Option<&'static Sinks> {
    tap().map(|t| &t.sinks)
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
    let a = &game.args;
    let store: Option<PathBuf> = Some(store_dir().join(format!("{instance}.duckdb")));
    let run = Run { seed: Some(a.seed), ports: (Some(a.panel_port), Some(a.mcp_port), Some(a.game_port)) };
    let sinks = Sinks::open(instance, session_of(), bus_port(), store, run);
    let (done_tx, done_rx) = mpsc::channel();
    let tap = Arc::new(Tap { sinks, stopping: AtomicBool::new(false), done: Mutex::new(Some(done_rx)) });
    let _ = TAP.set(tap.clone());
    let t = tap.clone();
    thread::spawn(move || { follow(&game, &t); let _ = done_tx.send(()); });
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

/// The tap's loop: every event after the cursor, in bus order, to both sinks (a game.sample, one
/// per game day, also to `samples`, one row per company), committed at least twice a second.
fn follow(game: &Game, tap: &Tap) {
    let mut cursor = 0u64;
    loop {
        let stopping = tap.stopping.load(Ordering::SeqCst);
        game.bus.wait_past(cursor, COMMIT_TICK);
        let state = game.latest_sample();
        for e in game.bus.since(cursor, &[], 1000) {
            cursor = e["seq"].as_u64().unwrap_or(cursor);
            tap.sinks.send(envelope(&e, &tap.sinks.instance, &tap.sinks.session, Some(game.args.seed), now_ms(), state.as_ref()));
        }
        tap.sinks.commit(stopping);
        if stopping {
            return;
        }
    }
}

// ---------------------------------------------------------------------- the routes

/// The bus endpoint, or null when the socket did not bind.
pub fn endpoint() -> Value {
    sinks().and_then(|s| s.endpoint.clone()).map_or(Value::Null, Value::String)
}

/// `STORE_ACCESS` when a store is open, else null.
pub fn store_access() -> Value {
    sinks().map_or(Value::Null, Sinks::store_access)
}

/// The store file, or "off" (built without the feature, or the file could not be opened).
pub fn store_status() -> Value {
    sinks().map_or(json!("off"), Sinks::store_status)
}

pub fn h_bus(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    match sinks() {
        None => Ok((503, json!({"error": "the bus tap is not running"}))),
        Some(s) => Ok((200, s.bus())),
    }
}

// ---------------------------------------------------------------------- every store (I0082)

/// One read-only statement answered by every store of the store directory (ADR 0034 point 1: "or,
/// across instances, of each instance's store"): this panel's own through `run_query` (the code of
/// GET|POST /api/db/query), every other file opened read-only, and a file another running panel
/// holds through that panel's /api/db/query (its registry record names its url). The rows of every
/// store in one list, and per store where it was read and how many rows it gave, or why not.
pub struct Across {
    pub columns: Value,
    pub rows: Vec<Value>,
    pub stores: Vec<Value>,
}

/// `Err` when this panel's store is off: then no history is read at all (each decision states its
/// fallback).
pub fn query_every_store(sql: &str) -> Result<Across, String> {
    read_only_sql(sql)?;
    let own = sinks().and_then(|s| s.store.clone());
    let Some(own) = own else {
        let why = sinks().and_then(|s| s.store_error.clone()).unwrap_or_else(|| "the store is not open".into());
        return Err(format!("store off: {why}"));
    };
    let (code, out) = run_query(sql);
    if code != 200 {
        return Err(format!("{}: {}", own.display(), py_err(&out)));
    }
    let mut across = Across { columns: out["columns"].clone(), rows: Vec::new(), stores: Vec::new() };
    across.add(&own, "this panel", Ok(out));
    let dir = own.parent().map(PathBuf::from).unwrap_or_else(store_dir);
    for path in other_stores(&dir, &own) {
        let answer = query_other(&path, sql);
        let via = answer.as_ref().map(|(via, _)| via.clone()).unwrap_or_default();
        across.add(&path, &via, answer.map(|(_, a)| a));
    }
    Ok(across)
}

impl Across {
    fn add(&mut self, path: &std::path::Path, via: &str, answer: Result<Value, String>) {
        let store = path.file_name().and_then(|s| s.to_str()).unwrap_or("").to_string();
        match answer {
            Ok(a) => {
                let rows = a["rows"].as_array().cloned().unwrap_or_default();
                // Every store answers the same statement, so the columns agree; the first names them.
                if self.columns.is_null() || self.columns.as_array().is_some_and(Vec::is_empty) { self.columns = a["columns"].clone(); }
                self.stores.push(json!({"store": store, "via": via, "rows": rows.len(), "capped": a["capped"]}));
                self.rows.extend(rows);
            }
            Err(err) => self.stores.push(json!({"store": store, "via": via, "error": err})),
        }
    }
}

fn py_err(v: &Value) -> String {
    v.get("error").and_then(Value::as_str).map_or_else(|| v.to_string(), str::to_string)
}

/// One other store: opened read-only, else through the running panel that holds it.
fn query_other(path: &std::path::Path, sql: &str) -> Result<(String, Value), String> {
    #[cfg(feature = "store")]
    let opened = bus_sinks::db::Store::open_read_only(path).and_then(|st| st.query(sql));
    #[cfg(not(feature = "store"))]
    let opened: Result<Value, String> = Err("built without the store feature".into());
    let locked = match opened {
        Ok(a) => return Ok(("file (read-only)".into(), a)),
        Err(err) => err,
    };
    let name = path.file_stem().and_then(|s| s.to_str()).unwrap_or("");
    let record: Option<Value> = std::fs::read_to_string(crate::panel::registry().join(format!("{name}.json"))).ok()
        .and_then(|t| serde_json::from_str(&t).ok());
    let Some(url) = record.as_ref().and_then(|r| r["url"].as_str()).map(|u| u.trim_end_matches('/').to_string()) else {
        return Err(format!("not readable ({locked}) and no running panel {name} in the registry"));
    };
    let via = format!("{url}/api/db/query");
    let answer: Value = match ureq::post(&via).timeout(Duration::from_secs(10)).send_json(json!({"sql": sql})) {
        Ok(r) => r.into_json().map_err(|err| format!("{via}: {err}"))?,
        Err(ureq::Error::Status(code, r)) => {
            let body: Value = r.into_json().unwrap_or(Value::Null);
            return Err(format!("{via}: HTTP {code}: {}", py_err(&body)));
        }
        Err(err) => return Err(format!("{via}: {err}")),
    };
    Ok((via, answer))
}

/// The instance (and file stem) the one import of the retired calibration file writes.
pub const IMPORT_INSTANCE: &str = "calibration-import";

/// YYYY-MM-DDTHH:MM[:SS]Z as milliseconds since 1970 (Howard Hinnant's days_from_civil).
pub fn utc_ms(at: &str) -> Option<i64> {
    let at = at.strip_suffix('Z')?;
    let (date, time) = at.split_once('T')?;
    let d: Vec<i64> = date.split('-').map(|p| p.parse().ok()).collect::<Option<_>>()?;
    let t: Vec<i64> = time.split(':').map(|p| p.parse().ok()).collect::<Option<_>>()?;
    if d.len() != 3 || !(2..=3).contains(&t.len()) { return None; }
    let (y, m, day) = (if d[1] <= 2 { d[0] - 1 } else { d[0] }, d[1], d[2]);
    let era = y.div_euclid(400);
    let yoe = y - era * 400;
    let doy = (153 * (if m > 2 { m - 3 } else { m + 9 }) + 2) / 5 + day - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    let days = era * 146_097 + doe - 719_468;
    Some(((days * 24 + t[0]) * 60 + t[1]) * 60_000 + t.get(2).copied().unwrap_or(0) * 1000)
}

/// The plan.calibration events of an import: one per reading of the retired file, each the payload
/// every plan.* event carries, at the time the reading was taken (`at`).
pub fn import_events(file: &Value, from: &str) -> Result<Vec<Value>, String> {
    let readings = crate::calibrate::file_readings(file)?;
    readings.iter().enumerate().map(|(i, r)| {
        let at = r["at"].as_str().ok_or("a reading without at")?;
        let ts = utc_ms(at).ok_or_else(|| format!("{at}: not YYYY-MM-DDTHH:MMZ"))?;
        let lines = crate::calibrate::measured(&r["lines"]);
        let data = crate::panel_events::plan("calibration.import", json!({"schema": file["schema"], "rule": file["rule"]}),
            json!({"imported_from": from, "at": at, "source": r["source"], "game": {"seed": null, "map": null}}),
            &crate::panel_events::Stored::Fallback(format!("imported once from {from} (I0082): the file had no seed, map or run attached")),
            json!({"reading": {"lines": lines, "modes": r["modes"]}, "readings": 1,
                   "factors": crate::calibrate::factors(&[json!({"lines": lines})])["params"]["calibration"]}));
        Ok(envelope(&json!({"seq": i + 1, "kind": "plan.calibration", "data": data}), IMPORT_INSTANCE, &json!("import"), None, ts, None))
    }).collect()
}

/// Import the retired calibration file once into `dir/calibration-import.duckdb`: a run of its own
/// (no seed, no ports) and one plan.calibration event per reading. A reading already there (same
/// `at`) is skipped, so a second import changes nothing.
#[cfg(feature = "store")]
pub fn import_calibration(dir: &std::path::Path, file: &Value, from: &str) -> Result<Value, String> {
    let envs = import_events(file, from)?;
    let path = dir.join(format!("{IMPORT_INSTANCE}.duckdb"));
    let mut st = bus_sinks::db::Store::open(&path, IMPORT_INSTANCE)?;
    let have = st.query("SELECT json_extract_string(data, '$.inputs.live.at') FROM events WHERE kind = 'plan.calibration'")?;
    let seen: Vec<Value> = have["rows"].as_array().into_iter().flatten().map(|r| r[0].clone()).collect();
    let fresh: Vec<Value> = envs.into_iter().filter(|e| !seen.contains(&e["data"]["inputs"]["live"]["at"])).collect();
    if let Some(first) = fresh.first() {
        st.insert_run(&json!("import"), None, first["ts_ms"].as_i64().unwrap_or(0), (None, None, None))?;
        st.insert_events(&fresh)?;
    }
    Ok(json!({"store": path.display().to_string(), "imported": fresh.len(), "already": seen.len()}))
}

/// Run one read-only statement on the store: 400 for anything else, 503 without a store.
pub fn run_query(sql: &str) -> (u16, Value) {
    bus_sinks::query(sinks(), sql)
}

pub fn h_db_tables(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    Ok(bus_sinks::tables(sinks()))
}

pub fn h_db_query(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let sql = i.body.get("sql").and_then(Value::as_str).or_else(|| i.query.get("sql")).unwrap_or(bus_sinks::DEFAULT_QUERY);
    Ok(run_query(sql))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[cfg(feature = "store")]
    use crate::bus_sinks::db;

    #[test]
    fn an_import_time_is_utc_milliseconds() {
        assert_eq!(utc_ms("1970-01-01T00:00Z"), Some(0));
        assert_eq!(utc_ms("2026-11-17T16:55Z"), Some(1_794_934_500_000));
        assert_eq!(utc_ms("2000-03-01T00:00:01Z"), Some(951_868_801_000));
        assert_eq!(utc_ms("2026-11-17 16:55"), None);
    }

    /// A plan.calibration envelope of a game, as the tap writes it.
    fn calibration_env(instance: &str, seq: i64, ts: i64, map: &str, lines: Value) -> Value {
        let data = crate::panel_events::plan("calibration.record", json!({}), json!({"game": {"seed": 3, "map": map}}),
                                             &crate::panel_events::Stored::Fallback("x".into()), json!({"reading": {"lines": lines}}));
        envelope(&json!({"seq": seq, "kind": "plan.calibration", "data": data}), instance, &Value::Null, Some(3), ts, None)
    }

    #[cfg(feature = "store")]
    #[test]
    fn the_calibration_query_reads_the_readings_of_a_store_by_seed_and_map() {
        use crate::calibrate::{factors, game_of, readings_of, readings_sql, Scope};
        let mut st = db::Store::open_in_memory("panel-t").unwrap();
        // Two runs of one instance: seed 3 from t=10, seed 9 from t=1000.
        st.insert_run(&json!("s1"), Some(3), 10, (Some(1), Some(2), Some(3))).unwrap();
        st.insert_run(&json!("s2"), Some(9), 1000, (Some(1), Some(2), Some(3))).unwrap();
        st.insert_events(&[
            calibration_env("panel-t", 1, 100, "256x256", json!([{"mode": "bus", "ratio": 0.2}, {"mode": "rail", "why": "far"}])),
            calibration_env("panel-t", 2, 200, "128x128", json!([{"mode": "bus", "ratio": 0.9}])),
            calibration_env("panel-t", 3, 1100, "256x256", json!([{"mode": "bus", "ratio": 0.4}])),
            envelope(&json!({"seq": 4, "kind": "ai.log", "data": {"text": "not a reading"}}), "panel-t", &Value::Null, Some(9), 1200, None),
        ]).unwrap();
        let game = game_of(3, &json!({"map_width": 256, "map_height": 256}));
        let same = st.query(&readings_sql(&Scope::Seed, &game).unwrap()).unwrap();
        let r = readings_of(&same);
        assert_eq!(r.len(), 1, "seed 3 on 256x256 only: {same}");
        assert_eq!((r[0]["seq"].clone(), r[0]["seed"].clone(), r[0]["map"].clone()), (json!(1), json!(3), json!("256x256")));
        let all = st.query(&readings_sql(&Scope::All, &game).unwrap()).unwrap();
        let r = readings_of(&all);
        assert_eq!(r.iter().map(|x| x["seq"].as_i64().unwrap()).collect::<Vec<_>>(), vec![3, 2, 1], "newest first");
        assert_eq!(r[0]["seed"], 9, "the run an event belongs to is the newest started before it");
        assert_eq!(factors(&r)["params"]["calibration"]["bus"], json!(0.4), "median of 0.2, 0.9, 0.4");
    }

    #[cfg(feature = "store")]
    #[test]
    fn the_retired_file_is_imported_once_as_a_run_of_its_own_and_read_back_read_only() {
        use crate::calibrate::{factors, game_of, readings_of, readings_sql, Scope};
        let dir = std::env::temp_dir().join(format!("i0082-store-{}-{}", std::process::id(), now_ms()));
        let file = json!({"schema": "openttd-research/calibration/v1", "rule": "r", "readings": [{"at": "2026-11-17T16:55Z", "source": "panel :8800",
            "modes": {}, "lines": [{"mode": "freight_rail", "ratio": 0.59}, {"mode": "bus", "ratio": 0.24}, {"mode": "freight_rail", "ratio": 0.78},
                                   {"mode": "bus", "ratio": 0.08}, {"mode": "freight_rail", "ratio": 0.49}, {"mode": "rail", "ratio": 0.15}]}]});
        let first = import_calibration(&dir, &file, "old file").unwrap();
        assert_eq!((first["imported"].clone(), first["already"].clone()), (json!(1), json!(0)));
        let again = import_calibration(&dir, &file, "old file").unwrap();
        assert_eq!((again["imported"].clone(), again["already"].clone()), (json!(0), json!(1)), "once");
        let path = dir.join(format!("{IMPORT_INSTANCE}.duckdb"));
        let st = db::Store::open_read_only(&path).unwrap();
        let runs = st.query("SELECT instance, session, seed, started_ms, panel_port FROM runs").unwrap();
        assert_eq!(runs["rows"], json!([[IMPORT_INSTANCE, "import", null, 1_794_934_500_000i64, null]]), "a run of its own, no seed, no ports");
        let all = st.query(&readings_sql(&Scope::All, &json!({"seed": 3, "map": null})).unwrap()).unwrap();
        let r = readings_of(&all);
        assert_eq!(r.len(), 1);
        assert_eq!(r[0]["instance"], IMPORT_INSTANCE);
        assert_eq!(factors(&r)["params"]["calibration"], json!({"bus": 0.16, "default": 0.37, "freight_rail": 0.59, "rail": 0.15}),
                   "the factors the file held");
        let seed = st.query(&readings_sql(&Scope::Seed, &game_of(3, &json!({"map_width": 256, "map_height": 256}))).unwrap()).unwrap();
        assert_eq!(seed["count"], 0, "no seed or map was attached to the file's reading");
        assert!(st.query("DELETE FROM events").is_err(), "read-only");
        drop(st);
        assert_eq!(other_stores(&dir, &dir.join("panel-1.duckdb")), vec![path.clone()]);
        assert!(other_stores(&dir, &path).is_empty());
        let _ = std::fs::remove_dir_all(&dir);
    }
}
