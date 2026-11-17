//! RESEARCH-ONLY: back-propagate what the game really earned into the planner.
//!
//! The planner predicts a line's monthly profit from the game's payment formula, vehicle speeds and
//! assumptions (load factor, supply share). The running game says what each vehicle really made
//! (openttd://vehicles `profit_last_year`). Here every running line (vehicles with two station orders,
//! grouped by the station pair) is matched to the planner's candidate between the same towns or
//! industries, and the ratio of real to predicted profit per vehicle is taken. The median ratio of
//! each mode is the `calibration` parameter (plan.rs, cargo.rs: predicted income × the factor), so the
//! next plan starts from what the game has shown, not from the model alone.
//!
//! Only vehicles with a whole last year count (age at least two years in days), so a new line's
//! first months do not read as a failure. A line no candidate explains is listed with the reason.
use serde_json::{json, Value};

/// The planner's modes a vehicle type can serve, passenger and freight.
fn modes_of(kind: &str) -> &'static [&'static str] {
    match kind {
        "train" => &["rail", "freight_rail"],
        "road" => &["bus", "truck"],
        "aircraft" => &["air"],
        _ => &[],
    }
}

fn xy(v: &Value) -> Option<(f64, f64)> {
    Some((v.get("x")?.as_f64()?, v.get("y")?.as_f64()?))
}

fn dist(a: (f64, f64), b: (f64, f64)) -> f64 {
    (a.0 - b.0).abs() + (a.1 - b.1).abs()
}

/// The two ends of a candidate: its producer and acceptor (freight) or its towns (passengers).
fn ends(c: &Value) -> Option<((f64, f64), (f64, f64))> {
    if c.get("producer").is_some() {
        Some((xy(&c["producer"])?, xy(&c["acceptor"])?))
    } else {
        Some((xy(&c["a"])?, xy(&c["b"])?))
    }
}

/// Lines as the game runs them: per station pair, the vehicles that shuttle between exactly those two
/// stations, with their type and last year's profit (only vehicles old enough to have one).
pub fn lines(vehicles: &Value, min_age_days: f64) -> Vec<Value> {
    let mut by_pair: std::collections::BTreeMap<(i64, i64), Vec<&Value>> = Default::default();
    for v in vehicles.get("vehicles").and_then(Value::as_array).into_iter().flatten() {
        let stations: Vec<i64> = v["orders"].as_array().into_iter().flatten()
            .filter(|o| o["type"] == "goto_station").filter_map(|o| o["destination"].as_i64()).collect();
        if stations.len() != 2 || stations[0] == stations[1] { continue; }
        let key = (stations[0].min(stations[1]), stations[0].max(stations[1]));
        by_pair.entry(key).or_default().push(v);
    }
    by_pair.into_iter().map(|((a, b), vs)| {
        let old: Vec<&&Value> = vs.iter().filter(|v| v["age"].as_f64().unwrap_or(0.0) >= min_age_days).collect();
        let profits: Vec<f64> = old.iter().filter_map(|v| v["profit_last_year"].as_f64()).collect();
        json!({"stations": [a, b], "type": vs[0]["type"], "vehicles": vs.len(), "measured": profits.len(),
               "real_per_vehicle_month": if profits.is_empty() { Value::Null } else { json!((profits.iter().sum::<f64>() / profits.len() as f64 / 12.0).round()) }})
    }).collect()
}

fn median(mut v: Vec<f64>) -> Option<f64> {
    if v.is_empty() { return None; }
    v.sort_by(f64::total_cmp);
    let m = v.len() / 2;
    Some(if v.len() % 2 == 1 { v[m] } else { (v[m - 1] + v[m]) / 2.0 })
}

/// Match each measured line to the nearest candidate of a mode its vehicles serve (both ends within
/// `reach` tiles of the stations, either way round) and compare. `candidates` are the planner's
/// (freight and passenger lists together), `stations` openttd://stations.
pub fn calibrate(vehicles: &Value, stations: &Value, candidates: &[Value], reach: f64) -> Value {
    let st: Vec<&Value> = stations.get("stations").and_then(Value::as_array).map(|a| a.iter().collect()).unwrap_or_default();
    let at = |id: i64| st.iter().find(|s| s["id"].as_i64() == Some(id)).and_then(|s| xy(s));
    let mut out = Vec::new();
    let mut ratios: std::collections::BTreeMap<String, Vec<f64>> = Default::default();
    for mut line in lines(vehicles, 730.0) {
        let Some(real) = line["real_per_vehicle_month"].as_f64() else {
            line["why"] = json!("no vehicle has a whole last year yet");
            out.push(line);
            continue;
        };
        let (Some(pa), Some(pb)) = (line["stations"][0].as_i64().and_then(at), line["stations"][1].as_i64().and_then(at)) else {
            line["why"] = json!("a station is not in openttd://stations");
            out.push(line);
            continue;
        };
        let modes = modes_of(line["type"].as_str().unwrap_or(""));
        let mut best: Option<(f64, &Value, &Value)> = None;
        for c in candidates {
            let Some((ea, eb)) = ends(c) else { continue };
            let d = (dist(pa, ea) + dist(pb, eb)).min(dist(pa, eb) + dist(pb, ea));
            if dist(pa, ea).min(dist(pa, eb)) > reach || dist(pb, ea).min(dist(pb, eb)) > reach { continue; }
            for o in c["options"].as_array().into_iter().flatten() {
                if !modes.contains(&o["mode"].as_str().unwrap_or("")) { continue; }
                if best.as_ref().map_or(true, |(bd, _, _)| d < *bd) { best = Some((d, c, o)); }
            }
        }
        let Some((_, c, o)) = best else {
            line["why"] = json!(format!("no planner candidate of mode {} ends within {reach} tiles of both stations", modes.join("/")));
            out.push(line);
            continue;
        };
        let n = o.pointer("/vehicles/count").and_then(Value::as_f64).filter(|n| *n >= 1.0).unwrap_or(1.0);
        let predicted = o["monthly_profit"].as_f64().unwrap_or(0.0) / n;
        let mode = o["mode"].as_str().unwrap_or("").to_string();
        line["mode"] = json!(mode);
        let name = |k: &str| c[k]["name"].as_str().map(str::to_string);
        line["candidate"] = json!(format!("{} - {}", name("producer").or_else(|| name("a")).unwrap_or_default(),
                                          name("acceptor").or_else(|| name("b")).unwrap_or_default()));
        line["predicted_per_vehicle_month"] = json!(predicted.round());
        if predicted > 0.0 {
            let r = real / predicted;
            line["ratio"] = json!((r * 100.0).round() / 100.0);
            ratios.entry(mode).or_default().push(r);
        } else {
            line["why"] = json!("the planner predicted no profit for it");
        }
        out.push(line);
    }
    let factors: serde_json::Map<String, Value> = ratios.into_iter()
        .filter_map(|(m, v)| median(v.clone()).map(|r| (m, json!({"factor": (r * 100.0).round() / 100.0, "lines": v.len()}))))
        .collect();
    let mut params: serde_json::Map<String, Value> = factors.iter().map(|(m, f)| (m.clone(), f["factor"].clone())).collect();
    let all: Vec<f64> = out.iter().filter_map(|l| l["ratio"].as_f64()).collect();
    if let Some(d) = median(all) { params.insert("default".into(), json!((d * 100.0).round() / 100.0)); }
    json!({"lines": out, "modes": factors, "params": {"calibration": params},
           "rule": "median of real / predicted profit per vehicle per month, over lines whose vehicles have a whole last year (age >= 730 days); apply with params"})
}

/// The calibration store (research/calibration.json) with one more reading: the reading's modes and
/// lines, when and from where it was taken. `params.calibration` is derived again from every reading:
/// per mode, the median over all readings' line ratios, so a reading of one line counts as one line.
pub fn record(store: &Value, reading: &Value, at: &str, source: &str) -> Value {
    let mut readings: Vec<Value> = store["readings"].as_array().cloned().unwrap_or_default();
    let lines: Vec<Value> = reading["lines"].as_array().into_iter().flatten()
        .filter(|l| l["ratio"].is_number())
        .map(|l| json!({"mode": l["mode"], "candidate": l["candidate"], "ratio": l["ratio"],
                        "real_per_vehicle_month": l["real_per_vehicle_month"], "predicted_per_vehicle_month": l["predicted_per_vehicle_month"]}))
        .collect();
    readings.push(json!({"at": at, "source": source, "modes": reading["modes"], "lines": lines}));
    let mut by_mode: std::collections::BTreeMap<String, Vec<f64>> = Default::default();
    for r in &readings {
        for l in r["lines"].as_array().into_iter().flatten() {
            if let (Some(m), Some(x)) = (l["mode"].as_str(), l["ratio"].as_f64()) { by_mode.entry(m.to_string()).or_default().push(x); }
        }
    }
    let modes: serde_json::Map<String, Value> = by_mode.into_iter()
        .filter_map(|(m, v)| median(v.clone()).map(|r| (m, json!({"factor": (r * 100.0).round() / 100.0, "lines": v.len()}))))
        .collect();
    let mut params: serde_json::Map<String, Value> = modes.iter().map(|(m, f)| (m.clone(), f["factor"].clone())).collect();
    // A mode nobody measured is not trusted more than the measured ones: it gets the median over every
    // line (`default`). Without it, an unmeasured mode kept factor 1 and won every comparison: on the
    // :8800 map the calibrated portfolio swapped two freight lines for three air routes (2026-11-17).
    let all: Vec<f64> = readings.iter().flat_map(|r| r["lines"].as_array().cloned().unwrap_or_default())
        .filter_map(|l| l["ratio"].as_f64()).collect();
    if let Some(d) = median(all) { params.insert("default".into(), json!((d * 100.0).round() / 100.0)); }
    json!({"schema": "openttd-research/calibration/v1",
           "rule": "per mode, the median of real / predicted profit per vehicle per month over every recorded line (tools/research/mcp-rhai src/calibrate.rs); written by mcp-plan --calibrate --record, never by hand",
           "modes": modes, "params": {"calibration": params}, "readings": readings})
}

#[cfg(test)]
mod tests {
    use super::*;

    fn veh(id: i64, kind: &str, a: i64, b: i64, age: f64, profit: f64) -> Value {
        json!({"id": id, "type": kind, "age": age, "profit_last_year": profit,
               "orders": [{"type": "goto_station", "destination": a}, {"type": "goto_station", "destination": b}]})
    }

    #[test]
    fn a_line_is_its_station_pair_and_only_old_vehicles_are_measured() {
        let v = json!({"vehicles": [veh(1, "train", 0, 1, 800.0, 12000.0), veh(2, "train", 1, 0, 900.0, 24000.0),
                                    veh(3, "train", 1, 0, 100.0, -5000.0), veh(4, "road", 2, 3, 100.0, 1.0)]});
        let l = lines(&v, 730.0);
        assert_eq!(l.len(), 2);
        assert_eq!(l[0]["vehicles"], json!(3));
        assert_eq!(l[0]["measured"], json!(2));
        assert_eq!(l[0]["real_per_vehicle_month"], json!(1500.0), "(12000 + 24000) / 2 / 12");
        assert!(l[1]["real_per_vehicle_month"].is_null());
    }

    #[test]
    fn the_factor_is_real_over_predicted_per_vehicle_and_a_far_line_is_explained() {
        let v = json!({"vehicles": [veh(1, "train", 0, 1, 800.0, 24000.0), veh(2, "road", 2, 3, 800.0, 1200.0)]});
        let st = json!({"stations": [{"id": 0, "x": 10, "y": 10}, {"id": 1, "x": 90, "y": 10}, {"id": 2, "x": 200, "y": 200}, {"id": 3, "x": 210, "y": 200}]});
        let cands = vec![json!({"producer": {"x": 88, "y": 12, "name": "Mine"}, "acceptor": {"x": 12, "y": 9, "name": "Plant"},
                                "options": [{"mode": "truck", "monthly_profit": 9.0}, {"mode": "freight_rail", "monthly_profit": 2000.0, "vehicles": {"count": 2}}]})];
        let c = calibrate(&v, &st, &cands, 8.0);
        let l = c["lines"].as_array().unwrap();
        assert_eq!(l[0]["mode"], "freight_rail", "a train is matched to the rail option, not the truck");
        assert_eq!(l[0]["predicted_per_vehicle_month"], json!(1000.0));
        assert_eq!(l[0]["ratio"], json!(2.0), "2000 a month against 1000 predicted");
        assert!(l[1]["why"].as_str().unwrap().contains("no planner candidate"));
        assert_eq!(c["params"]["calibration"]["freight_rail"], json!(2.0));
        assert_eq!(c["params"]["calibration"]["default"], json!(2.0));
    }

    #[test]
    fn readings_accumulate_and_the_factor_is_the_median_over_every_line() {
        let r1 = json!({"modes": {}, "lines": [{"mode": "bus", "ratio": 0.1}, {"mode": "bus", "ratio": 0.3}, {"mode": "rail", "why": "x"}]});
        let s1 = record(&json!({}), &r1, "2026-11-17T16:00Z", "panel :8800");
        assert_eq!(s1["params"]["calibration"]["bus"], json!(0.2));
        assert_eq!(s1["readings"][0]["lines"].as_array().unwrap().len(), 2, "unmeasured lines are not kept");
        let r2 = json!({"modes": {}, "lines": [{"mode": "bus", "ratio": 0.9}]});
        let s2 = record(&s1, &r2, "2026-11-18T10:00Z", "panel :8801");
        assert_eq!(s2["readings"].as_array().unwrap().len(), 2);
        assert_eq!(s2["modes"]["bus"], json!({"factor": 0.3, "lines": 3}), "median of 0.1, 0.3, 0.9");
        assert_eq!(s2["params"]["calibration"]["default"], json!(0.3), "an unmeasured mode gets the median over every line");
    }

    #[test]
    fn the_median_resists_one_outlier() {
        assert_eq!(median(vec![1.0, 100.0, 1.2]), Some(1.2));
        assert_eq!(median(vec![]), None);
    }
}
