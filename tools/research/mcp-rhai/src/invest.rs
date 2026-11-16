//! RESEARCH-ONLY: what to build, decided by AlgoGlyph's transport-investment kernel over the live game.
//!
//! The planner (plan.rs, cargo.rs) already derives every candidate from the game: town pairs with
//! their passenger supply, freight pairs with each cargo's production, payment rate and transit
//! periods, and the vehicles the game offers now. This module hands those same facts to the kernel
//! as places, demands and modes, so the kernel can also consider routes the planner does not:
//! two legs through a hub (bus to a station, rail onwards), ranked by return on investment.
//!
//! A kernel mode has one vehicle, but a freight vehicle depends on its cargo (a coal wagon carries
//! no wood). So the kernel runs once per cargo, each run with that cargo's own vehicles. The routes
//! it chooses then go through the planner's portfolio (one knapsack over every cargo, within the
//! budget), exactly like the planner's own candidates.
use serde_json::{json, Value};

use crate::cargo::{algoglyph, freight_vehicle, portfolio};
use crate::plan::pick_vehicle;

fn num(v: &Value, k: &str) -> f64 { v.get(k).and_then(Value::as_f64).unwrap_or(0.0) }

/// An id as text: a string as it is, a number in digits ("industry:35", 7), never JSON-quoted.
fn id(v: &Value) -> String { v.as_str().map_or_else(|| v.to_string(), str::to_string) }

fn f(p: &Value, path: &str, d: f64) -> f64 { p.pointer(path).and_then(Value::as_f64).unwrap_or(d) }

fn vehicle(name: String, capacity: f64, speed: f64, price: f64, running: f64) -> Value {
    json!({"name": name, "price": price.round(), "running_per_year": running.round(), "capacity": capacity, "speed_kmh": speed})
}

/// The kernel's mode for one of the planner's modes, or None when the game offers no vehicle for it.
fn mode(name: &str, p: &Value, engines: &Value, cargo: Option<&str>) -> Option<Value> {
    let wagons = f(p, "/rail_wagons", 4.0);
    let v = match cargo {
        Some(c) => freight_vehicle(engines, c, name == "freight_rail", wagons).map(|(n, cap, s, pr, r)| vehicle(n, cap, s, pr, r)),
        None => pick_vehicle(engines, name, wagons, p["small_airports"].as_bool().unwrap_or(true))
            .map(|v| vehicle(v.name, v.capacity, v.speed, v.price, v.running_per_year)),
    }?;
    let (road, rail) = (f(p, "/costs/road_tile", 190.0), f(p, "/costs/rail_tile", 100.0));
    let stop = f(p, "/freight/truck_stops", 400.0) / 2.0;
    // Per tile and per end: the planner's costs (each a pair of terminals) split in two.
    let (per_tile, terminal, band) = match name {
        "bus" => (road, stop, "/bands/bus"),
        "truck" => (road * 0.5, stop, "/freight/bands"),
        "rail" => (rail, f(p, "/costs/rail_stations", 1480.0) / 2.0, "/bands/rail"),
        "freight_rail" => (rail, f(p, "/costs/rail_stations", 1480.0) / 2.0, "/freight/bands"),
        "air" => (0.0, f(p, "/costs/airports", 14400.0) / 2.0, "/bands/air"),
        _ => return None,
    };
    let mut m = json!({"mode": name, "build_per_tile": per_tile, "terminal": terminal, "vehicle": v,
                       "overhead_periods": f(p, "/overhead_periods", 2.0)});
    if let Some(lo) = p.pointer(&format!("{band}/0")).and_then(Value::as_f64) { m["min_leg"] = json!(lo); }
    if let Some(hi) = p.pointer(&format!("{band}/1")).and_then(Value::as_f64) { m["max_leg"] = json!(hi); }
    Some(m)
}

fn enabled(p: &Value, name: &str) -> bool {
    p["modes"].as_array().is_none_or(|a| a.iter().any(|m| m == name))
}

/// The kernel inputs: one per cargo that has a candidate. `plan` is the planner's output (its
/// `params`, `by_mode` and `freight`), `engines` openttd://engines, `towns` openttd://towns.
pub fn inputs(plan: &Value, engines: &Value, towns: &Value) -> Vec<(String, Value)> {
    let p = &plan["params"];
    let params = json!({"payback_months": f(p, "/payback_months", 24.0), "load_factor": f(p, "/load_factor", 0.8),
                        "max_legs": 2, "k": 8});
    let cargos: Vec<Value> = engines["cargos"].as_array().cloned().unwrap_or_default();
    let mut out = Vec::new();

    // Passengers: every town is a place (a hub for a two-leg route), every planned pair a demand.
    let pax = cargos.iter().find(|c| c["passengers"] == true
        || c["name"].as_str().is_some_and(|n| n.to_lowercase().contains("passenger")));
    let pax_name = pax.and_then(|c| c["name"].as_str()).unwrap_or("Passengers").to_string();
    let (td1, td2) = pax.map_or((0.0, 24.0), |c| (c["transit_periods"][0].as_f64().unwrap_or(0.0), c["transit_periods"][1].as_f64().unwrap_or(24.0)));
    let payment = pax.map_or(f(p, "/payment", 3185.0), |c| num(c, "payment"));
    let mut seen = std::collections::HashSet::new();
    let mut demands = Vec::new();
    let mut prebuilt = Vec::new();
    for (_, pairs) in plan["by_mode"].as_object().into_iter().flatten() {
        for c in pairs.as_array().into_iter().flatten() {
            let (a, b) = (id(&c["a"]["id"]), id(&c["b"]["id"]));
            if !seen.insert((a.clone(), b.clone())) { continue; }
            let id = format!("t{a}-t{b}");
            demands.push(json!({"id": id, "from": format!("t{a}"), "to": format!("t{b}"), "cargo": pax_name,
                "volume": (num(c, "supply") * f(p, "/supply_factor", 0.1)).round(), "payment": payment,
                "td1": td1, "td2": td2, "two_way": true}));
            if c["road_connected"] == true { prebuilt.push(json!({"from": format!("t{a}"), "to": format!("t{b}"), "mode": "bus"})); }
        }
    }
    let modes: Vec<Value> = ["bus", "rail", "air"].iter().filter(|m| enabled(p, m)).filter_map(|m| mode(m, p, engines, None)).collect();
    if !demands.is_empty() && !modes.is_empty() {
        let places: Vec<Value> = towns["towns"].as_array().into_iter().flatten()
            .map(|t| json!({"id": format!("t{}", id(&t["id"])), "x": t["x"], "y": t["y"]})).collect();
        out.push((pax_name.clone(), json!({"places": places, "demands": demands, "modes": modes, "prebuilt": prebuilt, "params": params})));
    }

    // Freight: one run per cargo, with that cargo's trucks and wagons; the places are its industries.
    let mut by_cargo: std::collections::BTreeMap<String, Vec<&Value>> = Default::default();
    for c in plan["freight"].as_array().into_iter().flatten() {
        by_cargo.entry(c["cargo"].as_str().unwrap_or("").to_string()).or_default().push(c);
    }
    for (cargo, pairs) in by_cargo {
        let Some(cs) = cargos.iter().find(|c| c["name"] == cargo.as_str()) else { continue };
        let modes: Vec<Value> = ["truck", "freight_rail"].iter().filter(|m| enabled(p, m)).filter_map(|m| mode(m, p, engines, Some(&cargo))).collect();
        if modes.is_empty() { continue; }
        let mut places = Vec::new();
        let mut ids = std::collections::HashSet::new();
        let mut demands = Vec::new();
        for c in pairs {
            for end in ["producer", "acceptor"] {
                let pid = format!("i{}", id(&c[end]["id"]));
                if ids.insert(pid.clone()) { places.push(json!({"id": pid, "x": c[end]["x"], "y": c[end]["y"]})); }
            }
            let (from, to) = (format!("i{}", id(&c["producer"]["id"])), format!("i{}", id(&c["acceptor"]["id"])));
            demands.push(json!({"id": format!("{from}-{to}"), "from": from, "to": to, "cargo": cargo,
                "volume": num(c, "supply_per_month"), "payment": num(cs, "payment"),
                "td1": cs["transit_periods"][0].as_f64().unwrap_or(0.0), "td2": cs["transit_periods"][1].as_f64().unwrap_or(0.0)}));
        }
        out.push((cargo, json!({"places": places, "demands": demands, "modes": modes, "params": params})));
    }
    out
}

/// The label of a route's mode: its legs' modes joined, e.g. "bus+rail".
fn route_mode(route: &Value) -> String {
    route["legs"].as_array().into_iter().flatten().filter_map(|l| l["mode"].as_str()).collect::<Vec<_>>().join("+")
}

/// Run the kernel for every input and choose the portfolio over every cargo's chosen routes.
pub fn decide(plan: &Value, engines: &Value, towns: &Value) -> Value {
    let mut runs = Vec::new();
    let mut candidates = Vec::new();
    for (cargo, input) in inputs(plan, engines, towns) {
        match algoglyph("transport-investment", &input) {
            Ok(v) => {
                let r = &v["result"];
                for c in r["chosen"].as_array().into_iter().flatten() {
                    let route = &c["option"];
                    if num(route, "score") <= 0.0 { continue; }
                    let label = route_mode(route);
                    let why = r["why"].as_array().into_iter().flatten().find(|w| w["demand"] == c["demand"]).cloned();
                    candidates.push(json!({"kind": "investment", "cargo": cargo, "demand": c["demand"], "mode": label,
                        "score": num(route, "score").round(), "roi": route["roi"], "why": why,
                        "options": [{"mode": label, "investment": num(route, "investment").round(), "score": num(route, "score").round(),
                                     "monthly_profit": num(route, "monthly_profit").round(), "roi": route["roi"], "legs": route["legs"],
                                     "transfers": route["transfers"], "transit_periods": route["transit_periods"]}]}));
                }
                runs.push(json!({"cargo": cargo, "ok": true, "demands": input["demands"].as_array().map_or(0, Vec::len),
                                 "routes_considered": r["routes_considered"], "chosen": r["chosen"].as_array().map_or(0, Vec::len),
                                 "kernel": v["kernel"]}));
            }
            Err(e) => runs.push(json!({"cargo": cargo, "ok": false, "error": e})),
        }
    }
    candidates.sort_by(|a, b| num(b, "score").total_cmp(&num(a, "score")));
    let multi = candidates.iter().filter(|c| c["mode"].as_str().is_some_and(|m| m.contains('+'))).count();
    json!({"solver": "algoglyph transport-investment, one run per cargo, then the portfolio knapsack",
           "runs": runs, "candidates": candidates.len(), "multi_leg": multi,
           "portfolio": portfolio(&candidates, plan["params"]["budget"].as_f64())})
}

#[cfg(test)]
mod tests {
    use super::inputs;
    use serde_json::json;

    #[test]
    fn a_plan_becomes_one_kernel_input_per_cargo_with_that_cargos_vehicles() {
        let engines = json!({
            "cargos": [{"name": "Passengers", "passengers": true, "payment": 3185, "transit_periods": [0, 24]},
                       {"name": "Coal", "payment": 5916, "transit_periods": [7, 255]}],
            "engines": [
                {"name": "Bus", "type": "road", "cargo": "Passengers", "capacity": 31, "effective_speed_kmh": 56, "price": 8000, "running_cost_per_year": 1500},
                {"name": "Coal Truck", "type": "road", "cargo": "Coal", "capacity": 20, "effective_speed_kmh": 48, "price": 9000, "running_cost_per_year": 1600}]});
        let plan = json!({
            "params": {"modes": ["bus", "truck"], "supply_factor": 0.1, "bands": {"bus": [12, 70]}, "freight": {"bands": [20, 250]}},
            "by_mode": {"bus": [{"a": {"id": 1}, "b": {"id": 2}, "supply": 500, "road_connected": true}]},
            "freight": [{"cargo": "Coal", "supply_per_month": 90, "producer": {"id": "industry:7", "x": 1, "y": 1}, "acceptor": {"id": 9, "x": 40, "y": 1}}]});
        let towns = json!({"towns": [{"id": 1, "x": 5, "y": 5}, {"id": 2, "x": 40, "y": 5}]});
        let ins = inputs(&plan, &engines, &towns);
        assert_eq!(ins.len(), 2);
        let (pc, pax) = &ins[0];
        assert_eq!(pc, "Passengers");
        assert_eq!(pax["demands"][0]["volume"], 50.0);
        assert_eq!(pax["demands"][0]["two_way"], true);
        assert_eq!(pax["prebuilt"][0]["mode"], "bus");
        assert_eq!(pax["modes"][0]["min_leg"], 12.0);
        let (cc, coal) = &ins[1];
        assert_eq!(cc, "Coal");
        assert_eq!(coal["modes"][0]["vehicle"]["name"], "Coal Truck");
        assert_eq!(coal["demands"][0]["td1"], 7.0);
        assert_eq!(coal["places"].as_array().unwrap().len(), 2);
        assert_eq!(coal["demands"][0]["id"], "iindustry:7-i9");
    }
}
