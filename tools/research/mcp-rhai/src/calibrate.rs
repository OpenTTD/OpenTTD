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
//!
//! A reading outlives its game in the panel's store (issue I0082, ADR 0034 point 1): `POST
//! /api/calibration` publishes it as plan.calibration, so it lands in the instance's `events`, and the
//! planned bot reads every stored reading back with `readings_sql` (scope `PRISMATIC_PLAN_CALIBRATION`:
//! `all`, `seed`, or off by default) and derives the factors with `factors`. No file is read.
use serde_json::{json, Value};

/// Only vehicles at least this old (in days) have a whole last year to measure.
pub const MIN_AGE_DAYS: f64 = 730.0;
/// A line matches a candidate whose two ends lie within this many tiles of its two stations.
pub const REACH_TILES: f64 = 12.0;
/// How many of the planner's candidates a reading matches lines against (`mcp-plan --limit`): the
/// long list, not the top few.
pub const CANDIDATES: usize = 500;

/// The rule's parameters, as plan.calibration's `version.params` carries them: change one and the
/// digest changes with it.
pub fn rule_params() -> Value {
    json!({"min_age_days": MIN_AGE_DAYS, "reach_tiles": REACH_TILES, "candidates": CANDIDATES})
}

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
    for mut line in lines(vehicles, MIN_AGE_DAYS) {
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
           "rule": format!("median of real / predicted profit per vehicle per month, over lines whose vehicles have a whole last year (age >= {MIN_AGE_DAYS} days); apply with params")})
}

/// How the factors are derived from readings, named in every answer that carries them.
pub const RULE: &str = "per mode, the median of real / predicted profit per vehicle per month over every line of every reading read (src/calibrate.rs factors); a mode nobody measured gets default, the median over every line";

/// The measured lines of a reading (those with a ratio), as the store keeps them.
pub fn measured(lines: &Value) -> Vec<Value> {
    lines.as_array().into_iter().flatten().filter(|l| l["ratio"].is_number())
        .map(|l| json!({"mode": l["mode"], "candidate": l["candidate"], "ratio": l["ratio"],
                        "real_per_vehicle_month": l["real_per_vehicle_month"], "predicted_per_vehicle_month": l["predicted_per_vehicle_month"]}))
        .collect()
}

/// The factors of a set of readings (each `{lines}`): per mode the median over every reading's line
/// ratios, so a reading of one line counts as one line, and `default`, the median over every line.
/// `{modes: {mode: {factor, lines}}, params: {calibration}, lines}`.
pub fn factors(readings: &[Value]) -> Value {
    let mut by_mode: std::collections::BTreeMap<String, Vec<f64>> = Default::default();
    let mut all = Vec::new();
    for r in readings {
        for l in r["lines"].as_array().into_iter().flatten() {
            if let (Some(m), Some(x)) = (l["mode"].as_str(), l["ratio"].as_f64()) {
                by_mode.entry(m.to_string()).or_default().push(x);
                all.push(x);
            }
        }
    }
    let modes: serde_json::Map<String, Value> = by_mode.into_iter()
        .filter_map(|(m, v)| median(v.clone()).map(|r| (m, json!({"factor": (r * 100.0).round() / 100.0, "lines": v.len()}))))
        .collect();
    let mut params: serde_json::Map<String, Value> = modes.iter().map(|(m, f)| (m.clone(), f["factor"].clone())).collect();
    // A mode nobody measured is not trusted more than the measured ones: it gets the median over every
    // line (`default`). Without it, an unmeasured mode kept factor 1 and won every comparison: on the
    // :8800 map the calibrated portfolio swapped two freight lines for three air routes (2026-11-17).
    let n = all.len();
    if let Some(d) = median(all) { params.insert("default".into(), json!((d * 100.0).round() / 100.0)); }
    json!({"modes": modes, "params": {"calibration": params}, "lines": n})
}

/// Which recorded readings the planned bot's calibration reads (`PRISMATIC_PLAN_CALIBRATION`).
#[derive(Clone, Debug, PartialEq)]
pub enum Scope {
    /// Unset, empty or `off`: the model alone (the default; ADR 0034 point 6).
    Off,
    /// `all`: every reading of every store.
    All,
    /// `seed`: the readings of games of this seed and map size.
    Seed,
    /// Anything else, refused with the reason (a file name among them: no file is read any more).
    Unknown(String),
}

pub fn scope(text: Option<&str>) -> Scope {
    match text.map(str::trim).unwrap_or("") {
        "" | "off" | "0" => Scope::Off,
        "all" | "on" | "1" => Scope::All,
        "seed" => Scope::Seed,
        other => Scope::Unknown(other.to_string()),
    }
}

/// The game a reading was taken from, as plan.calibration carries it (`inputs.live.game`) and the
/// `seed` scope compares it: the seed and the map size `WxH`.
/// `state` is openttd://game/state as the server answers it (the envelope, its fields under `data`)
/// or those fields alone.
pub fn game_of(seed: i64, state: &Value) -> Value {
    let d = state.get("data").filter(|d| d.is_object()).unwrap_or(state);
    let map = match (d["map_width"].as_i64(), d["map_height"].as_i64()) {
        (Some(w), Some(h)) => json!(format!("{w}x{h}")),
        _ => Value::Null,
    };
    json!({"seed": seed, "map": map})
}

/// The newest readings one store answers; enough for every recorded game so far.
pub const QUERY_READINGS: usize = 200;

/// The read-only statement that reads the calibration readings of one store: every plan.calibration
/// event, with the seed of the run it belongs to (the newest run of its instance started before it)
/// and the map its game had, newest first. `game` (`game_of`) narrows it to one seed and map; an
/// error when the `seed` scope has no map to compare.
pub fn readings_sql(scope: &Scope, game: &Value) -> Result<String, String> {
    let filter = match scope {
        Scope::Seed => {
            let seed = game["seed"].as_i64().ok_or("the seed scope needs this game's seed")?;
            let map = game["map"].as_str().filter(|m| m.chars().all(|c| c.is_ascii_digit() || c == 'x'))
                .ok_or("the seed scope needs this game's map size, and openttd://game/state gave none")?;
            format!(" AND r.seed = {seed} AND json_extract_string(e.data, '$.inputs.live.game.map') = '{map}'")
        }
        Scope::All => String::new(),
        Scope::Off | Scope::Unknown(_) => return Err("no query: calibration is not asked for".into()),
    };
    Ok(format!("WITH r AS (SELECT instance, seed, started_ms, lead(started_ms) OVER (PARTITION BY instance ORDER BY started_ms) AS until_ms FROM runs) \
SELECT e.instance, e.seq, e.ts_ms, r.seed, json_extract_string(e.data, '$.inputs.live.game.map') AS map, CAST(json_extract(e.data, '$.choice.reading.lines') AS VARCHAR) AS lines \
FROM events e LEFT JOIN r ON r.instance = e.instance AND e.ts_ms >= r.started_ms AND (r.until_ms IS NULL OR e.ts_ms < r.until_ms) \
WHERE e.kind = 'plan.calibration'{filter} ORDER BY e.ts_ms DESC LIMIT {QUERY_READINGS}"))
}

/// The readings of a `readings_sql` answer (`{columns, rows}`), each `{instance, seq, ts_ms, seed, map,
/// lines}` with only its measured lines; a row whose lines measured nothing is left out.
pub fn readings_of(answer: &Value) -> Vec<Value> {
    let cols: Vec<&str> = answer["columns"].as_array().into_iter().flatten().filter_map(Value::as_str).collect();
    let at = |row: &Value, name: &str| cols.iter().position(|c| *c == name).map_or(Value::Null, |i| row[i].clone());
    answer["rows"].as_array().into_iter().flatten().filter_map(|row| {
        let lines = match at(row, "lines") {
            Value::String(t) => serde_json::from_str::<Value>(&t).unwrap_or(Value::Null),
            other => other,
        };
        let lines = measured(&lines);
        if lines.is_empty() { return None; }
        Some(json!({"instance": at(row, "instance"), "seq": at(row, "seq"), "ts_ms": at(row, "ts_ms"),
                    "seed": at(row, "seed"), "map": at(row, "map"), "lines": lines}))
    }).collect()
}

/// The readings of the retired calibration file (schema openttd-research/calibration/v1, written by
/// `mcp-plan --calibrate --record` until I0082 removed it), for the one import into the store.
pub fn file_readings(file: &Value) -> Result<Vec<Value>, String> {
    if file["schema"] != "openttd-research/calibration/v1" {
        return Err(format!("not a calibration file of schema openttd-research/calibration/v1: {}", file["schema"]));
    }
    Ok(file["readings"].as_array().cloned().unwrap_or_default())
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
    fn the_factor_is_the_median_over_every_line_of_every_reading() {
        let r1 = json!({"lines": measured(&json!([{"mode": "bus", "ratio": 0.1}, {"mode": "bus", "ratio": 0.3}, {"mode": "rail", "why": "x"}]))});
        assert_eq!(r1["lines"].as_array().unwrap().len(), 2, "unmeasured lines are not kept");
        let f1 = factors(&[r1.clone()]);
        assert_eq!(f1["params"]["calibration"]["bus"], json!(0.2));
        let r2 = json!({"lines": [{"mode": "bus", "ratio": 0.9}]});
        let f2 = factors(&[r1, r2]);
        assert_eq!(f2["modes"]["bus"], json!({"factor": 0.3, "lines": 3}), "median of 0.1, 0.3, 0.9");
        assert_eq!(f2["params"]["calibration"]["default"], json!(0.3), "an unmeasured mode gets the median over every line");
        assert_eq!(f2["lines"], 3);
        assert_eq!(factors(&[]), json!({"modes": {}, "params": {"calibration": {}}, "lines": 0}));
    }

    #[test]
    fn the_retired_file_gives_the_factors_it_held() {
        // The one reading the retired file held (2026-11-17T16:55Z, the :8800 game): the
        // store's rule gives the same factors the file's params carried.
        let file = json!({"schema": "openttd-research/calibration/v1", "readings": [{"at": "2026-11-17T16:55Z", "lines": [
            {"mode": "freight_rail", "ratio": 0.59}, {"mode": "bus", "ratio": 0.24}, {"mode": "freight_rail", "ratio": 0.78},
            {"mode": "bus", "ratio": 0.08}, {"mode": "freight_rail", "ratio": 0.49}, {"mode": "rail", "ratio": 0.15}]}]});
        let readings = file_readings(&file).unwrap();
        assert_eq!(factors(&readings)["params"]["calibration"], json!({"bus": 0.16, "default": 0.37, "freight_rail": 0.59, "rail": 0.15}));
        assert!(file_readings(&json!({"readings": []})).is_err());
    }

    #[test]
    fn the_scope_is_off_unless_asked_and_a_file_name_is_refused() {
        assert_eq!(scope(None), Scope::Off);
        assert_eq!(scope(Some("off")), Scope::Off);
        assert_eq!(scope(Some(" ")), Scope::Off);
        assert_eq!(scope(Some("all")), Scope::All);
        assert_eq!(scope(Some("seed")), Scope::Seed);
        assert_eq!(scope(Some("some/calibration.json")), Scope::Unknown("some/calibration.json".into()));
        let game = game_of(3, &json!({"map_width": 256, "map_height": 128}));
        assert_eq!(game, json!({"seed": 3, "map": "256x128"}));
        assert_eq!(game_of(3, &json!({"availability": "available", "data": {"map_width": 64, "map_height": 64}}))["map"], "64x64",
                   "the resource's envelope");
        assert!(readings_sql(&Scope::Seed, &game).unwrap().ends_with(&format!(
            "WHERE e.kind = 'plan.calibration' AND r.seed = 3 AND json_extract_string(e.data, '$.inputs.live.game.map') = '256x128' ORDER BY e.ts_ms DESC LIMIT {QUERY_READINGS}")));
        assert!(readings_sql(&Scope::All, &game).unwrap().contains("WHERE e.kind = 'plan.calibration' ORDER BY"));
        assert!(readings_sql(&Scope::Seed, &game_of(3, &json!({}))).unwrap_err().contains("map size"));
        assert!(readings_sql(&Scope::Off, &game).is_err());
    }

    #[test]
    fn an_answer_gives_its_measured_readings() {
        let answer = json!({"columns": ["instance", "seq", "ts_ms", "seed", "map", "lines"], "rows": [
            ["panel-1", 7, 100, 3, "256x256", "[{\"mode\":\"bus\",\"ratio\":0.5,\"candidate\":\"A - B\"},{\"mode\":\"rail\",\"why\":\"far\"}]"],
            ["panel-1", 5, 90, 3, "256x256", "[{\"mode\":\"rail\",\"why\":\"far\"}]"],
            ["calibration-import", 1, 80, null, null, "not json"]]});
        let r = readings_of(&answer);
        assert_eq!(r.len(), 1, "a reading that measured nothing is no reading");
        assert_eq!(r[0]["seq"], 7);
        assert_eq!(r[0]["lines"], json!([{"mode": "bus", "ratio": 0.5, "candidate": "A - B", "real_per_vehicle_month": null, "predicted_per_vehicle_month": null}]));
    }

    #[test]
    fn the_median_resists_one_outlier() {
        assert_eq!(median(vec![1.0, 100.0, 1.2]), Some(1.2));
        assert_eq!(median(vec![]), None);
    }
}
