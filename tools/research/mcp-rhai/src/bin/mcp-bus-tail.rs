//! RESEARCH-ONLY. Follow a panel's event bus on ZeroMQ (ADR 0033): subscribe to the PUB endpoint
//! the panel names in GET /api/bus (or one given directly) and print each message's envelope
//! (prismatic-bus/v1) as one JSON line. For agents and checks that want the live stream without HTTP
//! polling, and without pyzmq or a system libzmq: the pure-Rust `zeromq` crate, as the panel uses.
//!
//! Usage: mcp-bus-tail [--panel http://127.0.0.1:8800 | --endpoint tcp://127.0.0.1:<port>]
//!                     [-n N] [--topic PREFIX]... [--timeout SECONDS]
//!
//! -n N stops after N messages (0, the default, follows forever); --topic subscribes to a kind prefix
//! (`game` takes game.month, ...; repeatable; none takes everything); --timeout ends a wait with no
//! message. Exit status: 0 when N messages arrived (or the stream ended), 1 on a timeout before
//! that, 2 on bad usage, 3 when the panel or the endpoint cannot be reached.
use std::io::Write;
use std::time::Duration;

use zeromq::{Socket, SocketRecv};

const USAGE: &str = "usage: mcp-bus-tail [--panel URL | --endpoint tcp://HOST:PORT] [-n N] [--topic PREFIX]... [--timeout SECONDS]";

struct Args {
    panel: String,
    endpoint: Option<String>,
    count: u64,
    topics: Vec<String>,
    timeout: Option<Duration>,
}

fn parse(argv: &[String]) -> Result<Args, String> {
    let mut a = Args { panel: std::env::var("PRISMATIC_PANEL").unwrap_or_else(|_| "http://127.0.0.1:8800".into()),
                       endpoint: None, count: 0, topics: vec![], timeout: None };
    let mut it = argv.iter();
    while let Some(flag) = it.next() {
        let mut val = || it.next().cloned().ok_or(format!("{flag} needs a value"));
        match flag.as_str() {
            "--panel" => a.panel = val()?,
            "--endpoint" => a.endpoint = Some(val()?),
            "-n" | "--count" => a.count = val()?.parse().map_err(|_| "-n needs a number".to_string())?,
            "--topic" => a.topics.push(val()?),
            "--timeout" => a.timeout = Some(Duration::from_secs_f64(val()?.parse().map_err(|_| "--timeout needs seconds".to_string())?)),
            other => return Err(format!("unknown argument {other}")),
        }
    }
    Ok(a)
}

/// The endpoint the panel publishes in GET /api/bus.
fn endpoint_of(panel: &str) -> Result<String, String> {
    let url = format!("{}/api/bus", panel.trim_end_matches('/'));
    let v: serde_json::Value = ureq::get(&url).timeout(Duration::from_secs(5)).call().map_err(|e| format!("{url}: {e}"))?
        .into_json().map_err(|e| format!("{url}: {e}"))?;
    v["endpoint"].as_str().map(str::to_string).ok_or(format!("{url}: no endpoint (bus error: {})", v["error"]))
}

fn main() {
    let argv: Vec<String> = std::env::args().skip(1).collect();
    if argv.iter().any(|a| a == "-h" || a == "--help") {
        println!("{USAGE}");
        return;
    }
    let args = parse(&argv).unwrap_or_else(|e| { eprintln!("{USAGE}\nmcp-bus-tail: {e}"); std::process::exit(2) });
    let endpoint = match args.endpoint.clone().map_or_else(|| endpoint_of(&args.panel), Ok) {
        Ok(e) => e,
        Err(e) => { eprintln!("mcp-bus-tail: {e}"); std::process::exit(3) }
    };
    let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().expect("a tokio runtime");
    std::process::exit(rt.block_on(tail(&endpoint, &args)));
}

async fn tail(endpoint: &str, args: &Args) -> i32 {
    let mut sock = zeromq::SubSocket::new();
    if let Err(e) = sock.connect(endpoint).await {
        eprintln!("mcp-bus-tail: {endpoint}: {e}");
        return 3;
    }
    let topics = if args.topics.is_empty() { vec![String::new()] } else { args.topics.clone() };
    for t in &topics {
        if let Err(e) = sock.subscribe(t).await {
            eprintln!("mcp-bus-tail: subscribe {t:?}: {e}");
            return 3;
        }
    }
    eprintln!("mcp-bus-tail: following {endpoint} (topics: {})", if args.topics.is_empty() { "all".into() } else { args.topics.join(", ") });
    let mut seen = 0u64;
    let mut out = std::io::stdout().lock();
    while args.count == 0 || seen < args.count {
        let msg = match args.timeout {
            Some(t) => match tokio::time::timeout(t, sock.recv()).await {
                Ok(m) => m,
                Err(_) => {
                    eprintln!("mcp-bus-tail: {seen} message(s), then nothing for {:.1} s", t.as_secs_f64());
                    return 1;
                }
            },
            None => sock.recv().await,
        };
        let Ok(msg) = msg else { break };
        // Frame 1 is the topic (the kind, also in the envelope); frame 2 the envelope.
        let body = msg.get(1).or_else(|| msg.get(0)).map(|b| String::from_utf8_lossy(b).into_owned()).unwrap_or_default();
        if writeln!(out, "{body}").and_then(|_| out.flush()).is_err() {
            break;
        }
        seen += 1;
    }
    0
}
