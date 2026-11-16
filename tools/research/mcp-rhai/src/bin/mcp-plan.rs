//! RESEARCH-ONLY: print a route plan for a running game as JSON (see src/plan.rs): town pairs
//! ranked by model v2 (the game's payment formula, speeds and base prices), each with its suggested mode of transport.
//!
//! Usage: mcp-plan [--panel http://127.0.0.1:8800] [--limit 20] [--params '{"cost_weight":1}']
//!        mcp-plan --defaults   (print the default parameters)
use std::time::Duration;

#[path = "../graph.rs"]
mod graph;
#[path = "../plan.rs"]
mod plan;

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
    let (mut panel, mut limit) = ("http://127.0.0.1:8800".to_string(), 20usize);
    let mut params = serde_json::Value::Null;
    while let Some(a) = args.next() {
        match a.as_str() {
            "--panel" => panel = args.next().unwrap_or_default(),
            "--limit" => limit = args.next().and_then(|v| v.parse().ok()).unwrap_or(20),
            "--defaults" => {
                println!("{}", plan::defaults());
                return;
            }
            "--params" => match serde_json::from_str(&args.next().unwrap_or_default()) {
                Ok(v) => params = v,
                Err(e) => {
                    eprintln!("mcp-plan: --params is not JSON: {e}");
                    std::process::exit(2);
                }
            },
            _ => {
                eprintln!("usage: mcp-plan [--panel URL] [--limit N] [--params JSON] [--defaults]");
                std::process::exit(2);
            }
        }
    }
    let panel = panel.trim_end_matches('/');
    let out = (|| -> Result<serde_json::Value, String> {
        let towns = get(panel, "/api/resources/towns")?;
        let g = graph::derive(&get(panel, "/api/resources/map")?, &towns, &get(panel, "/api/resources/stations")?)?;
        plan::plan(&towns, &g, limit, &params)
    })();
    match out {
        Ok(v) => println!("{v}"),
        Err(e) => {
            eprintln!("mcp-plan: {e}");
            std::process::exit(3);
        }
    }
}
