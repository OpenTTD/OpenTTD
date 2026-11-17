//! RESEARCH-ONLY: a first route planner over a running game: which town pairs to connect and
//! with which mode of transport, derived from openttd://towns and the road graph (src/graph.rs).
//!
//! Model v3, calibrated to the game's own formulas (cited; .ai knowledge
//! cargo-payment-and-vehicle-speed) and to the vehicles on offer now (openttd://engines). Every number is a parameter with a default (DEFAULTS below);
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
//! - vehicles (when openttd://engines answers): per mode the passenger vehicle with the best
//!   capacity x effective speed (no big plane for small airports; rail: the fastest locomotive with
//!   rail_wagons of the biggest passenger carriage); its effective speed replaces speed_kmh; a
//!   vehicle carries capacity * load_factor * 12 / transit_periods passengers a month (a round trip
//!   is two loads); the fleet size is the one that maximises the score, up to max_vehicles, the
//!   demand, and the budget;
//! - score = (income - running costs) * payback_months - (track or airports + vehicles). The best
//!   mode of each pair is kept, pairs are sorted by score, and the best five pairs of each mode are
//!   listed separately.
use serde_json::{json, Value};

const MODE_NAMES: [&str; 3] = ["bus", "rail", "air"];
/// Freight modes, planned in cargo.rs; listed here so `modes` can enable or disable them too.
const FREIGHT_MODES: [&str; 2] = ["truck", "freight_rail"];

/// The default parameters; any key can be overridden by the caller.
/// The cost parameters scaled by the game's prices now: each default cost assumes the base price of
/// its enum Price entry, so it is multiplied by that entry's price now / base (inflation and the
/// construction cost setting together). `prices` is openttd://economy/prices (null: unchanged); a
/// cost the caller set in `overrides` is kept. The answer says which costs came from the game.
pub fn with_prices(overrides: &Value, prices: &Value) -> Value {
    let mut o = if overrides.is_object() { overrides.clone() } else { json!({}) };
    let (Some(now), Some(base)) = (prices.get("prices"), prices.get("base")) else { return o };
    let d = defaults();
    let mut derived = Vec::new();
    for (path, name) in [("/costs/road_tile", "build_road"), ("/costs/rail_tile", "build_rail"),
                         ("/costs/rail_stations", "build_station_rail"), ("/costs/airports", "build_station_airport"),
                         ("/freight/truck_stops", "build_station_truck")] {
        if o.pointer(path).is_some() { continue; }
        let (Some(n), Some(b)) = (now.get(name).and_then(Value::as_f64), base.get(name).and_then(Value::as_f64)) else { continue };
        let Some(def) = d.pointer(path).and_then(Value::as_f64).or(if path == "/freight/truck_stops" { Some(400.0) } else { None }) else { continue };
        if b <= 0.0 { continue; }
        let mut parts = path.trim_start_matches('/').split('/');
        let (group, key) = (parts.next().unwrap_or(""), parts.next().unwrap_or(""));
        if !o[group].is_object() { o[group] = json!({}); }
        o[group][key] = json!((def * n / b).round());
        derived.push(json!({"param": path, "price": name, "now": n, "base": b}));
    }
    o["costs_from_prices"] = Value::Array(derived);
    o
}

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
        "load_factor": 0.8,
        "rail_wagons": 4,
        "max_vehicles": {"bus": 10, "rail": 4, "air": 6},
        "small_airports": true,
        "budget": null,
        "modes": ["bus", "rail", "air", "truck", "freight_rail"],
    })
}

/// The game's time factor for any cargo, exactly as src/economy.cpp GetTransportedGoodsIncome
/// computes it from the cargo's transit periods (td1, td2): constant, then -1 a period after td1,
/// -2 after td1 + td2, never below 31 until the asymptotic tail (1/16 fixed point, continuous).
pub fn time_factor_for(t: f64, td1: f64, td2: f64) -> f64 {
    let t = t.max(0.0);
    let over1 = (t - td1).max(0.0);
    let over2 = (over1 - td2).max(0.0);
    let over_max = if td2 > 224.0 { -224.0 + (t - td1) } else { -224.0 + 2.0 * (t - td1) - td2 };
    if over_max > 0.0 {
        (15872.0 / (over_max + 32.0)).max(1.0) / 16.0
    } else {
        (255.0 - over1 - over2).max(31.0)
    }
}

/// Passengers: transit periods [0, 24].
fn time_factor(t: f64) -> f64 { time_factor_for(t, 0.0, 24.0) }

#[cfg(test)]
mod tests {
    use super::time_factor;
    #[test]
    fn costs_follow_the_games_prices_unless_the_caller_set_them() {
        let prices = serde_json::json!({"prices": {"build_rail": 75, "build_road": 71}, "base": {"build_rail": 100, "build_road": 95}});
        let o = super::with_prices(&serde_json::json!({"costs": {"road_tile": 500}}), &prices);
        assert_eq!(o["costs"]["rail_tile"], 75.0);
        assert_eq!(o["costs"]["road_tile"], 500);
        assert_eq!(o["costs_from_prices"].as_array().unwrap().len(), 1);
        assert_eq!(super::with_prices(&serde_json::Value::Null, &serde_json::Value::Null), serde_json::json!({}));
    }

    #[test]
    fn time_factor_follows_the_game() {
        assert_eq!(time_factor(0.0), 255.0);
        assert_eq!(time_factor(24.0), 231.0);
        assert_eq!(time_factor(50.0), 179.0);
        assert_eq!(time_factor(124.0), 31.0);
        assert!((time_factor(124.5) - 30.06).abs() < 0.01);
        assert!(time_factor(1000.0) < 3.0);
    }
    #[test]
    fn coal_keeps_paying_far_longer() {
        // Coal: transit periods [7, 255]: full rate to 7 periods, then -1 a period.
        assert_eq!(super::time_factor_for(7.0, 7.0, 255.0), 255.0);
        assert_eq!(super::time_factor_for(57.0, 7.0, 255.0), 205.0);
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

/// The vehicle a mode would run, from openttd://engines (what can be bought now): the passenger
/// road vehicle or plane with the best capacity x effective speed (no big plane for small
/// airports), or the fastest locomotive with `wagons` of the biggest passenger wagon. None when the
/// resource is missing or offers nothing for the mode.
pub struct Vehicle { pub name: String, pub capacity: f64, pub speed: f64, pub price: f64, pub running_per_year: f64 }

pub fn pick_vehicle(engines: &Value, mode: &str, wagons: f64, small_airports: bool) -> Option<Vehicle> {
    let list = engines.get("engines")?.as_array()?;
    let num = |e: &Value, k: &str| e.get(k).and_then(Value::as_f64).unwrap_or(0.0);
    let pax = |e: &Value| e.get("cargo").and_then(Value::as_str).is_some_and(|c| c.to_lowercase().contains("passenger"));
    let make = |e: &Value| Vehicle { name: e["name"].as_str().unwrap_or("").to_string(), capacity: num(e, "capacity"),
        speed: num(e, "effective_speed_kmh"), price: num(e, "price"), running_per_year: num(e, "running_cost_per_year") };
    match mode {
        "bus" | "air" => {
            let t = if mode == "bus" { "road" } else { "air" };
            list.iter().filter(|e| e["type"] == t && pax(e) && num(e, "capacity") > 0.0)
                .filter(|e| !(mode == "air" && small_airports && e["plane_type"] == "big"))
                .max_by(|a, b| (num(a, "capacity") * num(a, "effective_speed_kmh")).total_cmp(&(num(b, "capacity") * num(b, "effective_speed_kmh"))))
                .map(make)
        }
        _ => {
            let loco = list.iter().filter(|e| e["type"] == "rail" && num(e, "capacity") == 0.0 && num(e, "effective_speed_kmh") > 0.0)
                .max_by(|a, b| num(a, "effective_speed_kmh").total_cmp(&num(b, "effective_speed_kmh")))?;
            let wagon = list.iter().filter(|e| e["type"] == "rail" && pax(e) && num(e, "capacity") > 0.0)
                .max_by(|a, b| num(a, "capacity").total_cmp(&num(b, "capacity")))?;
            Some(Vehicle { name: format!("{} + {} x {}", loco["name"].as_str().unwrap_or(""), wagons, wagon["name"].as_str().unwrap_or("")),
                capacity: wagons * num(wagon, "capacity"), speed: num(loco, "effective_speed_kmh"),
                price: num(loco, "price") + wagons * num(wagon, "price"),
                running_per_year: num(loco, "running_cost_per_year") + wagons * num(wagon, "running_cost_per_year") })
        }
    }
}

pub fn plan(towns: &Value, graph: &Value, limit: usize, overrides: &Value, engines: &Value) -> Result<Value, String> {
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
        if !MODE_NAMES.contains(&m.as_str()) && !FREIGHT_MODES.contains(&m.as_str()) {
            return Err(format!("unknown mode {m} (bus, rail, air, truck, freight_rail)"));
        }
    }
    // Passenger planning below covers its own modes; freight modes are cargo.rs's.
    let enabled: Vec<String> = enabled.into_iter().filter(|m| MODE_NAMES.contains(&m.as_str())).collect();
    let (payback, supply_factor, payment, overhead) =
        (f("/payback_months")?, f("/supply_factor")?, f("/payment")?, f("/overhead_periods")?);
    let (road_tile, rail_tile, rail_stations, airports) =
        (f("/costs/road_tile")?, f("/costs/rail_tile")?, f("/costs/rail_stations")?, f("/costs/airports")?);
    let (load_factor, wagons) = (f("/load_factor")?, f("/rail_wagons")?);
    let small_airports = p["small_airports"].as_bool().unwrap_or(true);
    // What the player can spend (money plus loan headroom); null: no limit. A fleet is cut down to it,
    // and an option whose track or airports alone exceed it is not offered.
    let budget = p["budget"].as_f64();
    let mut modes = Vec::new();
    let mut vehicles = std::collections::HashMap::new();
    for m in &enabled {
        modes.push((m.clone(), f(&format!("/bands/{m}/0"))?, f(&format!("/bands/{m}/1"))?,
                    f(&format!("/speed_kmh/{m}"))?, f(&format!("/min_population/{m}"))?));
        vehicles.insert(m.clone(), (pick_vehicle(engines, m, wagons, small_airports), f(&format!("/max_vehicles/{m}"))?));
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
                let (vehicle, max_n) = &vehicles[name];
                // The vehicle's own effective speed when the game offers one, else the parameter.
                let speed = vehicle.as_ref().map_or(*speed, |v| v.speed.max(1.0));
                let periods = d / (speed * 0.0903) + overhead;
                // `calibration.<mode>`: what this mode really earned against what the model said, read
                // back from the running game (src/calibrate.rs); 1 when nothing was measured. It is a
                // ratio of NET profit, so it scales income minus running cost, never income alone:
                // r × income − cost charged the running cost a second time ((1 − r) × cost), which an
                // independent review found (codex, 2026-11-17) after the calibrated A/B lost 2.5%.
                let cal = p.pointer(&format!("/calibration/{name}")).or_else(|| p.pointer("/calibration/default")).and_then(Value::as_f64).unwrap_or(1.0);
                let per_passenger = d * time_factor(periods) * payment / 2097152.0;
                let demand = supply * supply_factor;
                let build = match name.as_str() {
                    "bus" => if connected { 0.0 } else { d * road_tile },
                    "rail" => d * rail_tile + rail_stations,
                    _ => airports,
                };
                // With a vehicle: as many as the demand fills (capped), what they carry, what they cost.
                if budget.is_some_and(|b| build > b) { continue; }
                // With a vehicle: the fleet size that maximises the score, from one vehicle up to what
                // the demand fills (capped, and within the budget); each extra vehicle carries only the
                // passengers the others leave, and costs its price and running cost.
                let (carried, running, purchase, fleet) = match vehicle {
                    Some(v) => {
                        // A round trip takes 2 x periods and carries a load each way: 12 / periods loads a month.
                        let per_vehicle = v.capacity * load_factor * 12.0 / periods;
                        let mut top = (demand / per_vehicle.max(0.1)).ceil().clamp(1.0, *max_n);
                        if let Some(b) = budget {
                            top = top.min(((b - build) / v.price.max(1.0)).floor());
                            if top < 1.0 { continue; }
                        }
                        let eval = |n: f64| {
                            let carried = demand.min(n * per_vehicle);
                            let running = n * v.running_per_year / 12.0;
                            ((carried * per_passenger - running) * cal * payback - build - n * v.price, carried, running)
                        };
                        let mut best_n = 1.0;
                        let mut best_s = f64::MIN;
                        let mut n = 1.0;
                        while n <= top {
                            let (sc, _, _) = eval(n);
                            if sc > best_s { best_s = sc; best_n = n; }
                            n += 1.0;
                        }
                        let (_, carried, running) = eval(best_n);
                        (carried, running, best_n * v.price,
                         json!({"name": v.name, "count": best_n, "capacity": v.capacity, "effective_speed_kmh": v.speed,
                                "price_each": v.price.round(), "running_per_month": running.round(),
                                "fills_demand_at": (demand / per_vehicle.max(0.1)).ceil()}))
                    }
                    None => (demand, 0.0, 0.0, Value::Null),
                };
                let income = carried * per_passenger;
                let profit = (income - running) * cal;
                let invest = build + purchase;
                let score = profit * payback - invest;
                options.push(json!({"mode": name, "transit_periods": (periods * 10.0).round() / 10.0,
                    "income_per_passenger": (per_passenger * 100.0).round() / 100.0, "monthly_income": income.round(),
                    "monthly_profit": profit.round(), "build_cost": build.round(), "investment": invest.round(),
                    "vehicles": fleet, "score": score.round()}));
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
        "model": "v3: the game's payment formula, vehicle movement, base prices, and live vehicles (openttd://engines): see tools/research/mcp-rhai/src/plan.rs",
        "params": p,
        "vehicles_from": if engines.get("engines").is_some() { "openttd://engines (live prices, running costs, speeds, capacities)" } else { "not available: vehicle purchase and running costs are left out" },
        "not_modelled": ["station rating and catchment beyond supply_factor", "terraforming and bridges along a route",
                         "inflation and the construction cost setting (all costs scale together)"],
        "pairs_considered": total, "pairs": out, "by_mode": by_mode,
    }))
}
