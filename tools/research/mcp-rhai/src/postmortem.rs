//! RESEARCH-ONLY: the post-mortem of LAN AI battle matches (research/battle-postmortem.md).
//!
//! Pure analysis, no I/O: a match record as `battle.py` writes it
//! (`~/ttd-battle/results/<edition>/matches/<id>.json`: lineup, standings and a quarterly series per
//! contestant) goes in, and the answer to "where does this contestant lose" comes out. The answer
//! has three parts:
//! - the facts of the contestant's game: final value, rank, peak and final fleet, the share of its
//!   value that is idle cash, income per vehicle, stations per vehicle;
//! - `gap_opens`: the first quarter from which the contestant's value stays below half of the
//!   winner's to the end;
//! - the losses: each one names a cause the facts show (`stalls_small`, `fleet_decays`,
//!   `hoards_cash`, `low_yield`, `idle_stations`), with the measured value and its threshold.
//!
//! An edition's post-mortem aggregates every finished match per contestant and turns the most
//! frequent losses into proposals: a settings string for PrismaticAI to try as a new contestant in
//! the next edition. A proposal is an input for the battle's owner, never applied by this module.
//! Every threshold is a named constant, so the report states the rule it applied.
use serde_json::{json, Value};
use std::collections::BTreeMap;

/// A fleet whose peak stays below this never grew: the bot stopped at its first few routes.
pub const STALL_PEAK_VEHICLES: f64 = 10.0;
/// A fleet that ends below this share of its peak decayed: vehicles were sold and not replaced.
pub const DECAY_FINAL_SHARE: f64 = 0.6;
/// Idle cash above this share of company value over the last year ...
pub const HOARD_CASH_SHARE: f64 = 0.6;
/// ... while the fleet is below this share of the winner's: money that is not working. Late in a
/// game a winner's value is mostly cash too (vehicles depreciate), so cash alone is no cause.
pub const HOARD_FLEET_SHARE: f64 = 0.25;
/// Income per vehicle below this share of the winner's: the vehicles earn too little.
pub const LOW_YIELD_SHARE: f64 = 0.5;
/// More than this many stations per vehicle: stations built and left unserved.
pub const IDLE_STATIONS_PER_VEHICLE: f64 = 2.0;
/// A first vehicle later than this many quarters after the first sample is a slow start: the
/// winner of edition 91a72662 ran 4 planes in its first quarter.
pub const SLOW_START_QUARTERS: usize = 1;
/// A mode that earns at least this share of the winner's profit over the last year, where the
/// contestant runs no vehicle of that mode, is a mode gap (referee v3 fields; absent before).
pub const MODE_GAP_SHARE: f64 = 0.3;
/// The referee's per-mode fields: vehicle count and last year's profit of each mode.
pub const MODES: [(&str, &str, &str); 4] = [("rail", "rail", "prail"), ("road", "road", "proad"), ("air", "air", "pair"), ("water", "water", "pwater")];

/// The gap to the winner is open while the value is below this share of the winner's.
pub const GAP_SHARE: f64 = 0.5;
/// Quarters averaged as "the last year".
pub const LAST_QUARTERS: usize = 4;

fn num(v: &Value, k: &str) -> f64 {
    v.get(k).and_then(Value::as_f64).unwrap_or(0.0)
}

fn mean(xs: &[f64]) -> f64 {
    if xs.is_empty() { 0.0 } else { xs.iter().sum::<f64>() / xs.len() as f64 }
}

/// The last `LAST_QUARTERS` rows of a series (fewer when the series is shorter).
fn tail(rows: &[Value]) -> &[Value] {
    &rows[rows.len().saturating_sub(LAST_QUARTERS)..]
}

/// The facts and losses of one contestant in one finished match, or None when the match did not
/// finish or the contestant has no series.
pub fn contestant(m: &Value, key: &str) -> Option<Value> {
    if m.get("status").and_then(Value::as_str) != Some("done") {
        return None;
    }
    let standings = m.get("standings")?.as_array()?;
    let rank = standings.iter().position(|s| s.get("key").and_then(Value::as_str) == Some(key))?;
    let field = standings.len();
    let winner_key = standings.first()?.get("key")?.as_str()?;
    let series = m.get("series")?.as_object()?;
    let rows = series.get(key)?.as_array()?;
    let win = series.get(winner_key)?.as_array()?;
    if rows.is_empty() || win.is_empty() {
        return None;
    }

    let last = tail(rows);
    let wlast = tail(win);
    let value = num(&standings[rank], "value");
    let peak = rows.iter().map(|r| num(r, "vehicles")).fold(0.0, f64::max);
    let final_vehicles = num(rows.last()?, "vehicles");
    let vehicles_last = mean(&last.iter().map(|r| num(r, "vehicles")).collect::<Vec<_>>());
    let income_last = mean(&last.iter().map(|r| num(r, "income")).collect::<Vec<_>>());
    // A quarter without company value (the first one) says nothing about idle cash: left out.
    let idle = mean(&last.iter().filter(|r| num(r, "value") > 0.0)
        .map(|r| (num(r, "money") / num(r, "value")).clamp(0.0, 1.0)).collect::<Vec<_>>());
    let stations = num(rows.last()?, "stations");
    let ipv = if vehicles_last > 0.0 { income_last / vehicles_last } else { 0.0 };
    let w_vehicles = mean(&wlast.iter().map(|r| num(r, "vehicles")).collect::<Vec<_>>());
    let w_income = mean(&wlast.iter().map(|r| num(r, "income")).collect::<Vec<_>>());
    let w_ipv = if w_vehicles > 0.0 { w_income / w_vehicles } else { 0.0 };
    let spv = if final_vehicles > 0.0 { stations / final_vehicles } else { stations };

    // The quarter from which the value stays below GAP_SHARE of the winner's (rows align by "t").
    let wv: BTreeMap<&str, f64> = win.iter().filter_map(|r| Some((r.get("t")?.as_str()?, num(r, "value")))).collect();
    let mut gap_opens: Option<String> = None;
    if rank > 0 {
        for r in rows {
            let t = r.get("t").and_then(Value::as_str).unwrap_or("");
            let behind = wv.get(t).is_some_and(|w| *w > 0.0 && num(r, "value") < GAP_SHARE * w);
            match (behind, &gap_opens) {
                (true, None) => gap_opens = Some(t.to_string()),
                (false, Some(_)) => gap_opens = None,
                _ => {}
            }
        }
    }

    // Quarters until the first vehicle (None: never).
    let first_vehicle = rows.iter().position(|r| num(r, "vehicles") > 0.0);
    // Per mode: the winner's share of its profit over the last year, and whether we run that mode.
    let w_profit: Vec<f64> = MODES.iter().map(|(_, _, p)| mean(&wlast.iter().map(|r| num(r, p).max(0.0)).collect::<Vec<_>>())).collect();
    let w_total: f64 = w_profit.iter().sum();
    let mut mode_gaps = Vec::new();
    let mut mix = serde_json::Map::new();
    if w_total > 0.0 {
        for (i, (mode, count, _)) in MODES.iter().enumerate() {
            let ours = last.iter().map(|r| num(r, count)).fold(0.0, f64::max);
            let share = w_profit[i] / w_total;
            mix.insert(mode.to_string(), json!({"winner_profit_share": round3(share), "vehicles": ours}));
            if rank > 0 && share >= MODE_GAP_SHARE && ours == 0.0 {
                mode_gaps.push((mode.to_string(), share));
            }
        }
    }

    let mut losses = Vec::new();
    let mut loss = |cause: &str, measured: f64, threshold: f64, what: &str| {
        losses.push(json!({"cause": cause, "measured": round3(measured), "threshold": threshold, "what": what}));
    };
    if peak < STALL_PEAK_VEHICLES {
        loss("stalls_small", peak, STALL_PEAK_VEHICLES, "peak fleet below the threshold: it never grew past its first routes");
    }
    if peak >= 1.0 && final_vehicles < DECAY_FINAL_SHARE * peak {
        loss("fleet_decays", final_vehicles / peak, DECAY_FINAL_SHARE, "final fleet as a share of its peak: vehicles sold, not replaced");
    }
    if rank > 0 && idle > HOARD_CASH_SHARE && vehicles_last < HOARD_FLEET_SHARE * w_vehicles {
        loss("hoards_cash", idle, HOARD_CASH_SHARE, "share of company value held as cash over the last year, with a fleet below a quarter of the winner's");
    }
    if rank > 0 && w_ipv > 0.0 && ipv < LOW_YIELD_SHARE * w_ipv {
        loss("low_yield", ipv / w_ipv, LOW_YIELD_SHARE, "income per vehicle as a share of the winner's");
    }
    match first_vehicle {
        Some(q) if q > SLOW_START_QUARTERS => loss("slow_start", q as f64, SLOW_START_QUARTERS as f64, "quarters before its first vehicle"),
        None => loss("slow_start", rows.len() as f64, SLOW_START_QUARTERS as f64, "no vehicle in the whole game"),
        _ => {}
    }
    if spv > IDLE_STATIONS_PER_VEHICLE {
        loss("idle_stations", spv, IDLE_STATIONS_PER_VEHICLE, "stations per vehicle at the end: stations left unserved");
    }

    for (mode, share) in &mode_gaps {
        losses.push(json!({"cause": "mode_gap", "mode": mode, "measured": round3(*share), "threshold": MODE_GAP_SHARE,
            "what": format!("the winner's share of profit from {mode}, where it runs no {mode} vehicle")}));
    }

    Some(json!({
        "match": m.get("id").cloned().unwrap_or(Value::Null),
        "host": m.get("host").cloned().unwrap_or(Value::Null),
        "seed": m.get("seed").cloned().unwrap_or(Value::Null),
        "key": key,
        "rank": rank + 1,
        "field": field,
        "last": rank + 1 == field,
        "winner": winner_key,
        "value": value,
        "winner_value": num(&standings[0], "value"),
        "value_share_of_winner": if num(&standings[0], "value") > 0.0 { round3(value / num(&standings[0], "value")) } else { 0.0 },
        "peak_vehicles": peak,
        "final_vehicles": final_vehicles,
        "winner_vehicles": round3(w_vehicles),
        "idle_cash_share": round3(idle),
        "income_per_vehicle": round3(ipv),
        "winner_income_per_vehicle": round3(w_ipv),
        "stations": stations,
        "stations_per_vehicle": round3(spv),
        "gap_opens": gap_opens,
        "first_vehicle_quarter": first_vehicle,
        "mode_mix": mix,
        "losses": losses,
    }))
}

fn round3(x: f64) -> f64 {
    (x * 1000.0).round() / 1000.0
}

/// The contestants of a match, in lineup order (their `key`s).
pub fn keys(m: &Value) -> Vec<String> {
    m.get("lineup").and_then(Value::as_array).map(|l| {
        l.iter().filter_map(|c| c.get("key").and_then(Value::as_str).map(String::from)).collect()
    }).unwrap_or_default()
}

/// The settings string a contestant ran with ("" for defaults), from the match lineup.
pub fn settings_of(m: &Value, key: &str) -> String {
    m.get("lineup").and_then(Value::as_array).and_then(|l| {
        l.iter().find(|c| c.get("key").and_then(Value::as_str) == Some(key))
            .and_then(|c| c.get("settings").and_then(Value::as_str).map(String::from))
    }).unwrap_or_default()
}

/// A settings string as an ordered map.
fn parse_settings(s: &str) -> BTreeMap<String, String> {
    s.split(',').filter_map(|kv| {
        let (k, v) = kv.split_once('=')?;
        Some((k.trim().to_string(), v.trim().to_string()))
    }).collect()
}

fn format_settings(m: &BTreeMap<String, String>) -> String {
    m.iter().map(|(k, v)| format!("{k}={v}")).collect::<Vec<_>>().join(",")
}

/// What each cause asks of PrismaticAI's settings: the knobs that address it (info.nut).
fn remedy(cause: &str, s: &mut BTreeMap<String, String>) {
    let mut set = |k: &str, v: &str| { s.insert(k.into(), v.into()); };
    match cause {
        "stalls_small" => { set("aggressiveness", "100"); set("max_routes", "30"); set("cash_reserve", "5000"); set("intercity", "1"); }
        "fleet_decays" | "hoards_cash" | "idle_stations" => { set("self_steward", "1"); set("cash_reserve", "5000"); }
        "low_yield" => { set("own_lines", "8"); set("intercity", "1"); }
        "slow_start" => { set("cash_reserve", "5000"); set("aggressiveness", "100"); }
        _ => {}
    }
}

/// The fields the yearly trajectory follows (the January sample of each year, median over matches).
pub const TRAJECTORY_FIELDS: [&str; 7] = ["value", "vehicles", "air", "rail", "pair", "prail", "proad"];

fn median(mut xs: Vec<f64>) -> f64 {
    if xs.is_empty() { return 0.0; }
    xs.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
    let n = xs.len();
    if n % 2 == 1 { xs[n / 2] } else { (xs[n / 2 - 1] + xs[n / 2]) / 2.0 }
}

/// Per contestant, per year: the median over finished matches of each TRAJECTORY_FIELDS value in
/// the January sample. It shows when and in what a contestant falls behind, e.g. the winner's air
/// profit in year 1 against none of ours.
pub fn trajectories(matches: &[Value]) -> Value {
    let mut acc: BTreeMap<String, BTreeMap<String, BTreeMap<&str, Vec<f64>>>> = BTreeMap::new();
    for m in matches {
        if m.get("status").and_then(Value::as_str) != Some("done") || m.get("replica_of").is_some_and(|r| !r.is_null()) {
            continue;
        }
        for (k, rows) in m.get("series").and_then(Value::as_object).into_iter().flatten() {
            for r in rows.as_array().into_iter().flatten() {
                let t = r.get("t").and_then(Value::as_str).unwrap_or("");
                if !t.ends_with("-01") { continue; }
                let year = acc.entry(k.clone()).or_default().entry(t[..4].to_string()).or_default();
                for f in TRAJECTORY_FIELDS {
                    if let Some(v) = r.get(f).and_then(Value::as_f64) { year.entry(f).or_default().push(v); }
                }
            }
        }
    }
    let mut out = serde_json::Map::new();
    for (k, years) in acc {
        let mut ys = serde_json::Map::new();
        for (y, fields) in years {
            let mut row = serde_json::Map::new();
            for (f, xs) in fields { row.insert(f.to_string(), json!(median(xs))); }
            ys.insert(y, Value::Object(row));
        }
        out.insert(k, Value::Object(ys));
    }
    Value::Object(out)
}

/// The post-mortem of one edition: every finished match's contestant analyses, aggregated per
/// contestant, and a proposal per PrismaticAI contestant that lost (`prismatic` says which keys are
/// PrismaticAI). `matches` may come in any order; the result is sorted, so it is deterministic.
pub fn edition(edition: &str, matches: &[Value], prismatic: &dyn Fn(&Value, &str) -> bool) -> Value {
    let mut per: BTreeMap<String, Vec<Value>> = BTreeMap::new();
    let mut settings: BTreeMap<String, String> = BTreeMap::new();
    let mut is_prismatic: BTreeMap<String, bool> = BTreeMap::new();
    let mut done = 0;
    for m in matches {
        if m.get("status").and_then(Value::as_str) != Some("done") || m.get("replica_of").is_some_and(|r| !r.is_null()) {
            continue;
        }
        done += 1;
        for k in keys(m) {
            if let Some(a) = contestant(m, &k) {
                settings.entry(k.clone()).or_insert_with(|| settings_of(m, &k));
                is_prismatic.entry(k.clone()).or_insert_with(|| prismatic(m, &k));
                per.entry(k).or_default().push(a);
            }
        }
    }
    let mut contestants = Vec::new();
    let mut proposals = Vec::new();
    for (k, rows) in &mut per {
        rows.sort_by(|a, b| a["match"].as_str().cmp(&b["match"].as_str()));
        let n = rows.len() as f64;
        let ranks: Vec<f64> = rows.iter().map(|r| num(r, "rank")).collect();
        let lasts = rows.iter().filter(|r| r["last"].as_bool() == Some(true)).count();
        let mut causes: BTreeMap<String, usize> = BTreeMap::new();
        for r in rows.iter() {
            for l in r["losses"].as_array().into_iter().flatten() {
                let c = l["cause"].as_str().unwrap_or("");
                let k = match l["mode"].as_str() { Some(m) => format!("{c}:{m}"), None => c.to_string() };
                *causes.entry(k).or_default() += 1;
            }
        }
        let mut gaps: Vec<&str> = rows.iter().filter_map(|r| r["gap_opens"].as_str()).collect();
        gaps.sort();
        let median_gap = gaps.get(gaps.len() / 2).map(|s| s.to_string());
        let p = is_prismatic.get(k).copied().unwrap_or(false);
        let entry = json!({
            "key": k,
            "prismatic": p,
            "settings": settings.get(k).cloned().unwrap_or_default(),
            "matches": rows.len(),
            "avg_rank": round3(mean(&ranks)),
            "last_place": lasts,
            "avg_value_share_of_winner": round3(mean(&rows.iter().map(|r| num(r, "value_share_of_winner")).collect::<Vec<_>>())),
            "avg_peak_vehicles": round3(mean(&rows.iter().map(|r| num(r, "peak_vehicles")).collect::<Vec<_>>())),
            "avg_final_vehicles": round3(mean(&rows.iter().map(|r| num(r, "final_vehicles")).collect::<Vec<_>>())),
            "avg_idle_cash_share": round3(mean(&rows.iter().map(|r| num(r, "idle_cash_share")).collect::<Vec<_>>())),
            "median_gap_opens": median_gap,
            "causes": causes.iter().map(|(c, n)| json!({"cause": c, "matches": n})).collect::<Vec<_>>(),
        });
        // Proposal: the causes seen in at least half of its matches, each remedied on its settings.
        if p && n > 0.0 {
            let mut s = parse_settings(settings.get(k).map(String::as_str).unwrap_or(""));
            let before = s.clone();
            let mut because = Vec::new();
            for (c, cnt) in &causes {
                if (*cnt as f64) >= n / 2.0 {
                    remedy(c, &mut s);
                    because.push(json!({"cause": c, "matches": cnt}));
                }
            }
            if s != before {
                proposals.push(json!({"for": k, "settings": format_settings(&s), "because": because}));
            }
        }
        contestants.push(entry);
    }
    contestants.sort_by(|a, b| num(a, "avg_rank").partial_cmp(&num(b, "avg_rank")).unwrap_or(std::cmp::Ordering::Equal)
        .then(a["key"].as_str().cmp(&b["key"].as_str())));
    let field = contestants.len();
    let prismatic_last: Vec<Value> = contestants.iter()
        .filter(|c| c["prismatic"].as_bool() == Some(true) && num(c, "last_place") > 0.0)
        .map(|c| json!({"key": c["key"], "last_place": c["last_place"], "matches": c["matches"]})).collect();
    let mut analyses = Vec::new();
    for rows in per.values() {
        analyses.extend(rows.iter().cloned());
    }
    analyses.sort_by(|a, b| (a["match"].as_str(), a["key"].as_str()).cmp(&(b["match"].as_str(), b["key"].as_str())));
    json!({
        "schema": "battle-postmortem/v1",
        "edition": edition,
        "matches": done,
        "field": field,
        "thresholds": {
            "stall_peak_vehicles": STALL_PEAK_VEHICLES, "decay_final_share": DECAY_FINAL_SHARE,
            "hoard_cash_share": HOARD_CASH_SHARE, "hoard_fleet_share": HOARD_FLEET_SHARE, "low_yield_share": LOW_YIELD_SHARE,
            "idle_stations_per_vehicle": IDLE_STATIONS_PER_VEHICLE, "gap_share": GAP_SHARE, "slow_start_quarters": SLOW_START_QUARTERS, "mode_gap_share": MODE_GAP_SHARE,
            "last_quarters": LAST_QUARTERS,
        },
        "contestants": contestants,
        "prismatic_last": prismatic_last,
        "proposals": proposals,
        "trajectories": trajectories(matches),
        "analyses": analyses,
    })
}

/// The post-mortem as Markdown, derived from its JSON only (so `check` can regenerate and compare).
pub fn markdown(pm: &Value) -> String {
    let mut s = String::new();
    let ed = pm["edition"].as_str().unwrap_or("?");
    s += &format!("# Battle post-mortem: edition {ed}\n\n");
    s += "**RESEARCH-ONLY.** Derived by `battle-postmortem` from the edition's match records; do not edit.\n";
    s += "How it is computed: [research/battle-postmortem.md](../battle-postmortem.md).\n\n";
    s += &format!("Finished matches: {}. Contestants: {}.\n\n", pm["matches"], pm["field"]);
    let last = pm["prismatic_last"].as_array().cloned().unwrap_or_default();
    if last.is_empty() {
        s += "**No PrismaticAI contestant finished last in any match.**\n\n";
    } else {
        s += "**PrismaticAI finished last:**\n\n";
        for l in &last {
            s += &format!("- `{}`: last in {} of {} matches\n", l["key"].as_str().unwrap_or(""), l["last_place"], l["matches"]);
        }
        s += "\n";
    }
    s += "## Contestants\n\n| Contestant | Matches | Avg rank | Last | Value / winner | Peak fleet | Final fleet | Idle cash | Gap opens (median) | Causes (matches) |\n|---|---|---|---|---|---|---|---|---|---|\n";
    for c in pm["contestants"].as_array().into_iter().flatten() {
        let causes = c["causes"].as_array().into_iter().flatten()
            .map(|x| format!("{} ({})", x["cause"].as_str().unwrap_or(""), x["matches"])).collect::<Vec<_>>().join(", ");
        s += &format!("| {}{} | {} | {} | {} | {:.3} | {:.1} | {:.1} | {:.2} | {} | {} |\n",
            c["key"].as_str().unwrap_or(""), if c["prismatic"].as_bool() == Some(true) { " *" } else { "" },
            c["matches"], c["avg_rank"], c["last_place"], num(c, "avg_value_share_of_winner"),
            num(c, "avg_peak_vehicles"), num(c, "avg_final_vehicles"), num(c, "avg_idle_cash_share"),
            c["median_gap_opens"].as_str().unwrap_or("-"), if causes.is_empty() { "-".into() } else { causes });
    }
    s += "\n`*` PrismaticAI. Causes and thresholds:\n\n";
    let t = &pm["thresholds"];
    s += &format!("- `stalls_small`: peak fleet below {}\n", t["stall_peak_vehicles"]);
    s += &format!("- `fleet_decays`: final fleet below {} of its peak\n", t["decay_final_share"]);
    s += &format!("- `hoards_cash`: cash above {} of company value over the last {} quarters, with a fleet below {} of the winner's\n",
        t["hoard_cash_share"], t["last_quarters"], t["hoard_fleet_share"]);
    s += &format!("- `low_yield`: income per vehicle below {} of the winner's\n", t["low_yield_share"]);
    s += &format!("- `idle_stations`: more than {} stations per vehicle at the end\n", t["idle_stations_per_vehicle"]);
    s += &format!("- `slow_start`: first vehicle more than {} quarter(s) after the start\n", t["slow_start_quarters"]);
    s += &format!("- `mode_gap`: a mode that earns at least {} of the winner's profit, with no vehicle of it (referee v3)\n", t["mode_gap_share"]);
    s += &format!("- gap opens: the first quarter from which value stays below {} of the winner's\n\n", t["gap_share"]);
    // When and in what: the yearly medians of the best contestant against every PrismaticAI one.
    if let (Some(best), Some(traj)) = (pm["contestants"].as_array().and_then(|c| c.first()), pm["trajectories"].as_object()) {
        let best = best["key"].as_str().unwrap_or("");
        let mut who: Vec<&str> = vec![best];
        for c in pm["contestants"].as_array().into_iter().flatten() {
            if c["prismatic"].as_bool() == Some(true) { who.push(c["key"].as_str().unwrap_or("")); }
        }
        if traj.contains_key(best) {
            s += "## Trajectory (median of the January samples over the matches)\n\n";
            s += "Value / vehicles (of which aircraft and trains) / last year's profit from air and rail, the \
                  best contestant first. Without the parenthesis, the referee of that edition had no per-mode fields.\n\n";
            s += "| Year |";
            for w in &who { s += &format!(" {w} |"); }
            s += "\n|---|";
            for _ in &who { s += "---|"; }
            s += "\n";
            let years: Vec<String> = traj[best].as_object().map(|y| y.keys().cloned().collect()).unwrap_or_default();
            for (i, y) in years.iter().enumerate() {
                if !(i < 3 || i % 5 == 0 || i + 1 == years.len()) { continue; }
                s += &format!("| {y} |");
                for w in &who {
                    let r = &traj[*w][y.as_str()];
                    let f = |k: &str| r.get(k).and_then(Value::as_f64);
                    let k = |x: Option<f64>| x.map(|v| if v.abs() >= 1e6 { format!("{:.1}M", v / 1e6) } else if v.abs() >= 1e3 { format!("{:.0}k", v / 1e3) } else { format!("{v:.0}") }).unwrap_or_default();
                    s += &format!(" {} / {} veh", k(f("value")), k(f("vehicles")));
                    if f("air").is_some() || f("rail").is_some() {
                        s += &format!(" ({} air, {} rail) / profit air {} rail {}", k(f("air")), k(f("rail")), k(f("pair")), k(f("prail")));
                    }
                    s += " |";
                }
                s += "\n";
            }
            s += "\n";
        }
    }
    s += "## Proposals for the next edition\n\n";
    let props = pm["proposals"].as_array().cloned().unwrap_or_default();
    if props.is_empty() {
        s += "None: no PrismaticAI contestant has a cause in at least half of its matches that its settings can address.\n";
    } else {
        s += "Settings the causes ask for. These are inputs for the battle's owner, who decides whether they \
              become contestants; nothing applies them.\n\n| For | Settings | Because (matches) |\n|---|---|---|\n";
        for p in &props {
            let because = p["because"].as_array().into_iter().flatten()
                .map(|b| format!("{} ({})", b["cause"].as_str().unwrap_or(""), b["matches"])).collect::<Vec<_>>().join(", ");
            s += &format!("| {} | `{}` | {} |\n", p["for"].as_str().unwrap_or(""), p["settings"].as_str().unwrap_or(""), because);
        }
    }
    s
}

/// Whether a lineup entry is PrismaticAI: its `ai` field.
pub fn lineup_is_prismatic(m: &Value, key: &str) -> bool {
    m.get("lineup").and_then(Value::as_array).is_some_and(|l| {
        l.iter().any(|c| c.get("key").and_then(Value::as_str) == Some(key)
            && c.get("ai").and_then(Value::as_str) == Some("PrismaticAI"))
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn q(t: &str, value: f64, money: f64, income: f64, vehicles: f64, stations: f64) -> Value {
        json!({"t": t, "value": value, "money": money, "income": income, "vehicles": vehicles, "stations": stations})
    }

    fn fixture() -> Value {
        json!({
            "id": "m-0001", "edition": "e1", "status": "done", "replica_of": null, "host": "h", "seed": 1,
            "lineup": [
                {"ai": "Winner", "key": "Winner"},
                {"ai": "PrismaticAI", "key": "PrismaticAI"},
                {"ai": "PrismaticAI", "key": "PrismaticAI hoard", "settings": "aggressiveness=100"},
            ],
            "standings": [
                {"key": "Winner", "value": 1000000.0},
                {"key": "PrismaticAI hoard", "value": 400000.0},
                {"key": "PrismaticAI", "value": 10000.0},
            ],
            "series": {
                "Winner": [q("1960-01", 0., 0., 0., 0., 0.), q("1961-01", 200000., 0., 50000., 50., 20.), q("1962-01", 1000000., 100000., 90000., 90., 40.)],
                "PrismaticAI": [q("1960-01", 0., 0., 0., 0., 0.), q("1961-01", 9000., 40000., 900., 4., 7.), q("1962-01", 10000., 45000., 800., 2., 7.)],
                "PrismaticAI hoard": [q("1960-01", 0., 0., 0., 0., 0.), q("1961-01", 150000., 100000., 30000., 20., 30.), q("1962-01", 400000., 380000., 9000., 8., 40.)],
            }
        })
    }

    fn causes(a: &Value) -> Vec<String> {
        a["losses"].as_array().unwrap().iter().map(|l| l["cause"].as_str().unwrap().to_string()).collect()
    }

    #[test]
    fn a_stalled_bot_is_last_and_stalls_small() {
        let a = contestant(&fixture(), "PrismaticAI").unwrap();
        assert_eq!(a["rank"], 3);
        assert_eq!(a["last"], true);
        assert_eq!(a["gap_opens"], "1961-01");
        let c = causes(&a);
        assert!(c.contains(&"stalls_small".to_string()), "{c:?}");
        assert!(c.contains(&"idle_stations".to_string()), "{c:?}");
    }

    #[test]
    fn a_hoarding_bot_hoards_cash_and_its_fleet_decays() {
        let a = contestant(&fixture(), "PrismaticAI hoard").unwrap();
        let c = causes(&a);
        assert!(c.contains(&"fleet_decays".to_string()), "{c:?}");
        assert!(c.contains(&"hoards_cash".to_string()), "{c:?}");
        assert!(!c.contains(&"stalls_small".to_string()), "{c:?}");
    }

    #[test]
    fn the_winner_has_no_gap() {
        let a = contestant(&fixture(), "Winner").unwrap();
        assert_eq!(a["rank"], 1);
        assert!(a["gap_opens"].is_null());
        assert!(causes(&a).is_empty(), "a winner with cash is not hoarding: {:?}", causes(&a));
    }

    #[test]
    fn an_unfinished_match_is_not_analysed() {
        let mut m = fixture();
        m["status"] = json!("running");
        assert!(contestant(&m, "PrismaticAI").is_none());
    }

    #[test]
    fn the_edition_reports_prismatic_last_and_proposes_remedies() {
        let mut other = fixture();
        other["id"] = json!("m-0002");
        let pm = edition("e1", &[other, fixture()], &lineup_is_prismatic);
        assert_eq!(pm["matches"], 2);
        assert_eq!(pm["prismatic_last"][0]["key"], "PrismaticAI");
        assert_eq!(pm["prismatic_last"][0]["last_place"], 2);
        let props = pm["proposals"].as_array().unwrap();
        let p = props.iter().find(|p| p["for"] == "PrismaticAI").unwrap();
        let s = p["settings"].as_str().unwrap();
        assert!(s.contains("max_routes=30") && s.contains("aggressiveness=100"), "{s}");
        let h = props.iter().find(|p| p["for"] == "PrismaticAI hoard").unwrap();
        assert!(h["settings"].as_str().unwrap().contains("self_steward=1"));
        // Deterministic: the same matches in another order give the same document.
        let mut a = fixture();
        a["id"] = json!("m-0002");
        assert_eq!(pm, edition("e1", &[fixture(), a], &lineup_is_prismatic));
        assert!(markdown(&pm).contains("PrismaticAI finished last"));
    }

    #[test]
    fn a_late_start_and_a_missing_mode_are_named() {
        let mut m = fixture();
        // The winner earns from air from the start; PrismaticAI starts late and never flies.
        for r in m["series"]["Winner"].as_array_mut().unwrap() {
            r["air"] = json!(5); r["pair"] = json!(80000); r["prail"] = json!(10000);
        }
        let s = m["series"]["PrismaticAI"].as_array_mut().unwrap();
        s[1]["vehicles"] = json!(0);
        s.push(q("1963-01", 10000., 45000., 800., 2., 7.));
        let a = contestant(&m, "PrismaticAI").unwrap();
        let c = causes(&a);
        assert!(c.contains(&"slow_start".to_string()), "{c:?}");
        let gap = a["losses"].as_array().unwrap().iter().find(|l| l["cause"] == "mode_gap").unwrap();
        assert_eq!(gap["mode"], "air");
        assert_eq!(a["first_vehicle_quarter"], 2);
        let pm = edition("e1", &[m], &lineup_is_prismatic);
        let pc = pm["contestants"].as_array().unwrap().iter().find(|c| c["key"] == "PrismaticAI").unwrap();
        assert!(pc["causes"].as_array().unwrap().iter().any(|c| c["cause"] == "mode_gap:air"));
    }

    #[test]
    fn a_replay_does_not_count() {
        let mut r = fixture();
        r["replica_of"] = json!("m-0001");
        r["id"] = json!("m-0009");
        assert_eq!(edition("e1", &[fixture(), r], &lineup_is_prismatic)["matches"], 1);
    }
}
