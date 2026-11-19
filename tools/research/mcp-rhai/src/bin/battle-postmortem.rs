//! RESEARCH-ONLY: the cross-match post-mortem of the LAN AI battle (research/battle-postmortem.md).
//!
//! Reads the battle's machine store, `~/ttd-battle/results/battle.sqlite` (written by `battle.py`,
//! opened read-only here), analyses every finished match of an edition in parallel (rayon, one task
//! per match, src/postmortem.rs), and writes the edition's post-mortem:
//!   research/results/postmortem-<edition>.json   the analysis (schema battle-postmortem/v1)
//!   research/results/postmortem-<edition>.md     derived from the JSON only
//!   ~/ttd-battle/results/analytics.duckdb        tables analyses, contestants, proposals (all editions)
//! An edition from before the store existed is read from its JSON match records instead
//! (`--matches <dir>`, default `~/ttd-battle/results/<edition>/matches`).
//!
//! Usage:
//!   battle-postmortem [--edition E | --newest] [--db PATH] [--config battle.json] [--out DIR]
//!                     [--duckdb PATH | --no-duckdb] [--matches DIR] [--json]
//!   battle-postmortem check [--out DIR]     every postmortem-*.md is what its .json derives (exit 11 if not)
//!   battle-postmortem editions [--db PATH]  the editions in the store, newest first
//! Exit codes: 0 ok, 2 usage, 11 a derived file is stale, 12 no data for the edition.
#[path = "../postmortem.rs"]
mod postmortem;

use rayon::prelude::*;
use serde_json::{json, Value};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::time::Instant;

fn home() -> PathBuf {
    PathBuf::from(std::env::var("HOME").unwrap_or_else(|_| ".".into()))
}

fn repo_root() -> PathBuf {
    let out = std::process::Command::new("git").args(["rev-parse", "--show-toplevel"]).output();
    match out {
        Ok(o) if o.status.success() => PathBuf::from(String::from_utf8_lossy(&o.stdout).trim()),
        _ => PathBuf::from("."),
    }
}

struct Args {
    cmd: String,
    edition: Option<String>,
    db: PathBuf,
    config: PathBuf,
    out: PathBuf,
    duckdb: Option<PathBuf>,
    matches: Option<PathBuf>,
    json: bool,
}

fn usage(msg: &str) -> ! {
    eprintln!("battle-postmortem: {msg}\nusage: battle-postmortem [check|editions] [--edition E|--newest] [--db PATH] \
               [--config battle.json] [--out DIR] [--duckdb PATH|--no-duckdb] [--matches DIR] [--json]");
    std::process::exit(2)
}

fn args() -> Args {
    let root = repo_root();
    let mut a = Args {
        cmd: "run".into(),
        edition: None,
        db: home().join("ttd-battle/results/battle.sqlite"),
        config: root.join("tools/research/ai-battle/battle.json"),
        out: root.join("research/results"),
        duckdb: Some(home().join("ttd-battle/results/analytics.duckdb")),
        matches: None,
        json: false,
    };
    let mut it = std::env::args().skip(1);
    while let Some(x) = it.next() {
        let mut val = |name: &str| it.next().unwrap_or_else(|| usage(&format!("{name} needs a value")));
        match x.as_str() {
            "check" | "editions" | "run" => a.cmd = x,
            "--edition" => a.edition = Some(val("--edition")),
            "--newest" => a.edition = None,
            "--db" => a.db = PathBuf::from(val("--db")),
            "--config" => a.config = PathBuf::from(val("--config")),
            "--out" => a.out = PathBuf::from(val("--out")),
            "--duckdb" => a.duckdb = Some(PathBuf::from(val("--duckdb"))),
            "--no-duckdb" => a.duckdb = None,
            "--matches" => a.matches = Some(PathBuf::from(val("--matches"))),
            "--json" => a.json = true,
            "-h" | "--help" => usage("help"),
            other => usage(&format!("unknown argument {other}")),
        }
    }
    a
}

/// The contestants of battle.json by key (label, else ai): their `ai` and `settings`.
fn contestants(config: &Path) -> BTreeMap<String, Value> {
    let mut out = BTreeMap::new();
    if let Ok(text) = std::fs::read_to_string(config) {
        if let Ok(v) = serde_json::from_str::<Value>(&text) {
            for c in v["contestants"].as_array().into_iter().flatten() {
                let key = c.get("label").or_else(|| c.get("ai")).and_then(Value::as_str).unwrap_or("").to_string();
                out.insert(key, c.clone());
            }
        }
    }
    out
}

fn open_ro(db: &Path) -> Result<rusqlite::Connection, String> {
    rusqlite::Connection::open_with_flags(db, rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY | rusqlite::OpenFlags::SQLITE_OPEN_NO_MUTEX)
        .map_err(|e| format!("{}: {e}", db.display()))
}

/// The editions in the store, newest first (by the latest match start).
fn editions(db: &Path) -> Result<Vec<String>, String> {
    let c = open_ro(db)?;
    let mut st = c.prepare("SELECT edition FROM matches GROUP BY edition ORDER BY max(started_at) DESC").map_err(|e| e.to_string())?;
    let rows = st.query_map([], |r| r.get::<_, String>(0)).map_err(|e| e.to_string())?;
    rows.collect::<Result<Vec<_>, _>>().map_err(|e| e.to_string())
}

/// A lineup entry: the store's lineup holds a key, or an object with key/ai/settings; battle.json
/// fills in what a bare key lacks.
fn lineup_entry(x: &Value, cfg: &BTreeMap<String, Value>) -> Value {
    let key = x.as_str().map(String::from).or_else(|| x.get("key").and_then(Value::as_str).map(String::from)).unwrap_or_default();
    let c = cfg.get(&key);
    let ai = x.get("ai").and_then(Value::as_str).or_else(|| c.and_then(|c| c["ai"].as_str())).unwrap_or(&key).to_string();
    let settings = x.get("settings").and_then(Value::as_str).or_else(|| c.and_then(|c| c["settings"].as_str())).unwrap_or("").to_string();
    json!({"key": key, "ai": ai, "settings": settings})
}

/// Every match of an edition from the store, shaped like battle.py's match record (id, status,
/// replica_of, lineup, standings, series) so src/postmortem.rs reads one shape.
fn matches_from_store(db: &Path, edition: &str, cfg: &BTreeMap<String, Value>) -> Result<Vec<Value>, String> {
    let c = open_ro(db)?;
    let e = |e: rusqlite::Error| e.to_string();
    let mut out: BTreeMap<String, Value> = BTreeMap::new();
    let mut st = c.prepare("SELECT id, host, seed, status, replica_of, lineup FROM matches WHERE edition = ?1").map_err(e)?;
    let rows = st.query_map([edition], |r| Ok((r.get::<_, String>(0)?, r.get::<_, Option<String>>(1)?, r.get::<_, Option<i64>>(2)?,
        r.get::<_, Option<String>>(3)?, r.get::<_, Option<String>>(4)?, r.get::<_, Option<String>>(5)?))).map_err(e)?;
    for row in rows {
        let (id, host, seed, status, replica, lineup) = row.map_err(e)?;
        let lineup: Value = lineup.and_then(|l| serde_json::from_str(&l).ok()).unwrap_or(json!([]));
        let lineup: Vec<Value> = lineup.as_array().into_iter().flatten().map(|x| lineup_entry(x, cfg)).collect();
        out.insert(id.clone(), json!({"id": id, "edition": edition, "host": host, "seed": seed, "status": status,
            "replica_of": replica, "lineup": lineup, "standings": [], "series": {}}));
    }
    let mut st = c.prepare("SELECT match_id, contestant, value, vehicles, stations FROM standings WHERE edition = ?1 ORDER BY match_id, rank").map_err(e)?;
    let rows = st.query_map([edition], |r| Ok((r.get::<_, String>(0)?, r.get::<_, String>(1)?, r.get::<_, Option<f64>>(2)?,
        r.get::<_, Option<f64>>(3)?, r.get::<_, Option<f64>>(4)?))).map_err(e)?;
    for row in rows {
        let (m, k, v, veh, sta) = row.map_err(e)?;
        if let Some(rec) = out.get_mut(&m) {
            rec["standings"].as_array_mut().unwrap().push(json!({"key": k, "value": v, "vehicles": veh, "stations": sta}));
        }
    }
    let mut st = c.prepare("SELECT match_id, contestant, t, value, money, income, vehicles, stations FROM samples WHERE edition = ?1 ORDER BY match_id, contestant, t").map_err(e)?;
    let rows = st.query_map([edition], |r| Ok((r.get::<_, String>(0)?, r.get::<_, String>(1)?, r.get::<_, String>(2)?,
        r.get::<_, Option<f64>>(3)?, r.get::<_, Option<f64>>(4)?, r.get::<_, Option<f64>>(5)?, r.get::<_, Option<f64>>(6)?, r.get::<_, Option<f64>>(7)?))).map_err(e)?;
    for row in rows {
        let (m, k, t, v, money, income, veh, sta) = row.map_err(e)?;
        if let Some(rec) = out.get_mut(&m) {
            let series = rec["series"].as_object_mut().unwrap();
            let list = series.entry(k).or_insert_with(|| json!([]));
            list.as_array_mut().unwrap().push(json!({"t": t, "value": v, "money": money, "income": income, "vehicles": veh, "stations": sta}));
        }
    }
    Ok(out.into_values().collect())
}

/// Every match record of an edition from its JSON files, read in parallel.
fn matches_from_json(dir: &Path, cfg: &BTreeMap<String, Value>) -> Result<Vec<Value>, String> {
    let mut files: Vec<PathBuf> = std::fs::read_dir(dir).map_err(|e| format!("{}: {e}", dir.display()))?
        .filter_map(|e| e.ok().map(|e| e.path())).filter(|p| p.extension().is_some_and(|x| x == "json")).collect();
    files.sort();
    Ok(files.par_iter().filter_map(|p| {
        let mut m: Value = serde_json::from_str(&std::fs::read_to_string(p).ok()?).ok()?;
        let lineup: Vec<Value> = m["lineup"].as_array().into_iter().flatten().map(|x| lineup_entry(x, cfg)).collect();
        m["lineup"] = json!(lineup);
        Some(m)
    }).collect())
}

#[cfg(feature = "store")]
fn write_duckdb(path: &Path, pm: &Value) -> Result<(), String> {
    use duckdb::{params, Connection};
    let e = |e: duckdb::Error| e.to_string();
    let mut c = Connection::open(path).map_err(|err| format!("{}: {err}", path.display()))?;
    c.execute_batch("
        CREATE TABLE IF NOT EXISTS analyses(edition VARCHAR, match_id VARCHAR, contestant VARCHAR, rank INTEGER, field INTEGER,
            last BOOLEAN, value DOUBLE, value_share_of_winner DOUBLE, peak_vehicles DOUBLE, final_vehicles DOUBLE,
            idle_cash_share DOUBLE, income_per_vehicle DOUBLE, stations_per_vehicle DOUBLE, gap_opens VARCHAR, causes VARCHAR[]);
        CREATE TABLE IF NOT EXISTS contestants(edition VARCHAR, contestant VARCHAR, prismatic BOOLEAN, settings VARCHAR,
            matches INTEGER, avg_rank DOUBLE, last_place INTEGER, avg_value_share_of_winner DOUBLE, avg_idle_cash_share DOUBLE,
            median_gap_opens VARCHAR, causes JSON);
        CREATE TABLE IF NOT EXISTS proposals(edition VARCHAR, contestant VARCHAR, settings VARCHAR, because JSON);").map_err(e)?;
    let ed = pm["edition"].as_str().unwrap_or("");
    let tx = c.transaction().map_err(e)?;
    for t in ["analyses", "contestants", "proposals"] {
        tx.execute(&format!("DELETE FROM {t} WHERE edition = ?"), params![ed]).map_err(e)?;
    }
    {
        let mut st = tx.prepare("INSERT INTO analyses VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?, string_split(?, ','))").map_err(e)?;
        for a in pm["analyses"].as_array().into_iter().flatten() {
            let causes = a["losses"].as_array().into_iter().flatten().filter_map(|l| l["cause"].as_str()).collect::<Vec<_>>().join(",");
            st.execute(params![ed, a["match"].as_str(), a["key"].as_str(), a["rank"].as_i64(), a["field"].as_i64(),
                a["last"].as_bool(), a["value"].as_f64(), a["value_share_of_winner"].as_f64(), a["peak_vehicles"].as_f64(),
                a["final_vehicles"].as_f64(), a["idle_cash_share"].as_f64(), a["income_per_vehicle"].as_f64(),
                a["stations_per_vehicle"].as_f64(), a["gap_opens"].as_str(), causes]).map_err(e)?;
        }
        let mut st = tx.prepare("INSERT INTO contestants VALUES (?,?,?,?,?,?,?,?,?,?,?)").map_err(e)?;
        for c in pm["contestants"].as_array().into_iter().flatten() {
            st.execute(params![ed, c["key"].as_str(), c["prismatic"].as_bool(), c["settings"].as_str(), c["matches"].as_i64(),
                c["avg_rank"].as_f64(), c["last_place"].as_i64(), c["avg_value_share_of_winner"].as_f64(),
                c["avg_idle_cash_share"].as_f64(), c["median_gap_opens"].as_str(), c["causes"].to_string()]).map_err(e)?;
        }
        let mut st = tx.prepare("INSERT INTO proposals VALUES (?,?,?,?)").map_err(e)?;
        for p in pm["proposals"].as_array().into_iter().flatten() {
            st.execute(params![ed, p["for"].as_str(), p["settings"].as_str(), p["because"].to_string()]).map_err(e)?;
        }
    }
    tx.commit().map_err(e)
}

#[cfg(not(feature = "store"))]
fn write_duckdb(_: &Path, _: &Value) -> Result<(), String> {
    Err("built without the store feature: no DuckDB output".into())
}

/// Every postmortem-*.md under `out` equals what its .json derives.
fn check(out: &Path) -> i32 {
    let mut stale = 0;
    let mut n = 0;
    let Ok(dir) = std::fs::read_dir(out) else { return 0 };
    let mut files: Vec<PathBuf> = dir.filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.file_name().and_then(|f| f.to_str()).is_some_and(|f| f.starts_with("postmortem-") && f.ends_with(".json"))).collect();
    files.sort();
    for j in files {
        n += 1;
        let md = j.with_extension("md");
        let pm: Value = match std::fs::read_to_string(&j).ok().and_then(|t| serde_json::from_str(&t).ok()) {
            Some(v) => v,
            None => { eprintln!("STALE {}: not valid JSON", j.display()); stale += 1; continue; }
        };
        if pm["schema"] != "battle-postmortem/v1" {
            eprintln!("STALE {}: schema is not battle-postmortem/v1", j.display());
            stale += 1;
            continue;
        }
        if std::fs::read_to_string(&md).ok().as_deref() != Some(postmortem::markdown(&pm).as_str()) {
            eprintln!("STALE {}: not what {} derives (run battle-postmortem --edition {})", md.display(), j.display(),
                pm["edition"].as_str().unwrap_or("?"));
            stale += 1;
        }
    }
    println!("battle-postmortem check: {n} record(s), {stale} stale");
    if stale > 0 { 11 } else { 0 }
}

fn main() {
    let a = args();
    match a.cmd.as_str() {
        "check" => std::process::exit(check(&a.out)),
        "editions" => {
            match editions(&a.db) {
                Ok(es) => es.iter().for_each(|e| println!("{e}")),
                Err(err) => { eprintln!("battle-postmortem: {err}"); std::process::exit(12) }
            }
            return;
        }
        _ => {}
    }
    let cfg = contestants(&a.config);
    let started = Instant::now();
    let from_store = a.matches.is_none() && a.db.exists();
    let edition = match (&a.edition, from_store) {
        (Some(e), _) => e.clone(),
        (None, true) => editions(&a.db).ok().and_then(|es| es.into_iter().next()).unwrap_or_else(|| usage("the store has no edition")),
        (None, false) => usage("no store at --db: name --edition (and --matches) to read JSON match records"),
    };
    let records = if from_store {
        matches_from_store(&a.db, &edition, &cfg)
    } else {
        let dir = a.matches.clone().unwrap_or_else(|| home().join("ttd-battle/results").join(&edition).join("matches"));
        matches_from_json(&dir, &cfg)
    };
    let records = records.unwrap_or_else(|err| { eprintln!("battle-postmortem: {err}"); std::process::exit(12) });
    if records.is_empty() {
        eprintln!("battle-postmortem: no match of edition {edition}");
        std::process::exit(12);
    }
    let read_s = started.elapsed().as_secs_f64();
    let pm = postmortem::edition(&edition, &records, &postmortem::lineup_is_prismatic);
    let analyse_s = started.elapsed().as_secs_f64() - read_s;
    if pm["matches"].as_i64() == Some(0) {
        // A record of nothing would pass for a round: write none until a match has finished.
        eprintln!("battle-postmortem: edition {edition}: {} match(es), none finished yet; nothing written", records.len());
        std::process::exit(12);
    }
    if a.json {
        println!("{}", serde_json::to_string_pretty(&pm).unwrap());
    }
    std::fs::create_dir_all(&a.out).ok();
    let base = a.out.join(format!("postmortem-{edition}"));
    std::fs::write(base.with_extension("json"), serde_json::to_string_pretty(&pm).unwrap() + "\n")
        .unwrap_or_else(|err| { eprintln!("battle-postmortem: {err}"); std::process::exit(13) });
    std::fs::write(base.with_extension("md"), postmortem::markdown(&pm))
        .unwrap_or_else(|err| { eprintln!("battle-postmortem: {err}"); std::process::exit(13) });
    let duck = match &a.duckdb {
        Some(p) => match write_duckdb(p, &pm) { Ok(()) => p.display().to_string(), Err(err) => format!("not written: {err}") },
        None => "off".into(),
    };
    eprintln!("battle-postmortem: edition {edition}: {} match(es) from {}, read {:.3}s, analysed {:.3}s on {} threads; \
               {} PrismaticAI last-place contestant(s), {} proposal(s); wrote {}.json/.md; duckdb {}",
        pm["matches"], if from_store { a.db.display().to_string() } else { "JSON records".into() }, read_s, analyse_s,
        rayon::current_num_threads(), pm["prismatic_last"].as_array().map_or(0, Vec::len),
        pm["proposals"].as_array().map_or(0, Vec::len), base.display(), duck);
}
