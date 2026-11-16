//! RESEARCH-ONLY: answer a sentence from the owner over the live game (see src/intent.rs):
//! "najdi nejbližší les + pilu", "vybuduj nejlukrativnější trať", "find the most productive coal mine".
//!
//! Usage: mcp-intent [--panel URL | --token FILE] [--lang-dir DIR] [--params JSON] "<sentence>"
use std::time::Duration;

#[path = "../graph.rs"]
mod graph;
#[path = "../plan.rs"]
mod plan;
#[path = "../cargo.rs"]
mod cargo;
#[path = "../intent.rs"]
mod intent;

fn get(base: &str, token: &Option<(String, String)>, name: &str) -> Result<serde_json::Value, String> {
    let v: serde_json::Value = match token {
        None => ureq::get(&format!("{base}/api/resources/{name}")).timeout(Duration::from_secs(30)).call()
            .map_err(|e| format!("{name}: {e}"))?.into_json().map_err(|e| format!("{name}: {e}"))?,
        Some((endpoint, tok)) => {
            let out: serde_json::Value = ureq::post(endpoint).timeout(Duration::from_secs(30))
                .set("Authorization", &format!("Bearer {tok}")).set("Accept", "application/json, text/event-stream")
                .send_json(serde_json::json!({"jsonrpc": "2.0", "id": 1, "method": "resources/read", "params": {"uri": format!("openttd://{name}")}}))
                .map_err(|e| format!("{name}: {e}"))?.into_json().map_err(|e| format!("{name}: {e}"))?;
            let text = out.pointer("/result/contents/0/text").and_then(serde_json::Value::as_str).ok_or(format!("{name}: no content"))?;
            serde_json::from_str(text).map_err(|e| format!("{name}: {e}"))?
        }
    };
    Ok(v.get("data").cloned().unwrap_or(v))
}

fn main() {
    let mut args = std::env::args().skip(1);
    let mut panel = "http://127.0.0.1:8800".to_string();
    let mut token: Option<(String, String)> = None;
    let mut lang_dir = concat!(env!("CARGO_MANIFEST_DIR"), "/../../../src/lang").to_string();
    let mut params = serde_json::Value::Null;
    let mut text = Vec::new();
    while let Some(a) = args.next() {
        match a.as_str() {
            "--panel" => panel = args.next().unwrap_or_default(),
            "--lang-dir" => lang_dir = args.next().unwrap_or_default(),
            "--params" => params = serde_json::from_str(&args.next().unwrap_or_default()).unwrap_or(serde_json::Value::Null),
            "--token" => {
                let f = args.next().unwrap_or_default();
                let v: serde_json::Value = std::fs::read_to_string(&f).ok().and_then(|t| serde_json::from_str(&t).ok()).unwrap_or_default();
                token = Some((v["endpoint"].as_str().unwrap_or("").into(), v["agent_token"].as_str().unwrap_or("").into()));
            }
            _ => text.push(a),
        }
    }
    let text = text.join(" ");
    if text.trim().is_empty() {
        eprintln!("usage: mcp-intent [--panel URL | --token FILE] [--lang-dir DIR] [--params JSON] \"<sentence>\"");
        std::process::exit(2);
    }
    let panel = panel.trim_end_matches('/').to_string();
    let out = (|| -> Result<serde_json::Value, String> {
        let towns = get(&panel, &token, "towns")?;
        let vocab = intent::vocabulary(&lang_dir);
        if vocab.is_empty() { return Err(format!("no game vocabulary under {lang_dir}")); }
        let world = get(&panel, &token, "world/graph").unwrap_or(serde_json::Value::Null);
        let engines = get(&panel, &token, "engines").unwrap_or(serde_json::Value::Null);
        // Names that exist in this game: the industry types on the map and the cargos on offer.
        let mut present: Vec<String> = world["nodes"].as_array().into_iter().flatten().filter(|n| n["kind"] == "industry")
            .filter_map(|n| n["name"].as_str().map(str::to_string)).collect();
        present.extend(engines["cargos"].as_array().into_iter().flatten().filter_map(|c| c["name"].as_str().map(str::to_string)));
        present.sort(); present.dedup();
        let parsed = intent::parse(&text, &vocab, &towns, &present);
        let g = graph::derive_with_ways(&get(&panel, &token, "map")?, &towns, &get(&panel, &token, "stations")?,
                                        &get(&panel, &token, "map/ways").unwrap_or(serde_json::Value::Null))?;
        // The same costs as mcp-plan: the game's prices where it offers them.
        let params = plan::with_prices(&params, &get(&panel, &token, "economy/prices").unwrap_or(serde_json::Value::Null));
        let mut p = plan::plan(&towns, &g, 10, &params, &engines)?;
        let freight = cargo::freight_pairs(&world, &engines, &p["params"], 20);
        let mut candidates: Vec<serde_json::Value> = freight.clone();
        for (_, ps) in p["by_mode"].as_object().cloned().unwrap_or_default() { candidates.extend(ps.as_array().cloned().unwrap_or_default()); }
        candidates.sort_by(|a, b| b["score"].as_f64().unwrap_or(0.0).total_cmp(&a["score"].as_f64().unwrap_or(0.0)));
        p["portfolio"] = cargo::portfolio(&candidates, p["params"]["budget"].as_f64());
        p["freight"] = serde_json::Value::Array(freight);
        let ans = intent::answer(&parsed, &world, &p);
        Ok(serde_json::json!({"intent": parsed, "answer": ans, "vocabulary_size": vocab.len(), "vocabulary_from": lang_dir}))
    })();
    match out {
        Ok(v) => println!("{v}"),
        Err(e) => { eprintln!("mcp-intent: {e}"); std::process::exit(3); }
    }
}
