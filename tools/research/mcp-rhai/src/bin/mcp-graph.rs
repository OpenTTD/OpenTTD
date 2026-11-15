//! RESEARCH-ONLY: print the road network of a running game as a JSON graph (see src/graph.rs),
//! read through a panel's derived resource routes.
//!
//! Usage: mcp-graph [--panel http://127.0.0.1:8800]
use std::time::Duration;

#[path = "../graph.rs"]
mod graph;

fn get(panel: &str, path: &str) -> Result<serde_json::Value, String> {
    let v: serde_json::Value = ureq::get(&format!("{panel}{path}"))
        .timeout(Duration::from_secs(30))
        .call()
        .map_err(|e| format!("{path}: {e}"))?
        .into_json()
        .map_err(|e| format!("{path}: {e}"))?;
    Ok(v.get("data").cloned().unwrap_or(v))
}

fn main() {
    let mut args = std::env::args().skip(1);
    let mut panel = "http://127.0.0.1:8800".to_string();
    while let Some(a) = args.next() {
        if a == "--panel" { panel = args.next().unwrap_or_default(); } else {
            eprintln!("usage: mcp-graph [--panel URL]");
            std::process::exit(2);
        }
    }
    let panel = panel.trim_end_matches('/');
    let out = (|| -> Result<serde_json::Value, String> {
        graph::derive(&get(panel, "/api/resources/map")?, &get(panel, "/api/resources/towns")?,
                      &get(panel, "/api/resources/stations")?)
    })();
    match out {
        Ok(v) => println!("{v}"),
        Err(e) => {
            eprintln!("mcp-graph: {e}");
            std::process::exit(3);
        }
    }
}
