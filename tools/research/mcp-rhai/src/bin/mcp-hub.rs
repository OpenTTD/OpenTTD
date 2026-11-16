//! RESEARCH-ONLY / UNSTABLE: one WebSocket JSON-RPC surface over every MCP control panel and MCP
//! server of a running cluster (see src/hub.rs and research/mcp-hub.md).
//!
//! Usage:
//!   mcp-hub [serve] [--port 8790] [--registry DIR] [--panel URL]... [--static DIR]
//!   mcp-hub check [--hub ws://127.0.0.1:8790/ws] [--event-wait SECONDS]
//!   mcp-hub audit [--hub ws://127.0.0.1:8790/ws] [--json]
//!
//! serve: discovers instances from $PRISMATIC_MCP_REGISTRY (default
//! ~/.local/state/prismatic-mcp/instances) and every --panel, serves ws://127.0.0.1:<port>/ws,
//! POST /hub/rpc, GET /hub/instances, /hub/catalogue, and the page / (static/hub.html).
//! check: connects to a hub, calls every read-only operation of every instance that needs no
//! required input, and waits for one streamed event; exits 1 on any failure.
//! audit: asks the hub for every instance's duplicate stations (one owner, a shared facility,
//! overlapping catchments); exits 1 when any instance has one.
use serde_json::{json, Value};
use std::net::{TcpListener, TcpStream};
use std::path::PathBuf;
use std::time::{Duration, Instant};

#[path = "../hub.rs"]
mod hub;

fn arg(args: &[String], name: &str) -> Option<String> {
    args.iter().position(|a| a == name).and_then(|i| args.get(i + 1).cloned())
}

fn default_registry() -> PathBuf {
    std::env::var_os("PRISMATIC_MCP_REGISTRY").map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(std::env::var("HOME").unwrap_or_default()).join(".local/state/prismatic-mcp/instances")
    })
}

fn default_static() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../mcp-panel/static")
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
        if op["read_only"] != json!(true) || op["stream"] == json!(true) || required_query || required_body { continue; }
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
    };
    let addr = listener.local_addr().map(|a| a.to_string()).unwrap_or_default();
    let state = hub::run(listener, cfg);
    {
        let s = state.lock().unwrap();
        println!("mcp-hub: http://{addr}/  ws://{addr}/ws  ({} instance(s), {} operation(s))",
                 s.instances.len(), s.ops.values().map(Vec::len).sum::<usize>());
    }
    loop { std::thread::sleep(Duration::from_secs(3600)); }
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let code = match args.first().map(String::as_str) {
        Some("check") => check(&args[1..]),
        Some("audit") => audit_cmd(&args[1..]),
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
        ];
        let d = hub::duplicate_stations(&s);
        let names: Vec<(String, String)> = d.iter().map(|x| (x["a"]["name"].as_str().unwrap().into(), x["b"]["name"].as_str().unwrap().into())).collect();
        assert_eq!(names, vec![("Chindingtown".to_string(), "Chindingtown Central".to_string()), ("Carnford".to_string(), "Carnford Central".to_string())],
                   "2 apart and 6 apart overlap (3+3); 10 apart does not; bus beside an airport and another owner's stop are not duplicates");
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
                                refresh: Duration::from_millis(200), poll: Duration::from_millis(50) };
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
        let audit: Value = ureq::get(&format!("http://{addr}/hub/audit")).call().unwrap().into_json().unwrap();
        assert_eq!(audit[0]["duplicates"].as_array().unwrap().len(), 1);
    }
}
