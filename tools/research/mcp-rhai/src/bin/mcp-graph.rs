//! RESEARCH-ONLY: print the road network of a running game as a JSON graph (see src/graph.rs),
//! read through a panel's derived resource routes.
//!
//! Usage: mcp-graph [--panel http://127.0.0.1:8800 | --token <prismatic_mcp token file>]
use std::time::Duration;

#[path = "../graph.rs"]
mod graph;

/// Where resources come from: a panel's derived routes, or an MCP server read directly through the
/// token file `prismatic_mcp start` writes ({"agent_token","endpoint"}), for a stack without a panel.
enum Source {
    Panel(String),
    Mcp { endpoint: String, token: String },
}

impl Source {
    fn from_token_file(path: &str) -> Result<Source, String> {
        let text = std::fs::read_to_string(path).map_err(|e| format!("{path}: {e}"))?;
        let v: serde_json::Value = serde_json::from_str(&text).map_err(|e| format!("{path}: {e}"))?;
        let field = |k: &str| v.get(k).and_then(serde_json::Value::as_str).map(str::to_string).ok_or(format!("{path}: no {k}"));
        Ok(Source::Mcp { endpoint: field("endpoint")?, token: field("agent_token")? })
    }

    /// The data of resource openttd://<name>.
    fn resource(&self, name: &str) -> Result<serde_json::Value, String> {
        let v: serde_json::Value = match self {
            Source::Panel(panel) => ureq::get(&format!("{panel}/api/resources/{name}"))
                .timeout(Duration::from_secs(30)).call().map_err(|e| format!("{name}: {e}"))?
                .into_json().map_err(|e| format!("{name}: {e}"))?,
            Source::Mcp { endpoint, token } => {
                let body = serde_json::json!({"jsonrpc": "2.0", "id": 1, "method": "resources/read",
                                              "params": {"uri": format!("openttd://{name}")}});
                let out: serde_json::Value = ureq::post(endpoint)
                    .timeout(Duration::from_secs(30))
                    .set("Authorization", &format!("Bearer {token}"))
                    .set("Accept", "application/json, text/event-stream")
                    .send_json(body).map_err(|e| format!("{name}: {e}"))?
                    .into_json().map_err(|e| format!("{name}: {e}"))?;
                let text = out.pointer("/result/contents/0/text").and_then(serde_json::Value::as_str)
                    .ok_or_else(|| format!("{name}: {}", out.get("error").cloned().unwrap_or(out.clone())))?;
                serde_json::from_str(text).map_err(|e| format!("{name}: {e}"))?
            }
        };
        Ok(v.get("data").cloned().unwrap_or(v))
    }
}

fn main() {
    let mut args = std::env::args().skip(1);
    let mut panel = "http://127.0.0.1:8800".to_string();
    let mut token: Option<String> = None;
    while let Some(a) = args.next() {
        if a == "-h" || a == "--help" {
            println!("usage: mcp-graph [--panel URL | --token FILE]");
            return;
        }
        if a == "--panel" { panel = args.next().unwrap_or_default(); } else if a == "--token" { token = args.next(); } else {
            eprintln!("usage: mcp-graph [--panel URL | --token FILE]");
            std::process::exit(2);
        }
    }
    let panel = panel.trim_end_matches('/').to_string();
    let out = (|| -> Result<serde_json::Value, String> {
        let src = match &token { Some(t) => Source::from_token_file(t)?, None => Source::Panel(panel.clone()) };
        graph::derive_with_ways(&src.resource("map")?, &src.resource("towns")?, &src.resource("stations")?, &src.resource("map/ways").unwrap_or(serde_json::Value::Null))
    })();
    match out {
        Ok(v) => println!("{v}"),
        Err(e) => {
            eprintln!("mcp-graph: {e}");
            std::process::exit(3);
        }
    }
}
