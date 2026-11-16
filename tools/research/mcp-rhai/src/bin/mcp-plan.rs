//! RESEARCH-ONLY: print a route plan for a running game as JSON (see src/plan.rs): town pairs
//! ranked by model v2 (the game's payment formula, speeds and base prices), each with its suggested mode of transport.
//!
//! Usage: mcp-plan [--panel http://127.0.0.1:8800 | --token <prismatic_mcp token file>] [--limit 20] [--params '{"cost_weight":1}']
//!        mcp-plan --invest ...  (also decide with AlgoGlyph's transport-investment, see src/invest.rs)
//!        mcp-plan --invest-inputs ... (print only the kernel input, to reproduce a case)
//!        mcp-plan --track N ...   (price the N best rail lines of each list on the real map, src/linecost.rs)
//!        mcp-plan --defaults   (print the default parameters)
use std::time::Duration;

#[path = "../graph.rs"]
mod graph;
#[path = "../plan.rs"]
mod plan;
#[path = "../cargo.rs"]
mod cargo;
#[path = "../sites.rs"]
mod sites;
#[path = "../invest.rs"]
mod invest;
#[path = "../linecost.rs"]
mod linecost;
#[path = "../hub_algoglyph.rs"]
#[allow(dead_code)]
mod hub_algoglyph;

/// The game's resources a track plan reads, as hub_algoglyph assembles them (from/to added later).
fn linecost_input(read: &dyn Fn(&str) -> Result<Option<serde_json::Value>, String>) -> Result<serde_json::Value, String> {
    let ends = serde_json::json!({"from": {}, "to": {}});
    hub_algoglyph::track_input(&ends, read)
}

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
    let mut invest_too = false;
    let mut invest_inputs = false;
    let mut track = 0usize;
    while let Some(a) = args.next() {
        match a.as_str() {
            "--panel" => panel = args.next().unwrap_or_default(),
            "--token" => token = args.next(),
            "--invest" => invest_too = true,
            "--invest-inputs" => invest_inputs = true,
            "--track" => track = args.next().and_then(|v| v.parse().ok()).unwrap_or(3),
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
                eprintln!("usage: mcp-plan [--panel URL | --token FILE] [--limit N] [--params JSON] [--invest | --invest-inputs] [--track N] [--defaults]");
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
        // Construction costs follow the game's prices now (openttd://economy/prices) where it offers
        // them; a parameter the caller set wins.
        let prices = src.resource("economy/prices").unwrap_or(serde_json::Value::Null);
        let params = plan::with_prices(&params, &prices);
        let mut out = plan::plan(&towns, &g, limit, &params, &engines)?;
        // Freight from the world graph's industries, and the portfolio over passengers and freight.
        let world = src.resource("world/graph").unwrap_or(serde_json::Value::Null);
        let freight = cargo::freight_pairs(&world, &engines, &out["params"], limit.max(10));
        out["freight"] = serde_json::Value::Array(freight);
        // Station sites for every rail candidate (freight and passengers), by the bot's own rule
        // (src/sites.rs), so an executor builds the plan without a rule of its own.
        let map = src.resource("map")?;
        let pos = |v: &serde_json::Value| (v["x"].as_i64().unwrap_or(0), v["y"].as_i64().unwrap_or(0));
        let add_sites = |c: &mut serde_json::Value| {
            let (a, b) = if c.get("producer").is_some() { (pos(&c["producer"]), pos(&c["acceptor"])) } else { (pos(&c["a"]), pos(&c["b"])) };
            c["stations"] = serde_json::json!({
                "from": sites::site(&map, a.0, a.1, b.0, b.1, 3), "to": sites::site(&map, b.0, b.1, a.0, a.1, 3),
                "rule": "axis toward the other end, buildable flat rectangle and exit tile, nearest site (as PrismaticAI rail.nut)"});
        };
        if let Some(arr) = out["freight"].as_array_mut() {
            for c in arr.iter_mut().filter(|c| c["mode"] == "freight_rail") { add_sites(c); }
        }
        if let Some(arr) = out["by_mode"]["rail"].as_array_mut() { for c in arr.iter_mut() { add_sites(c); } }
        // --track N: the N best rail lines of each list priced on the real map by AlgoGlyph's
        // track-planning (src/linecost.rs) instead of distance x tile cost, before the portfolio.
        if track > 0 {
            let read = |r: &str| -> Result<Option<serde_json::Value>, String> { Ok(src.resource(r).ok()) };
            let mut base = linecost_input(&read)?;
            if let Some(o) = base.as_object_mut() { o.remove("from"); o.remove("to"); }
            let stations = out["params"]["costs"]["rail_stations"].as_f64().unwrap_or(1480.0);
            let rail_tile = out["params"]["costs"]["rail_tile"].as_f64().unwrap_or(100.0);
            let mut log = Vec::new();
            for list in ["freight", "rail"] {
                let arr = if list == "freight" { out["freight"].as_array_mut() } else { out["by_mode"]["rail"].as_array_mut() };
                if let Some(arr) = arr { log.extend(linecost::price(arr, &base, stations, rail_tile, track)); }
            }
            out["line_costs"] = serde_json::json!({"solver": "algoglyph track-planning on map, map/ways, map/terrain, economy/prices, bridges",
                                                   "priced": log});
        }
        let mut candidates: Vec<serde_json::Value> = Vec::new();
        for (_, ps) in out["by_mode"].as_object().cloned().unwrap_or_default() {
            for p in ps.as_array().cloned().unwrap_or_default() { candidates.push(p); }
        }
        candidates.extend(out["freight"].as_array().cloned().unwrap_or_default());
        candidates.sort_by(|a, b| b["score"].as_f64().unwrap_or(0.0).total_cmp(&a["score"].as_f64().unwrap_or(0.0)));
        out["portfolio"] = cargo::portfolio(&candidates, out["params"]["budget"].as_f64());
        if invest_inputs { return Ok(invest::input(&out, &engines, &towns).unwrap_or(serde_json::Value::Null)); }
        if invest_too { out["investment"] = invest::decide(&out, &engines, &towns); }
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
