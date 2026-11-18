//! RESEARCH-ONLY control panel for OpenTTD's embedded MCP server (ADR 0008, issue I0035), in Rust:
//! the port of the Python panel server.py, with the same command line. server.py was removed in
//! 60aef3caa5, and tools/research/mcp-e2e starts this panel on every run.
//!
//! Starts a headless dedicated server, owns its console, starts its MCP server (the bearer token
//! stays in this process) and a company AI, and serves the control window, its declared and derived
//! routes, /openapi.json, /docs (Swagger UI), the event bus (REST and SSE) and Rhai scripts in process
//! on http://127.0.0.1:<panel-port>/. See src/panel.rs and research/mcp-control.md.
//!
//! Usage: mcp-panel --bin <openttd> [--panel-port 8800] [--mcp-port 8731] [--game-port 3979]
//!                  [--seed 1] [--ai PrismaticAI] [--auto-approve] [--no-plan-bot]
use std::net::TcpListener;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

#[path = "../hub.rs"]
#[allow(dead_code)]
mod hub;
#[path = "../main.rs"]
#[allow(dead_code)]
mod rhai_host;
#[path = "../panel.rs"]
mod panel;
#[path = "../panel_bot.rs"]
mod panel_bot;
#[path = "../panel_decision.rs"]
mod panel_decision;
#[path = "../panel_results.rs"]
mod panel_results;

static STOP: AtomicBool = AtomicBool::new(false);

extern "C" fn on_signal(_: libc::c_int) {
    STOP.store(true, Ordering::SeqCst);
}

fn main() {
    let argv: Vec<String> = std::env::args().skip(1).collect();
    if argv.iter().any(|a| a == "-h" || a == "--help") {
        println!("{}\n\nRESEARCH-ONLY control panel for OpenTTD's embedded MCP server (ADR 0008).", panel::USAGE);
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
    if let Err(e) = game.start() {
        eprintln!("mcp-panel: {e}");
        std::process::exit(1);
    }
    let p = Arc::new(panel::Panel { game: game.clone(), routes: panel::routes() });
    std::thread::spawn(move || panel::serve(listener, p));
    println!("mcp-panel: http://127.0.0.1:{}/  (game server 127.0.0.1:{}, MCP {})", args.panel_port, args.game_port, game.endpoint());
    let me = format!("panel-{}", args.panel_port);
    panel::registry_write(&me, &serde_json::json!({"kind": "panel", "role": "dedicated server + panel",
        "url": game.panel_url(), "mcp": game.endpoint(), "game_port": args.game_port}));
    while !STOP.load(Ordering::SeqCst) {
        std::thread::sleep(Duration::from_millis(100));
    }
    panel::registry_remove(&me);
    panel::registry_remove(&format!("client-{}", args.panel_port));
    game.stop();
}
