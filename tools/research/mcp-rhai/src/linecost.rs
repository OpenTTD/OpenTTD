//! RESEARCH-ONLY: what a planned rail line costs on this map, from AlgoGlyph's track-planning.
//!
//! The planner prices a line as distance × the rail tile cost, a straight line on flat land. The
//! track planner lays the real route on the game's map, ways, terrain, prices and bridges (the
//! input hub_algoglyph.rs `track_input` assembles, read once here and shared by every candidate),
//! and answers what it costs in game money, or that there is no route. For the best rail
//! candidates this replaces the estimate: the option's build cost becomes the track's money plus
//! the stations, its score and investment follow, and a candidate with no route loses its rail
//! option. The track input is the one the rail executor (mcp-human rail.rs `plan_track`) lays
//! from: each station site's end tile, leaving by its exit, arriving against the far one's.
use serde_json::{json, Value};

use crate::cargo::algoglyph;

fn num(v: &Value, k: &str) -> f64 { v.get(k).and_then(Value::as_f64).unwrap_or(0.0) }

fn reverse(dir: &str) -> &'static str {
    match dir { "NE" => "SW", "SW" => "NE", "SE" => "NW", _ => "SE" }
}

/// The track-planning endpoints between two station sites (src/sites.rs), as the executor uses them.
pub fn endpoints(a: &Value, b: &Value) -> Option<(Value, Value)> {
    let dir_a = a.pointer("/exit/dir")?.as_str()?;
    let dir_b = b.pointer("/exit/dir")?.as_str()?;
    Some((json!({"x": a.pointer("/end/x")?, "y": a.pointer("/end/y")?, "dir": dir_a}),
          json!({"x": b.pointer("/end/x")?, "y": b.pointer("/end/y")?, "dir": reverse(dir_b)})))
}

/// Price the line of every rail candidate in `candidates` (the `limit` best by score) with the
/// track planner. `base` is the track input without from/to (the game's resources), `rail_tile` the
/// planner's cost of one plain tile (used only when `base` has no prices). Each priced
/// option gets `track: {status, money, estimated_build, counts, pieces, runs}`; the candidate's mode
/// and score are those of its best option afterwards. Returns one log entry per line priced.
pub fn price(candidates: &mut [Value], base: &Value, station_cost: f64, rail_tile: f64, limit: usize) -> Vec<Value> {
    // With the game's prices in the input the planner answers in game money; without them its cost
    // is in plain-track tile units, which the planner's own rail tile cost turns into money.
    let in_money = base.get("prices").is_some_and(Value::is_object);
    let mut order: Vec<usize> = (0..candidates.len())
        .filter(|&i| candidates[i]["options"].as_array().is_some_and(|o| o.iter().any(|o| o["mode"] == "rail" || o["mode"] == "freight_rail")))
        .filter(|&i| candidates[i]["stations"]["from"].is_object() && candidates[i]["stations"]["to"].is_object())
        .collect();
    order.sort_by(|&a, &b| num(&candidates[b], "score").total_cmp(&num(&candidates[a], "score")));
    let mut log = Vec::new();
    for i in order.into_iter().take(limit) {
        let c = &mut candidates[i];
        let Some((from, to)) = endpoints(&c["stations"]["from"], &c["stations"]["to"]) else { continue };
        let mut input = base.clone();
        input["from"] = from.clone();
        input["to"] = to.clone();
        let answer = algoglyph("track-planning", &input);
        let (status, money, counts, pieces, runs) = match &answer {
            Ok(v) => {
                let r = v.get("result").unwrap_or(v);
                let money = if in_money { r["money"].as_f64() } else { r["cost"].as_f64().or_else(|| r["money"].as_f64()).map(|c| c * rail_tile) };
                (r["status"].as_str().unwrap_or("?").to_string(), money,
                 r["counts"].clone(), r["pieces"].as_array().map_or(0, Vec::len), r["runs"].as_array().map_or(0, Vec::len))
            }
            Err(e) => (format!("error: {e}"), None, Value::Null, 0, 0),
        };
        let found = status == "found" && money.is_some();
        for o in c["options"].as_array_mut().into_iter().flatten() {
            if o["mode"] != "rail" && o["mode"] != "freight_rail" { continue; }
            let estimated = num(o, "build_cost");
            if found {
                let build = money.unwrap_or(0.0) + station_cost;
                let delta = build - estimated;
                o["build_cost"] = json!(build.round());
                o["investment"] = json!((num(o, "investment") + delta).round());
                o["score"] = json!((num(o, "score") - delta).round());
            } else {
                // No route on this map: the option cannot be built as planned.
                o["score"] = json!(f64::MIN.round());
            }
            o["track"] = json!({"status": status, "money": money, "estimated_build": estimated, "stations": station_cost,
                                "money_from": if in_money { "the planner's money (openttd://economy/prices)" } else { "cost x rail_tile (the game publishes no openttd://economy/prices)" },
                                "counts": counts, "pieces": pieces, "runs": runs, "from": from, "to": to});
        }
        // The candidate follows its best option now.
        if let Some(best) = c["options"].as_array().and_then(|o| o.iter().max_by(|a, b| num(a, "score").total_cmp(&num(b, "score")))).cloned() {
            c["mode"] = best["mode"].clone();
            c["score"] = best["score"].clone();
        }
        log.push(json!({"candidate": i, "status": status, "money": money, "pieces": pieces, "mode_now": c["mode"], "score_now": c["score"]}));
    }
    log
}

#[cfg(test)]
mod tests {
    use super::endpoints;
    use serde_json::json;

    #[test]
    fn the_line_runs_from_one_station_end_to_the_other_arriving_against_its_exit() {
        let a = json!({"end": {"x": 12, "y": 10}, "exit": {"x": 13, "y": 10, "dir": "SW"}});
        let b = json!({"end": {"x": 40, "y": 10}, "exit": {"x": 39, "y": 10, "dir": "NE"}});
        let (from, to) = endpoints(&a, &b).unwrap();
        assert_eq!(from, json!({"x": 12, "y": 10, "dir": "SW"}));
        assert_eq!(to, json!({"x": 40, "y": 10, "dir": "SW"}));
    }
}
