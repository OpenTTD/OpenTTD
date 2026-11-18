//! RESEARCH-ONLY / UNSTABLE. AlgoGlyph's kernels fed from a panel's recorded history (I0086): one
//! long-lived `flow serve` answers every request, and each kernel's input is completed from a
//! trailing window of the store's samples (seeded by one read-only statement on the panel's
//! /api/db/query, kept current by the ZeroMQ bus's game.sample envelopes). src/kernel_feed.rs
//! holds the rules; this is the process around them.
//!
//! Usage: mcp-kernel-feed [--panel http://127.0.0.1:8800] [--window DAYS] [--no-store] [--no-bus] [--publish]
//!
//! Standard input: one JSON request per line, {id?, kernel, input?, company?}. Standard output: one
//! `plan.kernel` event per request, {kind, data: {decision, version, kernel, inputs: {live, stored
//! | fallback, kernel_input}, runner, choice}}, where `choice` is the kernel's answer. The process
//! ends when standard input closes. --no-store feeds nothing from history (the A/B arm "as today");
//! --no-bus keeps the seed only. AlgoGlyph is $MCP_ALGOGLYPH_DIR, else ~/dev/algoglyph-wt/main,
//! else ~/dev/algoglyph. Reads only (GET /api/bus, POST /api/db/query, the bus), unless --publish:
//! then each event also goes to the panel's POST /api/bus/publish (plan.kernel is a kind the panel
//! declares as external, src/panel_events.rs), so it reaches the bus, ZeroMQ and the store; the
//! panel's answer is the printed event's `published`.
//!
//! Exit status: 0 when standard input ended, 2 on bad usage.
use std::io::{BufRead, Write};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde_json::{json, Value};
use zeromq::{Socket, SocketRecv};

#[path = "../flow_serve.rs"]
mod flow_serve;
#[path = "../hub_algoglyph.rs"]
#[allow(dead_code)]
mod hub_algoglyph;
#[path = "../kernel_feed.rs"]
mod kernel_feed;
#[path = "../panel_events.rs"]
#[allow(dead_code)]
mod panel_events;

use kernel_feed::{Source, Window};

const USAGE: &str = "usage: mcp-kernel-feed [--panel URL] [--window DAYS] [--no-store] [--no-bus] [--publish]  (requests on stdin, one JSON per line: {id?, kernel, input?, company?})";

struct Args {
    panel: String,
    window: usize,
    store: bool,
    bus: bool,
    publish: bool,
}

fn parse(argv: &[String]) -> Result<Args, String> {
    let mut a = Args { panel: std::env::var("PRISMATIC_PANEL").unwrap_or_else(|_| "http://127.0.0.1:8800".into()),
                       window: 30, store: true, bus: true, publish: false };
    let mut it = argv.iter();
    while let Some(flag) = it.next() {
        let mut val = || it.next().cloned().ok_or(format!("{flag} needs a value"));
        match flag.as_str() {
            "--panel" => a.panel = val()?.trim_end_matches('/').to_string(),
            "--window" => a.window = val()?.parse().map_err(|_| "--window needs a number of days".to_string())?,
            "--no-store" => a.store = false,
            "--no-bus" => a.bus = false,
            "--publish" => a.publish = true,
            other => return Err(format!("unknown argument {other}")),
        }
    }
    Ok(a)
}

fn get(url: &str) -> Result<Value, String> {
    ureq::get(url).timeout(Duration::from_secs(10)).call().map_err(|e| format!("{url}: {e}"))?.into_json().map_err(|e| format!("{url}: {e}"))
}

/// The store's answer to one read-only statement, through the panel (the file is locked while it runs).
fn query(panel: &str, sql: &str) -> Result<Value, String> {
    let url = format!("{panel}/api/db/query");
    match ureq::post(&url).timeout(Duration::from_secs(20)).send_json(json!({"sql": sql})) {
        Ok(r) => r.into_json().map_err(|e| format!("{url}: {e}")),
        Err(ureq::Error::Status(code, r)) => Err(format!("{url}: {code} {}", r.into_string().unwrap_or_default())),
        Err(e) => Err(format!("{url}: {e}")),
    }
}

/// Put one event on the panel's bus (POST /api/bus/publish): the panel's answer, or the error.
fn publish(panel: &str, e: &Value) -> Value {
    let url = format!("{panel}/api/bus/publish");
    match ureq::post(&url).timeout(Duration::from_secs(10)).send_json(json!({"kind": e["kind"], "data": e["data"]})) {
        Ok(r) => r.into_json().unwrap_or_else(|err| json!({"error": format!("{url}: {err}")})),
        Err(ureq::Error::Status(code, r)) => json!({"error": format!("{url}: {code} {}", r.into_string().unwrap_or_default())}),
        Err(err) => json!({"error": format!("{url}: {err}")}),
    }
}

/// Seed the window from the instance's latest run; the statement, or why there is none.
fn seed(w: &mut Window, panel: &str, instance: &str) {
    let r = kernel_feed::seed_sql(instance, w.days).and_then(|sql| {
        let rows = kernel_feed::samples_of_answer(&query(panel, &sql)?)?;
        for s in rows { w.push(s, Source::Store); }
        Ok(sql)
    });
    w.seed = r;
}

/// Follow the bus on its own thread: game.sample and game.new_world into the window.
fn follow(endpoint: String, w: Arc<Mutex<Window>>, status: Arc<Mutex<Value>>) {
    std::thread::spawn(move || {
        let rt = match tokio::runtime::Builder::new_current_thread().enable_all().build() {
            Ok(rt) => rt,
            Err(e) => { *status.lock().unwrap() = json!({"endpoint": endpoint, "error": e.to_string()}); return; }
        };
        rt.block_on(async {
            let mut sock = zeromq::SubSocket::new();
            if let Err(e) = sock.connect(&endpoint).await {
                *status.lock().unwrap() = json!({"endpoint": endpoint, "error": e.to_string()});
                return;
            }
            for t in ["game.sample", "game.new_world"] {
                if let Err(e) = sock.subscribe(t).await {
                    *status.lock().unwrap() = json!({"endpoint": endpoint, "error": format!("subscribe {t}: {e}")});
                    return;
                }
            }
            *status.lock().unwrap() = json!({"endpoint": endpoint, "topics": ["game.sample", "game.new_world"], "error": null});
            loop {
                let Ok(msg) = sock.recv().await else { break };
                // Frame 1 is the topic (the kind), frame 2 the envelope.
                let Some(env) = msg.get(1).and_then(|f| serde_json::from_slice::<Value>(f).ok()) else { continue };
                w.lock().unwrap().apply(&env);
            }
            status.lock().unwrap()["error"] = json!("the bus ended");
        });
    });
}

fn main() {
    let argv: Vec<String> = std::env::args().skip(1).collect();
    if argv.iter().any(|a| a == "-h" || a == "--help") {
        println!("{USAGE}");
        return;
    }
    let args = parse(&argv).unwrap_or_else(|e| { eprintln!("{USAGE}\nmcp-kernel-feed: {e}"); std::process::exit(2) });
    let bus = get(&format!("{}/api/bus", args.panel));
    let instance = bus.as_ref().ok().and_then(|b| b["instance"].as_str()).unwrap_or("").to_string();
    let window = Arc::new(Mutex::new(Window::new(args.window)));
    let bus_status = Arc::new(Mutex::new(json!({"error": "not followed (--no-bus)"})));
    if args.store {
        match &bus {
            Ok(_) => seed(&mut window.lock().unwrap(), &args.panel, &instance),
            Err(e) => window.lock().unwrap().seed = Err(e.clone()),
        }
    } else {
        window.lock().unwrap().seed = Err("--no-store".into());
    }
    if args.bus {
        match bus.as_ref().ok().and_then(|b| b["endpoint"].as_str()) {
            Some(ep) => follow(ep.to_string(), window.clone(), bus_status.clone()),
            None => *bus_status.lock().unwrap() = json!({"error": bus.as_ref().err().cloned().unwrap_or_else(|| "the panel names no endpoint".into())}),
        }
    }
    let dir = hub_algoglyph::dir();
    let stdin = std::io::stdin();
    let mut out = std::io::stdout().lock();
    for line in stdin.lock().lines() {
        let Ok(line) = line else { break };
        if line.trim().is_empty() { continue; }
        let q: Value = match serde_json::from_str(&line) {
            Ok(q) => q,
            Err(e) => { let _ = writeln!(out, "{}", json!({"ok": false, "errors": [format!("not JSON: {e}")]})); continue; }
        };
        let Some(kernel) = q["kernel"].as_str() else {
            let _ = writeln!(out, "{}", json!({"id": q.get("id"), "ok": false, "errors": ["a request names its kernel: {id?, kernel, input?, company?}"]}));
            continue;
        };
        let w = window.lock().unwrap();
        let (input, fed) = kernel_feed::feed(kernel, &q["input"], q["company"].as_i64(), &w, args.store);
        drop(w);
        let answer = flow_serve::kernel(&dir, kernel, &input).unwrap_or_else(|e| json!({"ok": false, "errors": [e]}));
        let live = json!({"panel": args.panel, "instance": instance, "bus": bus_status.lock().unwrap().clone(),
                          "request_input": q["input"], "company": q.get("company")});
        let w = window.lock().unwrap();
        let mut e = kernel_feed::event(kernel, &input, &fed, &w, live, &answer, flow_serve::summary());
        drop(w);
        if args.publish {
            e["published"] = publish(&args.panel, &e);
        }
        if let Some(id) = q.get("id") { e["id"] = id.clone(); }
        let _ = writeln!(out, "{e}");
        let _ = out.flush();
    }
}
