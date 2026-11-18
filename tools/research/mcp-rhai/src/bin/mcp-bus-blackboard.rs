//! RESEARCH-ONLY. A digest of the panels' event bus (ADR 0033) on the Majordomus blackboard, so that
//! every agent session reading `majordomus_blackboard` / `majordomus_peers` sees the games, not only
//! subscribers of ZeroMQ. Off by default: it runs only when started, and only subscribes (read-only
//! for every panel).
//!
//! Usage: mcp-bus-blackboard [--panel URL]... [--endpoint tcp://HOST:PORT]... [--majordomus URL]
//!                           [--repo DIR] [--min-gap SECONDS] [--duration SECONDS] [--dry-run]
//!
//! Panels: every `--panel` (its `GET /api/bus`) and `--endpoint`; with neither, every live panel record
//! of the registry (`$PRISMATIC_MCP_REGISTRY`, default `~/.local/state/prismatic-mcp/instances`) that
//! names a `bus`, rescanned every 30 s.
//!
//! What it posts is a digest, never every event: one line per instance per game month (`game.month`:
//! the date, the fleet, and per company value, money and vehicles), and the newest rare events
//! (`company.founded`/`removed`, `game.new_world`, `decision.recorded`/`updated`, `plan.*`) at once.
//! At most one post per instance per `--min-gap` (default 30 s): what arrives inside the gap waits and
//! goes out with the next post, a rare event is never dropped, and a month only replaces an older one.
//!
//! Where it lands. The blackboard (Majordomus ADR 0176) is a read-only join of each checkout's board
//! (`peers.announce`), the mesh journal and the monitor; it writes nothing itself. A process that is
//! not an MCP client of the board has no write of its own there, so this one becomes one: it opens an
//! MCP session over HTTP (`POST /mcp`) on the primary checkout's Majordomus server and calls
//! `majordomus_announce` with named claims, no scope (nothing to collide with):
//! `openttd:<instance>:month` and `openttd:<instance>:events`, plus its own unnamed claim saying what
//! it follows. The board keeps a departed peer's named claims, so the last digest stays readable after
//! the bridge stops. The server: `--majordomus URL`, else `$MAJORDOMUS_URL`, else what
//! `majordomus serve ensure --format json` names for `--repo` (default: the primary checkout of the
//! current directory's repository).
//!
//! `--dry-run` prints each post instead of announcing it; `--duration` stops after that many seconds.
//! SIGINT/SIGTERM stop it: a last unnamed announcement says it stopped, then the session ends.
//! Exit status: 0 on a clean stop, 2 on bad usage, 3 when no panel or no Majordomus server is reachable.
use std::collections::{BTreeMap, BTreeSet};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde_json::Value;
use zeromq::{Socket, SocketRecv};

#[path = "../board.rs"]
mod board;
use board::{clip, Board};

const USAGE: &str = "usage: mcp-bus-blackboard [--panel URL]... [--endpoint tcp://HOST:PORT]... [--majordomus URL] [--repo DIR] [--min-gap SECONDS] [--duration SECONDS] [--dry-run]";

/// The default gap between two posts of one instance.
const MIN_GAP_MS: u64 = 30_000;
/// The longest line one claim carries (the board takes one line; keep it readable).
const MAX_TEXT: usize = 480;
/// How many rare events the events claim carries, newest first.
const RECENT_KEEP: usize = 3;
/// The kind prefixes subscribed to: what the digest reads, never the console or the AI log.
const TOPICS: [&str; 4] = ["game.", "company.", "decision.", "plan."];
/// How often the registry is read again (the MCP session is pinged on board::PING).
const RESCAN: Duration = Duration::from_secs(30);

// ------------------------------------------------------------------------------------ the digest

/// What an event does to the digest.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Class {
    /// `game.month`: one post per game month.
    Month,
    /// Posted at once (within the rate limit).
    Rare,
    /// Read for names and fleets, never posted.
    Track,
    /// Not the digest's business.
    Ignore,
}

fn classify(kind: &str) -> Class {
    match kind {
        "game.month" => Class::Month,
        "company.founded" | "company.removed" | "game.new_world" | "decision.recorded" | "decision.updated" => Class::Rare,
        k if k.starts_with("plan.") => Class::Rare,
        "game.sample" | "game.observed" | "company.observed" | "company.renamed" => Class::Track,
        _ => Class::Ignore,
    }
}

/// Whether a post may go out now: none yet, or at least `gap_ms` since the last.
fn allowed(last_ms: Option<u64>, now_ms: u64, gap_ms: u64) -> bool {
    last_ms.map_or(true, |l| now_ms.saturating_sub(l) >= gap_ms)
}

/// A company id as the digest keys it: `game.month` keys are strings, company objects carry numbers.
fn key_of(v: &Value) -> Option<String> {
    match v {
        Value::String(s) => Some(s.clone()),
        Value::Number(n) => Some(n.to_string()),
        _ => None,
    }
}

/// 1234567 -> "1.23M", 45210 -> "45.2k", -800 -> "-800".
fn compact(x: f64) -> String {
    let a = x.abs();
    let (v, unit) = if a >= 1e9 { (x / 1e9, "G") } else if a >= 1e6 { (x / 1e6, "M") } else if a >= 1e3 { (x / 1e3, "k") } else { (x, "") };
    if unit.is_empty() {
        return format!("{}", x.round() as i64);
    }
    let s = if v.abs() >= 100.0 { format!("{v:.0}") } else if v.abs() >= 10.0 { format!("{v:.1}") } else { format!("{v:.2}") };
    format!("{s}{unit}")
}

/// "1950-03-01" -> "1950-03".
fn month_of(date: &str) -> &str {
    date.get(..7).unwrap_or(date)
}

/// The month line: `OpenTTD panel-8800 1950-03: 14 vehicles, 2 companies; Name #0 value 120k money
/// 45.2k 9 veh; ...`, companies by value, highest first.
fn month_text(instance: &str, data: &Value, names: &BTreeMap<String, String>, fleet: &BTreeMap<String, u64>) -> String {
    let date = data["date"].as_str().unwrap_or("?");
    let vehicles = data["vehicles"].as_u64().unwrap_or(0);
    let mut rows: Vec<(String, f64, f64)> = data["companies"].as_object().map(|m| {
        m.iter().map(|(id, c)| (id.clone(), c["value"].as_f64().unwrap_or(0.0), c["money"].as_f64().unwrap_or(0.0))).collect()
    }).unwrap_or_default();
    rows.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap_or(std::cmp::Ordering::Equal).then(a.0.cmp(&b.0)));
    let mut s = format!("OpenTTD {instance} {}: {vehicles} vehicles, {} companies", month_of(date), rows.len());
    for (id, value, money) in rows {
        let name = names.get(&id).map_or_else(|| "company".to_string(), Clone::clone);
        s.push_str(&format!("; {name} #{id} value {} money {}", compact(value), compact(money)));
        if let Some(v) = fleet.get(&id) {
            s.push_str(&format!(" {v} veh"));
        }
    }
    clip(&s, MAX_TEXT)
}

/// One rare event as a short phrase: `1950-03-14 company founded: Name #1`.
fn rare_text(kind: &str, game_date: Option<&str>, data: &Value) -> String {
    let date = data["date"].as_str().or(game_date).unwrap_or("?");
    let company = || {
        let name = data["name"].as_str().unwrap_or("company");
        match data.get("id").and_then(key_of) { Some(id) => format!("{name} #{id}"), None => name.to_string() }
    };
    let fields = |keys: &[&str]| keys.iter().filter_map(|k| data.get(*k).filter(|v| !v.is_null()).map(|v| match v {
        Value::String(s) => format!("{k} {s}"),
        other => format!("{k} {other}"),
    })).collect::<Vec<_>>().join(", ");
    let body = match kind {
        "company.founded" => format!("company founded: {}", company()),
        "company.removed" => format!("company removed: {}", company()),
        "game.new_world" => format!("new world, {} towns", data["towns"].as_u64().unwrap_or(0)),
        "decision.recorded" | "decision.updated" => {
            let f = fields(&["id", "action_type", "status", "outcome"]);
            format!("{}: {}", kind.replace('.', " "), if f.is_empty() { clip(&data.to_string(), 120) } else { f })
        }
        _ => {
            let f = data["text"].as_str().map(str::to_string).unwrap_or_else(|| clip(&data.to_string(), 120));
            format!("{kind}: {f}")
        }
    };
    clip(&format!("{date} {body}"), 200)
}

/// The events line: `OpenTTD panel-8800 events: <newest>; <older>; ...`.
fn events_text(instance: &str, recent: &[String]) -> String {
    let newest_first: Vec<&str> = recent.iter().rev().map(String::as_str).collect();
    clip(&format!("OpenTTD {instance} events: {}", newest_first.join("; ")), MAX_TEXT)
}

/// What one post of one instance changes: the month claim, the events claim, or both.
#[derive(Debug, Default, PartialEq)]
struct Post {
    month: Option<String>,
    events: Option<String>,
}

/// What one instance's digest knows, and what waits to be posted.
#[derive(Debug, Default)]
struct Digest {
    instance: String,
    names: BTreeMap<String, String>,
    fleet: BTreeMap<String, u64>,
    /// The game month last taken (`1950-03`), so a month is posted once.
    month_key: Option<String>,
    /// The month line not posted yet.
    month: Option<String>,
    /// The newest rare events, oldest first.
    recent: Vec<String>,
    /// Whether `recent` changed since the last post.
    events_dirty: bool,
    last_post_ms: Option<u64>,
}

impl Digest {
    fn new(instance: &str) -> Self {
        Digest { instance: instance.to_string(), ..Default::default() }
    }

    /// Take one envelope (prismatic-bus/v1) into the digest. Returns what the event is.
    fn observe(&mut self, env: &Value) -> Class {
        let kind = env["kind"].as_str().unwrap_or("");
        let data = &env["data"];
        let class = classify(kind);
        match kind {
            "game.sample" => {
                for c in data["companies"].as_array().into_iter().flatten() {
                    let Some(id) = c.get("id").and_then(key_of) else { continue };
                    if let Some(n) = c["name"].as_str().filter(|n| !n.is_empty()) { self.names.insert(id.clone(), n.to_string()); }
                    let veh = c["fleet"].as_u64().or_else(|| c.pointer("/fleet/total").and_then(Value::as_u64));
                    if let Some(v) = veh { self.fleet.insert(id, v); }
                }
            }
            "company.observed" | "company.founded" => {
                if let (Some(id), Some(n)) = (data.get("id").and_then(key_of), data["name"].as_str()) {
                    self.names.insert(id, n.to_string());
                }
            }
            "company.renamed" => {
                if let (Some(id), Some(n)) = (data.get("id").and_then(key_of), data["to"].as_str()) {
                    self.names.insert(id, n.to_string());
                }
            }
            "company.removed" => {
                if let Some(id) = data.get("id").and_then(key_of) { self.fleet.remove(&id); }
            }
            "game.new_world" => {
                self.names.clear();
                self.fleet.clear();
                self.month_key = None;
                self.month = None;
            }
            _ => {}
        }
        match class {
            Class::Month => {
                let key = month_of(data["date"].as_str().unwrap_or("")).to_string();
                if self.month_key.as_deref() != Some(key.as_str()) {
                    self.month_key = Some(key);
                    self.month = Some(month_text(&self.instance, data, &self.names, &self.fleet));
                }
            }
            Class::Rare => {
                // a removed company's name is read before it is forgotten, so the phrase names it
                self.recent.push(rare_text(kind, env["game_date"].as_str(), data));
                let excess = self.recent.len().saturating_sub(RECENT_KEEP);
                self.recent.drain(..excess);
                self.events_dirty = true;
            }
            _ => {}
        }
        if kind == "company.removed" {
            if let Some(id) = data.get("id").and_then(key_of) { self.names.remove(&id); }
        }
        class
    }

    /// Whether something waits and the rate limit lets it out at `now_ms`.
    fn due(&self, now_ms: u64, gap_ms: u64) -> bool {
        (self.month.is_some() || self.events_dirty) && allowed(self.last_post_ms, now_ms, gap_ms)
    }

    /// Take what waits as one post, and start the gap.
    fn take(&mut self, now_ms: u64) -> Post {
        let events = std::mem::take(&mut self.events_dirty).then(|| events_text(&self.instance, &self.recent));
        let post = Post { month: self.month.take(), events };
        self.last_post_ms = Some(now_ms);
        post
    }
}

/// The bridge's own unnamed claim: what it follows and how it posts.
fn self_text(instances: &BTreeSet<String>, gap_ms: u64) -> String {
    let list = if instances.is_empty() { "no panel yet".to_string() } else { instances.iter().cloned().collect::<Vec<_>>().join(", ") };
    clip(&format!("OpenTTD bus digest (mcp-bus-blackboard, research-only): following {list}; claims openttd:<instance>:month once per game month and openttd:<instance>:events on company/world/decision/plan events, at most one post per instance per {} s", gap_ms / 1000), MAX_TEXT)
}

// ------------------------------------------------------------------------------------- discovery

struct Args {
    panels: Vec<String>,
    endpoints: Vec<String>,
    majordomus: Option<String>,
    repo: Option<String>,
    gap_ms: u64,
    duration: Option<Duration>,
    dry: bool,
}

fn parse(argv: &[String]) -> Result<Args, String> {
    let mut a = Args { panels: vec![], endpoints: vec![], majordomus: std::env::var("MAJORDOMUS_URL").ok().filter(|s| !s.is_empty()),
                       repo: None, gap_ms: MIN_GAP_MS, duration: None, dry: false };
    let mut it = argv.iter();
    while let Some(flag) = it.next() {
        let mut val = || it.next().cloned().ok_or(format!("{flag} needs a value"));
        let secs = |s: String| s.parse::<f64>().ok().filter(|x| *x >= 0.0).ok_or(format!("{flag} needs seconds"));
        match flag.as_str() {
            "--panel" => a.panels.push(val()?),
            "--endpoint" => a.endpoints.push(val()?),
            "--majordomus" => a.majordomus = Some(val()?),
            "--repo" => a.repo = Some(val()?),
            "--min-gap" => a.gap_ms = (secs(val()?)? * 1000.0) as u64,
            "--duration" => a.duration = Some(Duration::from_secs_f64(secs(val()?)?)),
            "--dry-run" => a.dry = true,
            other => return Err(format!("unknown argument {other}")),
        }
    }
    Ok(a)
}

/// The Majordomus server: given, or the one `majordomus serve ensure` names for the repository.
fn majordomus_url(args: &Args) -> Result<String, String> {
    board::majordomus_url(args.majordomus.as_deref(), args.repo.as_deref())
}

fn registry() -> std::path::PathBuf {
    match std::env::var_os("PRISMATIC_MCP_REGISTRY").filter(|v| !v.is_empty()) {
        Some(d) => d.into(),
        None => std::path::PathBuf::from(std::env::var("HOME").unwrap_or_default()).join(".local/state/prismatic-mcp/instances"),
    }
}

fn pid_alive(pid: i64) -> bool {
    // SAFETY: kill with signal 0 only checks that the process exists.
    pid > 0 && pid <= i64::from(i32::MAX) && unsafe { libc::kill(pid as libc::pid_t, 0) } == 0
}

/// (instance, endpoint) of every panel record naming a bus whose owner lives. Read-only: unlike the
/// panel's own discovery, no record is removed.
fn registry_panels() -> Vec<(String, String)> {
    let mut out = Vec::new();
    let Ok(dir) = std::fs::read_dir(registry()) else { return out };
    for e in dir.flatten() {
        let p = e.path();
        if p.extension().and_then(|x| x.to_str()) != Some("json") { continue; }
        let Some(r) = std::fs::read_to_string(&p).ok().and_then(|t| serde_json::from_str::<Value>(&t).ok()) else { continue };
        if r["kind"].as_str() != Some("panel") || !r["owner_pid"].as_i64().map_or(false, pid_alive) { continue; }
        if let (Some(name), Some(bus)) = (r["name"].as_str(), r["bus"].as_str()) {
            out.push((name.to_string(), bus.to_string()));
        }
    }
    out.sort();
    out
}

/// (instance, endpoint) of a panel from its `GET /api/bus`.
fn panel_bus(panel: &str) -> Result<(String, String), String> {
    let url = format!("{}/api/bus", panel.trim_end_matches('/'));
    let v: Value = ureq::get(&url).timeout(Duration::from_secs(5)).call().map_err(|e| format!("{url}: {e}"))?
        .into_json().map_err(|e| format!("{url}: {e}"))?;
    let endpoint = v["endpoint"].as_str().ok_or(format!("{url}: no endpoint (bus error: {})", v["error"]))?;
    Ok((v["instance"].as_str().unwrap_or(panel).to_string(), endpoint.to_string()))
}

enum Msg {
    Event(Value),
    Gone(String, String),
}

/// Follow one endpoint on a thread of its own, handing each envelope to `tx`.
fn subscribe(endpoint: String, tx: mpsc::Sender<Msg>) {
    std::thread::spawn(move || {
        let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().expect("a tokio runtime");
        let why = rt.block_on(async {
            let mut sock = zeromq::SubSocket::new();
            if let Err(e) = sock.connect(&endpoint).await { return format!("connect: {e}"); }
            for t in TOPICS {
                if let Err(e) = sock.subscribe(t).await { return format!("subscribe {t}: {e}"); }
            }
            loop {
                let msg = match sock.recv().await { Ok(m) => m, Err(e) => return format!("recv: {e}") };
                let Some(env) = msg.get(1).or_else(|| msg.get(0)).and_then(|b| serde_json::from_slice::<Value>(b).ok()) else { continue };
                if tx.send(Msg::Event(env)).is_err() { return String::new(); }
            }
        });
        let _ = tx.send(Msg::Gone(endpoint, why));
    });
}

// ------------------------------------------------------------------------------------------ main

static STOP: AtomicBool = AtomicBool::new(false);

extern "C" fn on_signal(_: libc::c_int) {
    STOP.store(true, Ordering::SeqCst);
}

fn now_ms() -> u64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map_or(0, |d| d.as_millis() as u64)
}

/// The wall clock in UTC, `2026-11-18T02:30:00Z`.
fn utc_now() -> String {
    let secs = (now_ms() / 1000) as libc::time_t;
    // SAFETY: gmtime_r writes only into the tm we own.
    let mut tm: libc::tm = unsafe { std::mem::zeroed() };
    unsafe { libc::gmtime_r(&secs, &mut tm) };
    format!("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec)
}

fn main() {
    let argv: Vec<String> = std::env::args().skip(1).collect();
    if argv.iter().any(|a| a == "-h" || a == "--help") {
        println!("{USAGE}");
        return;
    }
    let args = parse(&argv).unwrap_or_else(|e| { eprintln!("{USAGE}\nmcp-bus-blackboard: {e}"); std::process::exit(2) });
    let url = if args.dry { args.majordomus.clone().unwrap_or_else(|| "(dry run)".into()) } else {
        majordomus_url(&args).unwrap_or_else(|e| { eprintln!("mcp-bus-blackboard: {e}"); std::process::exit(3) })
    };
    // SAFETY: the handler only stores into an atomic.
    unsafe {
        libc::signal(libc::SIGINT, on_signal as libc::sighandler_t);
        libc::signal(libc::SIGTERM, on_signal as libc::sighandler_t);
    }
    let from_registry = args.panels.is_empty() && args.endpoints.is_empty();
    let (tx, rx) = mpsc::channel::<Msg>();
    let mut followed: BTreeSet<String> = BTreeSet::new();
    let mut instances: BTreeSet<String> = BTreeSet::new();
    let follow = |found: Vec<(String, String)>, followed: &mut BTreeSet<String>, instances: &mut BTreeSet<String>| -> bool {
        let mut changed = false;
        for (instance, endpoint) in found {
            if followed.insert(endpoint.clone()) {
                eprintln!("mcp-bus-blackboard: following {instance} at {endpoint}");
                subscribe(endpoint, tx.clone());
                changed |= instances.insert(instance);
            }
        }
        changed
    };
    let mut initial: Vec<(String, String)> = args.endpoints.iter().map(|e| (e.clone(), e.clone())).collect();
    for p in &args.panels {
        match panel_bus(p) { Ok(f) => initial.push(f), Err(e) => eprintln!("mcp-bus-blackboard: {e}") }
    }
    if from_registry { initial.extend(registry_panels()); }
    follow(initial, &mut followed, &mut instances);
    if followed.is_empty() && !from_registry {
        eprintln!("mcp-bus-blackboard: no panel bus reachable");
        std::process::exit(3);
    }
    // instances named by --endpoint are learnt from their envelopes
    instances.retain(|i| !i.starts_with("tcp://"));
    let mut board = Board::new(&url, args.dry, "openttd-bus-blackboard", "OpenTTD bus digest");
    if let Err(e) = board.announce("", &self_text(&instances, args.gap_ms)) {
        eprintln!("mcp-bus-blackboard: {e}");
        std::process::exit(3);
    }
    eprintln!("mcp-bus-blackboard: posting to the board of {url} (min gap {} s{})", args.gap_ms / 1000, if args.dry { ", dry run" } else { "" });
    let started = Instant::now();
    let mut last_scan = Instant::now();
    let mut digests: BTreeMap<String, Digest> = BTreeMap::new();
    while !STOP.load(Ordering::SeqCst) && args.duration.map_or(true, |d| started.elapsed() < d) {
        match rx.recv_timeout(Duration::from_secs(1)) {
            Ok(Msg::Event(env)) => {
                let instance = env["instance"].as_str().unwrap_or("panel").to_string();
                if instances.insert(instance.clone()) {
                    let _ = board.announce("", &self_text(&instances, args.gap_ms));
                }
                digests.entry(instance.clone()).or_insert_with(|| Digest::new(&instance)).observe(&env);
            }
            Ok(Msg::Gone(endpoint, why)) => {
                eprintln!("mcp-bus-blackboard: {endpoint} ended: {why}");
                followed.remove(&endpoint);
            }
            Err(_) => {}
        }
        let now = now_ms();
        for (instance, d) in digests.iter_mut() {
            if !d.due(now, args.gap_ms) { continue; }
            let post = d.take(now);
            for (suffix, text) in [("month", post.month), ("events", post.events)] {
                let Some(text) = text else { continue };
                if let Err(e) = board.announce(&format!("openttd:{instance}:{suffix}"), &text) {
                    eprintln!("mcp-bus-blackboard: {e}");
                }
            }
        }
        if from_registry && last_scan.elapsed() >= RESCAN {
            last_scan = Instant::now();
            if follow(registry_panels(), &mut followed, &mut instances) {
                let _ = board.announce("", &self_text(&instances, args.gap_ms));
            }
        }
        board.keep_alive();
    }
    let list = instances.iter().cloned().collect::<Vec<_>>().join(", ");
    let _ = board.announce("", &format!("OpenTTD bus digest (mcp-bus-blackboard) stopped at {}; the openttd:<instance>:* claims of {list} are its last posts", utc_now()));
    board.close();
    eprintln!("mcp-bus-blackboard: stopped");
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    fn env(kind: &str, data: Value) -> Value {
        json!({"schema": "prismatic-bus/v1", "seq": 1, "ts_ms": 0, "instance": "panel-8800", "game_date": "1950-03-14", "kind": kind, "data": data})
    }

    fn month(date: &str) -> Value {
        env("game.month", json!({"date": date, "vehicles": 14, "companies": {
            "0": {"money": 45210, "value": 120000}, "1": {"money": -800, "value": 1_234_567}}}))
    }

    #[test]
    fn only_months_and_rare_events_post() {
        assert_eq!(classify("game.month"), Class::Month);
        for k in ["company.founded", "company.removed", "game.new_world", "decision.recorded", "decision.updated", "plan.chosen"] {
            assert_eq!(classify(k), Class::Rare, "{k}");
        }
        for k in ["game.sample", "game.observed", "company.observed", "company.renamed"] {
            assert_eq!(classify(k), Class::Track, "{k}");
        }
        for k in ["ai.log", "console.line", "news.shown", "panel.ai", "vehicles.count", "game.paused", "planner"] {
            assert_eq!(classify(k), Class::Ignore, "{k}");
        }
        let mut d = Digest::new("panel-8800");
        for k in ["game.sample", "ai.log", "console.line", "game.paused", "vehicles.count"] {
            d.observe(&env(k, json!({"companies": []})));
        }
        assert!(!d.due(1_000_000, MIN_GAP_MS), "nothing to post from tracked or ignored kinds");
    }

    #[test]
    fn the_rate_limit_counts_from_the_last_post() {
        assert!(allowed(None, 0, 30_000));
        assert!(!allowed(Some(100_000), 129_999, 30_000));
        assert!(allowed(Some(100_000), 130_000, 30_000));
        assert!(!allowed(Some(100_000), 50_000, 30_000), "a clock that went back holds, never floods");
    }

    #[test]
    fn a_month_is_posted_once_and_waits_out_the_gap() {
        let mut d = Digest::new("panel-8800");
        d.observe(&month("1950-03-01"));
        assert!(d.due(1_000, MIN_GAP_MS));
        let p = d.take(1_000);
        assert!(p.month.unwrap().starts_with("OpenTTD panel-8800 1950-03:"));
        assert_eq!(p.events, None);
        d.observe(&month("1950-03-01"));
        assert!(!d.due(100_000, MIN_GAP_MS), "the same month again is not news");
        d.observe(&month("1950-04-01"));
        assert!(!d.due(20_000, MIN_GAP_MS), "inside the gap it waits");
        d.observe(&month("1950-05-01"));
        assert!(d.due(31_000, MIN_GAP_MS));
        assert!(d.take(31_000).month.unwrap().contains("1950-05"), "a newer month replaces the waiting one");
    }

    #[test]
    fn rare_events_are_kept_until_posted_newest_first() {
        let mut d = Digest::new("panel-8800");
        d.observe(&month("1950-03-01"));
        d.take(0);
        d.observe(&env("company.founded", json!({"id": 2, "name": "Rival Lines"})));
        assert!(!d.due(10_000, MIN_GAP_MS), "even a rare event waits out the gap");
        d.observe(&env("game.new_world", json!({"date": "1950-01-01", "towns": 24})));
        assert!(d.due(30_000, MIN_GAP_MS));
        let p = d.take(30_000);
        assert_eq!(p.month, None);
        assert_eq!(p.events.as_deref(), Some("OpenTTD panel-8800 events: 1950-01-01 new world, 24 towns; 1950-03-14 company founded: Rival Lines #2"));
        assert!(!d.due(90_000, MIN_GAP_MS), "posted once");
        for i in 0..5 { d.observe(&env("plan.chosen", json!({"text": format!("plan {i}")}))); }
        let text = d.take(90_000).events.unwrap();
        assert!(text.contains("plan 4") && text.contains("plan 2") && !text.contains("plan 1"), "{text}");
    }

    #[test]
    fn the_month_line_names_companies_by_value_with_their_fleet() {
        let mut d = Digest::new("panel-8800");
        d.observe(&env("game.sample", json!({"companies": [{"id": 0, "name": "Owner Transport", "fleet": 9}, {"id": 1, "name": "AI Co", "fleet": 3}]})));
        d.observe(&env("company.renamed", json!({"id": 1, "from": "AI Co", "to": "Rival Lines"})));
        d.observe(&month("1950-03-01"));
        assert_eq!(d.take(0).month.as_deref(), Some(
            "OpenTTD panel-8800 1950-03: 14 vehicles, 2 companies; Rival Lines #1 value 1.23M money -800 3 veh; Owner Transport #0 value 120k money 45.2k 9 veh"));
        let mut empty = Digest::new("panel-8800");
        empty.observe(&env("game.month", json!({"date": "1950-02-01", "vehicles": 0, "companies": {}})));
        assert_eq!(empty.take(0).month.as_deref(), Some("OpenTTD panel-8800 1950-02: 0 vehicles, 0 companies"));
    }

    #[test]
    fn a_new_world_forgets_the_old_one_and_a_removed_company_is_named() {
        let mut d = Digest::new("panel-8800");
        d.observe(&env("company.observed", json!({"id": 3, "name": "Gone Ltd"})));
        d.observe(&env("company.removed", json!({"id": 3, "name": "Gone Ltd"})));
        assert!(d.recent[0].ends_with("company removed: Gone Ltd #3"));
        assert!(d.names.is_empty());
        d.observe(&month("1950-03-01"));
        d.observe(&env("game.new_world", json!({"date": "1950-01-01", "towns": 10})));
        assert_eq!(d.month, None, "the old world's month is not posted after a new world");
        d.observe(&month("1950-03-01"));
        assert!(d.month.is_some(), "the new world's March is news again");
    }

    #[test]
    fn texts_are_one_bounded_line() {
        assert_eq!(compact(999.4), "999");
        assert_eq!(compact(45_210.0), "45.2k");
        assert_eq!(compact(-2_500_000.0), "-2.50M");
        assert_eq!(compact(150_000.0), "150k");
        let many: serde_json::Map<String, Value> = (0..40).map(|i| (i.to_string(), json!({"money": i, "value": i}))).collect();
        let t = month_text("panel-8800", &json!({"date": "1950-03-01", "vehicles": 1, "companies": many}), &BTreeMap::new(), &BTreeMap::new());
        assert_eq!(t.chars().count(), MAX_TEXT);
        assert!(t.ends_with('…'));
        let r = rare_text("decision.recorded", Some("1950-03-14"), &json!({"id": "d-1", "action_type": "game.set_pause", "status": "pending"}));
        assert_eq!(r, "1950-03-14 decision recorded: id d-1, action_type game.set_pause, status pending");
        assert!(!self_text(&BTreeSet::from(["panel-8800".to_string()]), 30_000).contains('\n'));
    }

    #[test]
    fn arguments_parse_and_bad_ones_are_refused() {
        let a = parse(&["--panel".into(), "http://x".into(), "--min-gap".into(), "45".into(), "--dry-run".into()]).unwrap();
        assert_eq!((a.panels.len(), a.gap_ms, a.dry), (1, 45_000, true));
        assert!(parse(&["--min-gap".into(), "soon".into()]).is_err());
        assert!(parse(&["--bogus".into()]).is_err());
    }
}
