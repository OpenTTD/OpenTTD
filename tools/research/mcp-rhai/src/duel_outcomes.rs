//! RESEARCH-ONLY: the planned bot's `own_lines` and `rail_after` derived from stored duel outcomes
//! (issue I0087, ADR 0034 points 1, 2 and 6). Off by default: `PRISMATIC_BOT_FROM_DUELS=1` turns it on
//! for one run, and the constants of `panel_bot.rs` (OWN_LINES, rail_after from the plan's scores)
//! stay the default until held-start duel pairs on three seeds favour the derived rule beyond each
//! pair's rival spread (rule `project.a-tuning-claim-needs-held-start-pairs`).
//!
//! The history is every store's `plan.duel` verdicts (published by `tools/research/mcp-duel` since
//! I0083), each joined with the `plan.bot_settings` event of its own run (what the planned bot was
//! handed: freight lines, own_lines, rail_after). Duels recorded before I0083 exist only as files in
//! research/results; `mcp-panel --import-duels` imports them once into a store of their own
//! (`duel-import`), as I0082 imported the retired calibration file, so the decision reads one kind
//! from the stores and never a file.
use serde_json::{json, Value};
use std::collections::BTreeMap;

use crate::panel_events::{self, Stored};
use crate::panel_store::Across;

/// The switch: `1` derives own_lines and rail_after from the stored outcomes for this run.
pub const SWITCH: &str = "PRISMATIC_BOT_FROM_DUELS";

/// Shortest duel the rule learns from: own lines cost company value until about month 50 (company
/// value counts no track), so a 36-month duel shows their cost and not their return (knowledge
/// the-bots-rail-pathfinder-lays-plain-track-only).
pub const MIN_MONTHS: i64 = 60;

/// Pairs whose plan handed over within this many freight lines of this plan's count are "near".
pub const NEAR: i64 = 1;

/// The most own lines the rule asks for (the bot declares 0 to 32).
pub const OWN_LINES_MAX: i64 = 16;

/// The instance (and file stem) of the one import of the duels recorded before I0083.
pub const IMPORT_INSTANCE: &str = "duel-import";

/// The rule, as the event names it.
pub const RULE: &str = "own-lines-by-handed-v1: outcomes are plan.duel verdicts of held-start duels of at least 60 months against \
PrismaticAI, each with what its run handed over (freight lines h, own_lines, rail_after). Pairs: within one seed, length and h, each \
own_lines value against the smallest, gain = A/A_smallest - 1, read against the rival's spread (max B - min B) / min B of that group: \
pays when gain > spread, costs when gain < -spread, else inside. Near pairs have h within 1 of this plan's (else every pair is read). \
own_lines: some near pair pays and none costs: twice the largest value that paid, at most 16 (the best measured still paid); none pays: 0; \
both: the constant. rail_after: groups that differ only in rail_after, whose best arm beats every other beyond the spread, give its share \
rail_after/h; the median near share times this plan's h, else the plan's scores. No pair at all, the store off or the switch off: the \
constants.";

/// What the switch said, read once per bot start.
pub fn enabled() -> bool {
    std::env::var(SWITCH).map_or(false, |v| v.trim() == "1")
}

/// The statement every store answers: the plan.duel verdicts of at least MIN_MONTHS months and the
/// plan.bot_settings events of the same instances, in order, so each verdict can be joined with the
/// settings its run started the planned bot with.
pub fn outcomes_sql() -> String {
    format!("WITH d AS (SELECT DISTINCT instance FROM events WHERE kind = 'plan.duel' \
AND TRY_CAST(json_extract_string(data, '$.choice.months') AS INTEGER) >= {MIN_MONTHS}) \
SELECT instance, seq, ts_ms, kind, CAST(data AS VARCHAR) AS data FROM events \
WHERE instance IN (SELECT instance FROM d) AND kind IN ('plan.duel', 'plan.bot_settings') ORDER BY instance, ts_ms, seq")
}

fn int(v: &Value) -> Option<i64> {
    v.as_i64().or_else(|| v.as_f64().map(|f| f.round() as i64)).or_else(|| v.as_str().and_then(|s| s.trim().parse().ok()))
}

/// What a run handed the planned bot, from the `choice` of its plan.bot_settings: freight lines,
/// own_lines (absent: 0, the bot's own default) and rail_after (absent: null).
pub fn handed_of(choice: &Value) -> Value {
    let settings = &choice["settings"];
    json!({"handed": choice["freight"].as_array().map_or(0, Vec::len),
           "own_lines": int(&settings["own_lines"]).unwrap_or(0),
           "rail_after": int(&settings["rail_after"])})
}

/// The outcomes of an `outcomes_sql` answer (`{columns, rows}`): one per plan.duel verdict that
/// qualifies, `{instance, seq, seed, months, held_start_tick, a, b, handed, own_lines, rail_after,
/// plan_from}`, and how many verdicts were left out, by reason.
pub fn outcomes_of(columns: &Value, rows: &[Value]) -> (Vec<Value>, BTreeMap<String, usize>) {
    let cols: Vec<&str> = columns.as_array().into_iter().flatten().filter_map(Value::as_str).collect();
    let at = |row: &Value, name: &str| cols.iter().position(|c| *c == name).map_or(Value::Null, |i| row[i].clone());
    let mut skipped: BTreeMap<String, usize> = BTreeMap::new();
    let mut skip = |why: &str| *skipped.entry(why.to_string()).or_default() += 1;
    let mut last_settings: BTreeMap<String, Value> = BTreeMap::new();
    let mut out = Vec::new();
    for row in rows {
        let instance = at(row, "instance").as_str().unwrap_or("").to_string();
        let data = match at(row, "data") {
            Value::String(t) => serde_json::from_str::<Value>(&t).unwrap_or(Value::Null),
            other => other,
        };
        match at(row, "kind").as_str() {
            Some("plan.bot_settings") => {
                last_settings.insert(instance, data);
            }
            Some("plan.duel") => {
                let c = &data["choice"];
                let opponent = data.pointer("/version/params/opponent").and_then(Value::as_str).unwrap_or("PrismaticAI");
                let (Some(months), Some(a), Some(b)) = (int(&c["months"]), c["a"]["value"].as_f64(), c["b"]["value"].as_f64()) else {
                    skip("no months or no final values");
                    continue;
                };
                if months < MIN_MONTHS {
                    skip("shorter than the minimum");
                    continue;
                }
                if c["held_start_tick"].is_null() {
                    skip("not a held start");
                    continue;
                }
                if opponent != "PrismaticAI" {
                    skip("another opponent");
                    continue;
                }
                // An imported verdict carries what its run handed over; a live one is joined with the
                // plan.bot_settings its run published before it.
                let (plan, from) = if c["plan"].is_object() {
                    (c["plan"].clone(), "the verdict (imported)")
                } else if let Some(s) = last_settings.get(&instance) {
                    (handed_of(&s["choice"]), "plan.bot_settings of its run")
                } else {
                    skip("no plan.bot_settings in its run");
                    continue;
                };
                let Some(handed) = int(&plan["handed"]) else {
                    skip("no handed-over count");
                    continue;
                };
                out.push(json!({"instance": instance, "seq": at(row, "seq"), "seed": c["seed"], "months": months,
                                "held_start_tick": c["held_start_tick"], "a": a, "b": b, "handed": handed,
                                "own_lines": int(&plan["own_lines"]).unwrap_or(0), "rail_after": plan["rail_after"],
                                "record": data.pointer("/inputs/live/record").cloned().unwrap_or(Value::Null), "plan_from": from}));
            }
            _ => {}
        }
    }
    (out, skipped)
}

fn mean(v: &[f64]) -> f64 {
    v.iter().sum::<f64>() / v.len().max(1) as f64
}

fn round4(x: f64) -> f64 {
    (x * 10_000.0).round() / 10_000.0
}

/// (max - min) / min of the rival's final values, None with fewer than two or a minimum of 0.
fn spread(b: &[f64]) -> Option<f64> {
    let (lo, hi) = b.iter().fold((f64::MAX, f64::MIN), |(lo, hi), x| (lo.min(*x), hi.max(*x)));
    (b.len() >= 2 && lo > 0.0).then(|| round4((hi - lo) / lo))
}

fn verdict(gain: f64, spread: Option<f64>) -> &'static str {
    match spread {
        Some(s) if gain > s => "pays",
        Some(s) if gain < -s => "costs",
        Some(_) => "inside",
        None => "unmeasured",
    }
}

type Key = (String, i64, i64);

fn key(o: &Value) -> Key {
    (o["seed"].to_string(), o["months"].as_i64().unwrap_or(0), o["handed"].as_i64().unwrap_or(0))
}

/// Every own_lines comparison the outcomes make: within one seed, length and handed-over count,
/// each own_lines value against the smallest, with the group's rival spread and the verdict.
pub fn own_pairs(outcomes: &[Value]) -> Vec<Value> {
    let mut groups: BTreeMap<Key, Vec<&Value>> = BTreeMap::new();
    for o in outcomes {
        groups.entry(key(o)).or_default().push(o);
    }
    let mut out = Vec::new();
    for ((seed, months, handed), arms) in groups {
        let mut by_own: BTreeMap<i64, Vec<f64>> = BTreeMap::new();
        for o in &arms {
            by_own.entry(o["own_lines"].as_i64().unwrap_or(0)).or_default().push(o["a"].as_f64().unwrap_or(0.0));
        }
        let Some((&base, base_a)) = by_own.iter().next() else { continue };
        let base_a = mean(base_a);
        let s = spread(&arms.iter().filter_map(|o| o["b"].as_f64()).collect::<Vec<_>>());
        for (&own, a) in by_own.iter().skip(1) {
            if base_a <= 0.0 { continue; }
            let gain = round4(mean(a) / base_a - 1.0);
            out.push(json!({"seed": serde_json::from_str::<Value>(&seed).unwrap_or(Value::Null), "months": months, "handed": handed,
                            "base_own": base, "own": own, "a_base": base_a, "a": mean(a), "gain": gain, "spread": s,
                            "verdict": verdict(gain, s),
                            "records": arms.iter().map(|o| o["record"].clone()).collect::<Vec<_>>()}));
        }
    }
    out
}

/// Every rail_after reading: within one seed, length, handed-over count and own_lines, groups whose
/// arms differ in rail_after; the best arm counts when it beats every other beyond the group's
/// rival spread, as its share rail_after / handed.
pub fn rail_pairs(outcomes: &[Value]) -> Vec<Value> {
    let mut groups: BTreeMap<(Key, i64), Vec<&Value>> = BTreeMap::new();
    for o in outcomes.iter().filter(|o| o["rail_after"].as_i64().is_some()) {
        groups.entry((key(o), o["own_lines"].as_i64().unwrap_or(0))).or_default().push(o);
    }
    let mut out = Vec::new();
    for (((seed, months, handed), own), arms) in groups {
        let mut by_after: BTreeMap<i64, Vec<f64>> = BTreeMap::new();
        for o in &arms {
            by_after.entry(o["rail_after"].as_i64().unwrap_or(0)).or_default().push(o["a"].as_f64().unwrap_or(0.0));
        }
        if by_after.len() < 2 || handed <= 0 { continue; }
        let means: Vec<(i64, f64)> = by_after.iter().map(|(k, v)| (*k, mean(v))).collect();
        let (best, best_a) = means.iter().copied().fold((0, f64::MIN), |m, x| if x.1 > m.1 { x } else { m });
        let s = spread(&arms.iter().filter_map(|o| o["b"].as_f64()).collect::<Vec<_>>());
        let beats = s.is_some_and(|s| means.iter().filter(|(k, _)| *k != best).all(|(_, a)| *a > 0.0 && best_a / a - 1.0 > s));
        out.push(json!({"seed": serde_json::from_str::<Value>(&seed).unwrap_or(Value::Null), "months": months, "handed": handed,
                        "own_lines": own, "arms": means.iter().map(|(k, a)| json!({"rail_after": k, "a": a})).collect::<Vec<_>>(),
                        "best": best, "share": round4(best as f64 / handed as f64), "spread": s, "counts": beats}));
    }
    out
}

fn near<'a>(pairs: &'a [Value], handed: i64) -> (Vec<&'a Value>, bool) {
    let near: Vec<&Value> = pairs.iter().filter(|p| (p["handed"].as_i64().unwrap_or(i64::MIN) - handed).abs() <= NEAR).collect();
    if near.is_empty() { (pairs.iter().collect(), false) } else { (near, true) }
}

/// What the outcomes give for a plan that handed over `handed` freight lines: own_lines and
/// rail_after (None keeps the constant or the plan's scores), each with why.
pub fn derive(outcomes: &[Value], handed: i64, constant_own: i64, scores_rail_after: Option<i64>) -> Value {
    let own = own_pairs(outcomes);
    let rail = rail_pairs(outcomes);
    let (read, is_near) = near(&own, handed);
    let paid: Vec<i64> = read.iter().filter(|p| p["verdict"] == "pays").filter_map(|p| p["own"].as_i64()).collect();
    let cost = read.iter().filter(|p| p["verdict"] == "costs").count();
    let scope = if is_near { format!("{} pair(s) with h within {NEAR} of {handed}", read.len()) }
                else { format!("no pair with h within {NEAR} of {handed}; all {} pair(s)", read.len()) };
    let (own_lines, own_why) = if own.is_empty() {
        (None, format!("no pair differing in own_lines among {} outcome(s): the constant {constant_own}", outcomes.len()))
    } else if !paid.is_empty() && cost == 0 {
        let top = *paid.iter().max().unwrap_or(&constant_own);
        let v = (2 * top).clamp(0, OWN_LINES_MAX);
        (Some(v), format!("{scope}: {} pay beyond the spread, none costs; twice the largest that paid ({top}), at most {OWN_LINES_MAX}", paid.len()))
    } else if paid.is_empty() {
        (Some(0), format!("{scope}: none pays beyond the rival's spread ({cost} cost(s)); no own lines"))
    } else {
        (None, format!("{scope}: {} pay and {cost} cost; the constant {constant_own}", paid.len()))
    };
    let counted: Vec<&Value> = rail.iter().filter(|r| r["counts"] == true).collect();
    let near_rail: Vec<&Value> = counted.iter().copied().filter(|r| (r["handed"].as_i64().unwrap_or(i64::MIN) - handed).abs() <= NEAR).collect();
    let (rail_after, rail_why) = match (scores_rail_after, near_rail.is_empty()) {
        (None, _) => (None, "no passenger rail pair in this plan".to_string()),
        (Some(_), true) => (None, format!("{} rail_after group(s), {} beyond the spread, none near h {handed}: the plan's scores",
                                          rail.len(), counted.len())),
        (Some(_), false) => {
            let mut shares: Vec<f64> = near_rail.iter().filter_map(|r| r["share"].as_f64()).collect();
            shares.sort_by(f64::total_cmp);
            let m = shares.len();
            let median = if m % 2 == 1 { shares[m / 2] } else { (shares[m / 2 - 1] + shares[m / 2]) / 2.0 };
            let v = ((median * handed as f64).round() as i64).clamp(0, handed);
            (Some(v), format!("median share {median} of {m} near group(s) beyond the spread, times h {handed}"))
        }
    };
    json!({"handed": handed, "outcomes": outcomes.len(), "own_pairs": own, "rail_pairs": rail, "near": is_near,
           "own_lines": {"derived": own_lines, "constant": constant_own, "why": own_why},
           "rail_after": {"derived": rail_after, "from_scores": scores_rail_after, "why": rail_why}})
}

/// The decision of one bot start: what it applies, what plan.bot_settings names as its stored input,
/// and the record it carries in `inputs.live.from_duels`.
pub struct Derivation {
    pub own_lines: Option<i64>,
    pub rail_after: Option<i64>,
    pub stored: Stored,
    pub record: Value,
}

impl Derivation {
    /// The constants, with the reason no history was used (ADR 0034 point 2: never silent).
    pub fn constants(why: String, extra: Value) -> Derivation {
        let mut record = json!({"switch": SWITCH, "used": false, "fallback": "constants", "why": why, "rule": RULE});
        if let (Some(r), Some(x)) = (record.as_object_mut(), extra.as_object()) { r.extend(x.clone()); }
        Derivation { own_lines: None, rail_after: None, stored: Stored::Fallback(format!("constants: {why}")), record }
    }
}

/// Why the constants are used when nobody turned the switch on.
pub const OFF: &str = "PRISMATIC_BOT_FROM_DUELS is off (the default until held-start pairs on three seeds favour the derived rule; ADR 0034 point 6, I0087)";

/// own_lines and rail_after for a plan that handed over `handed` freight lines, read with `read`
/// (every store, `panel_store::query_every_store`) when `on`; the constants with their reason when
/// the switch is off, nothing was handed over, the store is off or the outcomes make no pair.
pub fn from_duels(on: bool, handed: usize, constant_own: i64, scores_rail_after: Option<i64>,
                  read: impl FnOnce(&str) -> Result<Across, String>) -> Derivation {
    if !on {
        return Derivation::constants(OFF.into(), json!({}));
    }
    if handed == 0 {
        return Derivation::constants("no freight line was handed over, so own_lines is not set".into(), json!({}));
    }
    let sql = outcomes_sql();
    let across = match read(&sql) {
        Ok(a) => a,
        Err(why) => return Derivation::constants(why, json!({"query": sql})),
    };
    // A hub's store holds no duel (and answers an error while the hub runs); it is not history.
    let stores: Vec<Value> = across.stores.iter().filter(|s| !s["store"].as_str().unwrap_or("").starts_with("hub-")).cloned().collect();
    let (outcomes, skipped) = outcomes_of(&across.columns, &across.rows);
    let d = derive(&outcomes, handed as i64, constant_own, scores_rail_after);
    let base = json!({"query": sql, "rows": across.rows.len(), "stores": stores, "skipped": skipped, "outcomes": outcomes});
    if d["own_pairs"].as_array().map_or(true, Vec::is_empty) && d["rail_pairs"].as_array().map_or(true, Vec::is_empty) {
        let why = format!("the stores hold {} qualifying outcome(s) in {} row(s), which make no pair", outcomes.len(), across.rows.len());
        let mut c = Derivation::constants(why, base);
        c.record["derived"] = d;
        return c;
    }
    let mut record = json!({"switch": SWITCH, "used": true, "rule": RULE, "derived": d});
    if let (Some(r), Some(x)) = (record.as_object_mut(), base.as_object()) { r.extend(x.clone()); }
    Derivation { own_lines: d["own_lines"]["derived"].as_i64(), rail_after: d["rail_after"]["derived"].as_i64(),
                 stored: Stored::Query { sql, rows: across.rows.len() }, record }
}

// ---------------------------------------------------------------------- the one import

/// "game unpaused at tick N" of a record's plan sentences (mcp-duel's `was_held` reads the same).
fn unpaused_tick(plan: &[Value]) -> Option<i64> {
    plan.iter().filter_map(Value::as_str).find_map(|p| {
        let rest = p.split("game unpaused at tick ").nth(1)?;
        rest.split(|c: char| !c.is_ascii_digit()).next()?.parse().ok()
    })
}

/// The number after `before` and before `after` in the first sentence holding both.
fn number_between(plan: &[Value], before: &str, after: &str) -> Option<i64> {
    plan.iter().filter_map(Value::as_str).find_map(|p| {
        let rest = p.split(before).nth(1)?;
        let (n, _) = rest.split_once(after)?;
        n.trim().parse().ok()
    })
}

/// What a recorded duel handed its planned bot: from its plan.bot_settings event when the record
/// holds one (I0081 on), else from its planner sentences (the record has nothing structured): freight
/// lines are the "<cargo> by rail A -> B, N tiles" sentences, own_lines the run's own setting or the
/// sentence "may plan up to N more itself", rail_after "is built after N freight line(s)".
pub fn record_plan(rec: &Value) -> Value {
    if let Some(e) = rec["plan_events"].as_array().into_iter().flatten().find(|e| e["kind"] == "plan.bot_settings") {
        let mut p = handed_of(&e["data"]["choice"]);
        p["from"] = json!("plan_events: plan.bot_settings");
        return p;
    }
    let plan: Vec<Value> = rec["plan"].as_array().cloned().unwrap_or_default();
    let freight = plan.iter().filter_map(Value::as_str)
        .filter(|p| p.starts_with("planner: ") && p.contains(" by rail ") && p.contains(" -> ") && p.contains(" tiles")).count();
    let from_run = rec["bot_settings"].as_str().unwrap_or("").split(',')
        .find_map(|kv| kv.trim().strip_prefix("own_lines=").and_then(|v| v.trim().parse::<i64>().ok()));
    let own = from_run.or_else(|| number_between(&plan, "may plan up to ", " more itself")).unwrap_or(0);
    json!({"handed": freight, "own_lines": own, "rail_after": number_between(&plan, "is built after ", " freight line"),
           "from": "planner sentences of the record (it holds no plan.bot_settings)"})
}

/// "duel-20261118T011802-seed4242" as milliseconds (the time in the name is when the duel ended).
fn name_ms(name: &str) -> Option<i64> {
    let t = name.strip_prefix("duel-")?.split('-').next()?;
    if t.len() != 15 { return None; }
    crate::panel_store::utc_ms(&format!("{}-{}-{}T{}:{}:{}Z", &t[0..4], &t[4..6], &t[6..8], &t[9..11], &t[11..13], &t[13..15]))
}

/// The plan.duel event of a duel recorded before I0083, or why it is not imported: a record whose
/// verdict is already in a store (I0083 on) or that has no final values is left out.
pub fn import_event(name: &str, rec: &Value) -> Result<Value, String> {
    if rec["verdict_event"]["stored"] == true {
        return Err("its verdict is in a store already".into());
    }
    let (a, b) = (&rec["final"]["1"], &rec["final"]["2"]);
    if !a["value"].is_number() || !b["value"].is_number() {
        return Err("no final values".into());
    }
    let ts = name_ms(name).ok_or("not a duel-<time>-seed<N> name")?;
    let plan: Vec<Value> = rec["plan"].as_array().cloned().unwrap_or_default();
    let held = rec["held_start_tick"].as_i64().or_else(|| unpaused_tick(&plan));
    let side = |c: &Value, n: i64| json!({"company": n, "name": c["name"], "value": c["value"], "money": c["money"], "loan": c["loan"],
                                          "trains": c["trains"]});
    let params = json!({"months": rec["months"], "opponent": rec["opponent"].as_str().unwrap_or("PrismaticAI"),
                        "bot_settings": rec["bot_settings"].as_str().unwrap_or(""), "start_paused": rec["start_paused"]});
    let data = panel_events::plan("duel.import", params,
        json!({"record": format!("research/results/{name}.json"), "imported_from": "research/results"}),
        &Stored::Fallback("imported once from research/results (I0087): the duel ran before mcp-duel published plan.duel (I0083)".into()),
        json!({"seed": rec["seed"], "months": rec["months"], "held_start_tick": held, "a": side(a, 1), "b": side(b, 2),
               "rival_spread": rec["rival_spread"], "winner": rec["winner"], "plan": record_plan(rec)}));
    Ok(json!({"seq": 0, "ts_ms": ts, "kind": "plan.duel", "data": data}))
}

/// The duels of `dir` (duel-*.json) as plan.duel events in time order, with seq 1.., and per record
/// left out why.
pub fn import_events(dir: &std::path::Path) -> (Vec<Value>, Vec<Value>) {
    let mut files: Vec<std::path::PathBuf> = std::fs::read_dir(dir).map(|d| d.filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.extension().and_then(|x| x.to_str()) == Some("json")
                    && p.file_name().and_then(|n| n.to_str()).is_some_and(|n| n.starts_with("duel-"))).collect()).unwrap_or_default();
    files.sort();
    let (mut events, mut left) = (Vec::new(), Vec::new());
    for f in files {
        let name = f.file_stem().and_then(|s| s.to_str()).unwrap_or("").to_string();
        let rec = std::fs::read_to_string(&f).map_err(|e| e.to_string())
            .and_then(|t| serde_json::from_str::<Value>(&t).map_err(|e| e.to_string()));
        match rec.and_then(|r| import_event(&name, &r)) {
            Ok(mut e) => { e["seq"] = json!(events.len() + 1); events.push(e); }
            Err(why) => left.push(json!({"record": name, "why": why})),
        }
    }
    events.sort_by_key(|e| e["ts_ms"].as_i64().unwrap_or(0));
    for (i, e) in events.iter_mut().enumerate() { e["seq"] = json!(i + 1); }
    (events, left)
}

/// Import the duels of `results` once into `dir/duel-import.duckdb`: a run of its own (no seed, no
/// ports) and one plan.duel per record. A record already there (same `inputs.live.record`) is
/// skipped, so a second import changes nothing.
#[cfg(feature = "store")]
pub fn import_duels(dir: &std::path::Path, results: &std::path::Path) -> Result<Value, String> {
    let (events, left) = import_events(results);
    let path = dir.join(format!("{IMPORT_INSTANCE}.duckdb"));
    let mut st = crate::bus_sinks::db::Store::open(&path, IMPORT_INSTANCE)?;
    let have = st.query("SELECT json_extract_string(data, '$.inputs.live.record') FROM events WHERE kind = 'plan.duel'")?;
    let seen: Vec<Value> = have["rows"].as_array().into_iter().flatten().map(|r| r[0].clone()).collect();
    let next = seen.len();
    let fresh: Vec<Value> = events.into_iter().filter(|e| !seen.contains(&e["data"]["inputs"]["live"]["record"])).enumerate()
        .map(|(i, e)| crate::bus_sinks::envelope(&json!({"seq": next + i + 1, "kind": "plan.duel", "data": e["data"]}), IMPORT_INSTANCE,
                                                 &json!("import"), e["data"]["choice"]["seed"].as_i64(), e["ts_ms"].as_i64().unwrap_or(0), None))
        .collect();
    if let Some(first) = fresh.first() {
        st.insert_run(&json!("import"), None, first["ts_ms"].as_i64().unwrap_or(0), (None, None, None))?;
        st.insert_events(&fresh)?;
    }
    Ok(json!({"store": path.display().to_string(), "imported": fresh.len(), "already": seen.len(), "left_out": left}))
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A plan.duel row of `outcomes_sql` as a store answers it.
    fn duel(instance: &str, seq: i64, seed: i64, months: i64, a: f64, b: f64, plan: Option<Value>) -> Value {
        let mut choice = json!({"seed": seed, "months": months, "held_start_tick": 1400, "a": {"value": a}, "b": {"value": b}});
        if let Some(p) = plan { choice["plan"] = p; }
        let data = json!({"decision": "duel.verdict", "version": {"params": {"opponent": "PrismaticAI"}},
                          "inputs": {"live": {"record": format!("research/results/{instance}-{seq}.json")}}, "choice": choice});
        json!([instance, seq, seq * 10, "plan.duel", data.to_string()])
    }

    fn handed(h: i64, own: i64, after: Option<i64>) -> Option<Value> {
        Some(json!({"handed": h, "own_lines": own, "rail_after": after}))
    }

    fn settings(instance: &str, seq: i64, freight: usize, own: Option<i64>, after: i64) -> Value {
        let mut s = json!({"rail_a": 1, "rail_after": after});
        if let Some(o) = own { s["own_lines"] = json!(o); }
        let data = json!({"decision": "bot.settings", "choice": {"settings": s, "freight": vec![json!({}); freight]}});
        json!([instance, seq, seq * 10, "plan.bot_settings", data.to_string()])
    }

    fn cols() -> Value {
        json!(["instance", "seq", "ts_ms", "kind", "data"])
    }

    fn across(rows: Vec<Value>) -> Across {
        Across { columns: cols(), stores: vec![json!({"store": "duel-import.duckdb", "via": "file (read-only)", "rows": rows.len()}),
                                               json!({"store": "hub-1.duckdb", "via": "", "error": "locked"})], rows }
    }

    /// The three held-start ten-year pairs of 2026-11-17/18 (own_lines 0 against 8), as imported.
    fn measured() -> Vec<Value> {
        vec![duel("duel-import", 1, 31337, 120, 4479471.0, 79076.0, handed(13, 0, Some(13))),
             duel("duel-import", 2, 31337, 120, 4433637.0, 61159.0, handed(13, 8, Some(13))),
             duel("duel-import", 3, 4242, 120, 2770405.0, 91031.0, handed(10, 0, None)),
             duel("duel-import", 4, 4242, 120, 4022718.0, 90622.0, handed(10, 8, None)),
             duel("duel-import", 5, 20208, 120, 2824858.0, 22348.0, handed(9, 0, None)),
             duel("duel-import", 6, 20208, 120, 3690922.0, 14325.0, handed(9, 8, None))]
    }

    #[test]
    fn own_lines_are_derived_from_the_measured_pairs_by_the_handed_over_count() {
        let rows = measured();
        let (o, skipped) = outcomes_of(&cols(), &rows);
        assert_eq!(o.len(), 6);
        assert!(skipped.is_empty());
        let pairs = own_pairs(&o);
        let v: Vec<(i64, &str)> = pairs.iter().map(|p| (p["handed"].as_i64().unwrap(), p["verdict"].as_str().unwrap())).collect();
        assert_eq!(v, vec![(9, "inside"), (13, "inside"), (10, "pays")], "by seed 20208, 31337, 4242: 20208 +30.7% inside 56%, 4242 +45.2% against 0.45%");
        assert_eq!((pairs[1]["gain"].clone(), pairs[1]["spread"].clone()), (json!(-0.0102), json!(0.293)), "31337: -1.0% inside 29.3%");
        // 31337 hands over 13: only its own pair is near, and it does not pay.
        let d = derive(&o, 13, 8, Some(13));
        assert_eq!(d["own_lines"]["derived"], 0);
        assert_eq!(d["near"], true);
        // 4242 (10) and 20208 (9): the near pairs are 4242's (pays) and 20208's (inside); none costs.
        assert_eq!(derive(&o, 10, 8, None)["own_lines"]["derived"], 16, "twice the 8 that paid");
        assert_eq!(derive(&o, 9, 8, None)["own_lines"]["derived"], 16);
        // 8 lines: only 20208's pair is near, inside its spread.
        assert_eq!(derive(&o, 8, 8, None)["own_lines"]["derived"], 0);
        // Far from every pair: all pairs are read.
        let far = derive(&o, 3, 8, None);
        assert_eq!((far["near"].clone(), far["own_lines"]["derived"].clone()), (json!(false), json!(16)));
        // No rail_after group differs: the plan's scores stay.
        assert_eq!(derive(&o, 13, 8, Some(13))["rail_after"]["derived"], Value::Null);
    }

    #[test]
    fn rail_after_is_derived_from_arms_that_differ_in_it_only() {
        // Seed 7, 120 months, 10 lines handed over, own 8: building the passenger pair after 2 lines
        // beat after 8 by 20% against a 1% rival spread.
        let rows = vec![duel("p", 1, 7, 120, 1_200_000.0, 50_000.0, handed(10, 8, Some(2))),
                        duel("p", 2, 7, 120, 1_000_000.0, 50_500.0, handed(10, 8, Some(8)))];
        let (o, _) = outcomes_of(&cols(), &rows);
        let r = rail_pairs(&o);
        assert_eq!(r.len(), 1);
        assert_eq!((r[0]["best"].clone(), r[0]["share"].clone(), r[0]["counts"].clone()), (json!(2), json!(0.2), json!(true)));
        let d = derive(&o, 11, 8, Some(9));
        assert_eq!(d["rail_after"]["derived"], 2, "share 0.2 of 11 lines, rounded");
        assert_eq!(d["own_lines"]["derived"], Value::Null, "nothing differs in own_lines: the constant");
        assert_eq!(derive(&o, 11, 8, None)["rail_after"]["derived"], Value::Null, "no passenger pair, no rail_after");
        // Inside the spread the arm does not count.
        let rows = vec![duel("p", 1, 7, 120, 1_005_000.0, 50_000.0, handed(10, 8, Some(2))),
                        duel("p", 2, 7, 120, 1_000_000.0, 60_000.0, handed(10, 8, Some(8)))];
        let (o, _) = outcomes_of(&cols(), &rows);
        assert_eq!(rail_pairs(&o)[0]["counts"], false);
        assert_eq!(derive(&o, 10, 8, Some(9))["rail_after"]["derived"], Value::Null);
    }

    #[test]
    fn a_live_verdict_is_joined_with_the_settings_of_its_own_run_and_short_or_unheld_duels_are_left_out() {
        let mut short = duel("panel-2", 9, 3, 6, 1.0, 9601.0, None);
        short[0] = json!("panel-2");
        let mut unheld: Value = duel("panel-4", 3, 3, 120, 5.0, 5.0, handed(4, 0, None));
        let mut data: Value = serde_json::from_str(unheld[4].as_str().unwrap()).unwrap();
        data["choice"]["held_start_tick"] = Value::Null;
        unheld[4] = json!(data.to_string());
        let rows = vec![settings("panel-1", 3, 12, Some(8), 12), settings("panel-1", 5, 13, None, 13), duel("panel-1", 9, 3, 120, 7.0, 1.0, None),
                        settings("panel-2", 1, 13, Some(8), 13), short, duel("panel-3", 2, 3, 120, 1.0, 1.0, None), unheld];
        let (o, skipped) = outcomes_of(&cols(), &rows);
        assert_eq!(o.len(), 1);
        assert_eq!((o[0]["handed"].clone(), o[0]["own_lines"].clone(), o[0]["rail_after"].clone()), (json!(13), json!(0), json!(13)),
                   "the newest settings of its run; own_lines absent is the bot's 0");
        assert_eq!(o[0]["plan_from"], "plan.bot_settings of its run");
        assert_eq!(skipped, BTreeMap::from([("no plan.bot_settings in its run".to_string(), 1), ("not a held start".to_string(), 1),
                                            ("shorter than the minimum".to_string(), 1)]));
    }

    #[test]
    fn the_constants_stay_with_their_reason_when_off_unread_or_without_pairs() {
        let off = from_duels(false, 13, 8, Some(13), |_| panic!("off reads no store"));
        assert_eq!((off.own_lines, off.rail_after), (None, None));
        assert_eq!(off.stored.to_json(), json!({"fallback": format!("constants: {OFF}")}));
        assert_eq!(off.record["used"], false);
        let none = from_duels(true, 0, 8, None, |_| panic!("nothing handed over reads no store"));
        assert!(none.record["why"].as_str().unwrap().starts_with("no freight line"));
        let store_off = from_duels(true, 13, 8, Some(13), |_| Err("store off: built without the store feature".into()));
        assert_eq!(store_off.stored.to_json(), json!({"fallback": "constants: store off: built without the store feature"}));
        assert_eq!(store_off.record["query"], json!(outcomes_sql()));
        // One duel makes no pair.
        let one = from_duels(true, 13, 8, Some(13), |_| Ok(across(vec![measured()[0].clone()])));
        assert_eq!((one.own_lines, one.rail_after), (None, None));
        assert!(one.stored.to_json()["fallback"].as_str().unwrap().contains("1 qualifying outcome(s) in 1 row(s), which make no pair"));
        assert_eq!(one.record["outcomes"].as_array().unwrap().len(), 1);
        // The empty store.
        let empty = from_duels(true, 13, 8, Some(13), |_| Ok(across(vec![])));
        assert!(empty.stored.to_json()["fallback"].as_str().unwrap().contains("0 qualifying"));
    }

    #[test]
    fn the_derivation_names_its_query_rows_and_the_outcomes_it_used() {
        let mut asked = String::new();
        let d = from_duels(true, 10, 8, None, |sql| { asked = sql.to_string(); Ok(across(measured())) });
        assert_eq!(asked, outcomes_sql());
        assert_eq!((d.own_lines, d.rail_after), (Some(16), None));
        assert_eq!(d.stored.to_json(), json!({"query": asked, "rows": 6}));
        assert_eq!(d.record["used"], true);
        assert_eq!(d.record["rule"], RULE);
        assert_eq!(d.record["outcomes"].as_array().unwrap().len(), 6);
        assert_eq!(d.record["stores"], json!([{"store": "duel-import.duckdb", "via": "file (read-only)", "rows": 6}]), "a hub store is skipped");
        assert!(d.record["derived"]["own_lines"]["why"].as_str().unwrap().contains("2 pair(s) with h within 1 of 10"));
    }

    #[test]
    fn a_record_before_i0083_becomes_one_plan_duel_with_what_its_run_handed_over() {
        let rec = json!({"seed": 4242, "months": 120, "opponent": "PrismaticAI", "bot_settings": "own_lines=0", "start_paused": null,
            "held_start_tick": null, "final": {"1": {"value": 2770405, "trains": 11}, "2": {"value": 91031}}, "winner": "A (planned)",
            "plan": ["planner: game unpaused at tick 1400 with 2 AI company(ies) after 90 s",
                     "planner: after these lines the bot may plan up to 8 more itself (own_lines)",
                     "planner: Coal by rail Mine -> Plant, 212.0 tiles, profit 1/month, score 2",
                     "planner: Wood by rail Forest -> Sawmill, 177.0 tiles, profit 1/month, score 1 (line 2, built when the money allows)",
                     "planner: the passenger rail pair (score 3) is built after 1 freight line(s) that score more"]});
        let e = import_event("duel-20261118T011804-seed4242", &rec).unwrap();
        assert_eq!(e["ts_ms"], crate::panel_store::utc_ms("2026-11-18T01:18:04Z").unwrap());
        let c = &e["data"]["choice"];
        assert_eq!(c["held_start_tick"], 1400, "from the unpause sentence");
        assert_eq!(c["plan"], json!({"handed": 2, "own_lines": 0, "rail_after": 1, "from": "planner sentences of the record (it holds no plan.bot_settings)"}),
                   "the run's own_lines=0 over the sentence's 8");
        assert_eq!(e["data"]["version"]["params"]["opponent"], "PrismaticAI");
        assert_eq!(e["data"]["inputs"]["live"]["record"], "research/results/duel-20261118T011804-seed4242.json");
        let mut stored = rec.clone();
        stored["verdict_event"] = json!({"stored": true});
        assert!(import_event("duel-20261118T011804-seed4242", &stored).unwrap_err().contains("in a store already"));
        let with_events = json!({"plan_events": [{"kind": "plan.bot_settings", "data": {"choice": {"freight": [{}, {}, {}],
            "settings": {"own_lines": 8, "rail_after": 3}}}}]});
        assert_eq!(record_plan(&with_events), json!({"handed": 3, "own_lines": 8, "rail_after": 3, "from": "plan_events: plan.bot_settings"}));
    }

    #[cfg(feature = "store")]
    #[test]
    fn the_import_is_written_once_and_read_back_by_the_outcomes_query() {
        let base = std::env::temp_dir().join(format!("i0087-{}-{}", std::process::id(), crate::bus_sinks::now_ms()));
        let (results, dir) = (base.join("results"), base.join("store"));
        std::fs::create_dir_all(&results).unwrap();
        std::fs::create_dir_all(&dir).unwrap();
        let rec = |own: i64, a: i64, b: i64| json!({"seed": 4242, "months": 120, "opponent": "PrismaticAI", "bot_settings": format!("own_lines={own}"),
            "held_start_tick": 1400, "final": {"1": {"value": a}, "2": {"value": b}},
            "plan": ["planner: Coal by rail Mine -> Plant, 212.0 tiles, profit 1/month, score 2"]});
        std::fs::write(results.join("duel-20261118T011804-seed4242.json"), rec(0, 2770405, 91031).to_string()).unwrap();
        std::fs::write(results.join("duel-20261118T011802-seed4242.json"), rec(8, 4022718, 90622).to_string()).unwrap();
        std::fs::write(results.join("duel-20261118T011900-seed4242.json"), json!({"final": {}}).to_string()).unwrap();
        let first = import_duels(&dir, &results).unwrap();
        assert_eq!((first["imported"].clone(), first["already"].clone()), (json!(2), json!(0)));
        assert_eq!(first["left_out"][0]["why"], "no final values");
        let again = import_duels(&dir, &results).unwrap();
        assert_eq!((again["imported"].clone(), again["already"].clone()), (json!(0), json!(2)), "once");
        let st = crate::bus_sinks::db::Store::open_read_only(&dir.join(format!("{IMPORT_INSTANCE}.duckdb"))).unwrap();
        let a = st.query(&outcomes_sql()).unwrap();
        let (o, _) = outcomes_of(&a["columns"], a["rows"].as_array().unwrap());
        assert_eq!(o.iter().map(|x| x["own_lines"].as_i64().unwrap()).collect::<Vec<_>>(), vec![8, 0], "in time order: 011802 ended first");
        assert_eq!(own_pairs(&o)[0]["verdict"], "pays");
        drop(st);
        let _ = std::fs::remove_dir_all(&base);
    }
}
