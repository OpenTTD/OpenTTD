//! RESEARCH-ONLY: print a route plan for a running game as JSON (see src/plan.rs): town pairs
//! ranked by model v2 (the game's payment formula, speeds and base prices), each with its suggested mode of transport.
//!
//! Usage: mcp-plan [--panel http://127.0.0.1:8800 | --token <prismatic_mcp token file>] [--limit 20] [--params '{"cost_weight":1}']
//!        mcp-plan --defaults   (print the default parameters)
use std::time::Duration;

#[path = "../graph.rs"]
mod graph;
#[path = "../plan.rs"]
mod plan;
#[path = "../cargo.rs"]
mod cargo;

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
    let (mut panel, mut limit) = ("http://127.0.0.1:8800".to_string(), 20usize);
    let mut params = serde_json::Value::Null;
    let mut token: Option<String> = None;
    while let Some(a) = args.next() {
        match a.as_str() {
            "--panel" => panel = args.next().unwrap_or_default(),
            "--token" => token = args.next(),
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
                eprintln!("usage: mcp-plan [--panel URL | --token FILE] [--limit N] [--params JSON] [--defaults]");
                std::process::exit(2);
            }
        }
    }
    let panel = panel.trim_end_matches('/').to_string();
    let out = (|| -> Result<serde_json::Value, String> {
        let src = match &token { Some(t) => Source::from_token_file(t)?, None => Source::Panel(panel.clone()) };
        let towns = src.resource("towns")?;
        let g = graph::derive_with_ways(&src.resource("map")?, &towns, &src.resource("stations")?, &src.resource("map/ways").unwrap_or(serde_json::Value::Null))?;
        // Live vehicles when the game offers openttd://engines; the plan says so when it does not.
        let engines = src.resource("engines").unwrap_or(serde_json::Value::Null);
        let mut out = plan::plan(&towns, &g, limit, &params, &engines)?;
        // Freight from the world graph's industries, and the portfolio over passengers and freight.
        let world = src.resource("world/graph").unwrap_or(serde_json::Value::Null);
        let freight = cargo::freight_pairs(&world, &engines, &out["params"], limit.max(10));
        let mut candidates: Vec<serde_json::Value> = Vec::new();
        for (_, ps) in out["by_mode"].as_object().cloned().unwrap_or_default() {
            for p in ps.as_array().cloned().unwrap_or_default() { candidates.push(p); }
        }
        candidates.extend(freight.iter().cloned());
        candidates.sort_by(|a, b| b["score"].as_f64().unwrap_or(0.0).total_cmp(&a["score"].as_f64().unwrap_or(0.0)));
        let portfolio = cargo::portfolio(&candidates, out["params"]["budget"].as_f64());
        out["freight"] = serde_json::Value::Array(freight);
        out["portfolio"] = portfolio;
        out["model"] = serde_json::json!("v4: v3 for passengers, freight pairs from the world graph's industries priced with each cargo's own payment rate and transit periods, and a portfolio chosen by knapsack within the budget");
        Ok(out)
    })();
    match out {
        Ok(v) => println!("{v}"),
        Err(e) => {
            eprintln!("mcp-plan: {e}");
            std::process::exit(3);
        }
    }
}
