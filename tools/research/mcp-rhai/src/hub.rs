//! RESEARCH-ONLY / UNSTABLE: one WebSocket JSON-RPC surface over every MCP control panel and every
//! MCP server in the instance registry. Nothing here names an operation: the instances come from
//! the registry (`mcp-instance/v1`) and `--panel`, the operations from each panel's `/openapi.json`
//! and each MCP server's `tools/list` and `resources/list`, the events from each panel's bus.
//!
//! The same methods are a GraphQL schema at `POST /graphql` (SDL at `GET /graphql/schema`), see
//! hub_graphql.rs.
//!
//! JSON-RPC 2.0 methods (over `ws://<host>/ws`, or one call per `POST /hub/rpc`; the routes are
//! described by `GET /openapi.json`, derived from the method table):
//!   rpc.discover                         the methods below, with their parameters
//!   hub.instances                        every live instance: name, kind, base, operation count
//!   hub.catalogue  {instance?}           every operation: instance, id, method, path, schema, read_only
//!   hub.call       {instance, id, body?, query?}   call one operation; id is an operationId,
//!                                        "tool:<name>" or "resource:<uri>"
//!   hub.subscribe  {instances?, kinds?}  stream events as `hub.event` notifications (WebSocket only)
//!   hub.unsubscribe
//!   hub.audit                            per instance: duplicate stations (same owner, a shared
//!                                        facility, overlapping catchments)
//! The hub also pushes `hub.instances` notifications to subscribers when the set of instances changes.

use serde_json::{json, Map, Value};

#[path = "hub_graphql.rs"]
pub mod graphql;
use std::collections::{BTreeMap, HashMap};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::{Path, PathBuf};
use std::sync::{mpsc, Arc, Mutex};
use std::thread;
use std::time::Duration;

/// Where an instance is reached.
#[derive(Clone, Debug, PartialEq)]
pub enum Access {
    /// A control panel: REST routes described by its OpenAPI document, and an event bus.
    Panel { base: String },
    /// An MCP server read directly: JSON-RPC over HTTP with a bearer token.
    Mcp { endpoint: String, token: String },
}

#[derive(Clone, Debug, PartialEq)]
pub struct Instance {
    pub name: String,
    pub kind: String,
    pub access: Access,
}

impl Instance {
    pub fn base(&self) -> &str {
        match &self.access {
            Access::Panel { base } => base,
            Access::Mcp { endpoint, .. } => endpoint,
        }
    }
}

/// One callable operation of one instance.
#[derive(Clone, Debug, PartialEq)]
pub struct Op {
    pub instance: String,
    pub id: String,
    pub method: String,
    pub path: String,
    pub summary: String,
    pub tags: Vec<String>,
    pub read_only: bool,
    /// JSON Schema of the body (POST) or of the arguments (MCP tool).
    pub schema: Option<Value>,
    /// Query parameters, with whether each is required.
    pub query: Vec<(String, bool)>,
    /// True for a streaming route (Server-Sent Events): the hub streams it, it does not call it.
    pub stream: bool,
}

impl Op {
    pub fn to_json(&self) -> Value {
        json!({
            "instance": self.instance, "id": self.id, "method": self.method, "path": self.path,
            "summary": self.summary, "tags": self.tags, "read_only": self.read_only,
            "schema": self.schema, "stream": self.stream,
            "query": self.query.iter().map(|(n, r)| json!({"name": n, "required": r})).collect::<Vec<_>>(),
        })
    }
}

/// The operations a panel's OpenAPI document declares, in path order.
pub fn ops_from_openapi(instance: &str, spec: &Value) -> Vec<Op> {
    let mut out = Vec::new();
    let Some(paths) = spec.get("paths").and_then(Value::as_object) else { return out };
    let mut keys: Vec<&String> = paths.keys().collect();
    keys.sort();
    for path in keys {
        let Some(methods) = paths[path].as_object() else { continue };
        for method in ["get", "post", "put", "delete"] {
            let Some(op) = methods.get(method) else { continue };
            let id = op.get("operationId").and_then(Value::as_str)
                .map(str::to_string)
                .unwrap_or_else(|| format!("{method} {path}"));
            let annotations = op.get("x-mcp-annotations").cloned().unwrap_or(Value::Null);
            let read_only = annotations.get("readOnlyHint").and_then(Value::as_bool)
                .unwrap_or(method == "get");
            let schema = op.pointer("/requestBody/content/application~1json/schema").cloned();
            let query = op.get("parameters").and_then(Value::as_array).map(|ps| {
                ps.iter()
                    .filter(|p| p.get("in").and_then(Value::as_str) == Some("query"))
                    .filter_map(|p| Some((p.get("name")?.as_str()?.to_string(),
                                          p.get("required").and_then(Value::as_bool).unwrap_or(false))))
                    .collect()
            }).unwrap_or_default();
            let stream = op.pointer("/responses/200/content/text~1event-stream").is_some()
                || path.ends_with("/stream");
            out.push(Op {
                instance: instance.to_string(), id, method: method.to_uppercase(), path: path.clone(),
                summary: op.get("summary").or_else(|| op.get("description")).and_then(Value::as_str)
                    .unwrap_or("").to_string(),
                tags: op.get("tags").and_then(Value::as_array)
                    .map(|t| t.iter().filter_map(|x| x.as_str().map(str::to_string)).collect())
                    .unwrap_or_default(),
                read_only, schema, query, stream,
            });
        }
    }
    out
}

/// The operations an MCP server publishes: one per tool and one per resource.
pub fn ops_from_mcp(instance: &str, tools: &Value, resources: &Value) -> Vec<Op> {
    let mut out = Vec::new();
    for t in tools.get("tools").and_then(Value::as_array).into_iter().flatten() {
        let Some(name) = t.get("name").and_then(Value::as_str) else { continue };
        let read_only = t.pointer("/annotations/readOnlyHint").and_then(Value::as_bool).unwrap_or(false);
        out.push(Op {
            instance: instance.to_string(), id: format!("tool:{name}"), method: "TOOL".into(),
            path: name.to_string(),
            summary: t.get("description").and_then(Value::as_str).unwrap_or("").to_string(),
            tags: vec!["tools".into()], read_only, schema: t.get("inputSchema").cloned(),
            query: vec![], stream: false,
        });
    }
    for r in resources.get("resources").and_then(Value::as_array).into_iter().flatten() {
        let Some(uri) = r.get("uri").and_then(Value::as_str) else { continue };
        out.push(Op {
            instance: instance.to_string(), id: format!("resource:{uri}"), method: "READ".into(),
            path: uri.to_string(),
            summary: r.get("description").or_else(|| r.get("name")).and_then(Value::as_str)
                .unwrap_or("").to_string(),
            tags: vec!["resources".into()], read_only: true, schema: None, query: vec![], stream: false,
        });
    }
    out
}

/// One registry record as an instance, or None when it cannot be reached or its owner is gone.
pub fn instance_from_record(rec: &Value, alive: &dyn Fn(i64) -> bool) -> Option<Instance> {
    let name = rec.get("name")?.as_str()?.to_string();
    let kind = rec.get("kind")?.as_str()?.to_string();
    if let Some(pid) = rec.get("owner_pid").and_then(Value::as_i64) {
        if !alive(pid) { return None; }
    }
    if let Some(url) = rec.get("url").and_then(Value::as_str) {
        return Some(Instance { name, kind, access: Access::Panel { base: url.trim_end_matches('/').to_string() } });
    }
    let file = rec.get("token_file")?.as_str()?;
    let tok: Value = serde_json::from_str(&std::fs::read_to_string(file).ok()?).ok()?;
    let token = tok.get("agent_token")?.as_str()?.to_string();
    let endpoint = rec.get("endpoint").or_else(|| tok.get("endpoint"))?.as_str()?.to_string();
    Some(Instance { name, kind, access: Access::Mcp { endpoint, token } })
}

pub fn pid_alive(pid: i64) -> bool {
    std::process::Command::new("kill").args(["-0", &pid.to_string()])
        .stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null())
        .status().map(|s| s.success()).unwrap_or(false)
}

/// Every live instance: the registry's records, then the `--panel` bases not already among them.
pub fn discover(registry: &Path, panels: &[String]) -> Vec<Instance> {
    let mut out: Vec<Instance> = Vec::new();
    let mut files: Vec<PathBuf> = std::fs::read_dir(registry).map(|d| {
        d.filter_map(|e| e.ok().map(|e| e.path()))
            .filter(|p| p.extension().and_then(|x| x.to_str()) == Some("json")).collect()
    }).unwrap_or_default();
    files.sort();
    for f in files {
        let Ok(text) = std::fs::read_to_string(&f) else { continue };
        let Ok(rec) = serde_json::from_str::<Value>(&text) else { continue };
        if let Some(i) = instance_from_record(&rec, &pid_alive) {
            if !out.iter().any(|o| o.base() == i.base()) { out.push(i); }
        }
    }
    for p in panels {
        let base = p.trim_end_matches('/').to_string();
        if !out.iter().any(|o| o.base() == base) {
            let port = base.rsplit(':').next().unwrap_or("panel").to_string();
            out.push(Instance { name: format!("panel-{port}"), kind: "panel".into(), access: Access::Panel { base } });
        }
    }
    out
}

fn agent() -> ureq::Agent {
    ureq::AgentBuilder::new().timeout(Duration::from_secs(15)).build()
}

fn http_json(resp: Result<ureq::Response, ureq::Error>) -> Result<Value, String> {
    match resp {
        Ok(r) => {
            let text = r.into_string().map_err(|e| e.to_string())?;
            Ok(serde_json::from_str(&text).unwrap_or(Value::String(text)))
        }
        Err(ureq::Error::Status(code, r)) => {
            let text = r.into_string().unwrap_or_default();
            let body = serde_json::from_str(&text).unwrap_or(Value::String(text));
            Ok(json!({"http_error": code, "body": body}))
        }
        Err(e) => Err(e.to_string()),
    }
}

fn mcp_rpc(endpoint: &str, token: &str, method: &str, params: Value) -> Result<Value, String> {
    let v = http_json(agent().post(endpoint)
        .set("Authorization", &format!("Bearer {token}"))
        .set("Accept", "application/json, text/event-stream")
        .send_json(json!({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})))?;
    if let Some(e) = v.get("error") { return Err(e.to_string()); }
    Ok(v.get("result").cloned().unwrap_or(v))
}

/// The operations of one instance, read live.
pub fn catalogue_of(i: &Instance) -> Result<Vec<Op>, String> {
    match &i.access {
        Access::Panel { base } => {
            let spec = http_json(agent().get(&format!("{base}/openapi.json")).call())?;
            Ok(ops_from_openapi(&i.name, &spec))
        }
        Access::Mcp { endpoint, token } => {
            let tools = mcp_rpc(endpoint, token, "tools/list", json!({}))?;
            let resources = mcp_rpc(endpoint, token, "resources/list", json!({}))?;
            Ok(ops_from_mcp(&i.name, &tools, &resources))
        }
    }
}

fn encode(s: &str) -> String {
    s.bytes().map(|b| match b {
        b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => (b as char).to_string(),
        _ => format!("%{b:02X}"),
    }).collect()
}

/// Call one operation. A panel route answers as JSON (an HTTP error as `{http_error, body}`); an MCP
/// tool or resource answers with its JSON-RPC result.
pub fn call(i: &Instance, op: &Op, body: Option<&Value>, query: Option<&Map<String, Value>>) -> Result<Value, String> {
    if op.stream { return Err(format!("{} is a stream: use hub.subscribe", op.id)); }
    match &i.access {
        Access::Panel { base } => {
            let mut url = format!("{base}{}", op.path);
            if let Some(q) = query.filter(|q| !q.is_empty()) {
                let pairs: Vec<String> = q.iter().map(|(k, v)| {
                    let v = v.as_str().map(str::to_string).unwrap_or_else(|| v.to_string());
                    format!("{}={}", encode(k), encode(&v))
                }).collect();
                url.push('?');
                url.push_str(&pairs.join("&"));
            }
            match op.method.as_str() {
                "GET" => http_json(agent().get(&url).call()),
                "DELETE" => http_json(agent().delete(&url).call()),
                m => http_json(agent().request(m, &url).send_json(body.cloned().unwrap_or(json!({})))),
            }
        }
        Access::Mcp { endpoint, token } => match op.method.as_str() {
            "TOOL" => mcp_rpc(endpoint, token, "tools/call",
                              json!({"name": op.path, "arguments": body.cloned().unwrap_or(json!({}))})),
            _ => mcp_rpc(endpoint, token, "resources/read", json!({"uri": op.path})),
        },
    }
}

/// A subscriber's filter: which instances and which event kinds (a kind matches by prefix, so
/// `game` takes `game.month` and `game.paused`).
#[derive(Clone, Debug, Default)]
pub struct Filter {
    pub instances: Option<Vec<String>>,
    pub kinds: Vec<String>,
}

impl Filter {
    pub fn from_params(p: &Value) -> Filter {
        let list = |k: &str| p.get(k).and_then(Value::as_array)
            .map(|a| a.iter().filter_map(|x| x.as_str().map(str::to_string)).collect::<Vec<_>>());
        Filter { instances: list("instances").filter(|v| !v.is_empty()), kinds: list("kinds").unwrap_or_default() }
    }

    pub fn matches(&self, instance: &str, event: &Value) -> bool {
        if let Some(only) = &self.instances {
            if !only.iter().any(|i| i == instance) { return false; }
        }
        if self.kinds.is_empty() { return true; }
        let kind = event.get("kind").and_then(Value::as_str).unwrap_or("");
        self.kinds.iter().any(|k| kind == k || kind.starts_with(&format!("{k}.")))
    }
}

struct Subscriber {
    id: u64,
    tx: mpsc::Sender<Value>,
    filter: Option<Filter>,
}

/// The hub's live state, shared by the discovery, fan-in and connection threads.
#[derive(Default)]
pub struct State {
    pub instances: Vec<Instance>,
    pub ops: BTreeMap<String, Vec<Op>>,
    pub errors: BTreeMap<String, String>,
    seq: HashMap<String, i64>,
    subscribers: Vec<Subscriber>,
    next_subscriber: u64,
}

pub type Shared = Arc<Mutex<State>>;

impl State {
    fn broadcast(&mut self, instance: Option<&str>, msg: Value, event: Option<&Value>) {
        self.subscribers.retain(|s| {
            let wanted = match (&s.filter, instance, event) {
                (None, _, _) => false,
                (Some(f), Some(i), Some(e)) => f.matches(i, e),
                (Some(_), _, _) => true,
            };
            !wanted || s.tx.send(msg.clone()).is_ok()
        });
    }

    pub fn find(&self, instance: &str, id: &str) -> Option<(Instance, Op)> {
        let i = self.instances.iter().find(|i| i.name == instance)?.clone();
        let op = self.ops.get(instance)?.iter().find(|o| o.id == id)?.clone();
        Some((i, op))
    }

    pub fn instances_json(&self) -> Value {
        Value::Array(self.instances.iter().map(|i| json!({
            "name": i.name, "kind": i.kind, "base": i.base(),
            "access": match i.access { Access::Panel { .. } => "panel", Access::Mcp { .. } => "mcp" },
            "operations": self.ops.get(&i.name).map(Vec::len).unwrap_or(0),
            "error": self.errors.get(&i.name),
        })).collect())
    }
}

/// Re-read the registry and every new instance's catalogue; tell subscribers when the set changed.
pub fn refresh(state: &Shared, registry: &Path, panels: &[String]) {
    let found = discover(registry, panels);
    let known: Vec<Instance> = state.lock().unwrap().instances.clone();
    let mut fresh = BTreeMap::new();
    for i in &found {
        if !known.contains(i) || !state.lock().unwrap().ops.contains_key(&i.name) {
            fresh.insert(i.name.clone(), catalogue_of(i));
        }
    }
    let mut s = state.lock().unwrap();
    let changed = s.instances != found;
    s.instances = found;
    let names: Vec<String> = s.instances.iter().map(|i| i.name.clone()).collect();
    s.ops.retain(|k, _| names.contains(k));
    s.errors.retain(|k, _| names.contains(k));
    for (name, res) in fresh {
        match res {
            Ok(ops) => { s.ops.insert(name.clone(), ops); s.errors.remove(&name); }
            Err(e) => { s.errors.insert(name, e); }
        }
    }
    if changed {
        let msg = json!({"jsonrpc": "2.0", "method": "hub.instances", "params": s.instances_json()});
        s.broadcast(None, msg, None);
    }
}

/// Read every panel's events since the last one seen and push them to the subscribers that want
/// them. The first read of a panel only takes its position, so a subscriber sees what happens next.
pub fn fan_in(state: &Shared) {
    let panels: Vec<(String, String, Option<i64>)> = {
        let s = state.lock().unwrap();
        s.instances.iter().filter_map(|i| match &i.access {
            Access::Panel { base } => Some((i.name.clone(), base.clone(), s.seq.get(&i.name).copied())),
            _ => None,
        }).collect()
    };
    for (name, base, since) in panels {
        let url = format!("{base}/api/events?since={}&limit=500", since.unwrap_or(i64::MAX / 2));
        let Ok(v) = http_json(agent().get(&url).call()) else { continue };
        let last = v.get("last_seq").and_then(Value::as_i64);
        let mut s = state.lock().unwrap();
        if since.is_some() {
            for e in v.get("events").and_then(Value::as_array).into_iter().flatten() {
                let msg = json!({"jsonrpc": "2.0", "method": "hub.event", "params": {"instance": name, "event": e}});
                s.broadcast(Some(&name), msg, Some(e));
            }
        }
        if let Some(l) = last { s.seq.insert(name, l); }
    }
}

/// The catchment radius of each facility, used only for a station whose instance does not publish
/// `catchment_radius` (an older binary). Taken from src/station_type.h (CA_BUS, CA_TRUCK, CA_TRAIN,
/// CA_DOCK, and CA_UNMODIFIED for an airport, whose real radius comes from its spec).
const FALLBACK_RADIUS: [(&str, i64); 5] = [("bus", 3), ("truck", 3), ("train", 4), ("dock", 5), ("airport", 4)];

fn facilities(s: &Value) -> Vec<String> {
    s.get("facilities").and_then(Value::as_array)
        .map(|a| a.iter().filter_map(|f| f.as_str().map(str::to_string)).collect()).unwrap_or_default()
}

/// The station's catchment as (x0, y0, x1, y1), inclusive: the game's own area when published,
/// else a square of its radius around its sign tile.
pub fn catchment(s: &Value) -> Option<(i64, i64, i64, i64)> {
    if let Some(a) = s.get("catchment_area").filter(|a| a.is_object()) {
        let (x, y) = (a.get("x")?.as_i64()?, a.get("y")?.as_i64()?);
        return Some((x, y, x + a.get("w")?.as_i64()? - 1, y + a.get("h")?.as_i64()? - 1));
    }
    let (x, y) = (s.get("x")?.as_i64()?, s.get("y")?.as_i64()?);
    let r = s.get("catchment_radius").and_then(Value::as_i64).unwrap_or_else(|| {
        facilities(s).iter().filter_map(|f| FALLBACK_RADIUS.iter().find(|(n, _)| n == f).map(|(_, r)| *r)).max().unwrap_or(0)
    });
    Some((x - r, y - r, x + r, y + r))
}

/// Pairs of stations that duplicate each other: one owner, at least one facility in common, and
/// catchments that overlap, so both serve the same tiles with the same kind of transport. Two
/// stations of one owner far apart, or a bus stop beside its own airport, are not duplicates.
pub fn duplicate_stations(stations: &[Value]) -> Vec<Value> {
    let mut out = Vec::new();
    for (i, a) in stations.iter().enumerate() {
        for b in &stations[i + 1..] {
            let (Some(oa), Some(ob)) = (a.get("owner"), b.get("owner")) else { continue };
            if oa != ob || oa.is_null() { continue; }
            let fb = facilities(b);
            let shared: Vec<String> = facilities(a).into_iter().filter(|f| fb.contains(f)).collect();
            if shared.is_empty() { continue; }
            let (Some(ca), Some(cb)) = (catchment(a), catchment(b)) else { continue };
            if ca.0 > cb.2 || cb.0 > ca.2 || ca.1 > cb.3 || cb.1 > ca.3 { continue; }
            let pick = |s: &Value| json!({"id": s["id"], "name": s["name"], "x": s["x"], "y": s["y"], "tile": s["tile"], "town": s["town"]});
            let d = (a["x"].as_i64().unwrap_or(0) - b["x"].as_i64().unwrap_or(0)).abs()
                .max((a["y"].as_i64().unwrap_or(0) - b["y"].as_i64().unwrap_or(0)).abs());
            out.push(json!({"owner": oa, "facilities": shared, "a": pick(a), "b": pick(b), "distance": d}));
        }
    }
    out
}

/// The station list inside whatever an instance answered: an MCP resources/read result, a panel's
/// `{availability, data}` envelope, or a bare `{stations}`.
pub fn stations_in(v: &Value) -> Vec<Value> {
    let mut v = v.clone();
    if let Some(text) = v.pointer("/contents/0/text").and_then(Value::as_str) {
        v = serde_json::from_str(text).unwrap_or(Value::Null);
    }
    let data = v.get("data").filter(|d| d.is_object()).cloned().unwrap_or(v);
    data.get("stations").and_then(Value::as_array).cloned().unwrap_or_default()
}

/// The read-only operation of an instance that lists its stations, found by name, not configured.
pub fn stations_op(ops: &[Op]) -> Option<&Op> {
    ops.iter().find(|o| o.id == "resource:openttd://stations")
        .or_else(|| ops.iter().filter(|o| o.read_only && !o.stream && o.method == "GET" && o.query.iter().all(|q| !q.1))
            .find(|o| o.path.ends_with("/stations") && o.path.contains("resource")))
}

/// The road network as openttd://map/ways publishes it: per tile the directions a road vehicle can
/// leave it (NW=1 y-1, SW=2 x+1, SE=4 y+1, NE=8 x-1); two tiles are joined when each points at the other.
pub struct Roads { pub width: i64, pub height: i64, rows: Vec<Vec<u8>> }

impl Roads {
    pub fn from_ways(v: &Value) -> Option<Roads> {
        let d = unwrap_data(v);
        let width = d.get("width")?.as_i64()?;
        let rows: Vec<Vec<u8>> = d.get("roads")?.as_array()?.iter()
            .map(|r| r.as_str().unwrap_or("").chars().map(|c| c.to_digit(16).unwrap_or(0) as u8).collect()).collect();
        Some(Roads { width, height: rows.len() as i64, rows })
    }

    fn bits(&self, x: i64, y: i64) -> u8 {
        if x < 0 || y < 0 || x >= self.width || y >= self.height { return 0; }
        self.rows[y as usize].get(x as usize).copied().unwrap_or(0)
    }

    /// The number of road steps from one tile to another over joined tiles, if at most `limit`.
    pub fn distance(&self, from: (i64, i64), to: (i64, i64), limit: i64) -> Option<i64> {
        const STEPS: [(u8, i64, i64, u8); 4] = [(1, 0, -1, 4), (2, 1, 0, 8), (4, 0, 1, 1), (8, -1, 0, 2)];
        let mut seen = std::collections::HashSet::from([from]);
        let mut q = std::collections::VecDeque::from([(from, 0i64)]);
        while let Some(((x, y), d)) = q.pop_front() {
            if (x, y) == to { return Some(d); }
            if d >= limit { continue; }
            for (bit, dx, dy, back) in STEPS {
                let n = (x + dx, y + dy);
                if self.bits(x, y) & bit != 0 && self.bits(n.0, n.1) & back != 0 && seen.insert(n) {
                    q.push_back((n, d + 1));
                }
            }
        }
        None
    }
}

fn unwrap_data(v: &Value) -> Value {
    let mut v = v.clone();
    if let Some(text) = v.pointer("/contents/0/text").and_then(Value::as_str) {
        v = serde_json::from_str(text).unwrap_or(Value::Null);
    }
    v.get("data").filter(|d| d.is_object()).cloned().unwrap_or(v)
}

/// Pairs of depots that duplicate each other: one owner, one type, and the second within the reach a
/// vehicle searches for a depot to be serviced at (`service_reach_tiles`, the game's own setting),
/// measured along the road network when its ways are known, else as a straight distance (and said so).
pub fn duplicate_depots(depots: &[Value], reach: i64, roads: Option<&Roads>) -> Vec<Value> {
    let mut out = Vec::new();
    let xy = |d: &Value| (d["x"].as_i64().unwrap_or(0), d["y"].as_i64().unwrap_or(0));
    for (i, a) in depots.iter().enumerate() {
        for b in &depots[i + 1..] {
            if a["owner"] != b["owner"] || a["owner"].is_null() || a["type"] != b["type"] { continue; }
            let (pa, pb) = (xy(a), xy(b));
            let straight = (pa.0 - pb.0).abs().max((pa.1 - pb.1).abs());
            let (dist, by) = match (a["type"].as_str(), roads) {
                (Some("road"), Some(r)) => match r.distance(pa, pb, reach) { Some(d) => (d, "road network"), None => continue },
                _ => (straight, "straight distance"),
            };
            if dist > reach { continue; }
            let pick = |d: &Value| json!({"id": d["id"], "x": d["x"], "y": d["y"], "tile": d["tile"], "town": d["town"]});
            out.push(json!({"owner": a["owner"], "type": a["type"], "a": pick(a), "b": pick(b), "distance": dist, "by": by, "reach": reach}));
        }
    }
    out
}

/// The read-only operation of an instance that serves a resource, found by its name, not configured.
pub fn resource_op<'a>(ops: &'a [Op], resource: &str) -> Option<&'a Op> {
    let uri = format!("resource:openttd://{resource}");
    ops.iter().find(|o| o.id == uri).or_else(|| ops.iter()
        .filter(|o| o.read_only && !o.stream && o.method == "GET" && o.query.iter().all(|q| !q.1))
        .find(|o| o.path.contains("resource") && o.path.ends_with(&format!("/{resource}"))))
}

/// The depot findings of one instance: its depots, its ways when published, the duplicate pairs.
fn audit_depots(i: &Instance, ops: &[Op]) -> Value {
    let Some(op) = resource_op(ops, "depots") else { return json!({"error": "no depots operation"}) };
    let v = match call(i, op, None, None) { Ok(v) => unwrap_data(&v), Err(e) => return json!({"error": e}) };
    let depots = v.get("depots").and_then(Value::as_array).cloned().unwrap_or_default();
    let reach = v.get("service_reach_tiles").and_then(Value::as_i64).unwrap_or(20);
    let roads = resource_op(ops, "map/ways").and_then(|w| call(i, w, None, None).ok()).and_then(|w| Roads::from_ways(&w));
    json!({"depots": depots.len(), "reach": reach, "network": roads.is_some(),
           "duplicates": duplicate_depots(&depots, reach, roads.as_ref())})
}

/// Every instance's duplicate stations and duplicate depots.
pub fn audit(state: &Shared) -> Value {
    let targets: Vec<(Instance, Option<Op>, Vec<Op>)> = {
        let s = state.lock().unwrap();
        s.instances.iter().map(|i| {
            let ops = s.ops.get(&i.name).cloned().unwrap_or_default();
            (i.clone(), stations_op(&ops).cloned(), ops)
        }).collect()
    };
    Value::Array(targets.into_iter().map(|(i, op, ops)| match op {
        None => json!({"instance": i.name, "error": "no stations operation"}),
        Some(op) => match call(&i, &op, None, None) {
            Err(e) => json!({"instance": i.name, "error": e}),
            Ok(v) => {
                let st = stations_in(&v);
                json!({"instance": i.name, "via": op.id, "stations": st.len(),
                       "catchment": if st.iter().any(|s| s.get("catchment_area").is_some()) { "game" } else { "fallback radius" },
                       "duplicates": duplicate_stations(&st),
                       "depots": audit_depots(&i, &ops)})
            }
        },
    }).collect())
}

fn err(id: &Value, code: i64, message: impl Into<String>) -> Value {
    json!({"jsonrpc": "2.0", "id": id, "error": {"code": code, "message": message.into()}})
}

fn ok(id: &Value, result: Value) -> Value {
    json!({"jsonrpc": "2.0", "id": id, "result": result})
}

pub fn methods() -> Value {
    json!([
        {"method": "rpc.discover", "params": {}},
        {"method": "hub.instances", "params": {}},
        {"method": "hub.catalogue", "params": {"instance": "optional: one instance"}},
        {"method": "hub.call", "params": {"instance": "required", "id": "operationId | tool:<name> | resource:<uri>", "body": "object", "query": "object"}},
        {"method": "hub.subscribe", "params": {"instances": "optional list", "kinds": "optional list of kind prefixes"}},
        {"method": "hub.unsubscribe", "params": {}},
        {"method": "hub.audit", "params": {}, "result": "per instance: duplicate stations (same owner, a shared facility, overlapping catchments) and duplicate depots (same owner and type within the game's service reach, along the road network when known)"},
    ])
}

/// The hub's own OpenAPI document, derived from `methods()`: every JSON-RPC method is reachable as
/// `POST /hub/rpc`, and the read-only ones also as a GET route, so what the hub offers is in
/// OpenAPI and Swagger like every panel route.
pub fn openapi() -> Value {
    let methods = methods();
    let names: Vec<Value> = methods.as_array().into_iter().flatten().filter_map(|m| m.get("method").cloned()).collect();
    let get = |id: &str, summary: &str| json!({"get": {"operationId": id, "summary": summary, "tags": ["hub"],
        "x-mcp-annotations": {"readOnlyHint": true}, "responses": {"200": {"description": "JSON"}}}});
    json!({
        "openapi": "3.1.0",
        "info": {"title": "MCP hub (RESEARCH-ONLY)", "version": "1",
                 "description": "One surface over every MCP instance: research/mcp-hub.md. WebSocket: GET /ws (JSON-RPC 2.0, the same methods, plus hub.subscribe)."},
        "paths": {
            "/hub/rpc": {"post": {"operationId": "hubRpc", "summary": "One JSON-RPC 2.0 request", "tags": ["hub"],
                "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["jsonrpc", "method"],
                    "properties": {"jsonrpc": {"const": "2.0"}, "id": {}, "method": {"enum": names}, "params": {"type": "object"}}}}}},
                "responses": {"200": {"description": "JSON-RPC response"}}, "x-hub-methods": methods}},
            "/hub/instances": get("hubInstances", "Every live instance"),
            "/hub/catalogue": get("hubCatalogue", "Every operation of every instance"),
            "/hub/audit": get("hubAudit", "Duplicate stations per instance (one owner, a shared facility, overlapping catchments)"),
            "/openapi.json": get("hubOpenapi", "This document"),
            "/docs": get("hubDocs", "Swagger UI over this document"),
            "/": get("hubPage", "The hub page: every instance, operation, event and audit, from static/components.js"),
            "/graphql/schema": get("hubGraphqlSchema", "The GraphQL schema (SDL), derived from the method table"),
            "/graphql": {"post": {"operationId": "hubGraphql", "summary": "One GraphQL query or mutation over the hub's methods (a subset: no fragments, directives, introspection or subscriptions)", "tags": ["hub"],
                "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["query"],
                    "properties": {"query": {"type": "string"}, "variables": {"type": "object"}, "operationName": {"type": "string"}}}}}},
                "responses": {"200": {"description": "{data, errors?}"}}}},
            "/viz": get("hubWorldView", "The World view: the world graph, the terrain in 3D and the economy (static/viz.html)"),
        }
    })
}

/// Answer one JSON-RPC request. `subscriber` is the connection's subscriber id when it can stream.
pub fn handle(state: &Shared, req: &Value, subscriber: Option<u64>) -> Option<Value> {
    let id = req.get("id").cloned().unwrap_or(Value::Null);
    let p = req.get("params").cloned().unwrap_or(json!({}));
    let Some(method) = req.get("method").and_then(Value::as_str) else {
        return Some(err(&id, -32600, "invalid request: no method"));
    };
    let reply = match method {
        "rpc.discover" => ok(&id, methods()),
        "hub.audit" => ok(&id, audit(state)),
        "hub.instances" => ok(&id, state.lock().unwrap().instances_json()),
        "hub.catalogue" => {
            let s = state.lock().unwrap();
            let only = p.get("instance").and_then(Value::as_str);
            let ops: Vec<Value> = s.ops.iter()
                .filter(|(k, _)| only.map_or(true, |o| o == k.as_str()))
                .flat_map(|(_, v)| v.iter().map(Op::to_json)).collect();
            ok(&id, Value::Array(ops))
        }
        "hub.call" => {
            let (Some(inst), Some(op_id)) = (p.get("instance").and_then(Value::as_str), p.get("id").and_then(Value::as_str)) else {
                return Some(err(&id, -32602, "hub.call needs instance and id"));
            };
            let found = state.lock().unwrap().find(inst, op_id);
            match found {
                None => err(&id, -32602, format!("no operation {op_id} on {inst}")),
                Some((i, op)) => match call(&i, &op, p.get("body"), p.get("query").and_then(Value::as_object)) {
                    Ok(v) => ok(&id, v),
                    Err(e) => err(&id, -32000, e),
                },
            }
        }
        "hub.subscribe" | "hub.unsubscribe" => {
            let Some(sid) = subscriber else { return Some(err(&id, -32601, "subscribe needs a WebSocket")) };
            let mut s = state.lock().unwrap();
            let filter = (method == "hub.subscribe").then(|| Filter::from_params(&p));
            if let Some(sub) = s.subscribers.iter_mut().find(|x| x.id == sid) { sub.filter = filter; }
            ok(&id, json!({"subscribed": method == "hub.subscribe"}))
        }
        m => err(&id, -32601, format!("unknown method {m}")),
    };
    req.get("id").map(|_| reply)
}

fn register(state: &Shared) -> (u64, mpsc::Receiver<Value>) {
    let (tx, rx) = mpsc::channel();
    let mut s = state.lock().unwrap();
    s.next_subscriber += 1;
    let id = s.next_subscriber;
    s.subscribers.push(Subscriber { id, tx, filter: None });
    (id, rx)
}

fn serve_ws(state: Shared, stream: TcpStream) {
    let Ok(mut ws) = tungstenite::accept(stream) else { return };
    let _ = ws.get_mut().set_read_timeout(Some(Duration::from_millis(50)));
    let (sid, rx) = register(&state);
    loop {
        match ws.read() {
            Ok(tungstenite::Message::Text(t)) => {
                let reply = match serde_json::from_str::<Value>(t.as_str()) {
                    Ok(req) => handle(&state, &req, Some(sid)),
                    Err(e) => Some(err(&Value::Null, -32700, format!("parse error: {e}"))),
                };
                if let Some(r) = reply {
                    if ws.send(tungstenite::Message::text(r.to_string())).is_err() { break; }
                }
            }
            Ok(tungstenite::Message::Close(_)) => break,
            Ok(_) => {}
            Err(tungstenite::Error::Io(e)) if matches!(e.kind(), std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut) => {}
            Err(_) => break,
        }
        let mut gone = false;
        while let Ok(msg) = rx.try_recv() {
            if ws.send(tungstenite::Message::text(msg.to_string())).is_err() { gone = true; break; }
        }
        if gone { break; }
    }
    state.lock().unwrap().subscribers.retain(|s| s.id != sid);
}

/// Swagger UI over the hub's own OpenAPI document, the same page the panels serve at /docs.
const SWAGGER_PAGE: &str = r#"<!doctype html><html><head><meta charset="utf-8"><title>MCP hub API</title>
<link rel="stylesheet" href="/vendor/swagger-ui/swagger-ui.css"></head><body>
<div id="swagger"></div><script src="/vendor/swagger-ui/swagger-ui-bundle.js"></script>
<script>SwaggerUIBundle({url: '/openapi.json', dom_id: '#swagger', deepLinking: true});</script></body></html>"#;

fn respond(mut stream: TcpStream, code: u16, ctype: &str, body: &[u8]) {
    let reason = match code { 200 => "OK", 400 => "Bad Request", 404 => "Not Found", _ => "Error" };
    let head = format!("HTTP/1.1 {code} {reason}\r\nContent-Type: {ctype}\r\nContent-Length: {}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", body.len());
    let _ = stream.write_all(head.as_bytes());
    let _ = stream.write_all(body);
}

fn serve_http(state: Shared, mut stream: TcpStream, statics: &Path) {
    let _ = stream.set_read_timeout(Some(Duration::from_secs(5)));
    let mut buf = Vec::new();
    let mut chunk = [0u8; 4096];
    let header_end = loop {
        let Ok(n) = stream.read(&mut chunk) else { return };
        if n == 0 { return; }
        buf.extend_from_slice(&chunk[..n]);
        if let Some(p) = buf.windows(4).position(|w| w == b"\r\n\r\n") { break p + 4; }
        if buf.len() > 65536 { return; }
    };
    let head = String::from_utf8_lossy(&buf[..header_end]).to_string();
    let mut parts = head.split_whitespace();
    let (method, target) = (parts.next().unwrap_or(""), parts.next().unwrap_or("/"));
    let path = target.split('?').next().unwrap_or("/");
    let length: usize = head.lines()
        .find_map(|l| l.split_once(':').filter(|(k, _)| k.eq_ignore_ascii_case("content-length")).map(|(_, v)| v.trim().parse().unwrap_or(0)))
        .unwrap_or(0).min(1 << 20);
    while buf.len() < header_end + length {
        let Ok(n) = stream.read(&mut chunk) else { break };
        if n == 0 { break; }
        buf.extend_from_slice(&chunk[..n]);
    }
    let body = &buf[header_end..buf.len().min(header_end + length)];
    let json_ct = "application/json";
    match (method, path) {
        ("GET", "/" | "/hub") => match std::fs::read(statics.join("hub.html")) {
            Ok(b) => respond(stream, 200, "text/html; charset=utf-8", &b),
            Err(_) => respond(stream, 404, json_ct, br#"{"error":"hub.html not found"}"#),
        },
        ("GET", "/hub/instances") => respond(stream, 200, json_ct, state.lock().unwrap().instances_json().to_string().as_bytes()),
        ("GET", "/docs") => respond(stream, 200, "text/html; charset=utf-8", SWAGGER_PAGE.as_bytes()),
        ("GET", p) if p.starts_with("/vendor/swagger-ui/") => {
            // The panel's own copy (npm ci in tools/research/mcp-panel), so the hub ships no asset.
            let (rel, ctype) = match &p["/vendor/swagger-ui/".len()..] {
                "swagger-ui.css" => ("swagger-ui.css", "text/css"),
                "swagger-ui-bundle.js" => ("swagger-ui-bundle.js", "text/javascript"),
                _ => return respond(stream, 404, json_ct, br#"{"error":"not found"}"#),
            };
            match std::fs::read(statics.join("../node_modules/swagger-ui-dist").join(rel)) {
                Ok(b) => respond(stream, 200, ctype, &b),
                Err(_) => respond(stream, 404, json_ct, br#"{"error":"run npm ci in tools/research/mcp-panel"}"#),
            }
        }
        ("POST", "/graphql") => match serde_json::from_slice::<Value>(body) {
            Ok(req) => {
                let rpc = |method: &str, params: Value| -> Result<Value, String> {
                    let r = handle(&state, &json!({"jsonrpc": "2.0", "id": 0, "method": method, "params": params}), None)
                        .unwrap_or(Value::Null);
                    match r.get("error") { Some(e) => Err(e["message"].as_str().unwrap_or("error").to_string()), None => Ok(r["result"].clone()) }
                };
                respond(stream, 200, json_ct, graphql::execute(&req, &methods(), &rpc).to_string().as_bytes())
            }
            Err(e) => respond(stream, 400, json_ct, json!({"errors": [{"message": e.to_string()}]}).to_string().as_bytes()),
        },
        ("GET", "/graphql/schema") => respond(stream, 200, "text/plain; charset=utf-8", graphql::sdl(&methods()).as_bytes()),
        ("GET", "/hub/audit") => respond(stream, 200, json_ct, audit(&state).to_string().as_bytes()),
        ("GET", "/openapi.json") => respond(stream, 200, json_ct, openapi().to_string().as_bytes()),
        ("GET", "/hub/catalogue") => {
            let r = handle(&state, &json!({"jsonrpc": "2.0", "id": 0, "method": "hub.catalogue"}), None).unwrap();
            respond(stream, 200, json_ct, r["result"].to_string().as_bytes())
        }
        ("POST", "/hub/rpc") => match serde_json::from_slice::<Value>(body) {
            Ok(req) => {
                let r = handle(&state, &req, None).unwrap_or(json!(null));
                respond(stream, 200, json_ct, r.to_string().as_bytes())
            }
            Err(e) => respond(stream, 400, json_ct, json!({"error": e.to_string()}).to_string().as_bytes()),
        },
        ("GET", p) if p.starts_with("/vendor/npm/") => {
            // The panel's own front-end packages (npm ci in tools/research/mcp-panel), allow-listed, so
            // a page can load a library without the hub shipping or the repository committing a copy.
            const PACKAGES: [&str; 4] = ["echarts", "three", "d3", "flowbite"];
            let rel = &p["/vendor/npm/".len()..];
            let pkg = rel.split('/').next().unwrap_or("");
            if rel.contains("..") || !PACKAGES.contains(&pkg) { return respond(stream, 404, json_ct, br#"{"error":"not found"}"#); }
            let f = statics.join("../node_modules").join(rel);
            let ctype = match f.extension().and_then(|e| e.to_str()) {
                Some("js" | "mjs") => "text/javascript", Some("css") => "text/css", Some("map" | "json") => "application/json", _ => "application/octet-stream",
            };
            match std::fs::read(&f) {
                Ok(b) => respond(stream, 200, ctype, &b),
                Err(_) => respond(stream, 404, json_ct, br#"{"error":"not found (npm ci in tools/research/mcp-panel)"}"#),
            }
        }
        ("GET", "/viz") => match std::fs::read(statics.join("viz.html")) {
            Ok(b) => respond(stream, 200, "text/html; charset=utf-8", &b),
            Err(_) => respond(stream, 404, json_ct, br#"{"error":"viz.html not found"}"#),
        },
        ("GET", p) if p.starts_with("/static/") => {
            let rel = &p["/static/".len()..];
            if rel.contains("..") || rel.starts_with('/') { return respond(stream, 404, json_ct, b"{}"); }
            let f = statics.join(rel);
            let ctype = match f.extension().and_then(|e| e.to_str()) {
                Some("css") => "text/css", Some("js") => "text/javascript", Some("html") => "text/html", _ => "application/octet-stream",
            };
            match std::fs::read(&f) {
                Ok(b) => respond(stream, 200, ctype, &b),
                Err(_) => respond(stream, 404, json_ct, br#"{"error":"not found"}"#),
            }
        }
        _ => respond(stream, 404, json_ct, br#"{"error":"not found"}"#),
    }
}

/// Is the request waiting on this socket a WebSocket upgrade? Peeks, so the handshake still sees it.
fn is_upgrade(stream: &TcpStream) -> bool {
    let mut buf = [0u8; 2048];
    let _ = stream.set_read_timeout(Some(Duration::from_secs(5)));
    let n = stream.peek(&mut buf).unwrap_or(0);
    let head = String::from_utf8_lossy(&buf[..n]).to_ascii_lowercase();
    head.starts_with("get /ws") && head.contains("upgrade: websocket")
}

pub struct Config {
    pub registry: PathBuf,
    pub panels: Vec<String>,
    pub statics: PathBuf,
    pub refresh: Duration,
    pub poll: Duration,
}

/// Start the discovery and fan-in threads and serve connections on `listener` until the process ends.
pub fn run(listener: TcpListener, cfg: Config) -> Shared {
    let state: Shared = Arc::new(Mutex::new(State::default()));
    refresh(&state, &cfg.registry, &cfg.panels);
    {
        let (state, registry, panels, every) = (state.clone(), cfg.registry.clone(), cfg.panels.clone(), cfg.refresh);
        thread::spawn(move || loop { thread::sleep(every); refresh(&state, &registry, &panels); });
    }
    {
        let (state, every) = (state.clone(), cfg.poll);
        thread::spawn(move || loop { fan_in(&state); thread::sleep(every); });
    }
    let accept_state = state.clone();
    let statics = cfg.statics.clone();
    thread::spawn(move || {
        for conn in listener.incoming().flatten() {
            let (state, statics) = (accept_state.clone(), statics.clone());
            thread::spawn(move || {
                if is_upgrade(&conn) { serve_ws(state, conn) } else { serve_http(state, conn, &statics) }
            });
        }
    });
    state
}
