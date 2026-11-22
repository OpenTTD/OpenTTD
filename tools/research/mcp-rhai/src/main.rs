//! RESEARCH-ONLY: run Rhai scripts against the OpenTTD MCP control panel (tools/research/mcp-panel).
//!
//! A script drives the running game the way the panel and its developer console do: raw MCP
//! JSON-RPC, resources, OpenTTD console commands, and the approval-gated pause. It talks to the
//! panel's loopback API, so it never sees the bearer token.
//!
//! Usage: mcp-rhai [--panel http://127.0.0.1:8800] [--hub URL] (<script.rhai> | -e '<script>')
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
//!   board_post(claim, text)            -> bool  announce a named claim on the Majordomus board (board.rs):
//!                                               the server of this checkout, $MAJORDOMUS_URL, or printed
//!                                               only with $MCP_BOARD_DRY set
//!   loopback_get(url)                  -> map   GET a JSON document from a loopback URL only (127.0.0.1,
//!                                               localhost, [::1]), e.g. the LAN battle's /api/leaderboard;
//!                                               throws for any other host
//!   shared_get(key)                    -> any   a value the panel's scripts and services share, or () (rhai_shared.rs)
//!   shared_entry(key)                  -> map   the same with its rev, writer and time, or ()
//!   shared_set(key, value)             -> int   write it; answers the new revision
//!   shared_cas(key, rev, value)        -> bool  write only if the key is at rev (0: absent); false when not
//!   shared_delete(key)                 -> bool  delete it; false when it was absent
//!   shared_keys()  shared_keys(prefix) -> array the keys, in order; every write is a rhai.shared event,
//!                                               and the writer is $MCP_RHAI_WRITER, else "rhai"
//!   prismatic_decision()  prismatic_decision(company)  -> map  Prismatic's decision for the bot, checked
//!                                               against the contract (prismatic.rs; $PRISMATIC_URL, loopback)
//!   prismatic_tools()                  -> array Prismatic's MCP tools
//!   prismatic_exec(tool, args)         -> any   one of Prismatic's MCP tools, its data
//!   prismatic_bb_get(frame, key)  prismatic_bb_put(frame, key, map)  prismatic_bb_query(frame, limit)
//!                                               Prismatic's blackboard (its blackboard.* MCP tools); every
//!                                               failure is an error whose text begins "prismatic: <kind>"
//!   mesh_records()  mesh_records(filters)  -> map  the Majordomus mesh's typed coordination records
//!                                               (mesh_records.rs; $MAJORDOMUS_URL, loopback), filtered by
//!                                               record, task, kind, author, path, live
//!   mesh_record_write(record)          -> map   write or revise a record ({key, revision, ...}); the session
//!                                               is the record's, else $MAJORDOMUS_SESSION; every failure is
//!                                               an error whose text begins "mesh: <kind>"
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
#[allow(dead_code)]
mod board;
mod graph;
mod plan;
mod mesh_records;
mod prismatic;

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

/// Whether `url` is plain HTTP to this machine's loopback interface, the only place
/// `loopback_get` may read from: a script must not reach the network beyond this machine.
fn is_loopback_url(url: &str) -> bool {
    let Some(rest) = url.strip_prefix("http://") else { return false };
    let host = rest.split(['/', '?', '#']).next().unwrap_or("");
    if host.contains('@') {
        return false; // userinfo: "localhost:80@elsewhere" connects to elsewhere
    }
    let host = if host.starts_with('[') {
        host.split(']').next().map(|h| format!("{h}]")).unwrap_or_default()
    } else {
        host.split(':').next().unwrap_or("").to_string()
    };
    matches!(host.as_str(), "127.0.0.1" | "localhost" | "[::1]")
}

fn loopback_get(url: &str) -> R<Dynamic> {
    if !is_loopback_url(url) {
        return Err(format!("loopback_get: {url} is not a loopback http URL").into());
    }
    // No redirects: a loopback server must not be able to send the script to another host.
    let agent = ureq::AgentBuilder::new().redirects(0).timeout(Duration::from_secs(10)).build();
    let v: Value = agent
        .get(url)
        .call()
        .map_err(|e| format!("GET {url}: {e}"))?
        .into_json()
        .map_err(|e| format!("GET {url}: {e}"))?;
    to_dyn(v)
}

/// The writer a script declares on the shared memory: $MCP_RHAI_WRITER, else "rhai".
fn shared_writer() -> String {
    std::env::var("MCP_RHAI_WRITER").ok().filter(|w| !w.is_empty()).unwrap_or_else(|| "rhai".to_string())
}

/// One call of the panel's shared-memory routes, with its status: a 404 or 409 is an answer a
/// script acts on, not a failure; only an unreachable panel is an error here.
fn shared_call(base: &str, method: &str, path: &str, body: Option<Value>) -> Result<(u16, Value), Box<EvalAltResult>> {
    let url = format!("{base}{path}");
    let resp = match body {
        Some(b) => ureq::post(&url).timeout(Duration::from_secs(15)).send_json(b),
        None => ureq::request(method, &url).timeout(Duration::from_secs(15)).call(),
    };
    match resp {
        Ok(r) => Ok((r.status(), r.into_json().unwrap_or(Value::Null))),
        Err(ureq::Error::Status(code, r)) => Ok((code, r.into_json().unwrap_or(Value::Null))),
        Err(e) => Err(format!("{method} {path}: {e}").into()),
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
    // The Majordomus board (board.rs): one session for the whole script, opened on the first post on
    // the server `majordomus serve ensure` names for this checkout ($MAJORDOMUS_URL overrides), or
    // printed only when $MCP_BOARD_DRY is set.
    let shared: std::sync::Arc<std::sync::Mutex<Option<board::Board>>> = Default::default();
    let b1 = shared.clone();
    e.register_fn("board_post", move |claim: &str, text: &str| -> R<bool> {
        let mut slot = b1.lock().map_err(|_| "board_post: the session lock is poisoned".to_string())?;
        if slot.is_none() {
            let dry = !std::env::var("MCP_BOARD_DRY").unwrap_or_default().is_empty();
            let url = if dry { "(dry run)".to_string() } else { board::majordomus_url(None, None)? };
            *slot = Some(board::Board::new(&url, dry, "openttd-rhai-board", "OpenTTD Rhai script"));
        }
        let b = slot.as_mut().expect("just set");
        b.keep_alive();
        b.announce(claim, &board::clip(text, 480))?;
        Ok(true)
    });
    // The panel's shared memory (src/rhai_shared.rs, I0091), over its loopback API like every other
    // call, so a script reads and writes the same keys in the panel and from the command line.
    let b = panel.base.clone();
    e.register_fn("shared_get", move |key: &str| -> R<Dynamic> {
        match shared_call(&b, "GET", &format!("/api/rhai/shared?key={key}"), None)? {
            (200, v) => to_dyn(v.get("value").cloned().unwrap_or(Value::Null)),
            (404, _) => Ok(Dynamic::UNIT),
            (c, v) => Err(format!("shared_get({key}): HTTP {c}: {}", v["error"]).into()),
        }
    });
    let b = panel.base.clone();
    e.register_fn("shared_entry", move |key: &str| -> R<Dynamic> {
        match shared_call(&b, "GET", &format!("/api/rhai/shared?key={key}"), None)? {
            (200, v) => to_dyn(v),
            (404, _) => Ok(Dynamic::UNIT),
            (c, v) => Err(format!("shared_entry({key}): HTTP {c}: {}", v["error"]).into()),
        }
    });
    let b = panel.base.clone();
    e.register_fn("shared_set", move |key: &str, value: Dynamic| -> R<i64> {
        let body = json!({"key": key, "value": from_dyn(value)?, "writer": shared_writer()});
        match shared_call(&b, "POST", "/api/rhai/shared", Some(body))? {
            (200, v) => Ok(v["rev"].as_i64().unwrap_or(0)),
            (c, v) => Err(format!("shared_set({key}): HTTP {c}: {}", v["error"]).into()),
        }
    });
    let b = panel.base.clone();
    e.register_fn("shared_cas", move |key: &str, rev: i64, value: Dynamic| -> R<bool> {
        let body = json!({"key": key, "value": from_dyn(value)?, "writer": shared_writer(), "rev": rev.max(0)});
        match shared_call(&b, "POST", "/api/rhai/shared", Some(body))? {
            (200, _) => Ok(true),
            (409, _) => Ok(false),
            (c, v) => Err(format!("shared_cas({key}): HTTP {c}: {}", v["error"]).into()),
        }
    });
    let b = panel.base.clone();
    e.register_fn("shared_delete", move |key: &str| -> R<bool> {
        match shared_call(&b, "POST", "/api/rhai/shared/delete", Some(json!({"key": key, "writer": shared_writer()})))? {
            (200, _) => Ok(true),
            (409, _) => Ok(false),
            (c, v) => Err(format!("shared_delete({key}): HTTP {c}: {}", v["error"]).into()),
        }
    });
    let b = panel.base.clone();
    let keys = move |prefix: &str| -> R<Dynamic> {
        match shared_call(&b, "GET", &format!("/api/rhai/shared?prefix={prefix}"), None)? {
            (200, v) => to_dyn(Value::Array(v["entries"].as_array().into_iter().flatten().map(|e| e["key"].clone()).collect())),
            (c, v) => Err(format!("shared_keys: HTTP {c}: {}", v["error"]).into()),
        }
    };
    let k0 = keys.clone();
    e.register_fn("shared_keys", move || -> R<Dynamic> { k0("") });
    e.register_fn("shared_keys", move |prefix: &str| -> R<Dynamic> { keys(prefix) });
    // Prismatic over loopback HTTP (src/prismatic.rs, I0092).
    prismatic::register(&mut e);
    // The Majordomus mesh's typed records over loopback HTTP (src/mesh_records.rs, I4665).
    mesh_records::register(&mut e);
    e.register_fn("loopback_get", loopback_get);
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

/// The script library: $MCP_RHAI_PATH, else this checkout's tools/research/mcp-panel/rhai. `import`
/// resolves against it, and the panel's Scripts page (panel_scripts.rs) lists, edits and runs it.
pub(crate) fn rhai_dir() -> std::path::PathBuf {
    let dir = std::env::var_os("MCP_RHAI_PATH").filter(|v| !v.is_empty()).map(std::path::PathBuf::from)
        .unwrap_or_else(|| std::path::PathBuf::from(concat!(env!("CARGO_MANIFEST_DIR"), "/../mcp-panel/rhai")));
    // Without the `..`: the Scripts page shows where the library is.
    dir.canonicalize().unwrap_or(dir)
}

/// The engine a script runs in: the built-ins, every function derived from the live panel at `base`
/// (and from the hub, when given), and `import` resolved against MCP_RHAI_PATH, else the panel's
/// rhai/ library. mcp-rhai runs one script in it; mcp-panel runs /api/rhai scripts in it in process.
pub(crate) fn host(base: &str, hub: Option<&str>) -> Engine {
    let panel = Panel { base: base.trim_end_matches('/').to_string() };
    let mut engine = engine(panel.clone());
    engine.set_module_resolver(rhai::module_resolvers::FileModuleResolver::new_with_path(rhai_dir()));
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
    if args.iter().any(|a| a == "-h" || a == "--help") {
        println!("usage: mcp-rhai [--panel URL] [--hub URL] (<script.rhai> | -e '<script>')");
        return;
    }
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
    #[test]
    fn loopback_get_only_reads_this_machine() {
        use super::is_loopback_url as ok;
        assert!(ok("http://127.0.0.1:8765/api/leaderboard"));
        assert!(ok("http://localhost/x"));
        assert!(ok("http://[::1]:80/"));
        assert!(!ok("https://127.0.0.1/"));
        assert!(!ok("http://192.168.100.10:8765/"));
        assert!(!ok("http://127.0.0.1.evil.example/"));
        assert!(!ok("http://localhost.evil.example:80/"));
        assert!(!ok("http://user@evil.example/"));
        assert!(!ok("http://localhost:80@evil.example/"));
        assert!(!ok("http://127.0.0.1:80@evil.example/"));
        assert!(!ok("http://[::1]@evil.example/"));
        assert!(!ok("http://evil.example\\@127.0.0.1/"));
        assert!(super::loopback_get("http://example.com/").is_err());
    }

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

    /// An engine that resolves `import` from mcp-panel/rhai, as the panel's does.
    fn lib_engine() -> Engine {
        let mut e = Engine::new();
        e.set_module_resolver(rhai::module_resolvers::FileModuleResolver::new_with_path(
            concat!(env!("CARGO_MANIFEST_DIR"), "/../mcp-panel/rhai")));
        e
    }

    /// The fixture every rail_lib test reads: one company (0) with a coal line from station 1 (tile
    /// 2*64+2) to station 2 (tile 40*64+40) on a 64-wide map, its record logged twice (the newest
    /// wins), a second company's record on the same slot, and a train maker.
    const RAIL_FIXTURE: &str = r#"
        import "rail_lib" as rl;
        let ev = [
            #{ kind: "ai.log", seq: 1, data: #{ company: 0, text: "PrismaticAI: rail line 1: label=COAL A - B|a=130|b=2600|cargo=1|platforms=2|len=3|depot=129|trains_max=2|wagons_fit=4|tiles=60|state=open" } },
            #{ kind: "ai.log", seq: 2, data: #{ company: 0, text: "PrismaticAI: rail line 1: label=COAL A - B|a=130|b=2600|cargo=1|platforms=2|len=4|depot=129|trains_max=2|wagons_fit=6|tiles=60|state=open" } },
            #{ kind: "ai.log", seq: 3, data: #{ company: 1, text: "PrismaticAI: rail line 1: label=WOOD C - D|a=3000|b=3100|cargo=7|platforms=1|len=3|depot=1|trains_max=1|wagons_fit=4|tiles=9|state=open" } },
            #{ kind: "ai.log", seq: 4, data: #{ company: 0, text: "PrismaticAI: no rail path from tile 1 to 2 (90 tiles): every reachable tile tried, 1 nodes" } },
            #{ kind: "ai.log", seq: 5, data: #{ company: 0, text: "PrismaticAI heartbeat: date=1 money=2" } },
        ];
        // As openttd://stations has them: the sign's x, y, the station's own tile, the catchment.
        let stations = [ #{ id: 1, owner: 0, tile: 999, x: 3, y: 2, catchment_area: #{ x: 0, y: 0, w: 9, h: 7 } },
                         #{ id: 2, owner: 0, tile: 2600, x: 40, y: 40, catchment_area: #{ x: 36, y: 36, w: 9, h: 7 } },
                         #{ id: 3, owner: 1, tile: 3000, x: 56, y: 46, catchment_area: () },
                         #{ id: 4, owner: 0, tile: 5, x: 5, y: 0, catchment_area: #{ x: 0, y: 0, w: 20, h: 20 } } ];
        fn train(id, age, last, now, parts, cap, count) {
            #{ id: id, type: "train", owner: 0, engine_id: 10, tile: 500, age: age, max_age: 7000,
               profit_last_year: last, profit_this_year: now, parts: parts, consist_capacity: cap,
               consist_count: count, value: 9000, crashed: false, lost: false, broken_down: false,
               stopped: false, in_depot: false, reliability: 80, current_order: 0,
               orders: [#{ type: "goto_station", destination: 1 }, #{ type: "goto_depot", destination: 9 }, #{ type: "goto_station", destination: 2 }] }
        }
        let engines = [
            #{ id: 10, type: "rail", wagon: false, name: "Kirby", max_speed_kmh: 64, power_hp: 300, price: 6000, railtypes: ["RAIL"], buildable_by: [0] },
            #{ id: 11, type: "rail", wagon: false, name: "Ginzu", max_speed_kmh: 128, power_hp: 1000, price: 9000, railtypes: ["RAIL"], buildable_by: [0] },
            #{ id: 12, type: "rail", wagon: false, name: "Maglev", max_speed_kmh: 400, power_hp: 5000, price: 90000, railtypes: ["MGLV"], buildable_by: [0] },
            #{ id: 13, type: "rail", wagon: true, name: "Coal truck", max_speed_kmh: 500, power_hp: 0, price: 500, railtypes: ["RAIL"], buildable_by: [0] },
            #{ id: 14, type: "rail", wagon: false, name: "Unbuildable", max_speed_kmh: 300, power_hp: 900, price: 9000, railtypes: ["RAIL"], buildable_by: [] },
        ];
        let p = rl::policy();
    "#;

    fn rail(script: &str) -> Vec<String> {
        let out: rhai::Array = lib_engine().eval(&format!("{RAIL_FIXTURE}\n{script}")).unwrap();
        out.iter().map(|x| x.to_string()).collect()
    }

    /// A record is parsed from its own keys, the newest per company and slot wins, a line takes the
    /// trains whose orders name both stations, other companies' records are not its lines, and a
    /// record's tile finds its station by the station's own tile or else the smallest catchment.
    #[test]
    fn rail_lines_are_read_from_the_bots_records_and_the_trains_orders() {
        let v = rail(r#"
            let recs = rl::records_from_events(ev);
            let lines = rl::lines_of(0, recs, [train(5, 10, 0, 0, 4, 90, 0)], stations, 64);
            [recs.len(), recs[0].len, recs[0].wagons_fit, recs[0].label, lines.len(), lines[0].a, lines[0].b,
             lines[0].trains.len(), rl::parse_record("PrismaticAI heartbeat: x") == (), rl::lines_of(1, recs, [], stations, 64).len(),
             rl::station_at(stations, 130, 64).id, rl::station_at(stations, 2600, 64).id, rl::station_at(stations, 63 * 64, 64) == ()]
        "#);
        assert_eq!(v, vec!["2", "4", "6", "COAL A - B", "1", "1", "2", "1", "true", "1", "1", "2", "true"]);
    }

    /// The decisions, each from what the line did: close after a losing year and a losing this
    /// year; lengthen while a trainload waits and the platform has room; then add a train up to the
    /// platforms; drop the newest losing train while nothing waits; nothing for a young line.
    #[test]
    fn rail_targets_follow_from_profit_waiting_cargo_and_the_platforms() {
        let v = rail(r#"
            let recs = rl::records_from_events(ev);
            let losing = rl::lines_of(0, recs, [train(5, 500, -3000, -100, 4, 90, 0)], stations, 64)[0];
            let short = rl::lines_of(0, recs, [train(5, 500, 20000, 5000, 4, 90, 0)], stations, 64)[0];
            let full = rl::lines_of(0, recs, [train(5, 500, 20000, 5000, 7, 180, 0)], stations, 64)[0];
            let two = rl::lines_of(0, recs, [train(5, 900, 20000, 5000, 7, 180, 0), train(6, 450, -500, 100, 7, 180, 0)], stations, 64)[0];
            let young = rl::lines_of(0, recs, [train(5, 30, -3000, -100, 4, 90, 0)], stations, 64)[0];
            let maxed = rl::lines_of(0, recs, [train(5, 500, 1, 1, 7, 180, 0), train(6, 500, 1, 1, 7, 180, 0)], stations, 64)[0];
            [rl::target(losing, 0, p).action, rl::target(losing, 0, p).trains,
             rl::target(short, 200, p).action, rl::target(short, 200, p).wagons,
             rl::target(full, 200, p).action, rl::target(full, 200, p).trains,
             rl::target(two, 10, p).action, rl::target(two, 10, p).trains,
             rl::target(young, 0, p) == (), rl::target(short, 20, p) == (), rl::target(maxed, 500, p) == ()]
        "#);
        assert_eq!(v, vec!["close", "0", "lengthen", "6", "add_train", "2", "shrink", "1", "true", "true", "true"]);
    }

    /// Choosing: closings always; the rest within the budget by gain per cost when no kernel is
    /// given; nothing paid for without money.
    #[test]
    fn rail_choices_fit_the_budget_and_closing_is_always_chosen() {
        let v = rail(r#"
            let items = [
                #{ target: #{ action: "close", slot: 1 }, value: #{ cost: 0, gain: 100 } },
                #{ target: #{ action: "lengthen", slot: 2 }, value: #{ cost: 1000, gain: 500 } },
                #{ target: #{ action: "add_train", slot: 3 }, value: #{ cost: 9000, gain: 900 } },
                #{ target: #{ action: "lengthen", slot: 4 }, value: #{ cost: 3000, gain: 2400 } },
            ];
            let c = rl::choose(items, 4500, ());
            let slots = []; for t in c.chosen { slots.push(t.slot); }
            let broke = rl::choose(items, 0, ());
            [`${slots}`, c.solver, broke.chosen.len()]
        "#);
        assert_eq!(v, vec!["[1, 4, 2]", "greedy gain per cost", "1"]);
    }

    /// Renewal: only a buildable locomotive on the same rails and enough faster; never a wagon,
    /// never another rail type, never one the company cannot buy.
    #[test]
    fn rail_renewals_pick_the_fastest_buildable_locomotive_on_the_same_rails() {
        let v = rail(r#"
            let r = rl::renewals(0, [train(5, 10, 0, 0, 4, 90, 0)], engines, p);
            [r.len(), r[0].from, r[0].to, rl::renewals(0, [], engines, p).len()]
        "#);
        assert_eq!(v, vec!["1", "10", "11", "0"]);
    }

    /// Pricing: a wagon at the consist's own wagons (consist_value less the front vehicle's value),
    /// lengthening paid for every train of the line, a new train at the locomotive's price now plus
    /// its wagons; and a record's station ids win over its tiles.
    #[test]
    fn rail_values_price_the_whole_consist_and_every_train() {
        let v = rail(r#"
            let recs = rl::records_from_events(ev);
            let t1 = train(5, 500, 20000, 5000, 4, 90, 0); t1.consist_value = 9000 + 3 * 400;
            let t2 = train(6, 500, 20000, 5000, 4, 90, 0); t2.consist_value = 9000 + 3 * 400;
            let line = rl::lines_of(0, recs, [t1, t2], stations, 64)[0];
            let lengthen = rl::value(line, #{ action: "lengthen", wagons: 5 }, engines);
            let add = rl::value(line, #{ action: "add_train", trains: 3 }, engines);
            let old = train(7, 500, 20000, 5000, 4, 90, 0);
            let fallback = rl::value(rl::lines_of(0, recs, [old], stations, 64)[0], #{ action: "lengthen", wagons: 4 }, engines);
            let byid = rl::records_from_events([#{ kind: "ai.log", data: #{ company: 0, text: "PrismaticAI: rail line 2: label=X|a=3000|b=3100|st_a=2|st_b=1|cargo=1|trains_max=2|wagons_fit=4|state=open" } }]);
            let l2 = rl::lines_of(0, byid, [], stations, 64);
            [lengthen.cost, add.cost, fallback.cost, l2.len(), l2[0].a, l2[0].b]
        "#);
        assert_eq!(v, vec!["2000", "7500", "500", "1", "2", "1"],
                   "a wagon at the cheapest new one on the same rails (500, more than the line's own depreciated 400): 2 x 500 x 2 trains; 6000 + 3 x 500; st_a/st_b win over tiles");
    }

    /// The steward asks for a target once, not again while its approval is pending, again after
    /// RETRY rounds without approval, and at once for a different value.
    #[test]
    fn rail_targets_are_asked_once_and_retried_only_after_a_refusal() {
        let v = rail(r#"
            let asked = #{ want: 2, id: 7, round: 10 };
            [rl::should_ask((), 1, 2, 11, [], 30), rl::should_ask(asked, 1, 2, 12, [7], 30),
             rl::should_ask(asked, 1, 2, 30, [], 30), rl::should_ask(asked, 1, 2, 40, [], 30),
             rl::should_ask(asked, 1, 3, 12, [7], 30), rl::should_ask(asked, 2, 2, 99, [], 30)]
        "#);
        assert_eq!(v, vec!["true", "false", "false", "true", "true", "false"]);
    }

    /// The steward calls rail_lib from inside its own functions (hand_over) and keeps its memory
    /// through `this`: both work in the engine the scripts run in.
    #[test]
    fn a_script_function_reaches_an_imported_library_and_keeps_its_memory_through_this() {
        let out: rhai::Array = lib_engine().eval(r#"
            import "rail_lib" as rl;
            fn remember(key, want) { let ask = rl::should_ask(this[key], 0, want, 1, [], 30); if ask { this[key] = #{ want: want, id: 1, round: 1 }; } ask }
            let done = #{};
            [done.remember("a", 2), done.remember("a", 2), done.len()]
        "#).unwrap();
        let v: Vec<String> = out.iter().map(|x| x.to_string()).collect();
        assert_eq!(v, vec!["true", "false", "1"]);
    }

    /// The monitor's report: crashed and lost first, then a line without a train, then the bot's rail
    /// warnings; the digest counts each kind.
    #[test]
    fn rail_problems_rank_crashed_and_lost_trains_first() {
        let v = rail(r#"
            let recs = rl::records_from_events(ev);
            let lost = train(5, 10, 0, 0, 4, 90, 0); lost.lost = true; lost.orders = [];
            let broken = train(6, 10, 0, 0, 4, 90, 0); broken.broken_down = true; broken.orders = [];
            let crashed = train(7, 10, 0, 0, 4, 90, 0); crashed.crashed = true; crashed.orders = [];
            let lines = rl::lines_of(0, recs, [], stations, 64);
            let ps = rl::problems(0, lines, [broken, lost, crashed], rl::rail_warnings(ev));
            let kinds = []; for x in ps { kinds.push(x.kind); }
            let d = rl::digest("panel-1", "1950-01-01", ps, 2);
            [`${kinds}`, d.contains("1 lost"), d.contains("train 7 crashed"), rl::digest("panel-1", "d", [], 2)]
        "#);
        assert_eq!(v, vec![r#"["crashed", "lost", "unserved", "warning", "broken_down"]"#, "true", "true",
                           "OpenTTD panel-1 d rail: no problems"]);
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

    fn town(script: &str) -> Vec<String> {
        let out: rhai::Array = lib_engine().eval(&format!("import \"town_lib\" as tl;\n{script}")).unwrap();
        out.iter().map(|x| x.to_string()).collect()
    }

    /// The town model's numbers come from the engine (town_model.rhai, generated): the new-station
    /// threshold, the bands an AI sees and the removal thresholds by tolerance.
    #[test]
    fn town_rules_read_the_engine_thresholds() {
        let v = town(r#"
            [tl::allows_station(-200), tl::allows_station(-199), tl::band_of(-200).name, tl::band_from_enum(0).lowest,
             tl::allows_removal(63, "RoadRemove", "tolerant"), tl::allows_removal(64, "RoadRemove", "neutral"),
             tl::allows_removal(-1000, "TunnelBridgeRemove", "permissive")]
        "#);
        assert_eq!(v, vec!["false", "true", "TOWN_RATING_VERY_POOR", "500", "false", "true", "true"]);
    }

    /// A change stops at its limit and never crosses it (ChangeTownRating); a month recovers a low
    /// rating by 5 up to 200 and adds 12 per active and -15 per idle station; trees are +7 up to 220.
    #[test]
    fn town_rating_moves_like_the_engine() {
        let v = town(r#"
            [tl::changed(230, 7, 220), tl::changed(100, 7, 220), tl::after_clearing_trees(500, 20),
             tl::after_month(-300, 1, 0), tl::after_month(500, 0, 2), tl::trees_to_get_above(-300, -200),
             tl::trees_to_get_above(100, 300), tl::months_until_above(-300, -200, 0, 0, 120),
             tl::months_until_above(-300, -200, 0, 2, 120)]
        "#);
        assert_eq!(v, vec!["230", "107", "-200", "-283", "470", "15", "-1", "21", "-1"]);
    }

    /// Advice for a town that refuses stations names the remedies with their rules, and the likes
    /// and dislikes come from the model's effects.
    #[test]
    fn town_advice_explains_a_refusal() {
        let v = town(r#"
            let a = tl::advice(#{ rating: -260, active_stations: 0, idle_stations: 1 }, "lenient");
            let whats = []; for x in a { whats.push(x.what); }
            let ld = tl::likes_and_dislikes();
            [whats[0], whats[1], a[0].source, ld.likes.len() > 3, ld.dislikes.len() > 3,
             tl::advice(#{ rating: 900 }, "permissive")[0].what]
        "#);
        assert_eq!(v, vec!["plant 9 tree tiles near the town", "serve or remove the idle stations",
                           "src/tree_cmd.cpp CmdPlantTree", "true", "true", "build freely"]);
    }
}
