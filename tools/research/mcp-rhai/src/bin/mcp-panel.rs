//! RESEARCH-ONLY control panel for OpenTTD's embedded MCP server (ADR 0008, issue I0035), in Rust:
//! the port of the Python panel server.py, with the same command line. server.py was removed in
//! 60aef3caa5, and tools/research/mcp-e2e starts this panel on every run.
//!
//! Starts a headless dedicated server, owns its console, starts its MCP server (the bearer token
//! stays in this process) and a company AI, and serves the control window, its declared and derived
//! routes, /openapi.json, /docs (Swagger UI), the event bus (REST and SSE) and Rhai scripts in process
//! on http://127.0.0.1:<panel-port>/; the bus also goes out on ZeroMQ and into a DuckDB store
//! (src/panel_store.rs, ADR 0033). See src/panel.rs and research/mcp-control.md.
//!
//! Usage: mcp-panel --bin <openttd> [--panel-port 8800] [--mcp-port 8731] [--game-port 3979]
//!                  [--seed 1] [--ai PrismaticAI] [--auto-approve] [--no-plan-bot]
use std::net::TcpListener;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

#[path = "../bus_sinks.rs"]
mod bus_sinks;
#[path = "../calibrate.rs"]
#[allow(dead_code)]
mod calibrate;
#[path = "../duel_outcomes.rs"]
mod duel_outcomes;
#[path = "../hub.rs"]
#[allow(dead_code)]
mod hub;
#[path = "../kernel_feed.rs"]
#[allow(dead_code)] // the panel feeds its action plan (from_store); the window and the event are mcp-kernel-feed's
mod kernel_feed;
#[path = "../main.rs"]
#[allow(dead_code)]
mod rhai_host;
#[path = "../panel.rs"]
mod panel;
#[path = "../panel_bot.rs"]
mod panel_bot;
#[path = "../panel_play.rs"]
mod panel_play;
#[path = "../panel_decision.rs"]
mod panel_decision;
#[path = "../panel_events.rs"]
mod panel_events;
#[path = "../panel_results.rs"]
mod panel_results;
#[path = "../panel_scripts.rs"]
mod panel_scripts;
#[path = "../rhai_catalogue.rs"]
mod rhai_catalogue;
#[path = "../panel_npm.rs"]
mod panel_npm;
#[path = "../rhai_shared.rs"]
mod rhai_shared;
#[path = "../rhai_debug.rs"]
mod rhai_debug;
#[path = "../evidence.rs"]
mod evidence;
#[path = "../panel_realworld.rs"]
mod panel_realworld;
#[path = "../panel_store.rs"]
mod panel_store;

static STOP: AtomicBool = AtomicBool::new(false);

extern "C" fn on_signal(_: libc::c_int) {
    STOP.store(true, Ordering::SeqCst);
}

#[cfg(feature = "store")]
fn import(file: &serde_json::Value, from: &str) -> Result<serde_json::Value, String> {
    panel_store::import_calibration(&bus_sinks::store_dir(), file, from)
}

#[cfg(not(feature = "store"))]
fn import(_: &serde_json::Value, _: &str) -> Result<serde_json::Value, String> {
    Err("built without the store feature".into())
}

#[cfg(feature = "store")]
fn import_duels(results: &std::path::Path) -> Result<serde_json::Value, String> {
    duel_outcomes::import_duels(&bus_sinks::store_dir(), results)
}

#[cfg(not(feature = "store"))]
fn import_duels(_: &std::path::Path) -> Result<serde_json::Value, String> {
    Err("built without the store feature".into())
}

fn main() {
    let argv: Vec<String> = std::env::args().skip(1).collect();
    if argv.iter().any(|a| a == "-h" || a == "--help") {
        println!("{}\n\nRESEARCH-ONLY control panel for OpenTTD's embedded MCP server (ADR 0008).", panel::USAGE);
        return;
    }
    // The knowledge note of event kinds holds the declared table (src/panel_events.rs) as a region;
    // this writes it, and a cargo test fails while it differs.
    if argv.iter().any(|a| a == "--write-event-kinds") {
        let note = panel_results::repo().join(".ai/repo/knowledge/curated/the-panel-bus-carries-these-event-kinds.md");
        let done = std::fs::read_to_string(&note).map_err(|e| e.to_string())
            .and_then(|t| panel_events::with_region(&t))
            .and_then(|t| std::fs::write(&note, t).map_err(|e| e.to_string()));
        match done {
            Ok(()) => println!("mcp-panel: {} kinds written to {}", panel_events::KINDS.len(), note.display()),
            Err(e) => { eprintln!("mcp-panel: {}: {e}", note.display()); std::process::exit(1); }
        }
        return;
    }
    // The one import of the retired calibration file into the store (I0082): its readings become
    // plan.calibration events of a run of their own, in $PRISMATIC_STORE_DIR/calibration-import.duckdb,
    // where every planned bot start reads them. A second import of the same reading changes nothing.
    if let Some(i) = argv.iter().position(|a| a == "--import-calibration") {
        let Some(file) = argv.get(i + 1) else {
            eprintln!("mcp-panel: --import-calibration FILE");
            std::process::exit(2);
        };
        let done = std::fs::read_to_string(file).map_err(|e| format!("{file}: {e}"))
            .and_then(|t| serde_json::from_str::<serde_json::Value>(&t).map_err(|e| format!("{file}: {e}")))
            .and_then(|v| import(&v, argv.get(i + 2).map_or(file.as_str(), String::as_str)));
        match done {
            Ok(v) => println!("{v}"),
            Err(e) => { eprintln!("mcp-panel: {e}"); std::process::exit(1); }
        }
        return;
    }
    // The one import of the duels recorded before mcp-duel published plan.duel (I0087): each record of
    // research/results (or DIR) becomes a plan.duel event of a run of its own, in
    // $PRISMATIC_STORE_DIR/duel-import.duckdb, where a bot start with PRISMATIC_BOT_FROM_DUELS=1 reads
    // it. A record already imported, or whose verdict is in a store, changes nothing.
    if let Some(i) = argv.iter().position(|a| a == "--import-duels") {
        let dir = argv.get(i + 1).filter(|a| !a.starts_with("--")).map(std::path::PathBuf::from)
            .unwrap_or_else(|| panel_results::repo().join("research/results"));
        match import_duels(&dir) {
            Ok(v) => println!("{v}"),
            Err(e) => { eprintln!("mcp-panel: {e}"); std::process::exit(1); }
        }
        return;
    }
    let mut args = match panel::parse_args(&argv) {
        Ok(a) => a,
        Err(e) => {
            eprintln!("{}\nmcp-panel: error: {e}", panel::USAGE);
            std::process::exit(2);
        }
    };
    // A port another process already listens on is replaced by a free one: on macOS a second listener
    // can bind the same loopback port, and calls then reach either server at random (seen 2026-11-16).
    for (name, port) in [("mcp-port", &mut args.mcp_port), ("game-port", &mut args.game_port)] {
        if panel::port_busy(*port) {
            let old = *port;
            *port = panel::free_port().unwrap_or(old);
            println!("mcp-panel: port {old} is in use; --{name} {} instead", *port);
        }
    }
    let listener = match TcpListener::bind(("127.0.0.1", args.panel_port)) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("mcp-panel: cannot listen on 127.0.0.1:{}: {e}", args.panel_port);
            std::process::exit(1);
        }
    };
    // A process started in the background of a non-interactive shell inherits SIGINT as ignored; take
    // both signals explicitly so the dedicated server is always stopped with the panel.
    // SAFETY: the handler only stores into an atomic.
    unsafe {
        libc::signal(libc::SIGINT, on_signal as libc::sighandler_t);
        libc::signal(libc::SIGTERM, on_signal as libc::sighandler_t);
    }
    let game = panel::Game::new(args.clone());
    let me = format!("panel-{}", args.panel_port);
    // The bus tap follows the bus from its first event, so it starts before the game does (ADR 0033).
    let tap = panel_store::start(game.clone(), &me);
    if let Err(e) = game.start() {
        eprintln!("mcp-panel: {e}");
        std::process::exit(1);
    }
    let p = Arc::new(panel::Panel { game: game.clone(), routes: panel::routes() });
    std::thread::spawn(move || panel::serve(listener, p));
    println!("mcp-panel: http://127.0.0.1:{}/  (game server 127.0.0.1:{}, MCP {})", args.panel_port, args.game_port, game.endpoint());
    let text = |v: serde_json::Value| v.as_str().unwrap_or("off").to_string();
    println!("mcp-panel: bus {}  store {}", text(panel_store::endpoint()), text(panel_store::store_status()));
    for e in [&tap.sinks.bus_error, &tap.sinks.store_error].into_iter().flatten() {
        println!("mcp-panel: {e}");
    }
    panel::registry_write(&me, &serde_json::json!({"kind": "panel", "role": "dedicated server + panel",
        "url": game.panel_url(), "mcp": game.endpoint(), "game_port": args.game_port,
        "bus": panel_store::endpoint(), "store": panel_store::store_status()}));
    while !STOP.load(Ordering::SeqCst) {
        std::thread::sleep(Duration::from_millis(100));
    }
    panel::registry_remove(&me);
    panel::registry_remove(&format!("client-{}", args.panel_port));
    // The panel's subprocesses (the bot's planner, an intent, a plan) stop with it, before the game:
    // a planner left running was re-parented to PID 1 and kept calling a panel that was gone.
    let helpers = panel::stop_children(Duration::from_secs(3));
    if helpers > 0 {
        println!("mcp-panel: stopped {helpers} subprocess(es)");
    }
    game.stop();
    // The events of the stop itself reach the store before the process ends.
    panel_store::stop();
}
