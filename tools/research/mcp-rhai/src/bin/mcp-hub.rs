//! RESEARCH-ONLY / UNSTABLE: one WebSocket JSON-RPC surface over every MCP control panel and MCP
//! server of a running cluster (see src/hub.rs and research/mcp-hub.md).
//!
//! Usage:
//!   mcp-hub [serve] [--port 8790] [--registry DIR] [--panel URL]... [--static DIR] [--audit-every S]
//!                   [--bus-port N] [--no-store]
//!   mcp-hub check [--hub ws://127.0.0.1:8790/ws] [--event-wait SECONDS]
//!   mcp-hub audit [--hub ws://127.0.0.1:8790/ws] [--json]
//!   mcp-hub intent-check [--hub ws://127.0.0.1:8790/ws] ["<sentence>"]...
//!
//! serve: discovers instances from $PRISMATIC_MCP_REGISTRY (default
//! ~/.local/state/prismatic-mcp/instances) and every --panel, serves ws://127.0.0.1:<port>/ws,
//! POST /hub/rpc, GET /hub/instances, /hub/catalogue, and the page / (static/hub.html). Every
//! --audit-every seconds (default 60, 0 off) it runs hub.audit and announces each new finding as a
//! hub.event (audit.duplicate_station, audit.duplicate_depot, audit.over_capacity) and each one
//! that went away as audit.resolved. Every event the hub raises itself (those, and the game.* it
//! derives for an MCP server without a panel) also goes out in the prismatic-bus/v1 envelope on a
//! ZeroMQ PUB socket of its own (tcp://127.0.0.1:<--bus-port, default an ephemeral port>, GET
//! /api/bus) and into a DuckDB store of its own ($PRISMATIC_STORE_DIR/hub-<port>.duckdb, GET
//! /api/db/query; --no-store leaves it out). Relayed panel events stay in their panel's store.
//! check: connects to a hub, calls every read-only operation of every instance that needs no
//! required input, and waits for one streamed event; exits 1 on any failure.
//! intent-check: sends each sentence (the owner's own examples by default, Czech with and without
//! diacritics and English) to every instance whose catalogue has an operation on a path ending in
//! /intent, and passes when each answer names an action and its candidates (and, to build, a plan).
//! audit: asks the hub for every instance's duplicate stations (one owner, a shared facility,
//! overlapping catchments); exits 1 when any instance has one.
use serde_json::{json, Value};
use std::net::{TcpListener, TcpStream};
use std::path::PathBuf;
use std::time::{Duration, Instant};

#[path = "../bus_sinks.rs"]
#[allow(dead_code)]
mod bus_sinks;
#[path = "../hub.rs"]
mod hub;
#[path = "../panel_events.rs"]
#[allow(dead_code)]
mod panel_events;

fn arg(args: &[String], name: &str) -> Option<String> {
    args.iter().position(|a| a == name).and_then(|i| args.get(i + 1).cloned())
}

/// The first flag in `args` that `known` does not list (values after a valued flag are skipped).
fn unknown_flag<'a>(args: &'a [String], known: &[&str], valued: &[&str]) -> Option<&'a str> {
    let mut skip = false;
    for a in args {
        if std::mem::take(&mut skip) {
            continue;
        }
        if a.starts_with("--") && !known.contains(&a.as_str()) {
            return Some(a);
        }
        skip = valued.contains(&a.as_str());
    }
    None
}

fn default_registry() -> PathBuf {
    std::env::var_os("PRISMATIC_MCP_REGISTRY").map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(std::env::var("HOME").unwrap_or_default()).join(".local/state/prismatic-mcp/instances")
    })
}

/// The panel's pages: under `MCP_PANEL_DIR` when it is set, as the panel reads them (I0094).
fn default_static() -> PathBuf {
    match std::env::var_os("MCP_PANEL_DIR").filter(|v| !v.is_empty()) {
        Some(v) => PathBuf::from(v).join("static"),
        None => PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../mcp-panel/static"),
    }
}

type Ws = tungstenite::WebSocket<TcpStream>;

fn connect(url: &str) -> Result<Ws, String> {
    let addr = url.trim_start_matches("ws://").split('/').next().unwrap_or("").to_string();
    let stream = TcpStream::connect(&addr).map_err(|e| format!("{addr}: {e}"))?;
    let (ws, _) = tungstenite::client(url, stream).map_err(|e| e.to_string())?;
    ws.get_ref().set_read_timeout(Some(Duration::from_millis(200))).ok();
    Ok(ws)
}

/// Send one request and wait for its answer, collecting any notifications that arrive meanwhile.
fn request(ws: &mut Ws, id: i64, method: &str, params: Value, notes: &mut Vec<Value>) -> Result<Value, String> {
    let msg = json!({"jsonrpc": "2.0", "id": id, "method": method, "params": params});
    ws.send(tungstenite::Message::text(msg.to_string())).map_err(|e| e.to_string())?;
    let deadline = Instant::now() + Duration::from_secs(30);
    while Instant::now() < deadline {
        match ws.read() {
            Ok(tungstenite::Message::Text(t)) => {
                let v: Value = serde_json::from_str(t.as_str()).map_err(|e| e.to_string())?;
                if v.get("id") == Some(&json!(id)) { return Ok(v); }
                notes.push(v);
            }
            Ok(_) => {}
            Err(tungstenite::Error::Io(e)) if matches!(e.kind(), std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut) => {}
            Err(e) => return Err(e.to_string()),
        }
    }
    Err(format!("{method}: no answer in 30 s"))
}

/// An answer that counts as a failure of the operation itself (a 503 for "no GUI client attached"
/// on a headless stack is the route saying it does not apply, and is counted as skipped).
fn verdict(reply: &Value) -> &'static str {
    if reply.get("error").is_some() { return "fail"; }
    match reply.pointer("/result/http_error").and_then(Value::as_i64) {
        None => "pass",
        Some(503) => "skip",
        Some(_) => "fail",
    }
}

fn check(args: &[String]) -> i32 {
    let url = arg(args, "--hub").unwrap_or_else(|| "ws://127.0.0.1:8790/ws".into());
    let wait: u64 = arg(args, "--event-wait").and_then(|s| s.parse().ok()).unwrap_or(120);
    let mut ws = match connect(&url) { Ok(w) => w, Err(e) => { eprintln!("FAIL connect {url}: {e}"); return 1; } };
    let mut notes = Vec::new();
    let mut id = 0;
    let mut next = || { id += 1; id };
    let instances = request(&mut ws, next(), "hub.instances", json!({}), &mut notes).unwrap_or_default();
    let list = instances["result"].as_array().cloned().unwrap_or_default();
    println!("instances: {}", list.len());
    if list.is_empty() { eprintln!("FAIL no instance"); return 1; }
    let ops = request(&mut ws, next(), "hub.catalogue", json!({}), &mut notes).unwrap_or_default();
    let ops = ops["result"].as_array().cloned().unwrap_or_default();
    let (mut pass, mut skip, mut fail) = (0, 0, 0);
    for op in &ops {
        let required_query = op["query"].as_array().map_or(false, |q| q.iter().any(|p| p["required"] == json!(true)));
        let required_body = op["schema"]["required"].as_array().map_or(false, |r| !r.is_empty());
        // A templated path (/play/{file}) needs an input the check has none of, like a required query.
        let templated_path = op["path"].as_str().map_or(false, |p| p.contains('{'));
        if op["read_only"] != json!(true) || op["stream"] == json!(true) || required_query || required_body || templated_path { continue; }
        let params = json!({"instance": op["instance"], "id": op["id"]});
        let r = request(&mut ws, next(), "hub.call", params, &mut notes).unwrap_or_else(|e| json!({"error": e}));
        match verdict(&r) {
            "pass" => pass += 1,
            "skip" => skip += 1,
            _ => { fail += 1; println!("FAIL {} {} {}", op["instance"], op["id"], r.get("error").or(r.pointer("/result/http_error")).unwrap_or(&Value::Null)); }
        }
    }
    println!("calls: {} operations listed; read-only calls {pass} pass, {skip} skip (503), {fail} fail", ops.len());
    let _ = request(&mut ws, next(), "hub.subscribe", json!({}), &mut notes);
    let deadline = Instant::now() + Duration::from_secs(wait);
    let mut events = notes.iter().filter(|n| n["method"] == "hub.event").count();
    while events == 0 && Instant::now() < deadline {
        if let Ok(tungstenite::Message::Text(t)) = ws.read() {
            if serde_json::from_str::<Value>(t.as_str()).map(|v| v["method"] == "hub.event").unwrap_or(false) { events += 1; }
        }
    }
    println!("stream: {events} event(s) within {wait} s");
    if events == 0 { println!("FAIL no event streamed"); }
    i32::from(fail > 0 || events == 0)
}

fn audit_cmd(args: &[String]) -> i32 {
    let url = arg(args, "--hub").unwrap_or_else(|| "ws://127.0.0.1:8790/ws".into());
    let mut ws = match connect(&url) { Ok(w) => w, Err(e) => { eprintln!("FAIL connect {url}: {e}"); return 2; } };
    let r = match request(&mut ws, 1, "hub.audit", json!({}), &mut Vec::new()) { Ok(r) => r, Err(e) => { eprintln!("FAIL {e}"); return 2; } };
    let list = r["result"].as_array().cloned().unwrap_or_default();
    if args.iter().any(|a| a == "--json") { println!("{}", serde_json::to_string_pretty(&r["result"]).unwrap_or_default()); }
    let mut total = 0;
    for inst in &list {
        let dups = inst["duplicates"].as_array().cloned().unwrap_or_default();
        total += dups.len();
        if let Some(e) = inst.get("error") { println!("SKIP {} {}", inst["instance"], e); continue; }
        println!("{} {}: {} station(s), {} duplicate pair(s), catchment from the {}", if dups.is_empty() { "PASS" } else { "FAIL" },
                 inst["instance"], inst["stations"], dups.len(), inst["catchment"].as_str().unwrap_or("?"));
        for d in dups {
            println!("     owner {} {}: {} ({},{}) and {} ({},{}), {} tile(s) apart", d["owner"], d["facilities"], d["a"]["name"], d["a"]["x"], d["a"]["y"],
                     d["b"]["name"], d["b"]["x"], d["b"]["y"], d["distance"]);
        }
        let cap = &inst["capacity"];
        if let Some(over) = cap["over"].as_array() {
            println!("     capacity: {} {} link(s), {} vehicle(s), {} route(s) over capacity (advisory)", if over.is_empty() { "PASS" } else { "WARN" },
                     cap["links"], cap["vehicles"], over.len());
            for o in over {
                println!("       {} - {} cargo {}: {} vehicles, capacity {} usage {} -> {} too many", o["a"]["name"], o["b"]["name"], o["cargo"],
                         o["vehicles"], o["capacity"], o["usage"], o["too_many"]);
            }
        }
        let dep = &inst["depots"];
        if let Some(e) = dep.get("error") { println!("     depots: SKIP {e}"); continue; }
        let ddups = dep["duplicates"].as_array().cloned().unwrap_or_default();
        total += ddups.len();
        println!("     depots: {} {} depot(s), {} duplicate pair(s), reach {} tile(s), {}", if ddups.is_empty() { "PASS" } else { "FAIL" },
                 dep["depots"], ddups.len(), dep["reach"], if dep["network"] == json!(true) { "along the road network" } else { "straight distance only" });
        for d in ddups {
            println!("       owner {} {} depots {} ({},{}) and {} ({},{}), {} tile(s) by {}", d["owner"], d["type"], d["a"]["id"], d["a"]["x"], d["a"]["y"],
                     d["b"]["id"], d["b"]["x"], d["b"]["y"], d["distance"], d["by"]);
        }
    }
    println!("audit: {} instance(s), {total} duplicate pair(s) (stations and depots)", list.len());
    i32::from(total > 0)
}

/// The owner's own example requests (2026-11-15), in Czech with and without diacritics and English.
const OWNER_SENTENCES: [&str; 6] = [
    "najdi nejproduktivnější les", "najdi nejbližší les + pilu", "vystav trať mezi nejproduktivnějším lesem a pilou",
    "vybuduj nejlukrativnější trať", "najdi nejblizsi les + pilu", "find the nearest coal mine and power station",
];

/// Does an intent answer carry what the request asked for? An action, candidates, and to build, a plan.
fn intent_verdict(answer: &Value) -> Result<String, String> {
    let r = answer.get("result").ok_or_else(|| format!("error {}", answer.get("error").unwrap_or(&Value::Null)))?;
    if let Some(code) = r.get("http_error") { return Err(format!("HTTP {code}: {}", r["body"])); }
    let action = r.pointer("/intent/action").and_then(Value::as_str).ok_or("no intent.action")?;
    let n = r.pointer("/answer/candidates").and_then(Value::as_array).map(Vec::len).unwrap_or(0);
    if n == 0 { return Err(format!("{action}: no candidates")); }
    if action == "build" && r.pointer("/answer/build").is_none() { return Err("build: no plan in the answer".into()); }
    let subjects: Vec<String> = r.pointer("/intent/subjects").and_then(Value::as_array).into_iter().flatten()
        .map(|s| s.get("name").and_then(Value::as_str).map(str::to_string).unwrap_or_else(|| s.to_string())).collect();
    // Which language the sentence was read in, and why (openttd-80's language_from), when the answer says.
    let lang = match (r.pointer("/intent/language").and_then(Value::as_str), r.pointer("/intent/language_from").and_then(Value::as_str)) {
        (Some(l), Some(from)) => format!(", read as {l} ({from})"),
        (Some(l), None) => format!(", read as {l}"),
        _ => String::new(),
    };
    Ok(format!("{action} {} [{}] -> {n} candidate(s){lang}", r.pointer("/intent/ordering").and_then(Value::as_str).unwrap_or("-"), subjects.join(", ")))
}

/// How many subjects an intent answer resolved (0 when it carries none).
fn subjects_resolved(answer: &Value) -> usize {
    answer.pointer("/result/intent/subjects").and_then(Value::as_array).map(Vec::len).unwrap_or(0)
}

/// The answers that resolved no subject for a sentence another instance did resolve subjects for:
/// (instance, sentence, the instance that resolved it). Derived across the instances, nothing named
/// here: a sentence that names nothing ("the most lucrative line") resolves nothing anywhere and is
/// never flagged. Advisory, since a game may lack the industry, but its answer is a fallback.
fn unresolved_elsewhere_resolved(results: &[(String, String, usize)]) -> Vec<(String, String, String)> {
    let mut out = Vec::new();
    for (inst, s, n) in results {
        if *n > 0 { continue; }
        if let Some((other, _, _)) = results.iter().find(|(o, s2, n2)| s2 == s && *n2 > 0 && o != inst) {
            out.push((inst.clone(), s.clone(), other.clone()));
        }
    }
    out
}

fn intent_check(args: &[String]) -> i32 {
    let url = arg(args, "--hub").unwrap_or_else(|| "ws://127.0.0.1:8790/ws".into());
    let given: Vec<String> = args.iter().enumerate().filter(|(i, a)| !a.starts_with("--") && (*i == 0 || args[i - 1] != "--hub")).map(|(_, a)| a.clone()).collect();
    let sentences: Vec<String> = if given.is_empty() { OWNER_SENTENCES.iter().map(|s| s.to_string()).collect() } else { given };
    let mut ws = match connect(&url) { Ok(w) => w, Err(e) => { eprintln!("FAIL connect {url}: {e}"); return 2; } };
    let mut notes = Vec::new();
    let ops = request(&mut ws, 1, "hub.catalogue", json!({}), &mut notes).unwrap_or_default();
    let targets: Vec<(String, String)> = ops["result"].as_array().into_iter().flatten()
        .filter(|o| o["path"].as_str().map_or(false, |p| p.ends_with("/intent")))
        .map(|o| (o["instance"].as_str().unwrap_or("").to_string(), o["id"].as_str().unwrap_or("").to_string())).collect();
    if targets.is_empty() { println!("FAIL no instance offers an /intent operation"); return 1; }
    let (mut pass, mut fail) = (0, 0);
    let mut resolved: Vec<(String, String, usize)> = Vec::new();
    let mut id = 10;
    for (inst, op) in &targets {
        for s in &sentences {
            id += 1;
            let r = request(&mut ws, id, "hub.call", json!({"instance": inst, "id": op, "body": {"text": s}}), &mut notes).unwrap_or_else(|e| json!({"error": e}));
            match intent_verdict(&r) {
                Ok(v) => { pass += 1; resolved.push((inst.clone(), s.clone(), subjects_resolved(&r))); println!("PASS {inst} {s:?}: {v}"); }
                Err(e) => { fail += 1; println!("FAIL {inst} {s:?}: {e}"); }
            }
        }
    }
    let fallbacks = unresolved_elsewhere_resolved(&resolved);
    for (inst, s, other) in &fallbacks {
        println!("WARN {inst} {s:?}: no subject resolved, while {other} resolved them: a fallback answer (advisory)");
    }
    println!("intent: {} instance(s), {pass} pass, {fail} fail, {} fallback(s) (advisory)", targets.len(), fallbacks.len());
    i32::from(fail > 0)
}

/// The hub's own bus and store: `hub-<port>`, the PUB socket on --bus-port (0, the default, an
/// ephemeral port), the store in $PRISMATIC_STORE_DIR unless --no-store.
fn sink_config(args: &[String], port: u16) -> hub::SinkConfig {
    let name = format!("hub-{port}");
    let store = (!args.iter().any(|a| a == "--no-store")).then(|| bus_sinks::store_dir().join(format!("{name}.duckdb")));
    hub::SinkConfig { name, bus_port: arg(args, "--bus-port").and_then(|p| p.parse().ok()).unwrap_or(0), store }
}

fn serve(args: &[String]) -> i32 {
    let port = arg(args, "--port").unwrap_or_else(|| "8790".into());
    let listener = match TcpListener::bind(format!("127.0.0.1:{port}")) {
        Ok(l) => l,
        Err(e) => { eprintln!("mcp-hub: cannot bind 127.0.0.1:{port}: {e}"); return 1; }
    };
    let panels: Vec<String> = args.iter().enumerate().filter(|(_, a)| *a == "--panel").filter_map(|(i, _)| args.get(i + 1).cloned()).collect();
    let cfg = hub::Config {
        registry: arg(args, "--registry").map(PathBuf::from).unwrap_or_else(default_registry),
        panels,
        statics: arg(args, "--static").map(PathBuf::from).unwrap_or_else(default_static),
        refresh: Duration::from_secs(2),
        poll: Duration::from_millis(500),
        audit: Some(Duration::from_secs(arg(args, "--audit-every").and_then(|v| v.parse().ok()).unwrap_or(60))).filter(|d| !d.is_zero()),
        sinks: Some(sink_config(args, listener.local_addr().map(|a| a.port()).unwrap_or(0))),
    };
    let addr = listener.local_addr().map(|a| a.to_string()).unwrap_or_default();
    let state = hub::run(listener, cfg);
    {
        let s = state.lock().unwrap();
        println!("mcp-hub: http://{addr}/  ws://{addr}/ws  ({} instance(s), {} operation(s))",
                 s.instances.len(), s.ops.values().map(Vec::len).sum::<usize>());
        if let Some(k) = &s.sinks {
            println!("mcp-hub: bus {}  store {}", k.endpoint.as_deref().unwrap_or("off"), k.store_status().as_str().unwrap_or("off"));
            for e in [&k.bus_error, &k.store_error].into_iter().flatten() {
                println!("mcp-hub: {e}");
            }
        }
    }
    loop { std::thread::sleep(Duration::from_secs(3600)); }
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    // A typo must not start a server that runs forever: every flag is one the command reads.
    let (cmd, rest) = match args.first().map(String::as_str) {
        Some(c @ ("check" | "audit" | "intent-check" | "serve")) => (c, &args[1..]),
        Some(c) if !c.starts_with('-') => {
            eprintln!("mcp-hub: unknown command '{c}' (check, audit, intent-check, serve; -h for usage)");
            std::process::exit(2);
        }
        _ => ("serve", &args[..]),
    };
    let (known, valued): (&[&str], &[&str]) = match cmd {
        "check" => (&["--hub", "--event-wait"], &["--hub", "--event-wait"]),
        "audit" => (&["--hub", "--json"], &["--hub"]),
        "intent-check" => (&["--hub"], &["--hub"]),
        _ => (&["--port", "--panel", "--registry", "--static", "--audit-every", "--bus-port", "--no-store", "--help"],
              &["--port", "--panel", "--registry", "--static", "--audit-every", "--bus-port"]),
    };
    if let Some(f) = unknown_flag(rest, known, valued) {
        eprintln!("mcp-hub {cmd}: unknown option {f} (-h for usage)");
        std::process::exit(2);
    }
    let code = match args.first().map(String::as_str) {
        Some("check") => check(&args[1..]),
        Some("audit") => audit_cmd(&args[1..]),
        Some("intent-check") => intent_check(&args[1..]),
        Some("serve") => serve(&args[1..]),
        Some("-h" | "--help") => {
            let doc: Vec<&str> = include_str!("mcp-hub.rs").lines().take_while(|l| l.starts_with("//!"))
                .map(|l| l.trim_start_matches("//!").trim_start()).collect();
            println!("{}", doc.join("\n"));
            0
        }
        _ => serve(&args),
    };
    std::process::exit(code);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Read, Write};
    use std::sync::{Arc, Mutex};

    fn spec() -> Value {
        json!({"paths": {
            "/api/status": {"get": {"operationId": "status", "summary": "Status", "tags": ["game"]}},
            "/api/echo": {"post": {"operationId": "echo", "x-mcp-annotations": {"readOnlyHint": true},
                "requestBody": {"content": {"application/json": {"schema": {"type": "object", "properties": {"say": {"type": "string"}}}}}}}},
            "/api/pause": {"post": {"operationId": "pause", "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["pause"]}}}}}},
            "/api/events": {"get": {"operationId": "events", "parameters": [{"in": "query", "name": "since"}, {"in": "query", "name": "kinds", "required": false}]}},
            "/api/events/stream": {"get": {"operationId": "eventStream", "responses": {"200": {"content": {"text/event-stream": {}}}}}},
            "/api/resources/stations": {"get": {"operationId": "resource_stations"}}
        }})
    }

    #[test]
    fn openapi_operations_are_derived_with_read_only_schema_query_and_stream() {
        let ops = hub::ops_from_openapi("p", &spec());
        let by = |id: &str| ops.iter().find(|o| o.id == id).unwrap().clone();
        assert_eq!(ops.len(), 6);
        assert!(by("status").read_only && by("status").method == "GET");
        assert!(by("echo").read_only, "readOnlyHint wins over POST");
        assert!(!by("pause").read_only);
        assert_eq!(by("pause").schema.unwrap()["required"], json!(["pause"]));
        assert_eq!(by("events").query, vec![("since".to_string(), false), ("kinds".to_string(), false)]);
        assert!(by("eventStream").stream);
    }

    #[test]
    fn mcp_tools_and_resources_become_operations() {
        let tools = json!({"tools": [{"name": "openttd.view.zoom", "inputSchema": {"type": "object"}, "annotations": {"readOnlyHint": false}}]});
        let res = json!({"resources": [{"uri": "openttd://map", "name": "map"}]});
        let ops = hub::ops_from_mcp("gui", &tools, &res);
        assert_eq!(ops[0].id, "tool:openttd.view.zoom");
        assert_eq!(ops[1].id, "resource:openttd://map");
        assert!(ops[1].read_only && !ops[0].read_only);
    }

    #[test]
    fn registry_records_resolve_to_instances_and_dead_owners_drop() {
        let rec = json!({"name": "panel-1", "kind": "panel", "owner_pid": 7, "url": "http://127.0.0.1:1/"});
        let i = hub::instance_from_record(&rec, &|_| true).unwrap();
        assert_eq!(i.access, hub::Access::Panel { base: "http://127.0.0.1:1".into() });
        assert!(hub::instance_from_record(&rec, &|_| false).is_none());
        let dir = std::env::temp_dir().join(format!("hubtok-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let tf = dir.join("t.json");
        std::fs::write(&tf, r#"{"agent_token":"abc","endpoint":"http://127.0.0.1:2/mcp"}"#).unwrap();
        let rec = json!({"name": "gui-2", "kind": "gui", "token_file": tf.to_str().unwrap()});
        let i = hub::instance_from_record(&rec, &|_| true).unwrap();
        assert_eq!(i.access, hub::Access::Mcp { endpoint: "http://127.0.0.1:2/mcp".into(), token: "abc".into() });
    }

    #[test]
    fn filters_match_instances_and_kind_prefixes() {
        let f = hub::Filter::from_params(&json!({"instances": ["a"], "kinds": ["game"]}));
        assert!(f.matches("a", &json!({"kind": "game.month"})));
        assert!(!f.matches("a", &json!({"kind": "gamer"})));
        assert!(!f.matches("b", &json!({"kind": "game.month"})));
        assert!(hub::Filter::default().matches("x", &json!({"kind": "any"})));
    }

    #[test]
    fn verdicts_count_503_as_skipped() {
        assert_eq!(verdict(&json!({"result": {"ok": true}})), "pass");
        assert_eq!(verdict(&json!({"result": {"http_error": 503}})), "skip");
        assert_eq!(verdict(&json!({"result": {"http_error": 500}})), "fail");
        assert_eq!(verdict(&json!({"error": {"code": -1}})), "fail");
    }

    fn st(id: i64, name: &str, x: i64, y: i64, owner: i64, fac: &[&str]) -> Value {
        json!({"id": id, "name": name, "x": x, "y": y, "owner": owner, "facilities": fac, "town": 1})
    }

    #[test]
    fn duplicates_are_one_owner_one_facility_overlapping_catchments() {
        // The pairs observed live on 2026-11-16 (fallback radius: bus 3, airport 4, train 4).
        let s = vec![
            st(3, "Chindingtown", 74, 224, 0, &["bus"]), st(7, "Chindingtown Central", 73, 226, 0, &["bus"]),
            st(4, "Carnford", 209, 72, 0, &["bus"]), st(5, "Carnford Central", 215, 72, 0, &["bus"]),
            st(0, "Rindingbury", 107, 200, 0, &["bus"]), st(1, "Rindingbury West", 117, 199, 0, &["bus"]),
            st(2, "Pardham", 66, 238, 0, &["bus"]), st(9, "Pardham Central", 61, 236, 0, &["airport"]),
            st(10, "Rival", 75, 225, 1, &["bus"]),
            // Two oil rigs' own stations, owner 16 (nobody), seed 24455: the map, not a build.
            st(20, "Gardston Oilfield", 12, 228, 16, &["airport", "dock"]), st(21, "Nontown Oilfield", 14, 235, 16, &["airport", "dock"]),
        ];
        let d = hub::duplicate_stations(&s);
        let names: Vec<(String, String)> = d.iter().map(|x| (x["a"]["name"].as_str().unwrap().into(), x["b"]["name"].as_str().unwrap().into())).collect();
        assert_eq!(names, vec![("Chindingtown".to_string(), "Chindingtown Central".to_string()), ("Carnford".to_string(), "Carnford Central".to_string())],
                   "2 apart and 6 apart overlap (3+3); 10 apart does not; bus beside an airport, another owner's stop and the game's own oil rigs are not duplicates");
        assert_eq!(hub::company_owner(&st(1, "Town", 0, 0, 15, &["bus"])), None, "15 is a town");
        assert_eq!(hub::company_owner(&st(1, "A", 0, 0, 14, &["bus"])), Some(14));
        assert_eq!(d[0]["facilities"], json!(["bus"]));
        assert_eq!(d[1]["distance"], json!(6));
    }

    #[test]
    fn the_games_own_catchment_wins_over_the_fallback() {
        let mut a = st(1, "A", 10, 10, 0, &["bus"]);
        let mut b = st(2, "B", 15, 10, 0, &["bus"]);
        assert!(hub::duplicate_stations(&[a.clone(), b.clone()]).len() == 1, "fallback 3+3 overlaps at 5 apart");
        a["catchment_area"] = json!({"x": 9, "y": 9, "w": 3, "h": 3});
        b["catchment_area"] = json!({"x": 14, "y": 9, "w": 3, "h": 3});
        assert!(hub::duplicate_stations(&[a.clone(), b.clone()]).is_empty(), "the game says radius 1: no overlap");
        b["catchment_radius"] = json!(1);
        assert_eq!(hub::catchment(&b), Some((14, 9, 16, 11)));
    }

    #[test]
    fn station_lists_are_read_from_every_answer_shape() {
        let list = json!({"stations": [st(1, "A", 1, 1, 0, &["bus"])]});
        assert_eq!(hub::stations_in(&list).len(), 1);
        assert_eq!(hub::stations_in(&json!({"availability": "available", "data": list.clone()})).len(), 1);
        let mcp = json!({"contents": [{"text": json!({"data": list}).to_string()}]});
        assert_eq!(hub::stations_in(&mcp).len(), 1);
        let ops = hub::ops_from_openapi("p", &json!({"paths": {"/api/resources/stations": {"get": {"operationId": "resource_stations"}}, "/api/status": {"get": {}}}}));
        assert_eq!(hub::stations_op(&ops).unwrap().id, "resource_stations");
        let mops = hub::ops_from_mcp("g", &json!({"tools": []}), &json!({"resources": [{"uri": "openttd://stations"}]}));
        assert_eq!(hub::stations_op(&mops).unwrap().id, "resource:openttd://stations");
    }

    /// A ways payload: `rows` of hex digits, one per tile (NW=1, SW=2, SE=4, NE=8).
    fn ways(rows: &[&str]) -> hub::Roads {
        hub::Roads::from_ways(&json!({"data": {"width": rows[0].len(), "roads": rows}})).unwrap()
    }

    #[test]
    fn road_distance_follows_only_joined_tiles() {
        // A straight east-west road (SW=2 to x+1, NE=8 to x-1) of five tiles; the middle tile joins both.
        let r = ways(&["2aaa8", "00000"]);
        assert_eq!(r.distance((0, 0), (4, 0), 20), Some(4));
        assert_eq!(r.distance((0, 0), (4, 0), 3), None, "beyond the limit");
        // A piece that points east, beside one that does not point back, is not joined.
        let r = ways(&["2280", "0000"]);
        assert_eq!(r.distance((0, 0), (3, 0), 20), None);
    }

    #[test]
    fn duplicate_depots_are_one_owner_one_type_within_the_service_reach() {
        let d = |id: i64, x: i64, owner: i64, t: &str| json!({"id": id, "x": x, "y": 0, "tile": x, "owner": owner, "type": t, "town": 1});
        let r = ways(&["2aaaaaaaa8"]); // ten joined road tiles
        let depots = vec![d(1, 0, 0, "road"), d(2, 3, 0, "road"), d(3, 9, 1, "road"), d(4, 9, 0, "rail")];
        let found = hub::duplicate_depots(&depots, 20, Some(&r));
        assert_eq!(found.len(), 1, "depots 1 and 2 share a network 3 steps apart; another owner's and a rail depot do not count");
        assert_eq!(found[0]["by"], json!("road network"));
        assert_eq!(found[0]["distance"], json!(3));
        // Two road depots close together but on different, unjoined roads serve different vehicles.
        let split = ways(&["28028"]);
        assert!(hub::duplicate_depots(&[d(1, 0, 0, "road"), d(2, 3, 0, "road")], 20, Some(&split)).is_empty());
        // Without ways the straight distance is used, and the report says so.
        let rail = hub::duplicate_depots(&[d(5, 0, 0, "rail"), d(6, 4, 0, "rail")], 20, None);
        assert_eq!(rail[0]["by"], json!("straight distance"));
        assert!(hub::duplicate_depots(&[d(5, 0, 0, "rail"), d(6, 40, 0, "rail")], 20, None).is_empty(), "beyond the reach");
    }
    #[test]
    fn graphql_parses_aliases_arguments_variables_and_projects_selections() {
        use hub::graphql::{parse, project, execute, sdl};
        let vars = serde_json::Map::from_iter([("who".to_string(), json!("panel-1"))]);
        let op = parse(r#"query Q($who: String) { first: catalogue(instance: $who) { id method } audit }"#, &vars).unwrap();
        assert_eq!(op.kind, "query");
        assert_eq!(op.selection[0].key, "first");
        assert_eq!(op.selection[0].name, "catalogue");
        assert_eq!(op.selection[0].args["instance"], json!("panel-1"));
        let v = json!([{"id": "a", "method": "GET", "path": "/x"}, {"id": "b", "method": "POST", "path": "/y"}]);
        assert_eq!(project(&v, &op.selection[0].selection), json!([{"id": "a", "method": "GET"}, {"id": "b", "method": "POST"}]));
        let lit = parse(r#"mutation { call(instance: "p", id: "pause", body: {pause: true, n: [1, -2.5]}) }"#, &serde_json::Map::new()).unwrap();
        assert_eq!(lit.selection[0].args["body"], json!({"pause": true, "n": [1, -2.5]}));
        for bad in ["{ ...F }", "{ __schema { types { name } } }", "subscription { events }", "{ a @skip(if: true) }"] {
            assert!(parse(bad, &serde_json::Map::new()).is_err(), "{bad} must be refused by name");
        }
        // Execution goes through the hub's own dispatch; a Query field cannot run a mutation.
        let methods = hub::methods();
        let rpc = |m: &str, p: Value| -> Result<Value, String> { Ok(json!({"method": m, "params": p, "list": [{"x": 1, "y": 2}]})) };
        let out = execute(&json!({"query": "{ instances { method list { x } } }"}), &methods, &rpc);
        assert_eq!(out, json!({"data": {"instances": {"method": "hub.instances", "list": [{"x": 1}]}}}));
        let out = execute(&json!({"query": "{ call(instance: \"p\", id: \"x\") }"}), &methods, &rpc);
        assert!(out["errors"][0]["message"].as_str().unwrap().contains("Mutation"));
        let out = execute(&json!({"query": "{ nope }"}), &methods, &rpc);
        assert_eq!(out["data"]["nope"], Value::Null);
        let schema = sdl(&methods);
        assert!(schema.contains("audit: JSON") && schema.contains("type Mutation") && schema.contains("call("), "{schema}");
        assert!(!schema.contains("subscribe"), "streams stay on the WebSocket");
    }
    #[test]
    fn over_capacity_counts_vehicles_beyond_what_mature_traffic_needs() {
        let v = |id: i64, cap: i64, stops: &[i64]| json!({"id": id, "cargo_capacity": cap, "orders": stops.iter().map(|s| json!({"type": "goto_station", "destination": s})).collect::<Vec<_>>()});
        let link = |a: i64, b: i64, cap: i64, used: i64| json!({"kind": "cargo_link", "from": format!("station:{a}"), "to": format!("station:{b}"), "cargo": 0, "capacity": cap, "usage": used});
        let four = vec![v(1, 31, &[1, 2]), v(2, 31, &[1, 2]), v(3, 31, &[1, 2]), v(4, 31, &[1, 2])];
        let graph = |c1: i64, u1: i64, c2: i64, u2: i64| json!({"nodes": [{"id": "station:1", "name": "A"}, {"id": "station:2", "name": "B"}],
            "edges": [link(1, 2, c1, u1), link(2, 1, c2, u2)]});
        // Mature (all four ran it: 124 each way), carrying 20 and 40: one and two buses needed -> 2 too many.
        let over = hub::over_capacity(&graph(124, 20, 124, 40), &four);
        assert_eq!(over.len(), 1);
        assert_eq!((over[0]["needed"].clone(), over[0]["too_many"].clone()), (json!(2), json!(2)));
        // Not mature (only one bus has run it, the live case of 2026-11-16): nothing is said.
        assert!(hub::over_capacity(&graph(31, 0, 31, 0), &four).is_empty());
        // Full: all four needed.
        assert!(hub::over_capacity(&graph(124, 120, 124, 110), &four).is_empty());
        // A single vehicle is the route, never too many.
        assert!(hub::over_capacity(&graph(31, 0, 31, 0), &[v(1, 31, &[1, 2])]).is_empty());
        // A train's engine shows no capacity: the link's capacity / n is used.
        let trains = vec![v(1, 0, &[1, 2]), v(2, 0, &[1, 2])];
        let t = hub::over_capacity(&graph(200, 10, 200, 10), &trains);
        assert_eq!(t[0]["too_many"], json!(1));
    }

    #[test]
    fn a_catalogue_read_before_the_game_was_up_is_read_again() {
        use std::sync::atomic::{AtomicBool, Ordering};
        let up = Arc::new(AtomicBool::new(false));
        let l = TcpListener::bind("127.0.0.1:0").unwrap();
        let base = format!("http://{}", l.local_addr().unwrap());
        let flag = up.clone();
        std::thread::spawn(move || for mut c in l.incoming().flatten() {
            let mut buf = [0u8; 4096];
            let _ = c.read(&mut buf);
            let mut paths = json!({"/api/status": {"get": {"operationId": "status"}}});
            if flag.load(Ordering::SeqCst) { paths["/api/resources/stations"] = json!({"get": {"operationId": "resource_stations"}}); }
            let b = json!({"paths": paths}).to_string();
            let _ = write!(c, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{b}", b.len());
        });
        let empty = std::env::temp_dir().join(format!("hubreg-ttl-{}", std::process::id()));
        std::fs::create_dir_all(&empty).unwrap();
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let state = hub::run(listener, hub::Config { registry: empty, panels: vec![base], statics: default_static(),
                                                  refresh: Duration::from_millis(40), poll: Duration::from_secs(60), audit: None, sinks: None });
        let count = |s: &hub::Shared| s.lock().unwrap().ops.values().map(Vec::len).sum::<usize>();
        assert_eq!(count(&state), 1, "before the game is up: only the status route");
        up.store(true, Ordering::SeqCst);
        let deadline = Instant::now() + Duration::from_secs(5);
        while count(&state) < 2 && Instant::now() < deadline { std::thread::sleep(Duration::from_millis(50)); }
        assert_eq!(count(&state), 2, "the catalogue is read again within its time to live");
    }
    #[test]
    fn the_audit_watch_announces_findings_once_and_their_resolution() {
        let r = |pairs: &[(i64, i64)]| json!([{"instance": "p", "duplicates": pairs.iter().map(|(a, b)| json!({"a": {"id": a}, "b": {"id": b}, "distance": 2})).collect::<Vec<_>>(),
            "depots": {"duplicates": []}, "capacity": {"over": [{"a": {"id": 1}, "b": {"id": 2}, "cargo": 0, "too_many": 1}]}}]);
        let first = hub::audit_findings(&r(&[(3, 7)]));
        assert_eq!(first.iter().map(|f| f.2["kind"].as_str().unwrap()).collect::<Vec<_>>(), vec!["audit.duplicate_station", "audit.over_capacity"]);
        let (new, gone) = hub::audit_changes(&Default::default(), &first);
        assert_eq!((new.len(), gone.len()), (2, 0));
        let seen: std::collections::BTreeMap<_, _> = first.iter().cloned().map(|(k, i, e)| (k, (i, e))).collect();
        // The same findings with other numbers are not news.
        let (new, gone) = hub::audit_changes(&seen, &hub::audit_findings(&r(&[(3, 7)])));
        assert_eq!((new.len(), gone.len()), (0, 0));
        // The duplicate repaired: one resolution naming what it was.
        let (new, gone) = hub::audit_changes(&seen, &hub::audit_findings(&r(&[])));
        assert_eq!((new.len(), gone.len()), (0, 1));
        assert_eq!(gone[0].1["kind"], json!("audit.resolved"));
        assert_eq!(gone[0].1["data"]["was"], json!("audit.duplicate_station"));
    }
    #[test]
    fn a_game_without_a_panel_streams_events_derived_from_its_state() {
        let st = |date: &str, tick: i64, paused: bool, vehicles: i64| json!({"game_date": date, "game_tick": tick,
            "data": {"paused": paused, "num_vehicles": vehicles, "num_companies": 1}});
        assert!(hub::derive_state_events(None, &st("1950-01-01", 10, false, 0)).is_empty(), "the first read is no event");
        let kinds = |a: Value, b: Value| hub::derive_state_events(Some(&a), &b).iter().map(|e| e["kind"].as_str().unwrap().to_string()).collect::<Vec<_>>();
        assert_eq!(kinds(st("1950-01-30", 100, false, 2), st("1950-02-01", 200, true, 3)), vec!["game.month", "game.paused", "vehicles.count"]);
        assert_eq!(kinds(st("1950-02-01", 200, true, 3), st("1950-02-02", 300, false, 3)), vec!["game.resumed"]);
        assert_eq!(kinds(st("1950-02-02", 300, false, 3), st("1950-02-02", 300, false, 3)), Vec::<String>::new());
        assert_eq!(kinds(st("1955-06-01", 9000, false, 40), st("1950-01-01", 5, false, 0)), vec!["game.new_world"], "a tick counter that went back is a new world, and only that");
        let e = &hub::derive_state_events(Some(&st("1950-01-30", 1, false, 0)), &st("1950-02-01", 2, false, 0))[0];
        assert_eq!(e["derived_from"], json!("openttd://game/state"));
    }
    #[test]
    fn an_intent_answer_passes_only_with_an_action_candidates_and_a_plan_to_build() {
        let ok = json!({"result": {"intent": {"action": "find", "ordering": "nearest", "subjects": [{"name": "Forest"}, {"name": "Sawmill"}]},
                                   "answer": {"candidates": [{}, {}]}}});
        assert_eq!(intent_verdict(&ok).unwrap(), "find nearest [Forest, Sawmill] -> 2 candidate(s)");
        let build_without_plan = json!({"result": {"intent": {"action": "build"}, "answer": {"candidates": [{}]}}});
        assert!(intent_verdict(&build_without_plan).is_err());
        assert!(intent_verdict(&json!({"result": {"intent": {"action": "find"}, "answer": {"candidates": []}}})).is_err());
        assert!(intent_verdict(&json!({"result": {"http_error": 400, "body": {"error": "no text"}}})).is_err());
        assert_eq!(subjects_resolved(&ok), 2);
        let with_lang = json!({"result": {"intent": {"action": "find", "ordering": "nearest", "subjects": [{"name": "Forest"}],
                                          "language": "czech", "language_from": "names in this game, the tie by spelling"}, "answer": {"candidates": [{}]}}});
        assert_eq!(intent_verdict(&with_lang).unwrap(), "find nearest [Forest] -> 1 candidate(s), read as czech (names in this game, the tie by spelling)");
        assert_eq!(OWNER_SENTENCES.len(), 6);
    }
    #[test]
    fn a_sentence_resolved_on_one_instance_and_not_on_another_is_a_fallback_there_only() {
        let r = |i: &str, s: &str, n: usize| (i.to_string(), s.to_string(), n);
        let results = vec![r("a", "forest + sawmill", 2), r("b", "forest + sawmill", 0), r("a", "most lucrative", 0), r("b", "most lucrative", 0)];
        assert_eq!(unresolved_elsewhere_resolved(&results), vec![("b".to_string(), "forest + sawmill".to_string(), "a".to_string())]);
        assert!(unresolved_elsewhere_resolved(&[r("a", "x", 0)]).is_empty());
    }
    #[test]
    fn a_hub_raised_event_is_a_prismatic_bus_v1_envelope_naming_its_instance() {
        let ev = json!({"kind": "audit.duplicate_station", "data": {"owner": 0, "a": {"id": 3}, "b": {"id": 7}, "distance": 2}});
        let env = hub::hub_envelope(4, "panel-8800", &ev, &json!("s1"), 99, None);
        assert_eq!(env, json!({"schema": "prismatic-bus/v1", "seq": 4, "ts_ms": 99, "instance": "panel-8800", "session": "s1", "seed": null,
                               "game_tick": null, "game_date": null, "kind": "audit.duplicate_station", "data": ev["data"]}));
        // The panel's own envelope function, not a copy of it.
        assert_eq!(env, bus_sinks::envelope(&json!({"seq": 4, "kind": ev["kind"], "data": ev["data"]}), "panel-8800", &json!("s1"), None, 99, None));
        // A derived event carries the clock of the read it was seen at; its own date wins.
        let clock = hub::clock_of(&json!({"game_tick": 200, "game_date": "1950-02-01", "data": {}}));
        let month = json!({"kind": "game.month", "data": {"date": "1950-02-01"}, "derived_from": "openttd://game/state"});
        let env = hub::hub_envelope(5, "gui-1", &month, &Value::Null, 1, Some(&clock));
        assert_eq!((env["game_tick"].clone(), env["game_date"].clone(), env["instance"].clone()), (json!(200), json!("1950-02-01"), json!("gui-1")));
        assert_eq!(env["data"], json!({"date": "1950-02-01"}), "the envelope's data is the event's data");
    }

    #[test]
    fn every_kind_the_hub_raises_is_a_hub_row_of_the_one_table() {
        // The kinds the hub's code names where it raises events, read from its source like the panel's.
        let src = include_str!("../hub.rs");
        let mut raised = std::collections::BTreeSet::new();
        for f in ["pub fn derive_state_events", "pub fn audit_findings", "pub fn audit_changes"] {
            let at = src.find(f).expect(f);
            let body = &src[at..at + src[at..].find("\n}\n").expect("its end")];
            for part in body.split('"').skip(1).step_by(2) {
                if part.contains('.') && !part.starts_with('.') && part.chars().all(|c| c.is_ascii_lowercase() || c == '.' || c == '_') {
                    raised.insert(part.to_string());
                }
            }
        }
        let hub_rows: Vec<&panel_events::Kind> = panel_events::KINDS.iter().filter(|k| k.by == panel_events::By::Hub).collect();
        let declared: std::collections::BTreeSet<String> = hub_rows.iter().map(|k| k.kind.to_string()).collect();
        assert_eq!(raised, declared, "the kinds hub.rs raises and the hub's rows of src/panel_events.rs KINDS");
        for k in &hub_rows {
            let f = k.producer.strip_prefix("hub.rs:").expect("a hub row names a hub.rs producer");
            assert!(src.contains(&format!("pub fn {f}(")), "{}: no producer {f} in hub.rs", k.kind);
            assert!(panel_events::hub_kind(k.kind).is_some());
        }
        // What the hub's GET /api/bus lists is exactly those rows.
        let listed: Vec<Value> = panel_events::table(panel_events::HUB_BUS).as_array().unwrap().iter().map(|r| r["kind"].clone()).collect();
        assert_eq!(listed.len(), hub_rows.len());
        assert!(raised.contains("audit.resolved") && raised.contains("game.month") && raised.len() >= 10, "{raised:?}");
    }

    #[test]
    fn the_hubs_bus_and_store_routes_are_rows_of_its_method_table() {
        let spec = hub::openapi();
        for (path, id, method) in [("/api/bus", "hubBus", "hub.bus"), ("/api/db/query", "hubDbQuery", "hub.db_query"),
                                   ("/api/db/tables", "hubDbTables", "hub.db_tables"), ("/hub/instances", "hubInstances", "hub.instances"),
                                   ("/hub/catalogue", "hubCatalogue", "hub.catalogue"), ("/hub/audit", "hubAudit", "hub.audit")] {
            assert_eq!(spec["paths"][path]["get"]["operationId"], json!(id), "{path}");
            assert_eq!(hub::get_method(path).as_deref(), Some(method), "{path} is served by its row");
            assert_eq!(hub::operation_id(method), id);
        }
        assert_eq!(spec["paths"]["/api/db/query"]["get"]["parameters"][0]["name"], json!("sql"));
        let methods = spec.pointer("/paths/~1hub~1rpc/post/requestBody/content/application~1json/schema/properties/method/enum").unwrap();
        for m in ["hub.bus", "hub.db_tables", "hub.db_query"] {
            assert!(methods.as_array().unwrap().contains(&json!(m)), "{m} missing from OpenAPI");
        }
        let sdl = hub::graphql::sdl(&hub::methods());
        assert!(sdl.contains("db_query(sql: JSON): JSON") && sdl.contains("bus: JSON"), "{sdl}");
        assert_eq!(hub::query_params("/api/db/query?sql=SELECT%20kind+FROM%20events%3B&x="), json!({"sql": "SELECT kind FROM events;", "x": ""}));
        assert_eq!(hub::query_params("/api/bus"), json!({}));
    }

    /// A ZeroMQ SUB client on its own thread and runtime (the crate's zeromq, as mcp-bus-tail): every
    /// (topic, envelope) it receives on `topic`.
    fn subscribe(endpoint: &str, topic: &str) -> std::sync::mpsc::Receiver<(String, Value)> {
        let (tx, rx) = std::sync::mpsc::channel();
        let (ready_tx, ready_rx) = std::sync::mpsc::channel();
        let (ep, topic) = (endpoint.to_string(), topic.to_string());
        std::thread::spawn(move || {
            let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap();
            rt.block_on(async move {
                use zeromq::{Socket, SocketRecv};
                let mut sub = zeromq::SubSocket::new();
                sub.connect(&ep).await.unwrap();
                sub.subscribe(&topic).await.unwrap();
                let _ = ready_tx.send(());
                while let Ok(msg) = sub.recv().await {
                    let frame = |i: usize| msg.get(i).map(|b| String::from_utf8_lossy(b).into_owned()).unwrap_or_default();
                    if tx.send((frame(0), serde_json::from_str(&frame(1)).unwrap_or(Value::Null))).is_err() { break; }
                }
            });
        });
        ready_rx.recv_timeout(Duration::from_secs(5)).expect("the SUB client connected");
        rx
    }

    fn temp_dir(tag: &str) -> std::path::PathBuf {
        let d = std::env::temp_dir().join(format!("hub-{tag}-{}-{}", std::process::id(), bus_sinks::now_ms()));
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    #[cfg(feature = "store")]
    #[test]
    fn a_derived_game_event_enters_the_hubs_store_with_its_instance_and_clock_and_an_undeclared_kind_does_not() {
        let dir = temp_dir("derived");
        let sinks = Arc::new(bus_sinks::Sinks::open("hub-u", Value::Null, 0, Some(dir.join("hub-u.duckdb")),
                                                    bus_sinks::Run { seed: None, ports: (None, None, None) }));
        assert!(sinks.store_error.is_none(), "{:?}", sinks.store_error);
        let mut s = hub::State::default();
        s.sinks = Some(sinks.clone());
        let before = json!({"game_date": "1950-01-31", "game_tick": 100, "data": {"paused": false}});
        let now = json!({"game_date": "1950-02-01", "game_tick": 200, "data": {"paused": false}});
        s.mcp_state.insert("gui-1".into(), now.clone());
        for e in hub::derive_state_events(Some(&before), &now) { s.raise("gui-1", &e); }
        s.raise("gui-1", &json!({"kind": "not.declared", "data": {}}));
        sinks.commit(true);
        let (code, out) = bus_sinks::query(Some(&sinks), "SELECT instance, seq, kind, game_tick, game_date, data->>'date' FROM events ORDER BY seq");
        assert_eq!(code, 200, "{out}");
        assert_eq!(out["rows"], json!([["gui-1", 1, "game.month", 200, "1950-02-01", "1950-02-01"]]), "one derived event, not the undeclared one");
        let _ = std::fs::remove_dir_all(dir);
    }

    #[cfg(feature = "store")]
    #[test]
    fn a_raised_event_goes_out_on_zeromq_and_into_the_hubs_store_and_a_relayed_one_into_neither() {
        let events = Arc::new(Mutex::new(Vec::new()));
        let base = fake_panel(events.clone());
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let addr = listener.local_addr().unwrap();
        let dir = temp_dir("sinks");
        let registry = dir.join("registry");
        std::fs::create_dir_all(&registry).unwrap();
        let store = dir.join("hub-t.duckdb");
        // The first audit runs 1.5 s after the start, so the SUB clients below are connected by then.
        let cfg = hub::Config { registry, panels: vec![base], statics: default_static(), refresh: Duration::from_millis(200),
                                poll: Duration::from_millis(50), audit: Some(Duration::from_millis(1500)),
                                sinks: Some(hub::SinkConfig { name: "hub-t".into(), bus_port: 0, store: Some(store.clone()) }) };
        hub::run(listener, cfg);
        let get = |path: &str| -> Value { ureq::get(&format!("http://{addr}{path}")).call().unwrap().into_json().unwrap() };
        let sql = |q: &str| -> Value { ureq::get(&format!("http://{addr}/api/db/query")).query("sql", q).call().unwrap().into_json().unwrap() };
        let bus = get("/api/bus");
        assert_eq!((bus["schema"].clone(), bus["instance"].clone()), (json!("prismatic-bus/v1"), json!("hub-t")));
        assert_eq!(bus["store"], json!(store.display().to_string()));
        assert!(bus["kinds"].as_array().unwrap().iter().all(|k| k["by"] == "hub"), "the hub lists its own kinds");
        let endpoint = bus["endpoint"].as_str().expect("the hub's PUB endpoint").to_string();
        let audit = subscribe(&endpoint, "audit");
        let game = subscribe(&endpoint, "game");
        let name = get("/hub/instances")[0]["name"].as_str().unwrap().to_string();

        // Acceptance 1: a SUB client receives the audit event the hub raised, in the envelope, naming the instance.
        let (topic, env) = audit.recv_timeout(Duration::from_secs(10)).expect("an audit event on the hub's bus");
        let received = Instant::now();
        assert_eq!(topic, "audit.duplicate_station");
        let mut keys: Vec<&str> = env.as_object().unwrap().keys().map(String::as_str).collect();
        keys.sort_unstable();
        assert_eq!(keys, vec!["data", "game_date", "game_tick", "instance", "kind", "schema", "seed", "seq", "session", "ts_ms"]);
        assert_eq!((env["schema"].clone(), env["kind"].clone(), env["instance"].clone(), env["seq"].clone()),
                   (json!("prismatic-bus/v1"), json!("audit.duplicate_station"), json!(name), json!(1)));
        assert_eq!(env["data"]["b"]["name"], json!("Here Central"));

        // Acceptance 2: the hub's store has it within a second.
        let q = "SELECT instance, kind, seq, json_extract_string(data, '$.b.name') FROM events";
        let mut rows = sql(q)["rows"].clone();
        while rows.as_array().map_or(true, Vec::is_empty) && received.elapsed() < Duration::from_secs(2) {
            std::thread::sleep(Duration::from_millis(50));
            rows = sql(q)["rows"].clone();
        }
        let took = received.elapsed();
        assert_eq!(rows, json!([[name, "audit.duplicate_station", 1, "Here Central"]]));
        assert!(took < Duration::from_secs(1), "in the store {took:?} after it went out");
        assert_eq!(sql("SELECT instance, seed, panel_port, mcp_port, game_port FROM runs")["rows"], json!([["hub-t", null, null, null, null]]));

        // A panel event the hub only relays streams to its subscribers, and reaches neither the hub's bus nor its store.
        let mut ws = connect(&format!("ws://{addr}/ws")).unwrap();
        let mut notes = Vec::new();
        request(&mut ws, 1, "hub.subscribe", json!({"kinds": ["game"]}), &mut notes).unwrap();
        events.lock().unwrap().push(json!({"seq": 1, "kind": "game.month", "data": {"date": "1950-02-01"}}));
        let deadline = Instant::now() + Duration::from_secs(5);
        let mut relayed = notes.iter().any(|n| n["params"]["event"]["kind"] == "game.month");
        while !relayed && Instant::now() < deadline {
            if let Ok(tungstenite::Message::Text(t)) = ws.read() {
                relayed = serde_json::from_str::<Value>(t.as_str()).map(|v| v["params"]["event"]["kind"] == "game.month").unwrap_or(false);
            }
        }
        assert!(relayed, "the relayed panel event reached the WebSocket subscriber");
        assert!(game.recv_timeout(Duration::from_millis(1500)).is_err(), "a relayed panel event is not on the hub's bus");
        assert_eq!(sql("SELECT kind, count(*) FROM events GROUP BY 1")["rows"], json!([["audit.duplicate_station", 1]]),
                   "the hub's store holds its own event only, not the relayed one");

        // The store routes are read-only, and the schema is the panel's.
        assert_eq!(ureq::get(&format!("http://{addr}/api/db/query")).query("sql", "DROP TABLE events").call().err().map(|e| match e {
            ureq::Error::Status(code, _) => code, _ => 0 }), Some(400));
        let tables = get("/api/db/tables");
        let names: Vec<&str> = tables["tables"].as_array().unwrap().iter().map(|t| t["name"].as_str().unwrap()).collect();
        assert_eq!(names, vec!["events", "runs", "samples"]);
        let rpc: Value = ureq::post(&format!("http://{addr}/hub/rpc"))
            .send_json(json!({"jsonrpc": "2.0", "id": 2, "method": "hub.db_query", "params": {"sql": "SELECT count(*) FROM events"}})).unwrap().into_json().unwrap();
        assert_eq!(rpc["result"]["rows"], json!([[1]]));
        let _ = std::fs::remove_dir_all(dir);
    }

    /// A panel with a status route, an echo route and an event bus whose events the test appends.
    fn fake_panel(events: Arc<Mutex<Vec<Value>>>) -> String {
        let l = TcpListener::bind("127.0.0.1:0").unwrap();
        let base = format!("http://{}", l.local_addr().unwrap());
        std::thread::spawn(move || for mut c in l.incoming().flatten() {
            // Read the head, then as much body as Content-Length says: a POST body may arrive later.
            let mut data = Vec::new();
            let mut chunk = [0u8; 4096];
            loop {
                let n = c.read(&mut chunk).unwrap_or(0);
                if n == 0 { break; }
                data.extend_from_slice(&chunk[..n]);
                let text = String::from_utf8_lossy(&data).to_string();
                if let Some(end) = text.find("\r\n\r\n") {
                    let len = text.lines().find_map(|l| l.to_ascii_lowercase().strip_prefix("content-length:").map(|v| v.trim().parse::<usize>().unwrap_or(0))).unwrap_or(0);
                    if data.len() >= end + 4 + len { break; }
                }
            }
            let req = String::from_utf8_lossy(&data).to_string();
            let line = req.lines().next().unwrap_or("").to_string();
            let body = if line.starts_with("GET /openapi.json") { spec() }
                else if line.starts_with("GET /api/status") { json!({"ok": true}) }
                else if line.starts_with("GET /api/resources/stations") {
                    json!({"availability": "available", "data": {"stations": [
                        {"id": 1, "name": "Here", "x": 20, "y": 20, "owner": 0, "facilities": ["bus"]},
                        {"id": 2, "name": "Here Central", "x": 21, "y": 22, "owner": 0, "facilities": ["bus"]}]}})
                }
                else if line.starts_with("POST /api/echo") { json!({"echo": req.split("\r\n\r\n").nth(1).unwrap_or("")}) }
                else if line.starts_with("GET /api/events") {
                    let ev = events.lock().unwrap().clone();
                    let since: i64 = line.split("since=").nth(1).and_then(|s| s.split(['&', ' ']).next()).and_then(|s| s.parse().ok()).unwrap_or(0);
                    let newer: Vec<Value> = ev.iter().filter(|e| e["seq"].as_i64().unwrap_or(0) > since).cloned().collect();
                    json!({"events": newer, "last_seq": ev.len()})
                } else { json!({"error": "nf"}) };
            let b = body.to_string();
            let _ = write!(c, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{b}", b.len());
        });
        base
    }

    #[test]
    fn a_client_lists_calls_and_streams_through_the_hub() {
        let events = Arc::new(Mutex::new(Vec::new()));
        let base = fake_panel(events.clone());
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let addr = listener.local_addr().unwrap();
        let empty = std::env::temp_dir().join(format!("hubreg-{}", std::process::id()));
        std::fs::create_dir_all(&empty).unwrap();
        let cfg = hub::Config { registry: empty, panels: vec![base], statics: default_static(),
                                refresh: Duration::from_millis(200), poll: Duration::from_millis(50), audit: Some(Duration::from_millis(150)), sinks: None };
        hub::run(listener, cfg);
        let mut ws = connect(&format!("ws://{addr}/ws")).unwrap();
        let mut notes = Vec::new();
        let inst = request(&mut ws, 1, "hub.instances", json!({}), &mut notes).unwrap();
        let name = inst["result"][0]["name"].as_str().unwrap().to_string();
        assert_eq!(inst["result"][0]["operations"], json!(6));
        let a = request(&mut ws, 20, "hub.audit", json!({}), &mut notes).unwrap();
        assert_eq!(a["result"][0]["duplicates"][0]["b"]["name"], json!("Here Central"), "the hub finds the duplicate it is served");
        let r = request(&mut ws, 2, "hub.call", json!({"instance": name, "id": "status"}), &mut notes).unwrap();
        assert_eq!(r["result"], json!({"ok": true}));
        let r = request(&mut ws, 3, "hub.call", json!({"instance": name, "id": "echo", "body": {"say": "hi"}}), &mut notes).unwrap();
        assert!(r["result"]["echo"].as_str().unwrap().contains("hi"));
        let r = request(&mut ws, 4, "hub.call", json!({"instance": name, "id": "eventStream"}), &mut notes).unwrap();
        assert!(r.get("error").is_some(), "a stream is subscribed to, not called");
        let r = request(&mut ws, 5, "nope", json!({}), &mut notes).unwrap();
        assert_eq!(r["error"]["code"], json!(-32601));
        // The audit watch announces the fake panel's duplicate pair on the event stream.
        request(&mut ws, 30, "hub.subscribe", json!({"kinds": ["audit"]}), &mut notes).unwrap();
        let deadline = Instant::now() + Duration::from_secs(5);
        let mut audit_event = notes.iter().find(|n| n["params"]["event"]["kind"] == "audit.duplicate_station").cloned();
        while audit_event.is_none() && Instant::now() < deadline {
            if let Ok(tungstenite::Message::Text(t)) = ws.read() {
                let v: Value = serde_json::from_str(t.as_str()).unwrap();
                if v["params"]["event"]["kind"] == "audit.duplicate_station" { audit_event = Some(v); }
            }
        }
        assert_eq!(audit_event.expect("an audit event")["params"]["event"]["data"]["b"]["name"], json!("Here Central"));
        request(&mut ws, 6, "hub.subscribe", json!({"kinds": ["game"]}), &mut notes).unwrap();
        std::thread::sleep(Duration::from_millis(300));
        events.lock().unwrap().push(json!({"seq": 1, "kind": "panel.note"}));
        events.lock().unwrap().push(json!({"seq": 2, "kind": "game.month", "month": 3}));
        let deadline = Instant::now() + Duration::from_secs(5);
        let mut got = Vec::new();
        while got.is_empty() && Instant::now() < deadline {
            if let Ok(tungstenite::Message::Text(t)) = ws.read() {
                let v: Value = serde_json::from_str(t.as_str()).unwrap();
                if v["method"] == "hub.event" { got.push(v); }
            }
        }
        assert_eq!(got.len(), 1, "only the subscribed kind is streamed");
        assert_eq!(got[0]["params"]["event"]["kind"], json!("game.month"));
        assert_eq!(got[0]["params"]["instance"], json!(name));
        // The same surface answers over plain HTTP.
        let http = ureq::post(&format!("http://{addr}/hub/rpc"))
            .send_json(json!({"jsonrpc": "2.0", "id": 9, "method": "rpc.discover"})).unwrap();
        let v: Value = http.into_json().unwrap();
        assert!(v["result"].as_array().unwrap().iter().any(|m| m["method"] == "hub.call"));
        // Everything the hub offers is in its own OpenAPI document, derived from the method table.
        let spec: Value = ureq::get(&format!("http://{addr}/openapi.json")).call().unwrap().into_json().unwrap();
        let methods = spec.pointer("/paths/~1hub~1rpc/post/requestBody/content/application~1json/schema/properties/method/enum").unwrap();
        for m in ["hub.instances", "hub.catalogue", "hub.call", "hub.subscribe", "hub.audit"] {
            assert!(methods.as_array().unwrap().contains(&json!(m)), "{m} missing from OpenAPI");
        }
        let docs = ureq::get(&format!("http://{addr}/docs")).call().unwrap().into_string().unwrap();
        assert!(docs.contains("'/openapi.json'"), "Swagger UI reads the hub's own document");
        assert!(spec.pointer("/paths/~1docs/get").is_some(), "and /docs is itself in it");
        // Every page the hub serves is an operation of its own document.
        for page in ["~1", "~1viz"] {
            assert!(spec.pointer(&format!("/paths/{page}/get")).is_some(), "page {page} missing from OpenAPI");
        }
        // The same surface as GraphQL, through the same dispatch.
        let gql: Value = ureq::post(&format!("http://{addr}/graphql"))
            .send_json(json!({"query": "query($i: String) { catalogue(instance: $i) { id read_only } instances { name operations } }", "variables": {"i": name}})).unwrap().into_json().unwrap();
        assert!(gql.get("errors").is_none(), "{gql}");
        assert_eq!(gql["data"]["instances"][0]["operations"], json!(6));
        assert!(gql["data"]["catalogue"].as_array().unwrap().iter().any(|o| o == &json!({"id": "status", "read_only": true})));
        let called: Value = ureq::post(&format!("http://{addr}/graphql"))
            .send_json(json!({"query": format!("mutation {{ call(instance: \"{name}\", id: \"status\") }}")})).unwrap().into_json().unwrap();
        assert_eq!(called["data"]["call"], json!({"ok": true}));
        let sdl = ureq::get(&format!("http://{addr}/graphql/schema")).call().unwrap().into_string().unwrap();
        assert!(sdl.contains("type Query"));
        assert!(spec.pointer("/paths/~1graphql/post").is_some(), "GraphQL is in the OpenAPI document too");
        let audit: Value = ureq::get(&format!("http://{addr}/hub/audit")).call().unwrap().into_json().unwrap();
        assert_eq!(audit[0]["duplicates"].as_array().unwrap().len(), 1);
    }
}
