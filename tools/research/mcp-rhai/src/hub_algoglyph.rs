//! RESEARCH-ONLY / UNSTABLE: AlgoGlyph's kernels as hub methods.
//!
//! AlgoGlyph (`$MCP_ALGOGLYPH_DIR`, default `~/dev/algoglyph`) answers one kernel on caller data
//! through its flow CLI: `npm run --silent flow -- kernel <id> -` reads the input on stdin and
//! prints the answer as JSON; `flow -- kernels` lists every kernel with the input shape it reads.
//! Nothing here names a kernel except `track_plan`, which assembles the track-planning input from a
//! game: the `data` of its openttd://map, map/ways, map/terrain, economy/prices and bridges,
//! verbatim, read in that order (a resource the game does not publish is left out, and the kernel falls back to
//! its defaults). The planner checks slopes against heights, so the four are read back to back.
//!
//! Methods (added to the hub's table, so they are in rpc.discover, GraphQL and OpenAPI):
//!   algoglyph.kernels                          every kernel with the input it reads
//!   algoglyph.kernel     {kernel, input}       one kernel on that input
//!   algoglyph.track_plan {instance, from, to, costs?, rules?}   a rail route in that game

use serde_json::{json, Value};
use std::io::{Read, Write};
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

/// The longest a kernel may run before the hub gives up on it.
pub const KERNEL_TIMEOUT: Duration = Duration::from_secs(120);

/// The resources a track plan reads, as the keys of the kernel's input.
pub const TRACK_INPUTS: [(&str, &str); 5] = [("map", "map"), ("ways", "map/ways"), ("terrain", "map/terrain"),
    ("prices", "economy/prices"), ("bridges", "bridges")];

/// Where AlgoGlyph is checked out.
pub fn dir() -> PathBuf {
    std::env::var_os("MCP_ALGOGLYPH_DIR").map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(std::env::var_os("HOME").unwrap_or_default()).join("dev/algoglyph")
    })
}

/// The method table entries, appended to the hub's own.
pub fn methods() -> Vec<Value> {
    vec![
        json!({"method": "algoglyph.kernels", "params": {}, "result": "every AlgoGlyph kernel with the input shape it reads (flow -- kernels)"}),
        json!({"method": "algoglyph.kernel", "params": {"kernel": "required: a kernel id from algoglyph.kernels", "input": "object: that kernel's input"},
               "result": "the kernel's answer (flow -- kernel <id> -)"}),
        json!({"method": "algoglyph.track_plan",
               "params": {"instance": "required: a game instance", "from": "{x, y, dir: NE|SE|SW|NW}", "to": "{x, y, dir?}", "costs": "optional", "rules": "optional",
                          "railtype": "optional: a label (RAIL, ELRL, MONO, MGLV) or index of openttd://railtypes; plain rail when left out"},
               "result": "AlgoGlyph track-planning on the game's map, map/ways, map/terrain, economy/prices and bridges: status, cost, money, counts, pieces, runs"}),
    ]
}

/// Run the flow CLI with `args`, `stdin` on its standard input; its standard output parsed as JSON.
pub fn flow(args: &[&str], stdin: Option<&Value>) -> Result<Value, String> {
    let dir = dir();
    if !dir.join("package.json").is_file() {
        return Err(format!("no AlgoGlyph at {} (set MCP_ALGOGLYPH_DIR)", dir.display()));
    }
    let mut child = Command::new("npm").args(["run", "--silent", "flow", "--"]).args(args)
        .current_dir(&dir).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped())
        .spawn().map_err(|e| format!("npm: {e}"))?;
    {
        let mut input = child.stdin.take().ok_or("no stdin")?;
        if let Some(v) = stdin { input.write_all(v.to_string().as_bytes()).map_err(|e| format!("stdin: {e}"))?; }
    }
    let mut out = child.stdout.take().ok_or("no stdout")?;
    let mut errs = child.stderr.take().ok_or("no stderr")?;
    let reader = std::thread::spawn(move || { let mut s = String::new(); let _ = out.read_to_string(&mut s); s });
    let err_reader = std::thread::spawn(move || { let mut s = String::new(); let _ = errs.read_to_string(&mut s); s });
    let started = Instant::now();
    let status = loop {
        match child.try_wait().map_err(|e| e.to_string())? {
            Some(s) => break s,
            None if started.elapsed() > KERNEL_TIMEOUT => {
                let _ = child.kill();
                return Err(format!("flow {} ran over {} s", args.join(" "), KERNEL_TIMEOUT.as_secs()));
            }
            None => std::thread::sleep(Duration::from_millis(20)),
        }
    };
    let text = reader.join().unwrap_or_default();
    let stderr = err_reader.join().unwrap_or_default();
    parse_answer(&text).ok_or_else(|| {
        let why = stderr.lines().last().unwrap_or("").to_string();
        format!("flow {} exited {status}: {}", args.join(" "), if why.is_empty() { text.trim().chars().take(300).collect() } else { why })
    })
}

/// The JSON a flow run printed: the whole output, else its last line that parses.
pub fn parse_answer(text: &str) -> Option<Value> {
    serde_json::from_str(text.trim()).ok()
        .or_else(|| text.lines().rev().find_map(|l| serde_json::from_str::<Value>(l.trim()).ok().filter(|v| v.is_object() || v.is_array())))
}

/// What a read of one resource answered, as `track_input` takes it: a panel's 404 (the build has
/// no such resource) is not published, None; any other HTTP error is an error; anything else is
/// the resource.
pub fn published(v: Value) -> Result<Option<Value>, String> {
    match v.get("http_error").and_then(Value::as_u64) {
        Some(404) => Ok(None),
        Some(code) => Err(format!("HTTP {code}: {}", v.get("body").map(Value::to_string).unwrap_or_default())),
        None => Ok(Some(v)),
    }
}

/// The track-planning input: `{map, ways, terrain, prices}` from `read` (resource -> its `data`,
/// None when the game does not publish it), plus the caller's from, to, costs and rules.
pub fn track_input(params: &Value, read: &dyn Fn(&str) -> Result<Option<Value>, String>) -> Result<Value, String> {
    let from = params.get("from").filter(|v| v.is_object()).ok_or("track_plan needs from {x, y, dir}")?;
    let to = params.get("to").filter(|v| v.is_object()).ok_or("track_plan needs to {x, y, dir?}")?;
    let mut input = json!({"from": from, "to": to});
    for (key, resource) in TRACK_INPUTS {
        match read(resource)? {
            Some(data) => { input[key] = data; }
            None if key == "map" => return Err("the instance publishes no openttd://map".into()),
            None => {}
        }
    }
    for key in ["costs", "rules"] {
        if let Some(v) = params.get(key).filter(|v| !v.is_null()) { input[key] = v.clone(); }
    }
    if let Some(want) = params.get("railtype").filter(|v| !v.is_null()) {
        let types = read("railtypes")?.ok_or("the instance publishes no openttd://railtypes")?;
        input["railtype"] = pick_railtype(&types, want)?;
    }
    Ok(input)
}

/// The entry of openttd://railtypes the caller asked for, by label ("RAIL", "ELRL") or index.
pub fn pick_railtype(types: &Value, want: &Value) -> Result<Value, String> {
    let list = types.get("types").and_then(Value::as_array).ok_or("openttd://railtypes has no types")?;
    list.iter().find(|t| match want {
        Value::String(label) => t.get("label").and_then(Value::as_str) == Some(label.as_str()),
        Value::Number(n) => t.get("railtype").and_then(Value::as_u64) == n.as_u64(),
        _ => false,
    }).cloned().ok_or_else(|| {
        let known: Vec<&str> = list.iter().filter_map(|t| t.get("label").and_then(Value::as_str)).collect();
        format!("no rail type {want}; the game has {}", known.join(", "))
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_answer_is_the_output_or_its_last_json_line() {
        assert_eq!(parse_answer("{\"ok\":true}\n"), Some(json!({"ok": true})));
        assert_eq!(parse_answer("> algoglyph@1 flow\nnoise\n{\"status\":\"found\"}\n"), Some(json!({"status": "found"})));
        assert_eq!(parse_answer("nothing here"), None);
    }

    #[test]
    fn a_track_input_takes_the_games_resources_verbatim_and_skips_what_is_not_published() {
        let read = |r: &str| -> Result<Option<Value>, String> {
            Ok(match r { "map" => Some(json!({"width": 64})), "map/ways" => Some(json!({"roads": []})), _ => None })
        };
        let p = json!({"from": {"x": 1, "y": 2, "dir": "NE"}, "to": {"x": 9, "y": 2}, "rules": {"forbid90": true}, "costs": null});
        let input = track_input(&p, &read).unwrap();
        assert_eq!(input["map"], json!({"width": 64}));
        assert_eq!(input["ways"], json!({"roads": []}));
        assert!(input.get("terrain").is_none() && input.get("prices").is_none() && input.get("bridges").is_none() && input.get("costs").is_none());
        assert_eq!(input["rules"], json!({"forbid90": true}));
        assert!(track_input(&json!({"from": {"x": 1}}), &read).is_err());
        let nomap = |_: &str| -> Result<Option<Value>, String> { Ok(None) };
        assert!(track_input(&p, &nomap).unwrap_err().contains("openttd://map"));
    }

    #[test]
    fn a_railtype_is_picked_by_label_or_index_and_an_unknown_one_names_the_choices() {
        let types = json!({"types": [{"railtype": 0, "label": "RAIL", "cost_multiplier": 8}, {"railtype": 1, "label": "ELRL", "cost_multiplier": 12}]});
        assert_eq!(pick_railtype(&types, &json!("ELRL")).unwrap()["cost_multiplier"], 12);
        assert_eq!(pick_railtype(&types, &json!(0)).unwrap()["label"], "RAIL");
        assert!(pick_railtype(&types, &json!("MGLV")).unwrap_err().contains("RAIL, ELRL"));
        let read = |r: &str| -> Result<Option<Value>, String> {
            Ok(match r { "map" => Some(json!({})), "railtypes" => Some(types.clone()), _ => None })
        };
        let p = json!({"from": {"x": 1, "y": 1, "dir": "NE"}, "to": {"x": 5, "y": 1}, "railtype": "ELRL"});
        assert_eq!(track_input(&p, &read).unwrap()["railtype"]["label"], "ELRL");
        let plain = json!({"from": {"x": 1, "y": 1, "dir": "NE"}, "to": {"x": 5, "y": 1}});
        assert!(track_input(&plain, &read).unwrap().get("railtype").is_none());
    }

    #[test]
    fn a_panels_404_is_a_resource_not_published_and_any_other_error_fails() {
        assert_eq!(published(json!({"http_error": 404, "body": {"error": "no such resource"}})).unwrap(), None);
        assert!(published(json!({"http_error": 503, "body": "down"})).unwrap_err().contains("503"));
        assert_eq!(published(json!({"data": {"width": 8}})).unwrap(), Some(json!({"data": {"width": 8}})));
    }

    #[test]
    fn every_method_says_what_it_reads() {
        for m in methods() {
            assert!(m["method"].as_str().unwrap().starts_with("algoglyph."));
            assert!(m["params"].is_object() && m["result"].is_string());
        }
    }
}
