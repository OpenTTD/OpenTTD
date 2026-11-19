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

use serde_json::{json, Value};
use std::collections::BTreeMap;
use std::sync::Arc;
use std::time::Duration;

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

/// The directive in `v`, or every reason the contract refuses it. Fields beyond the contract's
/// three are the directive's own working, which the contract admits and no consumer reads (the
/// bridge ignores them too); `ignored` names them.
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
    let ignored = o.keys().filter(|k| !["ai", "company_id", "settings"].contains(&k.as_str())).cloned().collect();
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
    Ok(bridge_companies(&g.resource("openttd://companies")?, &g.resource("openttd://vehicles")?))
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
    if !ignored.is_empty() { lines.push(format!("not read (the directive's own working): {}", ignored.join(", "))); }
    g.event("ai", &format!("Prismatic decided ({url}): {}", lines.join("; ")));
    g.plan("plan.directive", directive_event(url, &raw, Ok(&d), &ignored, &steps));
    let errors: Vec<&str> = steps.iter().filter(|s| s["state"] == json!("error")).filter_map(|s| s["error"].as_str()).collect();
    if !errors.is_empty() {
        return Ok((502, json!({"error": format!("applying the directive failed: {}", errors.join("; ")), "called_url": url,
                               "raw_response": raw, "applied_rcon": {"lines": lines}})));
    }
    Ok((200, json!({"called_url": url, "raw_response": raw, "applied_rcon": {"lines": lines}})))
}

/// The data of plan.directive: Prismatic's directive as read, whether the contract accepted it, and
/// each step the panel took with the state it reached (or why it took none), and the directive's
/// own working the contract does not read.
pub fn directive_event(url: &str, raw: &Value, d: Result<&Directive, &Vec<String>>, ignored: &[String], steps: &[Value]) -> Value {
    let choice = match d {
        Ok(d) => json!({"accepted": true, "outcome": outcome(steps), "ai": d.ai, "company_id": d.company_id,
                        "settings": d.settings.iter().map(|(k, v)| (k.clone(), json!(v))).collect::<serde_json::Map<String, Value>>(),
                        "how": if d.company_id.is_some() { if d.settings.is_empty() { "reload" } else { "set_setting" } } else { "start" },
                        "steps": steps, "ignored": ignored}),
        Err(errors) => json!({"accepted": false, "outcome": "refused", "errors": errors}),
    };
    let working: serde_json::Map<String, Value> = raw.as_object().into_iter().flatten()
        .filter(|(k, _)| !["ai", "company_id", "settings"].contains(&k.as_str())).map(|(k, v)| (k.clone(), v.clone())).collect();
    panel_events::plan("prismatic.directive", json!({"contract": "research/prismatic-bridge/decision-contract.schema.json"}),
                       json!({"url": url, "directive": raw, "engine": raw.get("engine"), "working": working}),
                       &panel_events::live_only("I0085"), choice)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_contracts_example_and_prismatics_answer_are_directives() {
        let example: Value = serde_json::from_str(include_str!("../../../../research/prismatic-bridge/decision-contract.example.json")).unwrap();
        let (d, ignored) = directive(&example["request"]).unwrap();
        assert_eq!(d, Directive { ai: Some("PrismaticAI".into()), company_id: None,
                                  settings: vec![("intercity".into(), 0), ("aggressiveness".into(), 75)] });
        assert!(ignored.is_empty());
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
        assert!(e["inputs"]["stored"]["fallback"].as_str().unwrap().contains("I0085"));
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
