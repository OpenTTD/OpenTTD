//! RESEARCH-ONLY: a first route planner over a running game: which town pairs to connect and
//! with which mode of transport, derived from openttd://towns and the road graph (src/graph.rs).
//!
//! Heuristic v1, not calibrated against revenue yet. Every assumption is a parameter with a
//! default (DEFAULTS below); a caller overrides any of them (mcp-plan --params JSON, GET
//! /api/plan?params=JSON, Rhai world_plan_with(limit, #{...})) and the answer echoes the effective
//! parameters, so a plan is always reproducible from its output:
//!
//! - supply of a pair is the smaller population: passenger flow is limited by the smaller town;
//! - a mode is offered only within its distance band (Manhattan tiles) and above its minimum
//!   population;
//! - value is supply x distance, damped by the mode's reach (slow vehicles lose payment on long
//!   trips): value = supply * d / (1 + d / reach);
//! - cost is what has to be built: a bus pair already joined by road builds nothing, otherwise
//!   d road tiles; rail d tiles plus two stations; air two airports;
//! - score = value - cost * cost_weight. The best mode of each pair is kept, pairs are sorted by
//!   score, and the best five pairs of each mode are listed separately.
use serde_json::{json, Value};

const MODE_NAMES: [&str; 3] = ["bus", "rail", "air"];

/// The default parameters; any key can be overridden by the caller.
pub fn defaults() -> Value {
    json!({
        "bands": {"bus": [12, 70], "rail": [40, 250], "air": [120, 100000]},
        "min_population": {"bus": 0, "rail": 300, "air": 800},
        "reach": {"bus": 40, "rail": 160, "air": 600},
        "costs": {"road_tile": 10, "rail_tile": 30, "rail_stations": 400, "airports": 3000},
        "cost_weight": 2.0,
        "modes": ["bus", "rail", "air"],
    })
}

/// Deep-merge `over` into `base` (objects merge, everything else replaces).
fn merge(base: &mut Value, over: &Value) {
    match (base, over) {
        (Value::Object(b), Value::Object(o)) => {
            for (k, v) in o {
                merge(b.entry(k.clone()).or_insert(Value::Null), v);
            }
        }
        (b, o) => *b = o.clone(),
    }
}

pub fn plan(towns: &Value, graph: &Value, limit: usize, overrides: &Value) -> Result<Value, String> {
    let mut p = defaults();
    if !overrides.is_null() {
        if !overrides.is_object() { return Err("params must be a JSON object".into()); }
        merge(&mut p, overrides);
    }
    let f = |path: &str| -> Result<f64, String> {
        p.pointer(path).and_then(Value::as_f64).ok_or_else(|| format!("parameter {path} is missing or not a number"))
    };
    let enabled: Vec<String> = p["modes"].as_array().ok_or("parameter /modes must be a list")?
        .iter().filter_map(|m| m.as_str().map(str::to_string)).collect();
    for m in &enabled {
        if !MODE_NAMES.contains(&m.as_str()) { return Err(format!("unknown mode {m} (bus, rail, air)")); }
    }
    let cost_weight = f("/cost_weight")?;
    let (road_tile, rail_tile, rail_stations, airports) =
        (f("/costs/road_tile")?, f("/costs/rail_tile")?, f("/costs/rail_stations")?, f("/costs/airports")?);
    let mut modes = Vec::new();
    for m in &enabled {
        modes.push((m.clone(), f(&format!("/bands/{m}/0"))?, f(&format!("/bands/{m}/1"))?,
                    f(&format!("/reach/{m}"))?, f(&format!("/min_population/{m}"))?));
    }

    let towns_list = towns.pointer("/towns").and_then(Value::as_array).ok_or("towns has no list")?;
    // Road component of each town from the graph (None: no road near its centre).
    let comp = |id: &Value| -> Option<u64> {
        graph.get("towns")?.as_array()?.iter().find(|t| t.get("town") == Some(id))?
            .get("component")?.as_u64()
    };
    let num = |t: &Value, k: &str| t.get(k).and_then(Value::as_f64).unwrap_or(0.0);
    let mut out: Vec<Value> = Vec::new();
    for (i, a) in towns_list.iter().enumerate() {
        for b in towns_list.iter().skip(i + 1) {
            let d = (num(a, "x") - num(b, "x")).abs() + (num(a, "y") - num(b, "y")).abs();
            let supply = num(a, "population").min(num(b, "population"));
            let (ca, cb) = (comp(&a["id"]), comp(&b["id"]));
            let connected = ca.is_some() && ca == cb;
            let mut best: Option<String> = None;
            let mut best_score = f64::MIN;
            let mut options = Vec::new();
            for (name, min, max, reach, min_pop) in &modes {
                if d < *min || d > *max || supply < *min_pop { continue; }
                let value = supply * d / (1.0 + d / reach);
                let cost = match name.as_str() {
                    "bus" => if connected { 0.0 } else { d * road_tile },
                    "rail" => d * rail_tile + rail_stations,
                    _ => airports,
                };
                let score = value - cost * cost_weight;
                options.push(json!({"mode": name, "value": value.round(), "cost": cost.round(), "score": score.round()}));
                if score > best_score {
                    best_score = score;
                    best = Some(name.clone());
                }
            }
            let Some(mode) = best else { continue };
            out.push(json!({
                "a": {"id": a["id"], "name": a["name"], "x": a["x"], "y": a["y"], "population": a["population"]},
                "b": {"id": b["id"], "name": b["name"], "x": b["x"], "y": b["y"], "population": b["population"]},
                "distance": d, "supply": supply, "road_connected": connected,
                "mode": mode, "score": best_score.round(), "options": options,
            }));
        }
    }
    out.sort_by(|x, y| y["score"].as_f64().partial_cmp(&x["score"].as_f64()).unwrap_or(std::cmp::Ordering::Equal));
    let total = out.len();
    // The best pairs of each mode, so a plan can combine modes instead of taking only the top one.
    let mut by_mode = serde_json::Map::new();
    for (name, ..) in &modes {
        let top: Vec<Value> = out.iter().filter(|q| q["mode"] == name.as_str()).take(5).cloned().collect();
        by_mode.insert(name.clone(), Value::Array(top));
    }
    out.truncate(limit);
    Ok(json!({
        "derived_from": ["openttd://towns", "/api/graph"],
        "heuristic": "v1, not calibrated: see tools/research/mcp-rhai/src/plan.rs",
        "params": p,
        "pairs_considered": total, "pairs": out, "by_mode": by_mode,
    }))
}
