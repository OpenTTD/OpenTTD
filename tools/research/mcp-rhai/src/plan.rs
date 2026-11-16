//! RESEARCH-ONLY: a first route planner over a running game: which town pairs to connect and
//! with which mode of transport, derived from openttd://towns and the road graph (src/graph.rs).
//!
//! Model v2, calibrated to the game's own formulas (cited; .ai knowledge
//! cargo-payment-and-vehicle-speed). Every number is a parameter with a default (DEFAULTS below);
//! a caller overrides any of them (mcp-plan --params JSON, GET /api/plan?params=JSON, Rhai
//! world_plan_with) and the answer echoes the effective parameters:
//!
//! - income per passenger (src/economy.cpp GetTransportedGoodsIncome, passengers in
//!   src/table/cargo_const.h: payment 3185, transit periods [0, 24]):
//!   d * time_factor(t) * payment / 2^21, time_factor = 255 - t up to 24 periods, then 279 - 2t,
//!   never below 31;
//! - transit periods t = d / (speed_kmh * 0.0903) + overhead: a ground vehicle covers
//!   speed_kmh * 2 * 3/4 / (192 * 16) tiles a tick (src/ground_vehicle.hpp DoUpdateSpeed, trains run
//!   their handler twice a tick at half the unit), a cargo period is 185 ticks; aircraft speed is the
//!   effective one after the plane_speed setting (default 1/4);
//! - monthly passengers of a pair: the smaller population * supply_factor (houses generate about
//!   population * 0.54 a month, src/town_cmd.cpp TownGenerateCargoBinomial; the factor is the share
//!   a pair of stations catches and keeps, an assumption);
//! - cost in base price units (src/table/pricebase.h): road 95 a piece, 190 a tile, but nothing when
//!   the towns are already joined by road; rail 100 a tile plus two 3-tile stations
//!   (200 + 180 * 3 each); two small airports of 12 tiles at 600;
//! - score = monthly income * payback_months - cost. The best mode of each pair is kept, pairs are
//!   sorted by score, and the best five pairs of each mode are listed separately.
use serde_json::{json, Value};

const MODE_NAMES: [&str; 3] = ["bus", "rail", "air"];

/// The default parameters; any key can be overridden by the caller.
pub fn defaults() -> Value {
    json!({
        "bands": {"bus": [12, 70], "rail": [40, 250], "air": [120, 100000]},
        "min_population": {"bus": 0, "rail": 300, "air": 800},
        "speed_kmh": {"bus": 56, "rail": 80, "air": 100},
        "overhead_periods": 2.0,
        "supply_factor": 0.1,
        "payment": 3185.0,
        "costs": {"road_tile": 190, "rail_tile": 100, "rail_stations": 1480, "airports": 14400},
        "payback_months": 24.0,
        "modes": ["bus", "rail", "air"],
    })
}

/// The game's time factor for passengers (transit periods [0, 24]), src/economy.cpp
/// GetTransportedGoodsIncome: 255 - t, then 279 - 2t from 24 periods, floor 31 at 124 periods,
/// then 2*31*16*16 / (2t - 248 + 32) in 1/16 fixed point (continuous at 31).
fn time_factor(t: f64) -> f64 {
    let t = t.max(0.0);
    let over_max = 2.0 * t - 248.0;
    if over_max > 0.0 {
        (15872.0 / (over_max + 32.0)).max(1.0) / 16.0
    } else {
        (255.0 - t - (t - 24.0).max(0.0)).max(31.0)
    }
}

#[cfg(test)]
mod tests {
    use super::time_factor;
    #[test]
    fn time_factor_follows_the_game() {
        assert_eq!(time_factor(0.0), 255.0);
        assert_eq!(time_factor(24.0), 231.0);
        assert_eq!(time_factor(50.0), 179.0);
        assert_eq!(time_factor(124.0), 31.0);
        assert!((time_factor(124.5) - 30.06).abs() < 0.01);
        assert!(time_factor(1000.0) < 3.0);
    }
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
    let (payback, supply_factor, payment, overhead) =
        (f("/payback_months")?, f("/supply_factor")?, f("/payment")?, f("/overhead_periods")?);
    let (road_tile, rail_tile, rail_stations, airports) =
        (f("/costs/road_tile")?, f("/costs/rail_tile")?, f("/costs/rail_stations")?, f("/costs/airports")?);
    let mut modes = Vec::new();
    for m in &enabled {
        modes.push((m.clone(), f(&format!("/bands/{m}/0"))?, f(&format!("/bands/{m}/1"))?,
                    f(&format!("/speed_kmh/{m}"))?, f(&format!("/min_population/{m}"))?));
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
            for (name, min, max, speed, min_pop) in &modes {
                if d < *min || d > *max || supply < *min_pop { continue; }
                let periods = d / (speed * 0.0903) + overhead;
                let per_passenger = d * time_factor(periods) * payment / 2097152.0;
                let monthly = supply * supply_factor * per_passenger;
                let cost = match name.as_str() {
                    "bus" => if connected { 0.0 } else { d * road_tile },
                    "rail" => d * rail_tile + rail_stations,
                    _ => airports,
                };
                let score = monthly * payback - cost;
                options.push(json!({"mode": name, "transit_periods": (periods * 10.0).round() / 10.0,
                    "income_per_passenger": (per_passenger * 100.0).round() / 100.0, "monthly_income": monthly.round(),
                    "cost": cost.round(), "score": score.round()}));
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
        "model": "v2, from the game's payment formula, vehicle movement and base prices: see tools/research/mcp-rhai/src/plan.rs",
        "params": p,
        "not_modelled": ["vehicle purchase and running costs (query ScriptEngine::GetPrice and GetRunningCost live)",
                         "station rating and catchment beyond supply_factor", "terraforming and bridges along a route",
                         "inflation and the construction cost setting (all costs scale together)"],
        "pairs_considered": total, "pairs": out, "by_mode": by_mode,
    }))
}
