//! RESEARCH-ONLY: Prismatic decides the bot over the bridge's decision contract (I0011).
//!
//! The panel speaks both halves of research/prismatic-bridge/decision-contract.schema.json, so a
//! game run by `mcp-e2e` takes Prismatic's decisions without the Python bridge:
//!
//! - `GET /api/bridge/companies` answers what the bridge's `GET /companies` answers (its
//!   `CompanyOut`), from openttd://companies and openttd://vehicles. Prismatic reads the company it
//!   decides for there: point its `OPENTTD_BRIDGE_URL` at `<panel>/api/bridge`.
//! - `POST /api/decision/pull` GETs a decision directive from `PRISMATIC_DECISION_URL` (or the
//!   body's `url`), checks it against the contract and applies it, answering a DecisionPullResult.
//!
//! Applying differs from the bridge in one place, on purpose. The bridge answers a directive with a
//! `company_id` by `reload_ai`, which restarts the company's AI with the settings it already had,
//! so the settings Prismatic decided are dropped. The panel sets them on the running AI instead
//! (`openttd.ai.set_setting`, approval-gated like every change to the game), and reloads only when
//! the directive carries no settings. Without a `company_id` it starts `ai` with the settings, as the
//! bridge does.
//!
//! For Prismatic to fit its decision from recorded history instead of a snapshot and a hand model
//! (ADR 0034 point 4), the panel speaks two more parts of the contract (I0085), each version-gated
//! by its own `schema`; the fit itself is Prismatic's code:
//!
//! - every CompanyOut of `GET /api/bridge/companies` carries `history` (prismatic-history/v1): the
//!   panel's bus endpoint and the read-only store query of that company's samples (`samples_sql`), to
//!   run on the panel's `/api/db/query`;
//! - a directive's working may carry `fit` (prismatic-fit/v1): the model, its parameters, the samples
//!   it was fitted from (that query, or the bus, with the row count) and its quality, or the fallback
//!   Prismatic took. The panel checks it (`fit_stored`) and records it as plan.directive's
//!   `inputs.stored`: the query and rows with the fit, or a fallback that says why there is none (no
//!   fit sent, Prismatic fell back, or the fit breaks the contract). A fit never changes what is
//!   applied: the settings are the decision, the fit is how it was reached.
//!
//! A directive is bound to what Prismatic observed (research/agent-loop.md, step 1). Each answer of
//! `GET /api/bridge/companies` is recorded as an observation: the game tick and date it was read at
//! and a digest of the companies' identities (id, AI flag, name; money moves every tick, so it is
//! left out). A pull applies a directive only when Prismatic observed this game during that pull,
//! the companies are still the ones it observed, the observation is at most a month of game time
//! old, the company it names is an AI company, and a directive that echoes `observed_digest` names
//! that very observation. Anything else is refused before a step is taken (409), and recorded so.

use serde_json::{json, Value};
use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, OnceLock};
use std::time::Duration;

use crate::bus_sinks::{read_only_sql, SCHEMA as BUS_SCHEMA};
use crate::panel::Game;
use crate::panel_bot;
use crate::panel_events;

/// A directive the contract allows: at least one of `ai` and `company_id`, and the settings it carries.
#[derive(Debug, PartialEq)]
pub struct Directive {
    pub ai: Option<String>,
    pub company_id: Option<i64>,
    pub settings: Vec<(String, i64)>,
}

/// The fields of a directive the panel reads: the contract's three, the working's `fit` (I0085) and
/// the `observed_digest` it may echo (research/agent-loop.md step 1).
pub const READ: &[&str] = &["ai", "company_id", "settings", "fit", "observed_digest"];

/// The directive in `v`, or every reason the contract refuses it. Fields beyond the contract's
/// three are the directive's own working, which the contract admits; the panel reads only its `fit`
/// (`fit_stored`), and `ignored` names the rest (the bridge ignores all of them).
pub fn directive(v: &Value) -> Result<(Directive, Vec<String>), Vec<String>> {
    let mut errors = Vec::new();
    let Some(o) = v.as_object() else { return Err(vec!["a directive is a JSON object".into()]) };
    let ai = match o.get("ai") {
        None | Some(Value::Null) => None,
        Some(Value::String(s)) if !s.is_empty() => Some(s.clone()),
        Some(other) => { errors.push(format!("ai must be a non-empty string, not {other}")); None }
    };
    let company_id = match o.get("company_id") {
        None | Some(Value::Null) => None,
        Some(n) => match n.as_i64() {
            Some(i) if (0..=14).contains(&i) => Some(i),
            _ => { errors.push(format!("company_id must be an integer 0..14, not {n}")); None }
        },
    };
    let settings = match o.get("settings") {
        None | Some(Value::Null) => Vec::new(),
        Some(Value::String(s)) => settings(s).unwrap_or_else(|e| { errors.push(e); Vec::new() }),
        Some(other) => { errors.push(format!("settings must be a string \"name=value,...\", not {other}")); Vec::new() }
    };
    if ai.is_none() && company_id.is_none() && errors.is_empty() {
        errors.push("a directive needs ai or company_id, or there is nothing to apply".into());
    }
    if !errors.is_empty() { return Err(errors); }
    let ignored = o.keys().filter(|k| !READ.contains(&k.as_str())).cloned().collect();
    Ok((Directive { ai, company_id, settings }, ignored))
}

/// `"intercity=0,aggressiveness=75"` as name/value pairs; a script setting is an integer.
pub fn settings(s: &str) -> Result<Vec<(String, i64)>, String> {
    s.split(',').map(str::trim).filter(|p| !p.is_empty()).map(|p| {
        let (k, v) = p.split_once('=').ok_or_else(|| format!("setting {p:?} is not name=value"))?;
        let v = v.trim().parse::<i64>().map_err(|_| format!("setting {k} has no integer value ({v:?})"))?;
        Ok((k.trim().to_string(), v))
    }).collect()
}

/// The bridge's `CompanyOut` for each company: from openttd://companies, and from openttd://vehicles
/// the vehicle counts and `income`, the vehicles' profit last year (this year's until a year has
/// passed). The game publishes no president's name or performance rating, so `manager` is empty
/// and `performance` null; a road vehicle's cargo is not published either, so road vehicles are
/// counted in `road_vehicles` and `lorries`/`busses` are null.
pub fn bridge_companies(companies: &Value, vehicles: &Value) -> Value {
    let mut per: BTreeMap<i64, (BTreeMap<&str, i64>, i64, i64)> = BTreeMap::new();
    for v in vehicles.pointer("/data/vehicles").or_else(|| vehicles.get("vehicles")).and_then(Value::as_array).into_iter().flatten() {
        let Some(owner) = v.get("owner").and_then(Value::as_i64) else { continue };
        let e = per.entry(owner).or_default();
        let kind = match v.get("type").and_then(Value::as_str) {
            Some("train") => "trains", Some("road") => "road_vehicles", Some("aircraft") => "planes", Some("ship") => "ships", _ => continue,
        };
        *e.0.entry(kind).or_default() += 1;
        e.1 += v.get("profit_last_year").and_then(Value::as_i64).unwrap_or(0);
        e.2 += v.get("profit_this_year").and_then(Value::as_i64).unwrap_or(0);
    }
    let list = companies.pointer("/data/companies").or_else(|| companies.get("companies")).and_then(Value::as_array).cloned().unwrap_or_default();
    Value::Array(list.into_iter().map(|c| {
        let id = c.get("id").and_then(Value::as_i64).unwrap_or(-1);
        let (counts, last, this) = per.remove(&id).unwrap_or_default();
        let n = |k: &str| counts.get(k).copied().unwrap_or(0);
        json!({"id": id, "name": c.get("name").cloned().unwrap_or(json!("")), "manager": "",
               "is_ai": c.get("is_ai").cloned().unwrap_or(json!(false)),
               "money": c.get("money"), "loan": c.get("loan"), "company_value": c.get("company_value"),
               "income": if last != 0 { last } else { this }, "performance": null,
               "trains": n("trains"), "road_vehicles": n("road_vehicles"), "lorries": null, "busses": null,
               "planes": n("planes"), "ships": n("ships")})
    }).collect())
}

pub fn companies(g: &Arc<Game>) -> Result<Value, String> {
    let list = bridge_companies(&g.resource("openttd://companies")?, &g.resource("openttd://vehicles")?);
    Ok(offer_history(list, &crate::panel_store::endpoint(), &crate::panel_store::store_status(),
                     &format!("http://127.0.0.1:{}", g.args.panel_port)))
}

// ---------------------------------------------------------------------- observation (agent loop step 1)

/// Game ticks in a day (DAY_TICKS, src/timer/timer_game_tick.h).
pub const DAY_TICKS: i64 = 74;
/// How old an observation a directive may be decided from, in game days: Prismatic's inputs
/// (money, last year's income) move on a monthly scale. `$PRISMATIC_DIRECTIVE_MAX_AGE_DAYS` overrides it.
pub const MAX_AGE_DAYS: i64 = 31;

/// What one answer of `GET /api/bridge/companies` showed: when, and which companies.
#[derive(Clone, Debug, PartialEq)]
pub struct Observation {
    pub seq: u64,
    pub tick: i64,
    pub date: Value,
    pub digest: String,
    pub companies: Vec<(i64, bool)>,
}

impl Observation {
    pub fn json(&self) -> Value {
        json!({"seq": self.seq, "tick": self.tick, "date": self.date, "digest": self.digest,
               "companies": self.companies.iter().map(|(id, ai)| json!({"id": id, "is_ai": ai})).collect::<Vec<_>>()})
    }
}

/// The companies' identities in id order: (id, is_ai).
pub fn identities(list: &Value) -> Vec<(i64, bool)> {
    let mut out: Vec<(i64, bool)> = list.as_array().into_iter().flatten()
        .filter_map(|c| Some((c.get("id")?.as_i64()?, c.get("is_ai").and_then(Value::as_bool).unwrap_or(false)))).collect();
    out.sort();
    out
}

/// FNV-1a 64 over each company's id, AI flag and name in id order: the same companies give the same
/// digest whatever their money, and a company founded, gone, renamed or taken over changes it.
pub fn identity_digest(list: &Value) -> String {
    let mut rows: Vec<(i64, bool, String)> = list.as_array().into_iter().flatten().filter_map(|c| Some((
        c.get("id")?.as_i64()?, c.get("is_ai").and_then(Value::as_bool).unwrap_or(false),
        c.get("name").and_then(Value::as_str).unwrap_or("").to_string()))).collect();
    rows.sort();
    let mut h: u64 = 0xcbf29ce484222325;
    for (id, ai, name) in rows {
        for b in format!("{id}\u{1f}{ai}\u{1f}{name}\u{1e}").bytes() { h ^= b as u64; h = h.wrapping_mul(0x100000001b3); }
    }
    format!("{h:016x}")
}

fn observations() -> &'static Mutex<(u64, Option<Observation>)> {
    static O: OnceLock<Mutex<(u64, Option<Observation>)>> = OnceLock::new();
    O.get_or_init(|| Mutex::new((0, None)))
}

/// Record an answer of `GET /api/bridge/companies` as the latest observation.
pub fn record(tick: i64, date: Value, list: &Value) -> Observation {
    let mut g = observations().lock().unwrap_or_else(|e| e.into_inner());
    g.0 += 1;
    let o = Observation { seq: g.0, tick, date, digest: identity_digest(list), companies: identities(list) };
    g.1 = Some(o.clone());
    o
}

/// The number of observations so far and the latest one.
pub fn last() -> (u64, Option<Observation>) {
    let g = observations().lock().unwrap_or_else(|e| e.into_inner());
    (g.0, g.1.clone())
}

/// The game's tick and date now (openttd://game/state).
pub fn game_time(g: &Arc<Game>) -> Result<(i64, Value), String> {
    let st = g.resource("openttd://game/state")?;
    let tick = st.get("game_tick").and_then(Value::as_i64).ok_or("openttd://game/state has no game_tick")?;
    Ok((tick, st.get("game_date").cloned().unwrap_or(Value::Null)))
}

/// `GET /api/bridge/companies`: the companies, recorded as an observation.
pub fn observe(g: &Arc<Game>) -> Result<Value, String> {
    let list = companies(g)?;
    let (tick, date) = game_time(g)?;
    record(tick, date, &list);
    Ok(list)
}

/// Why a directive may not be applied (empty: it may). Fail closed: no observation during the pull,
/// one the directive does not name, companies changed since, an observation too old or from before
/// a new world, or a company that is not an AI company of the observation.
pub fn freshness(obs: Option<&Observation>, since: u64, now_tick: i64, now_digest: &str, d: &Directive, raw: &Value, max_age_days: i64) -> Vec<String> {
    let Some(o) = obs.filter(|o| o.seq > since) else {
        return vec!["Prismatic decided without observing this game during the pull: point its OPENTTD_BRIDGE_URL at <panel>/api/bridge".into()];
    };
    let mut why = Vec::new();
    if let Some(echo) = raw.get("observed_digest") {
        if echo.as_str() != Some(o.digest.as_str()) {
            why.push(format!("the directive names observation {echo}, not the one served ({})", o.digest));
        }
    }
    if now_digest != o.digest {
        why.push(format!("the companies changed since the observation (digest {} now {now_digest})", o.digest));
    }
    let age = now_tick - o.tick;
    if age < 0 {
        why.push(format!("the game went back since the observation (tick {} now {now_tick}): a new world", o.tick));
    } else if age > max_age_days * DAY_TICKS {
        why.push(format!("the observation is {} days old, more than {max_age_days}", age / DAY_TICKS));
    }
    if let Some(c) = d.company_id {
        if !o.companies.iter().any(|(id, ai)| *id == c && *ai) {
            why.push(format!("company {c} is not an AI company of the observation"));
        }
    }
    why
}

/// The age limit in force: `$PRISMATIC_DIRECTIVE_MAX_AGE_DAYS`, else MAX_AGE_DAYS.
pub fn max_age_days() -> i64 {
    std::env::var("PRISMATIC_DIRECTIVE_MAX_AGE_DAYS").ok().and_then(|v| v.parse().ok()).filter(|d| *d >= 0).unwrap_or(MAX_AGE_DAYS)
}

// ---------------------------------------------------------------------- history offered (I0085)

/// The version of the history a CompanyOut offers.
pub const HISTORY_SCHEMA: &str = "prismatic-history/v1";
/// The version of the fit this panel reads from a directive's working.
pub const FIT_SCHEMA: &str = "prismatic-fit/v1";
/// The bus kinds a fit reads: the daily sample and the settings an applied directive set.
pub const HISTORY_KINDS: &[&str] = &["game.sample", "plan.directive"];
/// The columns `samples_sql` answers, in order.
pub const SAMPLE_COLUMNS: &[&str] = &["instance", "seed", "run_ms", "game_tick", "game_date", "value", "money", "loan",
                                      "net", "vehicles", "aggressiveness", "directive_outcome"];
/// The fewest rows the panel offers as enough to fit from (advisory: twelve game months of one run).
pub const MIN_ROWS: usize = 12;
/// What Prismatic decided with before it fitted (prismatic_api OpenttdController, read at a8bc5a9d65).
pub const HAND_MODEL: &str = "mean_growth = aggressiveness * 0.08, std_growth = 0.05 + aggressiveness * 0.35";

/// The read-only statement of `company`'s recorded samples in every run of the store: the first
/// sample of each game month (so a long store stays under /api/db/query's cap), each with its run
/// (`run_ms`, the run's start, and `seed`) and the aggressiveness the latest plan.directive of that
/// run had set for the company by then, with its outcome (`applied`, or `pending` approval); null
/// before any, which is the AI's own default. A company id is reused across runs, so a fit reads
/// the rows whose aggressiveness is known.
pub fn samples_sql(company: i64) -> String {
    format!("WITH r AS (SELECT instance, seed, started_ms, lead(started_ms) OVER (PARTITION BY instance ORDER BY started_ms) AS until_ms FROM runs), \
s AS (SELECT x.instance, r.seed, r.started_ms AS run_ms, x.ts_ms, x.game_tick, x.game_date, x.value, x.money, x.loan, x.net, x.vehicles \
FROM samples x JOIN r ON r.instance = x.instance AND x.ts_ms >= r.started_ms AND (r.until_ms IS NULL OR x.ts_ms < r.until_ms) \
WHERE x.company = {company} \
QUALIFY row_number() OVER (PARTITION BY x.instance, r.started_ms, substr(x.game_date, 1, 7) ORDER BY x.game_tick, x.ts_ms) = 1), \
a AS (SELECT e.instance, r.started_ms AS run_ms, e.ts_ms, \
TRY_CAST(json_extract_string(e.data, '$.choice.settings.aggressiveness') AS BIGINT) AS aggressiveness, \
json_extract_string(e.data, '$.choice.outcome') AS directive_outcome \
FROM events e JOIN r ON r.instance = e.instance AND e.ts_ms >= r.started_ms AND (r.until_ms IS NULL OR e.ts_ms < r.until_ms) \
WHERE e.kind = 'plan.directive' AND json_extract_string(e.data, '$.choice.outcome') IN ('applied', 'pending') \
AND TRY_CAST(json_extract_string(e.data, '$.choice.company_id') AS BIGINT) = {company} \
AND json_extract_string(e.data, '$.choice.settings.aggressiveness') IS NOT NULL) \
SELECT s.instance, s.seed, s.run_ms, s.game_tick, s.game_date, s.value, s.money, s.loan, s.net, s.vehicles, a.aggressiveness, a.directive_outcome \
FROM s ASOF LEFT JOIN a ON s.instance = a.instance AND s.run_ms = a.run_ms AND s.ts_ms >= a.ts_ms \
ORDER BY s.instance, s.run_ms, s.game_tick")
}

/// The history the panel offers for `company` (companyHistory of the decision contract): the bus to
/// subscribe to, the store (or "off": then `query_url` answers 503), and the samples query.
pub fn history(company: i64, bus: &Value, store: &Value, panel: &str) -> Value {
    json!({"schema": HISTORY_SCHEMA,
           "bus": {"endpoint": bus, "envelope": BUS_SCHEMA, "kinds": HISTORY_KINDS},
           "store": store.as_str().map(str::to_string).unwrap_or_else(|| store.to_string()),
           "query_url": format!("{panel}/api/db/query"), "samples_query": samples_sql(company),
           "columns": SAMPLE_COLUMNS, "min_rows": MIN_ROWS})
}

/// The bridge's CompanyOut list with each company's `history`; the list stays a list, so a client of
/// the bridge's GET /companies reads it unchanged.
pub fn offer_history(list: Value, bus: &Value, store: &Value, panel: &str) -> Value {
    match list {
        Value::Array(cs) => Value::Array(cs.into_iter().map(|mut c| {
            if let Some(id) = c.get("id").and_then(Value::as_i64).filter(|i| *i >= 0) {
                c["history"] = history(id, bus, store, panel);
            }
            c
        }).collect()),
        other => other,
    }
}

// ---------------------------------------------------------------------- the fit recorded (I0085)

/// Why a directive's working carries no fit the panel can record as read.
pub fn no_fit() -> String {
    format!("Prismatic sent no fit: its directive names no recorded samples, so it came from a snapshot and its hand model ({HAND_MODEL})")
}

/// The fit in `raw`'s working as `prismatic-fit/v1` admits it (fitRecord of the decision contract),
/// or every reason it does not: `None` when the directive carries none.
pub fn fit_of(raw: &Value) -> Result<Option<Value>, Vec<String>> {
    let f = match raw.get("fit") {
        None | Some(Value::Null) => return Ok(None),
        Some(f) => f,
    };
    let Some(o) = f.as_object() else { return Err(vec![format!("fit must be an object, not {f}")]) };
    let mut errors = Vec::new();
    match o.get("schema").and_then(Value::as_str) {
        Some(FIT_SCHEMA) => {}
        Some(other) => return Err(vec![format!("fit is of schema {other}; this panel reads {FIT_SCHEMA}")]),
        None => return Err(vec![format!("fit names no schema (this panel reads {FIT_SCHEMA})")]),
    }
    if !o.get("model").and_then(Value::as_str).is_some_and(|m| !m.is_empty()) {
        errors.push("fit.model must be a non-empty string".into());
    }
    if !o.get("parameters").is_some_and(Value::is_object) {
        errors.push("fit.parameters must be an object".into());
    }
    if o.get("quality").is_some_and(|q| !q.is_object() && !q.is_null()) {
        errors.push("fit.quality must be an object".into());
    }
    let fallback = match o.get("fallback") {
        None | Some(Value::Null) => None,
        Some(Value::String(s)) => Some(s.as_str()),
        Some(other) => { errors.push(format!("fit.fallback must be a string or null, not {other}")); None }
    };
    match o.get("samples").and_then(Value::as_object) {
        None => errors.push("fit.samples must be an object with rows and a query or a bus".into()),
        Some(s) => {
            let rows = s.get("rows").and_then(Value::as_u64);
            if rows.is_none() { errors.push("fit.samples.rows must be an integer >= 0".into()); }
            if rows == Some(0) && fallback.is_none_or(str::is_empty) {
                errors.push("a fit from 0 rows must state its fallback".into());
            }
            let query = s.get("query");
            let bus = s.get("bus");
            if query.is_none() && bus.is_none() { errors.push("fit.samples names neither a query nor a bus".into()); }
            if let Some(q) = query {
                match q.as_str() {
                    Some(q) => if let Err(e) = read_only_sql(q) { errors.push(format!("fit.samples.query is not one read-only statement: {e}")); },
                    None => errors.push("fit.samples.query must be a string".into()),
                }
            }
            if let Some(b) = bus {
                if !b.as_str().is_some_and(|b| b.starts_with("tcp://")) { errors.push(format!("fit.samples.bus must be a tcp:// endpoint, not {b}")); }
            }
            if s.get("runs").is_some_and(|r| r.as_u64().is_none()) { errors.push("fit.samples.runs must be an integer >= 0".into()); }
        }
    }
    if errors.is_empty() { Ok(Some(f.clone())) } else { Err(errors) }
}

/// plan.directive's `inputs.stored` from `raw`: the samples Prismatic fitted from (the query, or the
/// bus, with its rows) beside the fit, or the fallback with why: no fit was sent, Prismatic itself
/// fell back (its fit stays recorded), or the fit breaks the contract (its errors).
pub fn fit_stored(raw: &Value) -> Value {
    match fit_of(raw) {
        Ok(None) => json!({"fallback": no_fit()}),
        Err(errors) => json!({"fallback": format!("Prismatic's fit breaks the decision contract and is not read: {}", errors.join("; ")),
                              "fit_errors": errors}),
        Ok(Some(fit)) => {
            let s = &fit["samples"];
            match fit.get("fallback").and_then(Value::as_str).filter(|f| !f.is_empty()) {
                Some(why) => json!({"fallback": format!("Prismatic fell back from fitting {}: {why}", fit["model"].as_str().unwrap_or("")),
                                    "rows": s["rows"], "fit": fit}),
                None => {
                    let mut out = json!({"query": s.get("query"), "rows": s["rows"], "fit": fit});
                    for k in ["query_url", "bus", "kinds", "runs"] {
                        if let Some(v) = s.get(k) { out[k] = v.clone(); }
                    }
                    out
                }
            }
        }
    }
}

/// The console line of what `stored` (from `fit_stored`) recorded.
pub fn fit_line(stored: &Value) -> String {
    match stored.get("fallback").and_then(Value::as_str) {
        Some(why) => format!("fit: none read ({why})"),
        None => format!("fit: {} from {} recorded rows ({})", stored["fit"]["model"].as_str().unwrap_or(""), stored["rows"],
                        if stored["query"].is_string() { "store query" } else { "bus" }),
    }
}

/// What one step of applying a directive printed, as the console lines of a DecisionPullResult.
fn line(what: &str, answer: &Value) -> String {
    let status = answer.get("status").or_else(|| answer.get("error")).map(|v| v.as_str().map(str::to_string).unwrap_or_else(|| v.to_string()));
    match (status, answer.get("approval_id")) {
        (Some(s), Some(id)) if !id.is_null() => format!("{what}: {s} (approval {id})"),
        (Some(s), _) => format!("{what}: {s}"),
        _ => format!("{what}: {answer}"),
    }
}

/// The state one step of applying a directive reached: `applied`, `pending` (it waits for the
/// operator's approval, who may still refuse it), `refused` (the game answered an error) or `error`
/// (the call itself failed, so whether the game changed is not known).
pub fn state_of(answer: &Result<Value, String>) -> &'static str {
    match answer {
        Err(_) => "error",
        Ok(a) if a.get("status").and_then(Value::as_str) == Some("pending_approval") => "pending",
        Ok(a) if a.get("error").is_some_and(|e| !e.is_null()) || a.get("ok") == Some(&json!(false)) => "refused",
        Ok(_) => "applied",
    }
}

/// One step of applying a directive as plan.directive carries it: what it asked, the state it
/// reached, the game's answer or the call's error, and its console line.
pub fn step(what: String, answer: Result<Value, String>) -> Value {
    let state = state_of(&answer);
    match answer {
        Ok(a) => json!({"what": what, "state": state, "line": line(&what, &a),
                        "approval_id": a.get("approval_id").filter(|v| !v.is_null()), "answer": a}),
        Err(e) => json!({"what": what, "state": state, "line": format!("{what}: error {e}"), "error": e}),
    }
}

/// The directive as a whole from its steps: `applied` when every step was, `pending` when the rest
/// wait for approval, `failed` when none got through, `partial` otherwise.
pub fn outcome(steps: &[Value]) -> &'static str {
    let n = |s: &str| steps.iter().filter(|x| x["state"] == json!(s)).count();
    let bad = n("error") + n("refused");
    match () {
        _ if steps.is_empty() => "nothing",
        _ if n("applied") == steps.len() => "applied",
        _ if bad == steps.len() => "failed",
        _ if bad > 0 => "partial",
        _ => "pending",
    }
}

/// Pull a directive from `url` and apply it: the DecisionPullResult, with `applied_rcon` null when
/// the directive asked for nothing. A directive the contract refuses is applied not at all. Every
/// step is tried and published in plan.directive with the state it reached, also when a call fails;
/// a failed call then answers 502 with the lines so far.
pub fn pull(g: &Arc<Game>, url: &str) -> Result<(u16, Value), String> {
    let since = last().0;
    let raw: Value = match ureq::get(url).timeout(Duration::from_secs(60)).call() {
        Ok(r) => r.into_json().map_err(|e| format!("{url}: not JSON ({e})"))?,
        Err(ureq::Error::Status(code, r)) => {
            return Ok((502, json!({"error": format!("{url} answered HTTP {code}"), "called_url": url,
                                   "body": r.into_string().unwrap_or_default()})));
        }
        Err(e) => return Ok((502, json!({"error": format!("{url}: {e}"), "called_url": url}))),
    };
    let (d, ignored) = match directive(&raw) {
        Ok(x) => x,
        Err(errors) => {
            g.event("ai", &format!("Prismatic's directive ({url}) breaks the decision contract and is not applied: {}", errors.join("; ")));
            g.plan("plan.directive", directive_event(url, &raw, Err(&errors), &[], &[]));
            return Ok((422, json!({"error": "the directive breaks the decision contract", "errors": errors,
                                   "called_url": url, "raw_response": raw})));
        }
    };
    // Bound to what Prismatic observed during this pull, or not applied at all.
    let (_, obs) = last();
    let now = game_time(g).and_then(|(t, _)| Ok((t, identity_digest(&bridge_companies(&g.resource("openttd://companies")?, &json!({}))))));
    let (now_tick, stale) = match now {
        Ok((tick, digest)) => (tick, freshness(obs.as_ref(), since, tick, &digest, &d, &raw, max_age_days())),
        Err(e) => (0, vec![format!("the game's state cannot be read now, so the directive cannot be checked against it: {e}")]),
    };
    let observed = obs.as_ref().filter(|o| o.seq > since).map(|o| { let mut j = o.json(); j["age_ticks"] = json!(now_tick - o.tick); j });
    if !stale.is_empty() {
        g.event("ai", &format!("Prismatic's directive ({url}) is not applied: {}", stale.join("; ")));
        let mut data = directive_event(url, &raw, Err(&stale), &ignored, &[]);
        data["inputs"]["live"]["observation"] = observed.clone().unwrap_or(Value::Null);
        data["choice"]["outcome"] = json!("stale");
        g.plan("plan.directive", data);
        return Ok((409, json!({"error": "the directive is not bound to a current observation", "errors": stale,
                               "observation": observed, "called_url": url, "raw_response": raw})));
    }
    let mut steps = Vec::new();
    if let Some(company) = d.company_id {
        if d.settings.is_empty() {
            steps.push(step(format!("reload company {company}'s AI"), g.tool("openttd.ai.reload", json!({"company": company}))));
        }
        for (name, value) in &d.settings {
            steps.push(step(format!("company {company} {name}={value}"), panel_bot::set_bot_setting(g, &json!(company), name, &json!(value))));
        }
    } else if let Some(ai) = &d.ai {
        let settings: serde_json::Map<String, Value> = d.settings.iter().map(|(k, v)| (k.clone(), json!(v))).collect();
        steps.push(step(format!("start {ai}"), panel_bot::start_bot(g, ai, Value::Object(settings))));
    }
    let mut lines: Vec<String> = steps.iter().map(|s| s["line"].as_str().unwrap_or_default().to_string()).collect();
    lines.push(fit_line(&fit_stored(&raw)));
    if !ignored.is_empty() { lines.push(format!("not read (the directive's own working): {}", ignored.join(", "))); }
    g.event("ai", &format!("Prismatic decided ({url}): {}", lines.join("; ")));
    let mut data = directive_event(url, &raw, Ok(&d), &ignored, &steps);
    data["inputs"]["live"]["observation"] = observed.unwrap_or(Value::Null);
    g.plan("plan.directive", data);
    let errors: Vec<&str> = steps.iter().filter(|s| s["state"] == json!("error")).filter_map(|s| s["error"].as_str()).collect();
    if !errors.is_empty() {
        return Ok((502, json!({"error": format!("applying the directive failed: {}", errors.join("; ")), "called_url": url,
                               "raw_response": raw, "applied_rcon": {"lines": lines}})));
    }
    Ok((200, json!({"called_url": url, "raw_response": raw, "applied_rcon": {"lines": lines}})))
}

/// The data of plan.directive: Prismatic's directive as read, whether the contract accepted it, and
/// each step the panel took with the state it reached (or why it took none), and the directive's
/// own working; `inputs.stored` is what Prismatic fitted from (`fit_stored`), the fit itself apart
/// from the rest of the working.
pub fn directive_event(url: &str, raw: &Value, d: Result<&Directive, &Vec<String>>, ignored: &[String], steps: &[Value]) -> Value {
    let choice = match d {
        Ok(d) => json!({"accepted": true, "outcome": outcome(steps), "ai": d.ai, "company_id": d.company_id,
                        "settings": d.settings.iter().map(|(k, v)| (k.clone(), json!(v))).collect::<serde_json::Map<String, Value>>(),
                        "how": if d.company_id.is_some() { if d.settings.is_empty() { "reload" } else { "set_setting" } } else { "start" },
                        "steps": steps, "ignored": ignored}),
        Err(errors) => json!({"accepted": false, "outcome": "refused", "errors": errors}),
    };
    let working: serde_json::Map<String, Value> = raw.as_object().into_iter().flatten()
        .filter(|(k, _)| !READ.contains(&k.as_str())).map(|(k, v)| (k.clone(), v.clone())).collect();
    let mut data = panel_events::plan("prismatic.directive",
                                      json!({"contract": "research/prismatic-bridge/decision-contract.schema.json", "fit_schema": FIT_SCHEMA}),
                                      json!({"url": url, "directive": raw, "engine": raw.get("engine"), "working": working}),
                                      &panel_events::Stored::Fallback(String::new()), choice);
    data["inputs"]["stored"] = fit_stored(raw);
    data
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cos(rows: &[(i64, bool, &str, i64)]) -> Value {
        Value::Array(rows.iter().map(|(id, ai, name, money)| json!({"id": id, "is_ai": ai, "name": name, "money": money})).collect())
    }

    #[test]
    fn the_identity_digest_ignores_money_and_order_but_not_identity() {
        let a = cos(&[(0, false, "Owner", 100), (1, true, "Bot", 5)]);
        let same = cos(&[(1, true, "Bot", 9999), (0, false, "Owner", -3)]);
        assert_eq!(identity_digest(&a), identity_digest(&same));
        for changed in [cos(&[(0, false, "Owner", 1)]), cos(&[(0, false, "Owner", 1), (1, true, "Bot2", 5)]),
                        cos(&[(0, false, "Owner", 1), (1, false, "Bot", 5)]), cos(&[(0, false, "Owner", 1), (2, true, "Bot", 5)])] {
            assert_ne!(identity_digest(&a), identity_digest(&changed), "{changed}");
        }
        assert_eq!(identity_digest(&json!([])).len(), 16);
        assert_eq!(identities(&a), vec![(0, false), (1, true)]);
    }

    #[test]
    fn a_directive_is_applied_only_against_a_current_observation() {
        let list = cos(&[(0, false, "Owner", 1), (1, true, "Bot", 1)]);
        let o = Observation { seq: 5, tick: 10_000, date: json!("1950-03-01"), digest: identity_digest(&list), companies: identities(&list) };
        let d = Directive { ai: Some("PrismaticAI".into()), company_id: Some(1), settings: vec![] };
        let now = identity_digest(&list);
        let f = |obs: Option<&Observation>, since, tick, digest: &str, d: &Directive, raw: Value| freshness(obs, since, tick, digest, d, &raw, 31);
        assert!(f(Some(&o), 4, 10_100, &now, &d, json!({})).is_empty(), "fresh, same companies, an AI company");
        assert!(f(Some(&o), 4, 10_100, &now, &d, json!({"observed_digest": o.digest})).is_empty(), "the echo matches");
        assert!(f(None, 0, 1, &now, &d, json!({}))[0].contains("without observing"));
        assert!(f(Some(&o), 5, 10_100, &now, &d, json!({}))[0].contains("without observing"), "an observation from before this pull");
        assert!(f(Some(&o), 4, 10_100, &now, &d, json!({"observed_digest": "0000"}))[0].contains("names observation"));
        assert!(f(Some(&o), 4, 10_100, "ffff", &d, json!({}))[0].contains("companies changed"));
        assert!(f(Some(&o), 4, 10_000 + 32 * DAY_TICKS, &now, &d, json!({}))[0].contains("32 days old"));
        assert!(f(Some(&o), 4, 10_000 + 31 * DAY_TICKS, &now, &d, json!({})).is_empty(), "31 days is still current");
        assert!(f(Some(&o), 4, 9_000, &now, &d, json!({}))[0].contains("new world"));
        let owner = Directive { company_id: Some(0), ..Directive { ai: None, company_id: None, settings: vec![] } };
        assert!(f(Some(&o), 4, 10_100, &now, &owner, json!({}))[0].contains("not an AI company"));
        let start = Directive { ai: Some("PrismaticAI".into()), company_id: None, settings: vec![] };
        assert!(f(Some(&o), 4, 10_100, &now, &start, json!({})).is_empty(), "starting an AI names no company");
        let all = f(Some(&o), 4, 9_000, "ffff", &owner, json!({"observed_digest": "x"}));
        assert_eq!(all.len(), 4, "every reason is named: {all:?}");
    }

    #[test]
    fn observations_are_numbered_and_the_latest_kept() {
        let (before, _) = last();
        let list = cos(&[(1, true, "Bot", 1)]);
        let o = record(7, json!("1950-01-01"), &list);
        assert!(o.seq > before);
        let (n, latest) = last();
        assert!(n >= o.seq && latest.is_some());
        assert_eq!(o.json()["companies"][0], json!({"id": 1, "is_ai": true}));
        std::env::set_var("PRISMATIC_DIRECTIVE_MAX_AGE_DAYS", "3");
        assert_eq!(max_age_days(), 3);
        std::env::set_var("PRISMATIC_DIRECTIVE_MAX_AGE_DAYS", "-1");
        assert_eq!(max_age_days(), MAX_AGE_DAYS);
        std::env::remove_var("PRISMATIC_DIRECTIVE_MAX_AGE_DAYS");
    }

    #[test]
    fn the_contracts_example_and_prismatics_answer_are_directives() {
        let example: Value = serde_json::from_str(include_str!("../../../../research/prismatic-bridge/decision-contract.example.json")).unwrap();
        let (d, ignored) = directive(&example["request"]).unwrap();
        assert_eq!(d, Directive { ai: Some("PrismaticAI".into()), company_id: Some(1), settings: vec![("aggressiveness".into(), 75)] });
        assert_eq!(ignored, vec!["chosen_aggressiveness", "engine"], "the fit is read; the rest of the working is not");
        assert_eq!(directive(&example["response"]["raw_response"]).unwrap().0, d);
        // What prismatic_api's OpenttdController#decision answers: the three fields and its working.
        let prismatic = json!({"ai": "PrismaticAI", "company_id": 1, "settings": "aggressiveness=100",
                               "engine": "PrismaticMonteCarlo.simulate/3", "chosen_aggressiveness": 100, "candidates": []});
        let (d, ignored) = directive(&prismatic).unwrap();
        assert_eq!((d.company_id, d.settings), (Some(1), vec![("aggressiveness".into(), 100)]));
        assert_eq!(ignored, vec!["candidates", "chosen_aggressiveness", "engine"]);
    }

    #[test]
    fn the_directive_event_says_what_each_step_reached_or_why_nothing_was_applied() {
        let raw = json!({"ai": "PrismaticAI", "company_id": 1, "settings": "aggressiveness=100,intercity=0", "engine": "PrismaticMonteCarlo.simulate/3"});
        let (d, ignored) = directive(&raw).unwrap();
        // set_setting is approval-gated: the first step waits for the operator, the second call failed.
        let steps = vec![step("company 1 aggressiveness=100".into(), Ok(json!({"status": "pending_approval", "approval_id": 3}))),
                         step("company 1 intercity=0".into(), Err("the game's MCP server is not reachable".into()))];
        assert_eq!(steps[0]["state"], "pending");
        assert_eq!(steps[0]["approval_id"], 3);
        assert_eq!(steps[0]["line"], "company 1 aggressiveness=100: pending_approval (approval 3)");
        assert_eq!(steps[1]["state"], "error");
        assert_eq!(steps[1]["line"], "company 1 intercity=0: error the game's MCP server is not reachable");
        let e = directive_event("http://x/decision", &raw, Ok(&d), &ignored, &steps);
        assert_eq!(e["decision"], "prismatic.directive");
        assert_eq!(e["choice"], json!({"accepted": true, "outcome": "partial", "ai": "PrismaticAI", "company_id": 1,
                                       "settings": {"aggressiveness": 100, "intercity": 0}, "how": "set_setting",
                                       "steps": steps, "ignored": ["engine"]}));
        assert!(e["choice"].get("applied").is_none(), "nothing claims the directive applied while a step waits or failed");
        assert_eq!(e["inputs"]["live"]["working"], json!({"engine": "PrismaticMonteCarlo.simulate/3"}));
        assert_eq!(e["inputs"]["stored"], json!({"fallback": no_fit()}), "no fit sent: the fallback says so");
        assert!(e["version"]["digest"].as_str().unwrap().starts_with("fnv1a64:"));
        let bad = json!({"company_id": 15});
        let errors = directive(&bad).unwrap_err();
        let e = directive_event("http://x/decision", &bad, Err(&errors), &[], &[]);
        assert_eq!((e["choice"]["accepted"].clone(), e["choice"]["outcome"].clone()), (json!(false), json!("refused")));
        assert_eq!(e["choice"]["errors"], json!(errors));
    }

    #[test]
    fn a_directive_is_applied_only_when_every_step_was() {
        let ok = || step("start PrismaticAI".into(), Ok(json!({"status": "started", "via": "console"})));
        let pending = || step("company 1 a=1".into(), Ok(json!({"status": "pending_approval", "approval_id": 9})));
        let refused = || step("company 1 a=1".into(), Ok(json!({"error": "this game binary has no openttd.ai.set_setting"})));
        let failed = || step("company 1 a=1".into(), Err("timeout".into()));
        assert_eq!(state_of(&Ok(json!({"ok": false, "error": null}))), "refused");
        assert_eq!(outcome(&[ok()]), "applied");
        assert_eq!(outcome(&[ok(), pending()]), "pending");
        assert_eq!(outcome(&[pending(), refused()]), "partial");
        assert_eq!(outcome(&[refused(), failed()]), "failed");
        assert_eq!(outcome(&[]), "nothing");
    }

    #[test]
    fn a_directive_outside_the_contract_is_refused_with_every_reason() {
        assert!(directive(&json!({})).unwrap_err()[0].contains("needs ai or company_id"));
        assert!(directive(&json!({"settings": "a=1"})).is_err());
        let e = directive(&json!({"ai": "", "company_id": 15, "settings": "aggressiveness=high"})).unwrap_err();
        assert_eq!(e.len(), 3, "{e:?}");
        assert!(directive(&json!(["PrismaticAI"])).is_err());
        assert!(settings("a=1,,b = 2 ").unwrap() == vec![("a".into(), 1), ("b".into(), 2)]);
        assert!(settings("a").is_err());
    }

    /// The contract's example, parsed.
    fn example() -> Value {
        serde_json::from_str(include_str!("../../../../research/prismatic-bridge/decision-contract.example.json")).unwrap()
    }

    /// The contract's schema, parsed.
    fn schema() -> Value {
        serde_json::from_str(include_str!("../../../../research/prismatic-bridge/decision-contract.schema.json")).unwrap()
    }

    /// A fit as Prismatic sends it: growth per aggressiveness from the offered query.
    fn a_fit() -> Value {
        json!({"schema": FIT_SCHEMA, "model": "growth_by_aggressiveness",
               "parameters": {"by_aggressiveness": [{"aggressiveness": 50, "mean_growth": 0.031, "std_growth": 0.12, "n": 40}]},
               "samples": {"query": samples_sql(1), "query_url": "http://127.0.0.1:8800/api/db/query", "rows": 40, "runs": 2},
               "quality": {"residual_std": 0.11}})
    }

    #[test]
    fn the_contract_and_the_code_name_the_same_versions_and_fields() {
        let s = schema();
        let d = &s["$defs"];
        assert_eq!(d["fitRecord"]["properties"]["schema"]["const"], FIT_SCHEMA);
        assert_eq!(d["fitRecord"]["required"], json!(["schema", "model", "parameters", "samples"]));
        assert_eq!(d["companyHistory"]["properties"]["schema"]["const"], HISTORY_SCHEMA);
        assert_eq!(d["companyHistory"]["properties"]["bus"]["properties"]["envelope"]["const"], BUS_SCHEMA);
        assert_eq!(d["decisionDirective"]["properties"]["fit"]["$ref"], "#/$defs/fitRecord");
        let h = history(1, &json!("tcp://127.0.0.1:5"), &json!("/x.duckdb"), "http://127.0.0.1:9");
        for k in d["companyHistory"]["required"].as_array().unwrap() {
            assert!(h.get(k.as_str().unwrap()).is_some(), "history lacks the contract's {k}");
        }
        for k in d["companyHistory"]["properties"].as_object().unwrap().keys() {
            assert!(h.get(k).is_some(), "history does not offer the contract's {k}");
        }
    }

    #[test]
    fn the_examples_history_and_fit_are_what_the_panel_offers_and_records() {
        let ex = example();
        // The fixture's companies carry the history the panel offers, query and all.
        let companies = ex["companies"].as_array().unwrap();
        assert!(!companies.is_empty());
        for c in companies {
            let id = c["id"].as_i64().unwrap();
            let h = &c["history"];
            let panel = h["query_url"].as_str().unwrap().trim_end_matches("/api/db/query");
            assert_eq!(h, &history(id, &h["bus"]["endpoint"], &h["store"], panel), "company {id}'s history in the fixture");
        }
        // The request carries a fit from that query; it is recorded with its rows.
        let (d, ignored) = directive(&ex["request"]).unwrap();
        assert_eq!(d.company_id, Some(1));
        assert!(ignored.iter().all(|k| k != "fit"), "the fit is read, not ignored: {ignored:?}");
        let stored = fit_stored(&ex["request"]);
        assert_eq!(stored["query"], ex["companies"][1]["history"]["samples_query"]);
        assert_eq!(stored["rows"], ex["request"]["fit"]["samples"]["rows"]);
        assert_eq!(stored["fit"], ex["request"]["fit"]);
        assert!(stored.get("fallback").is_none());
    }

    #[test]
    fn bridge_companies_stay_a_list_and_each_offers_its_history() {
        let list = json!([{"id": 0, "name": "A", "is_ai": false}, {"id": 1, "name": "B", "is_ai": true}]);
        let out = offer_history(list, &Value::Null, &json!("off"), "http://127.0.0.1:7");
        assert_eq!(out.as_array().unwrap().len(), 2);
        let h = &out[1]["history"];
        assert_eq!(h["schema"], HISTORY_SCHEMA);
        assert_eq!(h["bus"], json!({"endpoint": null, "envelope": BUS_SCHEMA, "kinds": ["game.sample", "plan.directive"]}));
        assert_eq!((h["store"].clone(), h["query_url"].clone()), (json!("off"), json!("http://127.0.0.1:7/api/db/query")));
        assert_eq!(h["samples_query"], samples_sql(1));
        assert!(samples_sql(1).contains("x.company = 1") && samples_sql(1).contains("AS BIGINT) = 1"));
        assert!(read_only_sql(&samples_sql(7)).is_ok());
        assert_eq!(out[0]["history"]["samples_query"], samples_sql(0));
    }

    #[test]
    fn a_directive_with_a_fit_publishes_its_query_rows_and_fit_as_the_stored_input() {
        let raw = json!({"ai": "PrismaticAI", "company_id": 1, "settings": "aggressiveness=50",
                         "engine": "PrismaticMonteCarlo.simulate/3", "fit": a_fit()});
        let (d, ignored) = directive(&raw).unwrap();
        assert_eq!(ignored, vec!["engine"]);
        let steps = vec![step("company 1 aggressiveness=50".into(), Ok(json!({"status": "set"})))];
        let e = directive_event("http://x/decision", &raw, Ok(&d), &ignored, &steps);
        assert_eq!(e["inputs"]["stored"], json!({"query": samples_sql(1), "rows": 40, "fit": a_fit(),
                                                  "query_url": "http://127.0.0.1:8800/api/db/query", "runs": 2}));
        assert_eq!(e["inputs"]["live"]["working"], json!({"engine": "PrismaticMonteCarlo.simulate/3"}), "the fit is not repeated as working");
        assert_eq!(e["version"]["params"]["fit_schema"], FIT_SCHEMA);
        assert_eq!(e["choice"]["outcome"], "applied");
        assert_eq!(fit_line(&e["inputs"]["stored"]), "fit: growth_by_aggressiveness from 40 recorded rows (store query)");
        // From the bus instead of the store.
        let mut f = a_fit();
        f["samples"] = json!({"bus": "tcp://127.0.0.1:5555", "kinds": ["game.sample"], "rows": 300});
        let stored = fit_stored(&json!({"company_id": 1, "fit": f}));
        assert_eq!((stored["query"].clone(), stored["bus"].clone(), stored["rows"].clone()), (Value::Null, json!("tcp://127.0.0.1:5555"), json!(300)));
        assert_eq!(fit_line(&stored), "fit: growth_by_aggressiveness from 300 recorded rows (bus)");
    }

    #[test]
    fn without_a_usable_fit_the_stored_input_states_its_fallback() {
        // Prismatic fell back itself: the fit stays recorded, the reason leads.
        let mut f = a_fit();
        f["samples"]["rows"] = json!(3);
        f["fallback"] = json!("3 rows, fewer than 12: the hand constants");
        let s = fit_stored(&json!({"company_id": 1, "fit": f.clone()}));
        assert_eq!(s["fallback"], "Prismatic fell back from fitting growth_by_aggressiveness: 3 rows, fewer than 12: the hand constants");
        assert_eq!((s["rows"].clone(), s["fit"].clone()), (json!(3), f));
        assert!(fit_line(&s).starts_with("fit: none read (Prismatic fell back"));
        // A fit that breaks the contract: every reason, nothing claimed as read; the directive still applies.
        let bad = |f: Value| fit_stored(&json!({"company_id": 1, "settings": "aggressiveness=50", "fit": f}));
        let mut v2 = a_fit();
        v2["schema"] = json!("prismatic-fit/v2");
        assert_eq!(bad(v2)["fit_errors"], json!(["fit is of schema prismatic-fit/v2; this panel reads prismatic-fit/v1"]));
        let mut none = a_fit();
        none["samples"]["rows"] = json!(0);
        assert_eq!(bad(none)["fit_errors"], json!(["a fit from 0 rows must state its fallback"]));
        let mut write = a_fit();
        write["samples"]["query"] = json!("DELETE FROM samples");
        assert!(bad(write)["fit_errors"][0].as_str().unwrap().starts_with("fit.samples.query is not one read-only statement"));
        let e = bad(json!({"schema": FIT_SCHEMA, "model": "", "parameters": [], "samples": {"rows": -1}}));
        assert_eq!(e["fit_errors"].as_array().unwrap().len(), 4, "{e}");
        assert!(e["fallback"].as_str().unwrap().starts_with("Prismatic's fit breaks the decision contract and is not read: "));
        assert!(e.get("query").is_none());
        assert!(bad(json!("growth")).get("fit_errors").is_some());
        assert!(directive(&json!({"company_id": 1, "settings": "aggressiveness=50", "fit": "growth"})).is_ok(),
                "a fit never refuses the directive");
    }

    #[cfg(feature = "store")]
    #[test]
    fn the_samples_query_gives_each_runs_months_with_the_aggressiveness_then_in_force() {
        use crate::bus_sinks::{db, envelope, sample_rows};
        let mut st = db::Store::open_in_memory("panel-t").unwrap();
        // Two runs of one instance: from t=1000 and from t=100000.
        st.insert_run(&Value::Null, Some(31337), 1000, (None, None, None)).unwrap();
        st.insert_run(&Value::Null, Some(4242), 100_000, (None, None, None)).unwrap();
        let day = |ts: i64, tick: i64, date: &str, value: f64| sample_rows(&json!({"t": tick, "date": date, "companies": [
            {"id": 1, "name": "P", "money": 10.0, "loan": 5.0, "value": value, "net": 1.0, "fleet": 2},
            {"id": 0, "name": "H", "money": 1.0, "loan": 0.0, "value": 1.0, "net": 0.0, "fleet": 0}]}), ts);
        let mut rows = Vec::new();
        for (ts, tick, date, v) in [(1100, 1, "1950-01-01", 100.0), (1200, 2, "1950-01-02", 101.0), (3100, 30, "1950-02-01", 120.0),
                                    (5100, 60, "1950-03-01", 150.0), (100_100, 1, "1950-01-01", 100.0), (102_100, 30, "1950-02-01", 90.0)] {
            rows.extend(day(ts, tick, date, v));
        }
        st.insert_samples(&rows).unwrap();
        // Applied at t=3000 in run 1 (aggressiveness 75), refused in run 2 (nothing in force there),
        // and one for another company.
        let event = |seq: i64, ts: i64, raw: Value, applied: bool| {
            let (d, ignored) = directive(&raw).unwrap();
            let answer = if applied { Ok(json!({"status": "set"})) } else { Ok(json!({"error": "refused"})) };
            let steps = vec![step("set".into(), answer)];
            envelope(&json!({"seq": seq, "kind": "plan.directive", "data": directive_event("u", &raw, Ok(&d), &ignored, &steps)}),
                     "panel-t", &Value::Null, None, ts, None)
        };
        st.insert_events(&[event(1, 3000, json!({"company_id": 1, "settings": "aggressiveness=75", "fit": a_fit()}), true),
                           event(2, 4000, json!({"company_id": 0, "settings": "aggressiveness=25"}), true),
                           event(3, 100_050, json!({"company_id": 1, "settings": "aggressiveness=100"}), false)]).unwrap();
        let out = st.query(&samples_sql(1)).unwrap();
        assert_eq!(out["columns"], json!(SAMPLE_COLUMNS));
        let got: Vec<(i64, String, f64, Value)> = out["rows"].as_array().unwrap().iter()
            .map(|r| (r[2].as_i64().unwrap(), r[4].as_str().unwrap().to_string(), r[5].as_f64().unwrap(), r[10].clone())).collect();
        assert_eq!(got, vec![(1000, "1950-01-01".into(), 100.0, Value::Null), (1000, "1950-02-01".into(), 120.0, json!(75)),
                             (1000, "1950-03-01".into(), 150.0, json!(75)), (100_000, "1950-01-01".into(), 100.0, Value::Null),
                             (100_000, "1950-02-01".into(), 90.0, Value::Null)],
                   "one row per game month and run; the setting holds from its directive on, within its run and company");
        assert_eq!(out["rows"][1][1], json!(31337));
        assert_eq!(out["rows"][1][11], json!("applied"));
        // The directive's own event names what it was fitted from.
        let stored = st.query("SELECT json_extract_string(data, '$.inputs.stored.query') AS q, json_extract(data, '$.inputs.stored.rows') AS n FROM events WHERE seq = 1").unwrap();
        assert_eq!(stored["rows"][0][0], json!(samples_sql(1)));
    }

    #[test]
    fn companies_take_the_bridges_shape_with_income_from_the_vehicles() {
        let companies = json!({"data": {"companies": [
            {"id": 0, "name": "Human Transport", "is_ai": false, "money": 100, "loan": 50, "company_value": 400},
            {"id": 1, "name": "Prismatic", "is_ai": true, "money": 900, "loan": 0, "company_value": 1200}]}});
        let vehicles = json!({"data": {"vehicles": [
            {"owner": 1, "type": "road", "profit_last_year": 300, "profit_this_year": 10},
            {"owner": 1, "type": "train", "profit_last_year": 700, "profit_this_year": 20},
            {"owner": 0, "type": "ship", "profit_last_year": 0, "profit_this_year": 55}]}});
        let out = bridge_companies(&companies, &vehicles);
        assert_eq!(out[1]["income"], json!(1000));
        assert_eq!((out[1]["trains"].clone(), out[1]["road_vehicles"].clone(), out[1]["is_ai"].clone()), (json!(1), json!(1), json!(true)));
        assert_eq!(out[0]["income"], json!(55), "this year's profit until a year has passed");
        assert_eq!(out[0]["ships"], json!(1));
        for k in ["id", "name", "manager", "is_ai", "money", "loan", "income", "company_value", "performance", "trains", "lorries", "busses", "planes", "ships"] {
            assert!(out[0].get(k).is_some(), "CompanyOut field {k}");
        }
    }
}
