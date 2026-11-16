//! RESEARCH-ONLY: freight routes and the investment portfolio of planner v4.
//!
//! - Freight pairs: every producing industry (openttd://world/graph, `produces` with last month's
//!   production) against every industry that accepts that cargo, within a distance band. Priced
//!   with the cargo's own payment rate and transit periods (openttd://engines `cargos`, the inputs
//!   of GetTransportedGoodsIncome), carried by trucks or a freight train that the game offers for
//!   that cargo now. Freight runs loaded one way and empty back: one load per round trip.
//! - Portfolio: from the best option of every passenger and freight candidate, the set of
//!   investments with the highest total score that fits the budget, a 0/1 knapsack. Solved by
//!   AlgoGlyph's knapsack-dp kernel when its runner is available (MCP_ALGOGLYPH_DIR, a checkout of
//!   algoglyph's flow CLI), else by the same dynamic programme built in here; the answer names the
//!   solver, the kernel, its input and its work, so it is visible which one decided.
use serde_json::{json, Value};

use crate::plan::time_factor_for;

fn num(v: &Value, k: &str) -> f64 { v.get(k).and_then(Value::as_f64).unwrap_or(0.0) }

/// A freight vehicle for `cargo` (by name): the road vehicle with the best capacity x speed, or the
/// fastest locomotive with `wagons` of the biggest wagon for that cargo.
fn freight_vehicle(engines: &Value, cargo: &str, rail: bool, wagons: f64) -> Option<(String, f64, f64, f64, f64)> {
    let list = engines.get("engines")?.as_array()?;
    let carries = |e: &Value| e.get("cargo").and_then(Value::as_str) == Some(cargo);
    if !rail {
        let e = list.iter().filter(|e| e["type"] == "road" && carries(e) && num(e, "capacity") > 0.0)
            .max_by(|a, b| (num(a, "capacity") * num(a, "effective_speed_kmh")).total_cmp(&(num(b, "capacity") * num(b, "effective_speed_kmh"))))?;
        return Some((e["name"].as_str().unwrap_or("").into(), num(e, "capacity"), num(e, "effective_speed_kmh"), num(e, "price"), num(e, "running_cost_per_year")));
    }
    let loco = list.iter().filter(|e| e["type"] == "rail" && num(e, "capacity") == 0.0 && num(e, "effective_speed_kmh") > 0.0)
        .max_by(|a, b| num(a, "effective_speed_kmh").total_cmp(&num(b, "effective_speed_kmh")))?;
    let wagon = list.iter().filter(|e| e["type"] == "rail" && carries(e) && num(e, "capacity") > 0.0)
        .max_by(|a, b| num(a, "capacity").total_cmp(&num(b, "capacity")))?;
    Some((format!("{} + {} x {}", loco["name"].as_str().unwrap_or(""), wagons, wagon["name"].as_str().unwrap_or("")),
          wagons * num(wagon, "capacity"), num(loco, "effective_speed_kmh"),
          num(loco, "price") + wagons * num(wagon, "price"),
          num(loco, "running_cost_per_year") + wagons * num(wagon, "running_cost_per_year")))
}

/// Freight pairs, best option first. `p` is the planner's effective parameters.
pub fn freight_pairs(world: &Value, engines: &Value, p: &Value, limit: usize) -> Vec<Value> {
    let f = |path: &str, d: f64| p.pointer(path).and_then(Value::as_f64).unwrap_or(d);
    let (min_d, max_d) = (f("/freight/bands/0", 20.0), f("/freight/bands/1", 250.0));
    let share = f("/freight/transported_share", 0.7);
    let (load, payback, overhead) = (f("/load_factor", 0.8), f("/payback_months", 24.0), f("/overhead_periods", 2.0));
    let (road_tile, rail_tile, rail_stations) = (f("/costs/road_tile", 190.0), f("/costs/rail_tile", 100.0), f("/costs/rail_stations", 1480.0));
    let truck_stops = f("/freight/truck_stops", 400.0);
    let wagons = f("/rail_wagons", 4.0);
    let budget = p["budget"].as_f64();
    let cargos: Vec<Value> = engines.get("cargos").and_then(Value::as_array).cloned().unwrap_or_default();
    let industries: Vec<&Value> = world.get("nodes").and_then(Value::as_array).map(|n| n.iter().filter(|x| x["kind"] == "industry").collect()).unwrap_or_default();
    let mut out = Vec::new();
    for prod in &industries {
        for pc in prod.get("produces").and_then(Value::as_array).into_iter().flatten() {
            let monthly = num(pc, "last_month") * share;
            if monthly <= 0.0 { continue; }
            let cname = pc["name"].as_str().unwrap_or("");
            let Some(cs) = cargos.iter().find(|c| c["name"] == cname) else { continue };
            let payment = num(cs, "payment");
            let (td1, td2) = (cs["transit_periods"][0].as_f64().unwrap_or(0.0), cs["transit_periods"][1].as_f64().unwrap_or(0.0));
            for acc in &industries {
                if acc["id"] == prod["id"] { continue; }
                if !acc.get("accepts").and_then(Value::as_array).into_iter().flatten().any(|a| a["name"] == cname) { continue; }
                let d = (num(prod, "x") - num(acc, "x")).abs() + (num(prod, "y") - num(acc, "y")).abs();
                if d < min_d || d > max_d { continue; }
                let mut best: Option<Value> = None;
                let mut options = Vec::new();
                for (mode, rail) in [("truck", false), ("freight_rail", true)] {
                    let Some((vname, cap, speed, price, running)) = freight_vehicle(engines, cname, rail, wagons) else { continue };
                    let periods = d / (speed.max(1.0) * 0.0903) + overhead;
                    let per_unit = d * time_factor_for(periods, td1, td2) * payment / 2097152.0;
                    let per_vehicle = cap * load * 12.0 / (2.0 * periods); // loaded one way, empty back
                    let build = if rail { d * rail_tile + rail_stations } else { d * road_tile * 0.5 + truck_stops };
                    if budget.is_some_and(|b| build > b) { continue; }
                    let top = (monthly / per_vehicle.max(0.1)).ceil().clamp(1.0, if rail { 4.0 } else { 12.0 });
                    let mut chosen = (f64::MIN, 1.0, 0.0, 0.0);
                    let mut n = 1.0;
                    while n <= top {
                        if budget.is_some_and(|b| build + n * price > b) { break; }
                        let carried = monthly.min(n * per_vehicle);
                        let profit = carried * per_unit - n * running / 12.0;
                        let score = profit * payback - build - n * price;
                        if score > chosen.0 { chosen = (score, n, carried, profit); }
                        n += 1.0;
                    }
                    if chosen.0 == f64::MIN { continue; }
                    let (score, n, carried, profit) = chosen;
                    let opt = json!({"mode": mode, "transit_periods": (periods * 10.0).round() / 10.0,
                        "income_per_unit": (per_unit * 100.0).round() / 100.0, "carried_per_month": carried.round(),
                        "monthly_profit": profit.round(), "build_cost": build.round(), "investment": (build + n * price).round(),
                        "vehicles": {"name": vname, "count": n, "capacity": cap, "effective_speed_kmh": speed}, "score": score.round()});
                    if best.as_ref().map_or(true, |b| score > num(b, "score")) { best = Some(opt.clone()); }
                    options.push(opt);
                }
                let Some(b) = best else { continue };
                out.push(json!({
                    "kind": "freight", "cargo": cname, "cargo_id": pc["cargo"], "distance": d, "supply_per_month": monthly.round(),
                    "producer": {"id": prod["id"], "name": prod["name"], "x": prod["x"], "y": prod["y"]},
                    "acceptor": {"id": acc["id"], "name": acc["name"], "x": acc["x"], "y": acc["y"]},
                    "mode": b["mode"], "score": b["score"], "options": options,
                }));
            }
        }
    }
    out.sort_by(|a, b| num(b, "score").total_cmp(&num(a, "score")));
    out.truncate(limit);
    out
}

/// The 0/1 knapsack the portfolio needs: maximise the sum of values with weights within capacity.
fn knapsack_builtin(weights: &[usize], values: &[f64], capacity: usize) -> (f64, Vec<usize>) {
    let n = weights.len();
    let mut dp = vec![vec![0.0f64; capacity + 1]; n + 1];
    for i in 1..=n {
        for c in 0..=capacity {
            dp[i][c] = dp[i - 1][c];
            if weights[i - 1] <= c { dp[i][c] = dp[i][c].max(dp[i - 1][c - weights[i - 1]] + values[i - 1]); }
        }
    }
    let (mut c, mut items) = (capacity, Vec::new());
    for i in (1..=n).rev() {
        if dp[i][c] != dp[i - 1][c] { items.push(i - 1); c -= weights[i - 1]; }
    }
    items.reverse();
    (dp[n][capacity], items)
}

/// AlgoGlyph's knapsack-dp through its flow CLI, when MCP_ALGOGLYPH_DIR names a checkout of it.
fn knapsack_algoglyph(input: &Value) -> Option<Value> {
    let dir = std::env::var("MCP_ALGOGLYPH_DIR").ok()?;
    let mut child = std::process::Command::new("npm")
        .args(["run", "--silent", "flow", "--", "kernel", "knapsack-dp", "-"])
        .current_dir(dir).stdin(std::process::Stdio::piped()).stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::null()).spawn().ok()?;
    use std::io::Write;
    child.stdin.take()?.write_all(input.to_string().as_bytes()).ok()?;
    let out = child.wait_with_output().ok()?;
    let v: Value = serde_json::from_slice(&out.stdout).ok()?;
    if v["ok"] == true { Some(v) } else { None }
}

/// The portfolio: from the candidates (each with its best option's investment and score), the set
/// with the highest total score within the budget. Money is counted in thousands for the solver.
pub fn portfolio(candidates: &[Value], budget: Option<f64>) -> Value {
    let items: Vec<&Value> = candidates.iter().filter(|c| num(c, "score") > 0.0).take(40).collect();
    let invest = |c: &Value| -> f64 {
        let mode = c["mode"].as_str().unwrap_or("");
        c["options"].as_array().into_iter().flatten().find(|o| o["mode"] == mode).map_or(0.0, |o| num(o, "investment"))
    };
    let Some(budget) = budget else {
        return json!({"solver": "none", "note": "no budget given: every positive candidate is affordable", "chosen": items});
    };
    let weights: Vec<usize> = items.iter().map(|c| (invest(c) / 1000.0).ceil() as usize).collect();
    let values: Vec<f64> = items.iter().map(|c| num(c, "score").round()).collect();
    let capacity = (budget / 1000.0).floor().max(0.0) as usize;
    let input = json!({"weights": weights, "values": values, "capacity": capacity});
    let (solver, best, chosen, kernel) = match knapsack_algoglyph(&input) {
        Some(v) => {
            let idx: Vec<usize> = v["result"]["items"].as_array().into_iter().flatten().filter_map(Value::as_u64).map(|i| i as usize).collect();
            ("algoglyph knapsack-dp", num(&v["result"], "best"), idx, json!({"id": v["kernel"]["id"], "plate": v["kernel"]["plate"], "work": v["work"]}))
        }
        None => {
            let (b, idx) = knapsack_builtin(&weights, &values, capacity);
            ("builtin knapsack (MCP_ALGOGLYPH_DIR not set or the runner refused)", b, idx, Value::Null)
        }
    };
    json!({"solver": solver, "kernel": kernel, "input": input, "total_score": best,
           "investment_k": chosen.iter().map(|&i| weights[i]).sum::<usize>(), "budget_k": capacity,
           "chosen": chosen.iter().map(|&i| items[i].clone()).collect::<Vec<_>>()})
}

#[cfg(test)]
mod tests {
    use super::knapsack_builtin;
    #[test]
    fn knapsack_matches_the_algoglyph_case() {
        // The case checked against AlgoGlyph's kernel: items 0 and 3, best 197159.
        let (best, items) = knapsack_builtin(&[135, 46, 20, 5], &[196159.0, 64387.0, 16633.0, 1000.0], 150);
        assert_eq!(best, 197159.0);
        assert_eq!(items, vec![0, 3]);
    }
}
