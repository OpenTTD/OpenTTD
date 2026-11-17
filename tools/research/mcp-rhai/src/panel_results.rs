//! RESEARCH-ONLY. Duels from the panel: every research result (research/results/duel-*.json, written by
//! tools/research/mcp-duel) as a list and one page each, and a duel started with a click. The owner
//! asked for experiments to be "exposed over MCP/API/Swagger/UI so I can click them": the routes are
//! rows of the panel's route table, so they are in OpenAPI, Swagger UI and Rhai like every other.

use std::path::PathBuf;
use std::process::{Child, Command, Stdio};
use std::sync::{Arc, Mutex};
use std::time::Instant;

use serde_json::{json, Value};

use crate::panel::{here, Game, Input};

/// The repository root (here() is tools/research/mcp-panel).
pub fn repo() -> PathBuf {
    here().join("../../..")
}

fn results_dir() -> PathBuf {
    repo().join("research/results")
}

/// A result name as mcp-duel writes it (duel-<time>-seed<N>), and nothing that walks out of the
/// directory.
pub fn valid_name(name: &str) -> bool {
    name.starts_with("duel-") && name.len() < 80 && name.chars().all(|c| c.is_ascii_alphanumeric() || c == '-')
}

/// One line of the list: seed, months, label, opponent, winner and both companies' final standing.
pub fn summary(name: &str, d: &Value) -> Value {
    let side = |k: &str| {
        let c = &d["final"][k];
        json!({"name": c["name"], "value": c["value"], "money": c["money"], "loan": c["loan"], "trains": c["trains"], "road": c["road"]})
    };
    let (a, b) = (side("1"), side("2"));
    let ratio = match (a["value"].as_f64(), b["value"].as_f64()) {
        (Some(x), Some(y)) if y > 0.0 => json!((x / y * 10.0).round() / 10.0),
        _ => Value::Null,
    };
    json!({"name": name, "seed": d["seed"], "months": d["months"], "label": d["label"], "opponent": d["opponent"],
           "winner": d["winner"], "a": a, "b": b, "ratio": ratio})
}

fn read(name: &str) -> Option<Value> {
    let text = std::fs::read_to_string(results_dir().join(format!("{name}.json"))).ok()?;
    serde_json::from_str(&text).ok()
}

/// Every duel result, newest first (the name carries its time).
pub fn list() -> Vec<Value> {
    let mut names: Vec<String> = std::fs::read_dir(results_dir()).into_iter().flatten().flatten()
        .filter_map(|e| e.file_name().to_str().map(str::to_string))
        .filter_map(|f| f.strip_suffix(".json").map(str::to_string))
        .filter(|n| valid_name(n))
        .collect();
    names.sort_by(|a, b| b.cmp(a));
    names.iter().filter_map(|n| read(n).map(|d| summary(n, &d))).collect()
}

pub fn h_results(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let items = list();
    Ok((200, json!({"count": items.len(), "results": items})))
}

pub fn h_result(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = i.query.get("name").unwrap_or("").to_string();
    if !valid_name(&name) {
        return Ok((400, json!({"error": "name: a duel result name, e.g. duel-20261117T041705-seed31337"})));
    }
    let Some(d) = read(&name) else { return Ok((404, json!({"error": format!("no result {name}")}))) };
    let reading = std::fs::read_to_string(results_dir().join(format!("{name}.md"))).unwrap_or_default();
    Ok((200, json!({"summary": summary(&name, &d), "series": d["series"], "plan": d["plan"], "bot_log": d["bot_log"],
                    "experiment": d["experiment"], "markdown": reading})))
}

/// The duel started from the panel: its process, log file, arguments and start.
struct Running {
    child: Child,
    log: PathBuf,
    args: Vec<String>,
    started: Instant,
}

static DUEL: Mutex<Option<Running>> = Mutex::new(None);

/// The arguments mcp-duel gets from a request body, checked: months 1..=240, a seed, a label, an AI
/// name of letters, digits and underscores.
pub fn duel_args(body: &Value) -> Result<Vec<String>, String> {
    let months = body.get("months").and_then(Value::as_i64).unwrap_or(36);
    if !(1..=240).contains(&months) {
        return Err("months: 1 to 240".into());
    }
    let mut args = vec!["--months".to_string(), months.to_string()];
    if let Some(seed) = body.get("seed").filter(|v| !v.is_null()) {
        let seed = seed.as_i64().ok_or("seed: a whole number")?;
        args.extend(["--seed".to_string(), seed.to_string()]);
    }
    if let Some(label) = body.get("label").and_then(Value::as_str).filter(|s| !s.is_empty()) {
        args.extend(["--label".to_string(), label.chars().take(200).collect()]);
    }
    if let Some(op) = body.get("opponent").and_then(Value::as_str).filter(|s| !s.is_empty()) {
        if !op.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
            return Err("opponent: an AI name (letters, digits, _)".into());
        }
        args.extend(["--opponent".to_string(), op.to_string()]);
    }
    Ok(args)
}

pub fn h_duel_start(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let args = match duel_args(&i.body) {
        Ok(a) => a,
        Err(e) => return Ok((400, json!({"error": e}))),
    };
    let mut slot = DUEL.lock().unwrap();
    if let Some(r) = slot.as_mut() {
        if r.child.try_wait().ok().flatten().is_none() {
            return Ok((409, json!({"error": "a duel is running; GET /api/duel shows it", "args": r.args})));
        }
    }
    let log = std::env::temp_dir().join(format!("mcp-duel-panel-{}.log", std::process::id()));
    let out = std::fs::File::create(&log).map_err(|e| format!("{}: {e}", log.display()))?;
    let err = out.try_clone().map_err(|e| e.to_string())?;
    let child = Command::new("bash").arg(repo().join("tools/research/mcp-duel")).args(&args).current_dir(repo())
        .stdin(Stdio::null()).stdout(out).stderr(err).spawn().map_err(|e| format!("mcp-duel: {e}"))?;
    let pid = child.id();
    g.event("duel", &format!("duel started from the panel: mcp-duel {}", args.join(" ")));
    *slot = Some(Running { child, log: log.clone(), args: args.clone(), started: Instant::now() });
    Ok((202, json!({"status": "started", "pid": pid, "args": args, "log": log})))
}

/// The panel's duel: running or finished, the last month line and the winner line it printed.
pub fn h_duel_status(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let mut slot = DUEL.lock().unwrap();
    let Some(r) = slot.as_mut() else { return Ok((200, json!({"running": false, "started": false}))) };
    let exit = r.child.try_wait().ok().flatten();
    let text = std::fs::read_to_string(&r.log).unwrap_or_default();
    let lines: Vec<&str> = text.lines().collect();
    let month = lines.iter().rev().find(|l| l.starts_with("month ")).map(|s| s.to_string());
    let result = lines.iter().rev().find(|l| l.starts_with("mcp-duel: winner")).map(|s| s.to_string());
    let name = result.as_deref().and_then(|l| l.rsplit('/').next()).and_then(|f| f.strip_suffix(".md")).map(str::to_string);
    Ok((200, json!({"running": exit.is_none(), "started": true, "exit": exit.and_then(|s| s.code()), "args": r.args,
                    "seconds": r.started.elapsed().as_secs(), "month": month, "result": result, "name": name,
                    "tail": lines.iter().rev().take(12).rev().collect::<Vec<_>>()})))
}

/// The page title and description of /results and /result/<name>, and whether it exists.
pub fn page_meta(path: &str) -> (String, String, bool) {
    if path == "/results" {
        return ("Duels".into(), "Every duel of the planned bot against an unplanned one, with its result, and a duel started with a click".into(), true);
    }
    let name = path.strip_prefix("/result/").unwrap_or("");
    match (valid_name(name), read(name)) {
        (true, Some(d)) => {
            let s = summary(name, &d);
            (format!("Duel seed {} over {} months: {} to {}", s["seed"], s["months"], s["a"]["value"], s["b"]["value"]),
             d["label"].as_str().unwrap_or("").to_string(), true)
        }
        _ => (format!("{name}: no such duel"), String::new(), false),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_and_arguments_are_checked() {
        assert!(valid_name("duel-20261117T041705-seed31337"));
        assert!(!valid_name("../secret"));
        assert!(!valid_name("duel-x/../../etc"));
        assert_eq!(duel_args(&json!({})).unwrap(), ["--months", "36"]);
        assert_eq!(duel_args(&json!({"months": 12, "seed": 4242, "label": "x", "opponent": "AdmiralAI"})).unwrap(),
                   ["--months", "12", "--seed", "4242", "--label", "x", "--opponent", "AdmiralAI"]);
        assert!(duel_args(&json!({"months": 0})).is_err());
        assert!(duel_args(&json!({"opponent": "a; rm"})).is_err());
        assert!(duel_args(&json!({"seed": "1"})).is_err());
    }

    #[test]
    fn a_summary_carries_both_sides_and_their_ratio() {
        let d = json!({"seed": 7, "months": 36, "label": "l", "winner": "A (planned)",
                       "final": {"1": {"name": "A", "value": 500, "trains": 7}, "2": {"name": "B", "value": 50, "trains": 0}}});
        let s = summary("duel-1-seed7", &d);
        assert_eq!(s["ratio"], json!(10.0));
        assert_eq!(s["a"]["trains"], json!(7));
        assert_eq!(summary("duel-2", &json!({"final": {"1": {"value": 3}, "2": {"value": 0}}}))["ratio"], Value::Null);
    }

    #[test]
    fn the_committed_results_are_listed() {
        let items = list();
        assert!(!items.is_empty(), "research/results holds duels");
        assert!(items.windows(2).all(|w| w[0]["name"].as_str() >= w[1]["name"].as_str()), "newest first");
    }
}
