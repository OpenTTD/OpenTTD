//! RESEARCH-ONLY: run Rhai scripts against the OpenTTD MCP control panel (tools/research/mcp-panel).
//!
//! A script drives the running game the way the panel and its developer console do: raw MCP
//! JSON-RPC, resources, OpenTTD console commands, and the approval-gated pause. It talks to the
//! panel's loopback API, so it never sees the bearer token.
//!
//! Usage: mcp-rhai [--panel http://127.0.0.1:8800] (<script.rhai> | -e '<script>')
//!
//! Functions a script gets:
//!   rpc(method)  rpc(method, params)  -> map   raw MCP JSON-RPC answer
//!   read(uri)                          -> map   one resource's payload
//!   overview()                         -> map   game state, companies, decisions
//!   status()                           -> map   panel status: pending approvals, events
//!   console(command)                   -> array the OpenTTD console output of one command
//!   pause(on)                          -> map   set_pause through the approval flow
//!   approve(id)                        -> map   the operator's approval of a pending decision
//!   auto_approve(on)                   -> map
//!   events(since)  events(since, "game,company")  -> map  the panel's event feed after a sequence number
//!   mcp_call(token_file, tool, args)  mcp_read(token_file, uri)  another process's own MCP server
//!   world_graph()                      -> map   the road network as nodes, edges and components
//!   env(name)
//!   sleep(ms)   to_json(value)
//!   tools()  commands()  help()          the derived functions below, by name
//!
//! Derived at start-up from the live game, never listed here: one function per MCP tool in
//! tools/list (dots become underscores: `openttd_game_set_pause(#{pause: true})`, `openttd_decision_validate(#{...})`),
//! and one per OpenTTD console command in `list_cmds` (`con_companies()`, `con_date()`, `con_start_ai("PrismaticAI")`).
use std::time::Duration;

mod graph;

use rhai::{Dynamic, Engine, EvalAltResult, Map};
use serde_json::{json, Value};

type R<T> = Result<T, Box<EvalAltResult>>;

#[derive(Clone)]
struct Panel {
    base: String,
}

impl Panel {
    fn get(&self, path: &str) -> R<Dynamic> {
        let v: Value = ureq::get(&format!("{}{}", self.base, path))
            .timeout(Duration::from_secs(15))
            .call()
            .map_err(|e| format!("GET {path}: {e}"))?
            .into_json()
            .map_err(|e| format!("GET {path}: {e}"))?;
        to_dyn(v)
    }

    fn post(&self, path: &str, body: Value) -> R<Dynamic> {
        let resp = ureq::post(&format!("{}{}", self.base, path))
            .timeout(Duration::from_secs(15))
            .send_json(body);
        let v: Value = match resp {
            Ok(r) => r.into_json().map_err(|e| format!("POST {path}: {e}"))?,
            Err(ureq::Error::Status(code, r)) => {
                json!({"http_error": code, "body": r.into_string().unwrap_or_default()})
            }
            Err(e) => return Err(format!("POST {path}: {e}").into()),
        };
        to_dyn(v)
    }
}

fn to_dyn(v: Value) -> R<Dynamic> {
    rhai::serde::to_dynamic(v)
}

fn from_dyn(d: Dynamic) -> R<Value> {
    rhai::serde::from_dynamic(&d)
}

fn engine(panel: Panel) -> Engine {
    let mut e = Engine::new();
    let p = panel.clone();
    e.register_fn("rpc", move |method: &str| -> R<Dynamic> { p.post("/api/rpc", json!({"method": method})) });
    let p = panel.clone();
    e.register_fn("rpc", move |method: &str, params: Map| -> R<Dynamic> {
        let params = from_dyn(Dynamic::from_map(params))?;
        p.post("/api/rpc", json!({"method": method, "params": params}))
    });
    let p = panel.clone();
    e.register_fn("read", move |uri: &str| -> R<Dynamic> {
        p.get(&format!("/api/resource?uri={}", uri.replace('/', "%2F").replace(':', "%3A")))
    });
    let p = panel.clone();
    e.register_fn("overview", move || -> R<Dynamic> { p.get("/api/overview") });
    let p = panel.clone();
    e.register_fn("status", move || -> R<Dynamic> { p.get("/api/status") });
    let p = panel.clone();
    e.register_fn("console", move |command: &str| -> R<Dynamic> {
        let out = p.post("/api/console", json!({"command": command}))?;
        let map = out.try_cast::<Map>().ok_or("console: unexpected answer")?;
        Ok(map.get("output").cloned().unwrap_or_else(|| Dynamic::from_array(vec![])))
    });
    let p = panel.clone();
    e.register_fn("pause", move |on: bool| -> R<Dynamic> { p.post("/api/pause", json!({"pause": on})) });
    let p = panel.clone();
    e.register_fn("approve", move |id: i64| -> R<Dynamic> { p.post("/api/approve", json!({"id": id})) });
    let p = panel.clone();
    e.register_fn("auto_approve", move |on: bool| -> R<Dynamic> {
        p.post("/api/auto-approve", json!({"on": on}))
    });
    let p = panel.clone();
    e.register_fn("events", move |since: i64| -> R<Dynamic> { p.get(&format!("/api/events?since={since}")) });
    let p = panel.clone();
    e.register_fn("events", move |since: i64, kinds: &str| -> R<Dynamic> {
        p.get(&format!("/api/events?since={since}&kinds={kinds}"))
    });
    // Any OpenTTD process's own MCP server (a GUI client's, another instance of a cluster): the
    // endpoint and token come from the token file `prismatic_mcp start <port> <file>` writes.
    e.register_fn("mcp_call", |token_file: &str, tool: &str, args: Map| -> R<Dynamic> {
        let cfg: Value = serde_json::from_str(&std::fs::read_to_string(token_file).map_err(|e| format!("{token_file}: {e}"))?)
            .map_err(|e| format!("{token_file}: {e}"))?;
        let endpoint = cfg.get("endpoint").and_then(Value::as_str).ok_or("token file has no endpoint")?;
        let token = cfg.get("agent_token").and_then(Value::as_str).ok_or("token file has no agent_token")?;
        let body = json!({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                          "params": {"name": tool, "arguments": from_dyn(Dynamic::from_map(args))?}});
        let v: Value = ureq::post(endpoint).timeout(Duration::from_secs(10))
            .set("Authorization", &format!("Bearer {token}")).set("Accept", "application/json, text/event-stream")
            .send_json(body).map_err(|e| format!("{endpoint}: {e}"))?.into_json().map_err(|e| format!("{endpoint}: {e}"))?;
        match v.pointer("/result/content/0/text").and_then(Value::as_str) {
            Some(text) => to_dyn(serde_json::from_str(text).unwrap_or(Value::String(text.to_string()))),
            None => to_dyn(v),
        }
    });
    e.register_fn("mcp_read", |token_file: &str, uri: &str| -> R<Dynamic> {
        let cfg: Value = serde_json::from_str(&std::fs::read_to_string(token_file).map_err(|e| format!("{token_file}: {e}"))?)
            .map_err(|e| format!("{token_file}: {e}"))?;
        let endpoint = cfg.get("endpoint").and_then(Value::as_str).ok_or("token file has no endpoint")?;
        let token = cfg.get("agent_token").and_then(Value::as_str).ok_or("token file has no agent_token")?;
        let v: Value = ureq::post(endpoint).timeout(Duration::from_secs(10))
            .set("Authorization", &format!("Bearer {token}")).set("Accept", "application/json, text/event-stream")
            .send_json(json!({"jsonrpc": "2.0", "id": 1, "method": "resources/read", "params": {"uri": uri}}))
            .map_err(|e| format!("{endpoint}: {e}"))?.into_json().map_err(|e| format!("{endpoint}: {e}"))?;
        match v.pointer("/result/contents/0/text").and_then(Value::as_str) {
            Some(text) => to_dyn(serde_json::from_str(text).unwrap_or(Value::Null)),
            None => to_dyn(v),
        }
    });
    let p = panel.clone();
    // The road network as a graph (src/graph.rs), derived from openttd://map, towns and stations.
    e.register_fn("world_graph", move || -> R<Dynamic> {
        let get = |path: &str| -> R<Value> {
            let v = from_dyn(p.get(path)?)?;
            Ok(v.get("data").cloned().unwrap_or(v))
        };
        to_dyn(graph::derive(&get("/api/resources/map")?, &get("/api/resources/towns")?, &get("/api/resources/stations")?)?)
    });
    e.register_fn("env", |name: &str| -> String { std::env::var(name).unwrap_or_default() });
    e.register_fn("sleep", |ms: i64| std::thread::sleep(Duration::from_millis(ms.max(0) as u64)));
    e.register_fn("to_json", |d: Dynamic| -> R<String> {
        serde_json::to_string_pretty(&from_dyn(d)?).map_err(|e| e.to_string().into())
    });
    e
}

/// Rhai identifier for an MCP tool or console command name: dots and dashes become underscores.
fn ident(name: &str) -> String {
    name.chars().map(|c| if c.is_ascii_alphanumeric() { c } else { '_' }).collect()
}

/// Derive functions from the live game instead of naming them here: one per MCP tool in
/// tools/list (`openttd_game_set_pause(#{pause: true})`, the tool's JSON answer parsed), and one
/// per OpenTTD console command from `list_cmds` (`con_companies()`, `con_date("...")`). Returns
/// the derived names, which `tools()`, `commands()` and `help()` report.
fn derive(e: &mut Engine, panel: &Panel) -> (Vec<String>, Vec<String>) {
    let mut tools = Vec::new();
    let call = |panel: &Panel, name: &str, args: Value| -> R<Dynamic> {
        let out = panel.post("/api/rpc", json!({"method": "tools/call", "params": {"name": name, "arguments": args}}))?;
        let v = from_dyn(out)?;
        match v.pointer("/result/content/0/text").and_then(Value::as_str) {
            Some(text) => to_dyn(serde_json::from_str(text).unwrap_or(Value::String(text.to_string()))),
            None => to_dyn(v),
        }
    };
    if let Ok(cat) = panel.get("/api/catalogue").and_then(from_dyn) {
        for t in cat.pointer("/tools/result/tools").and_then(Value::as_array).into_iter().flatten() {
            let Some(name) = t.get("name").and_then(Value::as_str) else { continue };
            let (p1, n1) = (panel.clone(), name.to_string());
            e.register_fn(ident(name), move |args: Map| -> R<Dynamic> { call(&p1, &n1, from_dyn(Dynamic::from_map(args))?) });
            let (p0, n0) = (panel.clone(), name.to_string());
            e.register_fn(ident(name), move || -> R<Dynamic> { call(&p0, &n0, json!({})) });
            tools.push(ident(name));
        }
    }
    // One function per operation of the panel's own OpenAPI document (api_<operationId>): every
    // route the panel declares, and every route it derives from the catalogue, is callable here.
    if let Ok(spec) = panel.get("/openapi.json").and_then(from_dyn) {
        for (path, ops) in spec.get("paths").and_then(Value::as_object).into_iter().flatten() {
            for (method, op) in ops.as_object().into_iter().flatten() {
                let Some(id) = op.get("operationId").and_then(Value::as_str) else { continue };
                if path.contains("/stream") { continue }
                let fname = format!("api_{}", ident(id));
                let (p, route) = (panel.clone(), path.clone());
                if method == "post" {
                    e.register_fn(fname.clone(), move |body: Map| -> R<Dynamic> { p.post(&route, from_dyn(Dynamic::from_map(body))?) });
                    let (p0, r0) = (panel.clone(), path.clone());
                    e.register_fn(fname.clone(), move || -> R<Dynamic> { p0.post(&r0, json!({})) });
                } else {
                    e.register_fn(fname.clone(), move || -> R<Dynamic> { p.get(&route) });
                }
                tools.push(fname);
            }
        }
    }
    let mut commands = Vec::new();
    if let Ok(out) = panel.post("/api/console", json!({"command": "list_cmds", "wait": 1.0})).and_then(from_dyn) {
        for line in out.get("output").and_then(Value::as_array).into_iter().flatten() {
            let Some(cmd) = line.as_str().map(str::trim) else { continue };
            if cmd.is_empty() || !cmd.chars().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '_') {
                continue;
            }
            let fname = format!("con_{}", ident(cmd));
            let (p0, c0) = (panel.clone(), cmd.to_string());
            e.register_fn(fname.clone(), move || -> R<Dynamic> { console_lines(&p0, &c0) });
            let (p1, c1) = (panel.clone(), cmd.to_string());
            e.register_fn(fname.clone(), move |args: &str| -> R<Dynamic> { console_lines(&p1, &format!("{c1} {args}")) });
            commands.push(fname);
        }
    }
    (tools, commands)
}

fn console_lines(panel: &Panel, command: &str) -> R<Dynamic> {
    let out = from_dyn(panel.post("/api/console", json!({"command": command}))?)?;
    to_dyn(out.get("output").cloned().unwrap_or(Value::Array(vec![])))
}

fn main() {
    let mut args = std::env::args().skip(1).collect::<Vec<_>>();
    let mut base = "http://127.0.0.1:8800".to_string();
    if let Some(i) = args.iter().position(|a| a == "--panel") {
        if i + 1 >= args.len() {
            eprintln!("mcp-rhai: --panel needs a URL");
            std::process::exit(2);
        }
        base = args.remove(i + 1);
        args.remove(i);
    }
    let script = match args.as_slice() {
        [flag, code] if flag == "-e" => code.clone(),
        [path] if path != "-h" && path != "--help" => match std::fs::read_to_string(path) {
            Ok(s) => s,
            Err(e) => {
                eprintln!("mcp-rhai: {path}: {e}");
                std::process::exit(2);
            }
        },
        _ => {
            eprintln!("usage: mcp-rhai [--panel URL] (<script.rhai> | -e '<script>')");
            std::process::exit(2);
        }
    };
    let panel = Panel { base: base.trim_end_matches('/').to_string() };
    let mut engine = engine(panel.clone());
    // `import "ttd" as ttd;` resolves against MCP_RHAI_PATH, else the panel's rhai/ library.
    let lib = std::env::var("MCP_RHAI_PATH").unwrap_or_else(|_| {
        concat!(env!("CARGO_MANIFEST_DIR"), "/../mcp-panel/rhai").to_string()
    });
    engine.set_module_resolver(rhai::module_resolvers::FileModuleResolver::new_with_path(lib));
    let (tools, commands) = derive(&mut engine, &panel);
    let (t, c) = (tools.clone(), commands.clone());
    engine.register_fn("tools", move || t.iter().cloned().map(Dynamic::from).collect::<rhai::Array>());
    engine.register_fn("commands", move || c.iter().cloned().map(Dynamic::from).collect::<rhai::Array>());
    engine.register_fn("help", move || -> String {
        format!(
            "built-in: rpc read overview status console pause approve auto_approve events mcp_call mcp_read world_graph env sleep to_json tools commands help\n\
             derived from tools/list and the panel OpenAPI ({}): {}\nderived from list_cmds ({}): {}",
            tools.len(), tools.join(" "), commands.len(), commands.join(" ")
        )
    });
    match engine.eval::<Dynamic>(&script) {
        Ok(v) if !v.is_unit() => println!("{v}"),
        Ok(_) => {}
        Err(e) => {
            eprintln!("mcp-rhai: {e}");
            std::process::exit(1);
        }
    }
}
