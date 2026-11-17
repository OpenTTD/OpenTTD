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
//!   world_plan(limit)                  -> map   town pairs ranked, each with its mode (bus, rail, air)
//!   world_plan_with(limit, params)     -> map   the same with any parameter overridden (see mcp-plan --defaults)
//!   world_graph()                      -> map   the road network as nodes, edges and components
//!   kernel(id, input)                  -> map   one AlgoGlyph kernel on that input (its `result`), through
//!                                               the flow CLI of $MCP_ALGOGLYPH_DIR (hub_algoglyph.rs); throws
//!                                               when there is no AlgoGlyph or the kernel refused
//!   kernels()                          -> array every AlgoGlyph kernel with the input it reads
//!   env(name)
//!   sleep(ms)   to_json(value)
//!   tools()  commands()  help()          the derived functions below, by name
//!
//! Derived at start-up from the live game, never listed here: one function per MCP tool in
//! tools/list (dots become underscores: `openttd_game_set_pause(#{pause: true})`, `openttd_decision_validate(#{...})`),
//! and one per OpenTTD console command in `list_cmds` (`con_companies()`, `con_date()`, `con_start_ai("PrismaticAI")`).
use std::time::Duration;

#[path = "hub_algoglyph.rs"]
#[allow(dead_code)]
mod algoglyph;
mod graph;
mod plan;

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
        to_dyn(graph::derive_with_ways(&get("/api/resources/map")?, &get("/api/resources/towns")?, &get("/api/resources/stations")?, &get("/api/resources/map/ways").unwrap_or(Value::Null))?)
    });
    let p = panel.clone();
    // A route plan (src/plan.rs): town pairs ranked by model v2 (the game's payment formula), each with its mode.
    e.register_fn("world_plan", move |limit: i64| -> R<Dynamic> {
        let get = |path: &str| -> R<Value> {
            let v = from_dyn(p.get(path)?)?;
            Ok(v.get("data").cloned().unwrap_or(v))
        };
        let towns = get("/api/resources/towns")?;
        let g = graph::derive_with_ways(&get("/api/resources/map")?, &towns, &get("/api/resources/stations")?, &get("/api/resources/map/ways").unwrap_or(Value::Null))?;
        let engines = get("/api/resources/engines").unwrap_or(Value::Null);
        let params = plan::with_prices(&Value::Null, &get("/api/resources/economy/prices").unwrap_or(Value::Null));
        to_dyn(plan::plan(&towns, &g, limit.max(1) as usize, &params, &engines)?)
    });
    let p = panel.clone();
    // The same with any parameter overridden, e.g. world_plan_with(10, #{cost_weight: 1.0, modes: ["bus", "rail"]}).
    e.register_fn("world_plan_with", move |limit: i64, params: rhai::Map| -> R<Dynamic> {
        let get = |path: &str| -> R<Value> {
            let v = from_dyn(p.get(path)?)?;
            Ok(v.get("data").cloned().unwrap_or(v))
        };
        let towns = get("/api/resources/towns")?;
        let g = graph::derive_with_ways(&get("/api/resources/map")?, &towns, &get("/api/resources/stations")?, &get("/api/resources/map/ways").unwrap_or(Value::Null))?;
        let engines = get("/api/resources/engines").unwrap_or(Value::Null);
        let params = plan::with_prices(&from_dyn(Dynamic::from_map(params))?, &get("/api/resources/economy/prices").unwrap_or(Value::Null));
        to_dyn(plan::plan(&towns, &g, limit.max(1) as usize, &params, &engines)?)
    });
    // AlgoGlyph's kernels, e.g. kernel("human-pointing", #{from: [0, 0], to: [640, 320], width: 64.0, seed: 7}).
    e.register_fn("kernel", |id: &str, input: Map| -> R<Dynamic> {
        let v = algoglyph::flow(&["kernel", id, "-"], Some(&from_dyn(Dynamic::from_map(input))?))?;
        to_dyn(kernel_result(id, v)?)
    });
    e.register_fn("kernels", || -> R<Dynamic> { to_dyn(algoglyph::flow(&["kernels"], None)?) });
    e.register_fn("env", |name: &str| -> String { std::env::var(name).unwrap_or_default() });
    e.register_fn("sleep", |ms: i64| std::thread::sleep(Duration::from_millis(ms.max(0) as u64)));
    e.register_fn("to_json", |d: Dynamic| -> R<String> {
        serde_json::to_string_pretty(&from_dyn(d)?).map_err(|e| e.to_string().into())
    });
    e
}

/// What a kernel answered, as a script reads it: the `result` of `{ok: true, result}`; a refusal
/// (`ok: false` on the answer or on its result) is an error naming the kernel and its reasons.
fn kernel_result(id: &str, v: Value) -> Result<Value, String> {
    let r = v.get("result").cloned().unwrap_or_else(|| v.clone());
    if v.get("ok") == Some(&Value::Bool(false)) || r.get("ok") == Some(&Value::Bool(false)) {
        let why = v.get("errors").or_else(|| r.get("errors")).cloned().unwrap_or(v);
        return Err(format!("{id} refused: {why}"));
    }
    Ok(r)
}

/// Rhai identifier for an MCP tool or console command name: dots and dashes become underscores.
/// A query string from a Rhai map: strings as they are, everything else as JSON, percent-encoded.
fn query(q: &Map) -> R<String> {
    let enc = |v: &str| -> String {
        v.bytes().map(|b| if b.is_ascii_alphanumeric() || b"-_.~".contains(&b) { (b as char).to_string() } else { format!("%{b:02X}") }).collect()
    };
    let mut parts = Vec::new();
    for (k, v) in q {
        let text = if v.is_string() { v.clone().into_string().unwrap_or_default() } else { from_dyn(v.clone())?.to_string() };
        parts.push(format!("{}={}", enc(k), enc(&text)));
    }
    Ok(parts.join("&"))
}

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
                    // With a map: its entries become the query (a map or list value is sent as JSON),
                    // e.g. api_plan(#{limit: 5, params: #{modes: ["bus", "rail"]}}).
                    let (pq, rq) = (panel.clone(), path.clone());
                    e.register_fn(fname.clone(), move |q: Map| -> R<Dynamic> { pq.get(&format!("{rq}?{}", query(&q)?)) });
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

/// The Rhai name of a hub method: `hub.audit` is `hub_audit`, `rpc.discover` is `hub_rpc_discover`.
fn hub_ident(method: &str) -> String {
    let flat = method.replace('.', "_");
    if flat.starts_with("hub_") { flat } else { format!("hub_{flat}") }
}

/// One Rhai function per method the hub's own OpenAPI document lists (`x-hub-methods` of
/// POST /hub/rpc), each called as one JSON-RPC request: `hub_audit()`, `hub_call(#{instance, id})`.
/// Streaming methods need a WebSocket and are left out. Nothing here names a method.
fn derive_hub(e: &mut Engine, hub: &str) -> Vec<String> {
    let spec: Value = match ureq::get(&format!("{hub}/openapi.json")).timeout(Duration::from_secs(15)).call()
        .map_err(|e| e.to_string()).and_then(|r| r.into_json::<Value>().map_err(|e| e.to_string())) {
        Ok(v) => v,
        Err(err) => { eprintln!("mcp-rhai: hub {hub}: {err}"); return vec![]; }
    };
    let methods = spec.pointer("/paths/~1hub~1rpc/post/x-hub-methods").and_then(Value::as_array).cloned().unwrap_or_default();
    let mut names = Vec::new();
    for m in methods {
        let Some(method) = m.get("method").and_then(Value::as_str).map(str::to_string) else { continue };
        if method.ends_with("subscribe") { continue; }
        let name = hub_ident(&method);
        let rpc = move |hub: &str, method: &str, params: Value| -> R<Dynamic> {
            let v: Value = ureq::post(&format!("{hub}/hub/rpc")).timeout(Duration::from_secs(60))
                .send_json(json!({"jsonrpc": "2.0", "id": 1, "method": method, "params": params}))
                .map_err(|e| format!("{method}: {e}"))?.into_json().map_err(|e| format!("{method}: {e}"))?;
            if let Some(err) = v.get("error") { return Err(format!("{method}: {err}").into()); }
            to_dyn(v.get("result").cloned().unwrap_or(Value::Null))
        };
        let (h, m1) = (hub.to_string(), method.clone());
        e.register_fn(name.as_str(), move || -> R<Dynamic> { rpc(&h, &m1, json!({})) });
        let (h, m2) = (hub.to_string(), method.clone());
        e.register_fn(name.as_str(), move |params: Map| -> R<Dynamic> { rpc(&h, &m2, from_dyn(Dynamic::from(params))?) });
        names.push(name);
    }
    names
}

/// The engine a script runs in: the built-ins, every function derived from the live panel at `base`
/// (and from the hub, when given), and `import` resolved against MCP_RHAI_PATH, else the panel's
/// rhai/ library. mcp-rhai runs one script in it; mcp-panel runs /api/rhai scripts in it in process.
pub(crate) fn host(base: &str, hub: Option<&str>) -> Engine {
    let panel = Panel { base: base.trim_end_matches('/').to_string() };
    let mut engine = engine(panel.clone());
    let lib = std::env::var("MCP_RHAI_PATH").unwrap_or_else(|_| {
        concat!(env!("CARGO_MANIFEST_DIR"), "/../mcp-panel/rhai").to_string()
    });
    engine.set_module_resolver(rhai::module_resolvers::FileModuleResolver::new_with_path(lib));
    let (mut tools, commands) = derive(&mut engine, &panel);
    // The hub (research/mcp-hub.md): every instance of the cluster through one surface.
    if let Some(h) = hub { tools.extend(derive_hub(&mut engine, h.trim_end_matches('/'))); }
    let (t, c) = (tools.clone(), commands.clone());
    engine.register_fn("tools", move || t.iter().cloned().map(Dynamic::from).collect::<rhai::Array>());
    engine.register_fn("commands", move || c.iter().cloned().map(Dynamic::from).collect::<rhai::Array>());
    engine.register_fn("help", move || -> String {
        format!(
            "built-in: rpc read overview status console pause approve auto_approve events mcp_call mcp_read world_graph world_plan world_plan_with env sleep to_json tools commands help\n\
             derived from tools/list and the panel OpenAPI ({}): {}\nderived from list_cmds ({}): {}",
            tools.len(), tools.join(" "), commands.len(), commands.join(" ")
        )
    });
    engine
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
    let mut hub = std::env::var("MCP_HUB").ok();
    if let Some(i) = args.iter().position(|a| a == "--hub") {
        if i + 1 >= args.len() {
            eprintln!("mcp-rhai: --hub needs a URL");
            std::process::exit(2);
        }
        hub = Some(args.remove(i + 1));
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
            eprintln!("usage: mcp-rhai [--panel URL] [--hub URL] (<script.rhai> | -e '<script>')");
            std::process::exit(2);
        }
    };
    let engine = host(&base, hub.as_deref());
    match engine.eval::<Dynamic>(&script) {
        Ok(v) if !v.is_unit() => println!("{v}"),
        Ok(_) => {}
        Err(e) => {
            eprintln!("mcp-rhai: {e}");
            std::process::exit(1);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Read, Write};

    /// A hub that publishes two methods, one of them streaming, and answers every request.
    fn fake_hub() -> String {
        let l = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
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
            let body = if req.starts_with("GET /openapi.json") {
                json!({"paths": {"/hub/rpc": {"post": {"x-hub-methods": [{"method": "hub.audit"}, {"method": "hub.subscribe"}, {"method": "rpc.discover"}]}}}})
            } else {
                let sent: Value = serde_json::from_str(req.split("\r\n\r\n").nth(1).unwrap_or("{}")).unwrap_or(Value::Null);
                json!({"jsonrpc": "2.0", "id": 1, "result": {"method": sent["method"], "params": sent["params"]}})
            }.to_string();
            let _ = write!(c, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{body}", body.len());
        });
        base
    }

    /// The director's coalescing (mcp-panel/rhai/director_lib.rhai), run in the engine it runs in.
    #[test]
    fn the_director_acts_once_per_run_of_one_kind_and_never_replays_it() {
        let mut e = Engine::new();
        e.set_module_resolver(rhai::module_resolvers::FileModuleResolver::new_with_path(
            concat!(env!("CARGO_MANIFEST_DIR"), "/../mcp-panel/rhai")));
        let script = r#"
            import "director_lib" as d;
            let ev = [];
            for i in 0..119 { ev.push(#{ kind: "ai.log", data: #{ text: `PrismaticAI: built rail at tile ${1000 + i}` } }); }
            ev.push(#{ kind: "ai.log", data: #{ text: "PrismaticAI: built rail station in Runinghall at tile 9866" } });
            ev.push(#{ kind: "ai.log", data: #{ text: "PrismaticAI: survey stop A in X at tile 5 (drive-through)" } });
            ev.push(#{ kind: "ai.log", data: #{ text: "PrismaticAI: survey stop A in X at tile 6 (drive-through)" } });
            ev.push(#{ kind: "game.new_world", data: () });
            ev.push(#{ kind: "ai.log", data: #{ text: "PrismaticAI: survey rail station at Mine at tile 58165" } });
            ev.push(#{ kind: "ai.log", data: #{ text: "PrismaticAI: built rail station at Mine at tile 58165" } });
            let c = d::coalesce(ev);
            [c.len(), c[0].repeats, d::tile_of(c[0].data.text), c[1].repeats, c[2].repeats, d::tile_of(c[2].data.text),
             d::is_linear(c[0], ""), d::is_linear(c[1], ""), d::is_linear(c[1], d::kind(c[1].data.text)),
             d::kind("PrismaticAI: built rail at tile 1234") == d::kind("PrismaticAI: built rail at tile 99"),
             d::kind("PrismaticAI: built rail at tile 1") != d::kind("PrismaticAI: built rail station in A at tile 1"),
             c[3].kind, c.len() == 5, c[4].data.text.contains("built"), c[4].repeats]
        "#;
        let out: rhai::Array = e.eval(script).unwrap();
        let v: Vec<String> = out.iter().map(|x| x.to_string()).collect();
        assert_eq!(v, vec!["5", "119", "1118", "1", "2", "6", "true", "false", "true", "true", "true", "game.new_world", "true", "true", "2"],
                   "119 rail tiles -> one action at the newest tile; a station is a single structure; retried surveys -> the last; a survey and its outcome on one tile -> one action, the outcome");
    }

    #[test]
    fn every_hub_method_but_streams_is_a_rhai_function() {
        assert_eq!(hub_ident("hub.audit"), "hub_audit");
        assert_eq!(hub_ident("rpc.discover"), "hub_rpc_discover");
        let mut e = Engine::new();
        let names = derive_hub(&mut e, &fake_hub());
        assert_eq!(names, vec!["hub_audit".to_string(), "hub_rpc_discover".to_string()]);
        let out: Map = e.eval("hub_audit()").unwrap();
        assert_eq!(out["method"].clone().into_string().unwrap(), "hub.audit");
        let out: Map = e.eval(r#"hub_audit(#{instance: "panel-1"})"#).unwrap();
        let params = out["params"].clone().cast::<Map>();
        assert_eq!(params["instance"].clone().into_string().unwrap(), "panel-1");
        assert!(e.eval::<Dynamic>("hub_subscribe()").is_err(), "a stream needs the WebSocket, not Rhai");
    }

    #[test]
    fn a_kernel_answer_is_its_result_and_a_refusal_is_an_error() {
        let ok = json!({"ok": true, "kernel": {"id": "human-pointing"}, "result": {"ok": true, "samples": [[1, 2, 16]]}});
        assert_eq!(kernel_result("human-pointing", ok).unwrap()["samples"], json!([[1, 2, 16]]));
        let refused = json!({"ok": false, "errors": ["width must be positive"]});
        let e = kernel_result("human-pointing", refused).unwrap_err();
        assert!(e.contains("human-pointing refused") && e.contains("width must be positive"), "{e}");
        let inner = json!({"ok": true, "result": {"ok": false, "errors": ["no path"]}});
        assert!(kernel_result("track-planning", inner).unwrap_err().contains("no path"));
    }

    /// A script's kernel("human-pointing", ...) moves exactly as the golden vectors mcp-human keeps
    /// for AlgoGlyph parity; runs only where an AlgoGlyph checkout with that kernel is found.
    #[test]
    fn a_script_gets_the_human_pointing_kernel_bit_for_bit() {
        if !algoglyph::dir().join("src/plates/human-pointing").is_dir() {
            eprintln!("skip: no AlgoGlyph checkout with human-pointing at {}", algoglyph::dir().display());
            return;
        }
        let golden: Value = serde_json::from_str(&std::fs::read_to_string(
            concat!(env!("CARGO_MANIFEST_DIR"), "/../mcp-human/golden/human-pointing.v1.json")).unwrap()).unwrap();
        let e = engine(Panel { base: "http://127.0.0.1:9".into() });
        for c in golden["cases"].as_array().unwrap().iter().take(3) {
            let script = format!("kernel(\"human-pointing\", #{{from: {}, to: {}, width: {:.1}, seed: {}, drag: {}}})",
                c["from"], c["to"], c["width"].as_f64().unwrap(), c["seed"], c["drag"]);
            let got = from_dyn(e.eval::<Dynamic>(&script).unwrap()).unwrap();
            assert_eq!(got["samples"], c["samples"], "case {c}");
        }
    }
}
