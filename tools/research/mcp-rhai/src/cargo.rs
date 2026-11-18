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

/// One long-lived AlgoGlyph `flow serve` per process, behind MCP_ALGOGLYPH_SERVE (I0086).
#[path = "flow_serve.rs"]
pub mod flow_serve;

fn num(v: &Value, k: &str) -> f64 { v.get(k).and_then(Value::as_f64).unwrap_or(0.0) }

/// A freight vehicle for `cargo` (by name): the road vehicle with the best capacity x speed, or the
/// fastest locomotive with `wagons` of the biggest wagon for that cargo.
pub fn freight_vehicle(engines: &Value, cargo: &str, rail: bool, wagons: f64) -> Option<(String, f64, f64, f64, f64)> {
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
    let enabled: Vec<String> = p["modes"].as_array().map(|a| a.iter().filter_map(|m| m.as_str().map(str::to_string)).collect())
        .unwrap_or_else(|| vec!["truck".into(), "freight_rail".into()]);
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
                    if !enabled.iter().any(|m| m == mode) { continue; }
                    let Some((vname, cap, speed, price, running)) = freight_vehicle(engines, cname, rail, wagons) else { continue };
                    let periods = d / (speed.max(1.0) * 0.0903) + overhead;
                    // `calibration.<mode>`, read back from the running game (src/calibrate.rs); 1 unmeasured.
                    // A ratio of net profit, so it scales income minus running cost (plan.rs says why).
                    let cal = p.pointer(&format!("/calibration/{mode}")).or_else(|| p.pointer("/calibration/default")).and_then(Value::as_f64).unwrap_or(1.0);
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
                        let profit = (carried * per_unit - n * running / 12.0) * cal;
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

/// An AlgoGlyph kernel through its flow CLI (`npm run flow -- kernel <id> -`), when MCP_ALGOGLYPH_DIR
/// names a checkout of it. Err says why there is no answer: no runner, a failed run, or a refusal.
/// With `MCP_ALGOGLYPH_SERVE=1` every call of the process goes to one long-lived `flow serve`
/// (src/flow_serve.rs), and to the per-call runner only if that serve cannot start or fails.
pub fn algoglyph(kernel: &str, input: &Value) -> Result<Value, String> {
    let dir = std::env::var("MCP_ALGOGLYPH_DIR").map_err(|_| "MCP_ALGOGLYPH_DIR is not set".to_string())?;
    if flow_serve::wanted() {
        match flow_serve::kernel(std::path::Path::new(&dir), kernel, input) {
            Ok(v) if v["ok"] == true => return Ok(v),
            Ok(v) => return Err(format!("{kernel} refused: {}", v.get("errors").cloned().unwrap_or(v.clone()))),
            Err(_) => flow_serve::fell_back(),
        }
    }
    // The answer goes to a file, not a pipe: the flow CLI ends with process.exit(), which drops what
    // a pipe has not taken yet, so an answer above 64 KiB arrived cut off (2026-11-16).
    let nanos = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_nanos());
    let path = std::env::temp_dir().join(format!("mcp-algoglyph-{}-{nanos}.json", std::process::id()));
    let file = std::fs::File::create(&path).map_err(|e| format!("{kernel}: {e}"))?;
    let mut child = std::process::Command::new("npm")
        .args(["run", "--silent", "flow", "--", "kernel", kernel, "-"])
        .current_dir(&dir).stdin(std::process::Stdio::piped()).stdout(file)
        .stderr(std::process::Stdio::null()).spawn().map_err(|e| format!("{kernel}: {e}"))?;
    use std::io::Write;
    child.stdin.take().ok_or("no stdin")?.write_all(input.to_string().as_bytes()).map_err(|e| format!("{kernel}: {e}"))?;
    child.wait().map_err(|e| format!("{kernel}: {e}"))?;
    let text = std::fs::read(&path).map_err(|e| format!("{kernel}: {e}"));
    let _ = std::fs::remove_file(&path);
    let v: Value = serde_json::from_slice(&text?).map_err(|e| format!("{kernel}: not JSON ({e})"))?;
    if v["ok"] == true { Ok(v) } else { Err(format!("{kernel} refused: {}", v.get("errors").cloned().unwrap_or(v.clone()))) }
}

fn knapsack_algoglyph(input: &Value) -> Option<Value> { algoglyph("knapsack-dp", input).ok() }

/// The portfolio: from the candidates (each with its best option's investment and score), the set
/// with the highest total score within the budget. Money is counted in thousands for the solver.
pub fn portfolio(candidates: &[Value], budget: Option<f64>) -> Value {
    // A producer's cargo is counted once: each freight candidate assumes it carries the producer's
    // whole production, so two lines from one mine would each count all of it. Only the best-scoring
    // candidate of each (cargo, producer) enters the knapsack; the others are reported as shadowed.
    let source = |c: &Value| -> Option<String> {
        let from = c.pointer("/producer/id").or_else(|| c.get("from").filter(|_| c["passengers"] != true))?;
        Some(format!("{}@{}", c["cargo"].as_str().unwrap_or(""), from.as_str().map_or_else(|| from.to_string(), str::to_string)))
    };
    let mut best_of: std::collections::HashMap<String, f64> = Default::default();
    for c in candidates.iter().filter(|c| num(c, "score") > 0.0) {
        if let Some(k) = source(c) { let e = best_of.entry(k).or_insert(f64::MIN); *e = e.max(num(c, "score")); }
    }
    let mut shadowed = Vec::new();
    let mut taken = std::collections::HashSet::new();
    let items: Vec<&Value> = candidates.iter().filter(|c| num(c, "score") > 0.0).filter(|c| match source(c) {
        Some(k) if num(c, "score") < best_of[&k] || !taken.insert(k.clone()) => { shadowed.push(k); false }
        _ => true,
    }).take(40).collect();
    let invest = |c: &Value| -> f64 {
        let mode = c["mode"].as_str().unwrap_or("");
        c["options"].as_array().into_iter().flatten().find(|o| o["mode"] == mode).map_or(0.0, |o| num(o, "investment"))
    };
    let Some(budget) = budget else {
        return json!({"solver": "none", "note": "no budget given: every positive candidate is affordable", "chosen": items,
                      "shadowed": shadowed});
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
    json!({"solver": solver, "kernel": kernel, "input": input, "total_score": best, "shadowed": shadowed,
           "investment_k": chosen.iter().map(|&i| weights[i]).sum::<usize>(), "budget_k": capacity,
           "chosen": chosen.iter().map(|&i| items[i].clone()).collect::<Vec<_>>()})
}

#[cfg(test)]
mod tests {
    use super::knapsack_builtin;
    #[test]
    fn a_producer_enters_the_portfolio_once_with_its_best_line() {
        let c = |acc: u32, score: f64| serde_json::json!({"cargo": "Coal", "producer": {"id": 13}, "acceptor": {"id": acc}, "mode": "freight_rail",
            "score": score, "options": [{"mode": "freight_rail", "investment": 1000.0}]});
        let other = serde_json::json!({"cargo": "Coal", "producer": {"id": 44}, "mode": "freight_rail", "score": 5.0, "options": [{"mode": "freight_rail", "investment": 1000.0}]});
        let pax = serde_json::json!({"kind": "investment", "cargo": "Passengers", "passengers": true, "from": "t1", "mode": "rail", "score": 4.0, "options": [{"mode": "rail", "investment": 1000.0}]});
        let p = super::portfolio(&[c(20, 10.0), c(38, 9.0), other, pax], Some(100000.0));
        let chosen = p["chosen"].as_array().unwrap();
        assert_eq!(chosen.len(), 3);
        assert_eq!(chosen.iter().filter(|x| x.pointer("/producer/id") == Some(&serde_json::json!(13))).count(), 1);
        assert_eq!(p["shadowed"], serde_json::json!(["Coal@13"]));
    }

    #[test]
    fn calibration_scales_net_profit_never_income_alone() {
        // One truck line, 100 tiles: a factor of 0.5 must halve the monthly profit exactly. Scaling the
        // income and then subtracting the running cost charged the cost twice (codex review, 2026-11-17).
        let world = serde_json::json!({"nodes": [
            {"kind": "industry", "id": 1, "x": 10, "y": 10, "produces": [{"name": "Coal", "last_month": 200}]},
            {"kind": "industry", "id": 2, "x": 110, "y": 10, "accepts": [{"name": "Coal"}]}]});
        let engines = serde_json::json!({
            "cargos": [{"name": "Coal", "payment": 5916, "transit_periods": [7, 255]}],
            "engines": [{"type": "road", "name": "Truck", "cargo": "Coal", "capacity": 20, "effective_speed_kmh": 48, "price": 4000, "running_cost_per_year": 2400}]});
        let p = |cal: Option<f64>| {
            let mut v = serde_json::json!({"modes": ["truck"], "freight": {"bands": [20, 250]}});
            if let Some(c) = cal { v["calibration"] = serde_json::json!({"truck": c}); }
            v
        };
        let profit = |cal| super::freight_pairs(&world, &engines, &p(cal), 5)[0]["options"][0]["monthly_profit"].as_f64().unwrap();
        let (plain, half) = (profit(None), profit(Some(0.5)));
        assert!(plain > 0.0);
        assert!((half - plain / 2.0).abs() <= 1.0, "{half} is not half of {plain}");
    }

    #[test]
    fn knapsack_matches_the_algoglyph_case() {
        // The case checked against AlgoGlyph's kernel: items 0 and 3, best 197159.
        let (best, items) = knapsack_builtin(&[135, 46, 20, 5], &[196159.0, 64387.0, 16633.0, 1000.0], 150);
        assert_eq!(best, 197159.0);
        assert_eq!(items, vec![0, 3]);
    }
}
