//! RESEARCH-ONLY: Rhai calls Prismatic over loopback HTTP (I0092, research/rhai-ide.md).
//!
//! What prismatic_api serves today (research/rhai-ide.md, "What Prismatic offers over HTTP today"):
//! the OpenTTD decision (`GET /api/openttd/decision`), its MCP tools (`GET /api/mcp/tools`,
//! `POST /api/mcp/exec`) and, through them, the `blackboard.*` tools of `Prismatic.MCP.Blackboard`.
//! Every answer is checked against what this client expects before a script sees it, and every
//! failure is one typed error a script can catch, never invented data.
//!
//! - The address is `$PRISMATIC_URL` (default `http://127.0.0.1:4004`) and must be loopback.
//! - Tokens (`$PRISMATIC_OPENTTD_TOKEN`, `$PRISMATIC_MCP_TOKEN`) go only into an `Authorization`
//!   header: never into a URL, an error or a log line.
//! - prismatic-platform is never changed from here.

use std::time::Duration;

use rhai::{Dynamic, Engine, EvalAltResult, Map};
use serde_json::{json, Value};

/// Why a call to Prismatic did not give a usable answer.
#[derive(Debug, PartialEq)]
pub enum Error {
    /// The configuration is not usable (an address off loopback).
    Config(String),
    /// Nothing answered: refused, timed out, or the connection broke.
    Unreachable(String),
    /// Prismatic answered with an error status: the HTTP status, its code and message.
    Refused { status: u16, code: String, message: String },
    /// Prismatic answered, but not with what the contract says.
    Invalid(String),
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Config(m) => write!(f, "prismatic: configuration: {m}"),
            Error::Unreachable(m) => write!(f, "prismatic: unreachable: {m}"),
            Error::Refused { status, code, message } => write!(f, "prismatic: refused ({status} {code}): {message}"),
            Error::Invalid(m) => write!(f, "prismatic: invalid answer: {m}"),
        }
    }
}

/// Whether `url` is plain HTTP to this machine's loopback interface.
pub fn is_loopback(url: &str) -> bool {
    let Some(rest) = url.strip_prefix("http://") else { return false };
    let host = rest.split(['/', '?', '#']).next().unwrap_or("");
    if host.contains('@') { return false; }
    let name = if host.starts_with('[') { host.split(']').next().map(|h| format!("{h}]")).unwrap_or_default() } else { host.split(':').next().unwrap_or("").to_string() };
    matches!(name.as_str(), "127.0.0.1" | "localhost" | "[::1]")
}

#[derive(Clone, Debug)]
pub struct Client {
    base: String,
    openttd_token: Option<String>,
    mcp_token: Option<String>,
    timeout: Duration,
}

fn env_token(name: &str) -> Option<String> {
    std::env::var(name).ok().filter(|t| !t.is_empty())
}

impl Client {
    /// A client for `base`, refused unless it is loopback.
    pub fn new(base: &str, openttd_token: Option<String>, mcp_token: Option<String>, timeout: Duration) -> Result<Client, Error> {
        let base = base.trim_end_matches('/').to_string();
        if !is_loopback(&base) { return Err(Error::Config(format!("{base} is not a loopback http address"))); }
        Ok(Client { base, openttd_token, mcp_token, timeout })
    }

    /// The client the environment configures: `$PRISMATIC_URL` and the two tokens.
    pub fn from_env() -> Result<Client, Error> {
        let base = std::env::var("PRISMATIC_URL").ok().filter(|u| !u.is_empty()).unwrap_or_else(|| "http://127.0.0.1:4004".into());
        Client::new(&base, env_token("PRISMATIC_OPENTTD_TOKEN"), env_token("PRISMATIC_MCP_TOKEN"), Duration::from_secs(30))
    }

    fn call(&self, method: &str, path: &str, token: Option<&str>, body: Option<Value>) -> Result<Value, Error> {
        let mut req = ureq::request(method, &format!("{}{path}", self.base)).timeout(self.timeout);
        if let Some(t) = token { req = req.set("Authorization", &format!("Bearer {t}")); }
        let resp = match body { Some(b) => req.send_json(b), None => req.call() };
        match resp {
            Ok(r) => r.into_json::<Value>().map_err(|e| Error::Invalid(format!("{method} {path}: not JSON ({e})"))),
            Err(ureq::Error::Status(status, r)) => {
                let v: Value = r.into_json().unwrap_or(Value::Null);
                // Prismatic answers {ok: false, error: {code, message}}; a bare {error} or no JSON also occurs.
                let e = v.get("error").cloned().unwrap_or(Value::Null);
                let code = e.get("code").and_then(Value::as_str).unwrap_or(if status == 404 { "NOT_FOUND" } else { "HTTP_ERROR" }).to_string();
                let mut message = e.get("message").and_then(Value::as_str).or(e.as_str()).unwrap_or("no message").to_string();
                if code == "PROFILE_READ_ONLY" {
                    message.push_str(" (Prismatic's MCP profile is read_only: its blackboard.* tools, reads included, need PRISMATIC_MCP_PROFILE=full_access there)");
                }
                Err(Error::Refused { status, code, message })
            }
            Err(e) => Err(Error::Unreachable(format!("{method} {path}: {}", e.kind()))),
        }
    }

    /// Prismatic's decision for the bot (`GET /api/openttd/decision`), checked against the contract.
    pub fn decision(&self, company_id: Option<i64>) -> Result<Value, Error> {
        let path = match company_id { Some(c) => format!("/api/openttd/decision?company_id={c}"), None => "/api/openttd/decision".into() };
        let v = self.call("GET", &path, self.openttd_token.as_deref(), None)?;
        check_decision(&v)?;
        Ok(v)
    }

    /// The MCP tools Prismatic offers (`GET /api/mcp/tools`): their names and descriptions.
    pub fn tools(&self) -> Result<Value, Error> {
        let v = self.call("GET", "/api/mcp/tools", self.mcp_token.as_deref(), None)?;
        let tools = v.get("data").and_then(|d| d.get("tools").or(Some(d))).filter(|t| t.is_array()).or_else(|| v.get("tools").filter(|t| t.is_array()));
        tools.cloned().ok_or_else(|| Error::Invalid("GET /api/mcp/tools: no tool list".into()))
    }

    /// One MCP tool of Prismatic (`POST /api/mcp/exec`): its `data` on `{ok: true}`.
    pub fn exec(&self, tool: &str, args: Value) -> Result<Value, Error> {
        if tool.is_empty() || !tool.chars().all(|c| c.is_ascii_alphanumeric() || c == '.' || c == '_') {
            return Err(Error::Config(format!("{tool:?} is not a tool name")));
        }
        let v = self.call("POST", "/api/mcp/exec", self.mcp_token.as_deref(), Some(json!({"tool": tool, "arguments": args})))?;
        match v.get("ok").and_then(Value::as_bool) {
            Some(true) => Ok(v.get("data").cloned().unwrap_or(Value::Null)),
            Some(false) => {
                let e = v.get("error").cloned().unwrap_or(Value::Null);
                Err(Error::Refused { status: 200, code: e.get("code").and_then(Value::as_str).unwrap_or("TOOL_ERROR").into(),
                                     message: e.get("message").and_then(Value::as_str).unwrap_or("no message").into() })
            }
            None => Err(Error::Invalid(format!("POST /api/mcp/exec {tool}: no ok field"))),
        }
    }

    /// A blackboard entry (`blackboard.get {frame, key}`).
    pub fn bb_get(&self, frame: &str, key: &str) -> Result<Value, Error> {
        self.exec("blackboard.get", json!({"frame": frame, "key": key}))
    }

    /// Write a blackboard entry (`blackboard.put {frame, key, value}`); the value is an object.
    pub fn bb_put(&self, frame: &str, key: &str, value: Value) -> Result<Value, Error> {
        if !value.is_object() { return Err(Error::Config("a blackboard value is an object (a Rhai map)".into())); }
        self.exec("blackboard.put", json!({"frame": frame, "key": key, "value": value}))
    }

    /// The entries of a frame (`blackboard.query {frame, limit}`).
    pub fn bb_query(&self, frame: &str, limit: i64) -> Result<Value, Error> {
        self.exec("blackboard.query", json!({"frame": frame, "limit": limit.clamp(1, 1000)}))
    }
}

/// The decision contract as Prismatic answers it: `ai`, `settings` (`name=int` pairs),
/// `chosen_aggressiveness` (0-100), `expected_value` and its `candidates`.
pub fn check_decision(v: &Value) -> Result<(), Error> {
    let bad = |m: &str| Err(Error::Invalid(format!("decision: {m}")));
    if !v.get("ai").is_some_and(Value::is_string) { return bad("ai is not a string"); }
    let settings = v.get("settings").and_then(Value::as_str).unwrap_or("");
    let pair_ok = |p: &str| p.split_once('=').is_some_and(|(n, x)| !n.is_empty() && x.parse::<i64>().is_ok());
    if settings.is_empty() || !settings.split(',').all(pair_ok) { return bad("settings is not name=int pairs"); }
    if !v.get("chosen_aggressiveness").and_then(Value::as_i64).is_some_and(|a| (0..=100).contains(&a)) { return bad("chosen_aggressiveness is not 0-100"); }
    if !v.get("expected_value").is_some_and(Value::is_number) { return bad("expected_value is not a number"); }
    let candidates = v.get("candidates").and_then(Value::as_array).filter(|c| !c.is_empty());
    let cand_ok = |c: &Value| c.get("aggressiveness").is_some_and(Value::is_i64) && c.get("expected_value").is_some_and(Value::is_number);
    if !candidates.is_some_and(|c| c.iter().all(cand_ok)) { return bad("candidates is not a list of {aggressiveness, expected_value}"); }
    Ok(())
}

fn to_rhai(r: Result<Value, Error>) -> Result<Dynamic, Box<EvalAltResult>> {
    let v = r.map_err(|e| e.to_string())?;
    rhai::serde::to_dynamic(v)
}

fn from_map(m: Map) -> Result<Value, Box<EvalAltResult>> {
    rhai::serde::from_dynamic(&Dynamic::from_map(m))
}

/// The `prismatic_*` functions a script calls, each with the client the environment configures at
/// the call (so a changed `$PRISMATIC_URL` applies to the next call). A failure is a Rhai error
/// whose text says which kind it is, for `try { ... } catch (e) { ... }`.
pub fn register(e: &mut Engine) {
    fn client() -> Result<Client, Box<EvalAltResult>> { Client::from_env().map_err(|e| e.to_string().into()) }
    e.register_fn("prismatic_decision", || to_rhai(client()?.decision(None)));
    e.register_fn("prismatic_decision", |company: i64| to_rhai(client()?.decision(Some(company))));
    e.register_fn("prismatic_tools", || to_rhai(client()?.tools()));
    e.register_fn("prismatic_exec", |tool: &str, args: Map| to_rhai(client()?.exec(tool, from_map(args)?)));
    e.register_fn("prismatic_bb_get", |frame: &str, key: &str| to_rhai(client()?.bb_get(frame, key)));
    e.register_fn("prismatic_bb_put", |frame: &str, key: &str, value: Map| to_rhai(client()?.bb_put(frame, key, from_map(value)?)));
    e.register_fn("prismatic_bb_query", |frame: &str, limit: i64| to_rhai(client()?.bb_query(frame, limit)));
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Read, Write};
    use std::net::TcpListener;
    use std::sync::{Arc, Mutex};

    /// A fixture of prismatic_api on loopback: each request answered from `answers` by its path
    /// (`METHOD /path`), every request's head and body recorded. A path answered with status 0
    /// closes the connection without an answer; one prefixed `delay:` waits two seconds first.
    struct Fixture {
        base: String,
        seen: Arc<Mutex<Vec<String>>>,
    }

    fn fixture(answers: Vec<(&'static str, u16, &'static str)>) -> Fixture {
        let l = TcpListener::bind("127.0.0.1:0").unwrap();
        let base = format!("http://{}", l.local_addr().unwrap());
        let seen = Arc::new(Mutex::new(Vec::new()));
        let log = seen.clone();
        std::thread::spawn(move || {
            for s in l.incoming() {
                let Ok(mut s) = s else { continue };
                let mut buf = vec![0u8; 65536];
                let mut n = 0;
                loop {
                    let k = s.read(&mut buf[n..]).unwrap_or(0);
                    n += k;
                    if k == 0 || complete(&buf[..n]) { break; }
                }
                let text = String::from_utf8_lossy(&buf[..n]).to_string();
                log.lock().unwrap().push(text.clone());
                let line = text.lines().next().unwrap_or("");
                let mut parts = line.split(' ');
                let key = format!("{} {}", parts.next().unwrap_or(""), parts.next().unwrap_or("").split('?').next().unwrap_or(""));
                let Some((_, status, body)) = answers.iter().find(|(k, _, _)| k.trim_start_matches("delay:") == key) else { continue };
                if answers.iter().any(|(k, _, _)| k.starts_with("delay:") && k.trim_start_matches("delay:") == key) { std::thread::sleep(Duration::from_secs(2)); }
                if *status == 0 { continue; }
                let _ = write!(s, "HTTP/1.1 {status} X\r\ncontent-type: application/json\r\ncontent-length: {}\r\nconnection: close\r\n\r\n{body}", body.len());
            }
        });
        Fixture { base, seen }
    }

    /// Whether `b` holds a whole request: its head and as many body bytes as Content-Length says.
    fn complete(b: &[u8]) -> bool {
        let text = String::from_utf8_lossy(b);
        let len = text.lines().find_map(|l| l.to_ascii_lowercase().strip_prefix("content-length: ").map(|v| v.trim().parse::<usize>().unwrap_or(0))).unwrap_or(0);
        text.find("\r\n\r\n").is_some_and(|end| b.len() >= end + 4 + len)
    }

    const DECISION: &str = r#"{"ai": "PrismaticAI", "company_id": 0, "settings": "aggressiveness=75", "chosen_aggressiveness": 75, "expected_value": 1234.5,
        "candidates": [{"aggressiveness": 50, "expected_value": 1000.0}, {"aggressiveness": 75, "expected_value": 1234.5}], "company_input": {}}"#;

    fn client(f: &Fixture) -> Client {
        Client::new(&f.base, Some("ottd-secret".into()), Some("mcp-secret".into()), Duration::from_millis(800)).unwrap()
    }

    #[test]
    fn only_loopback_addresses_are_accepted() {
        for ok in ["http://127.0.0.1:4004", "http://localhost", "http://[::1]:1/x"] { assert!(is_loopback(ok), "{ok}"); }
        for bad in ["https://127.0.0.1", "http://10.0.0.1", "http://localhost:1@evil.com", "http://example.com/127.0.0.1", "ftp://localhost"] {
            assert!(!is_loopback(bad), "{bad}");
        }
        assert!(matches!(Client::new("http://192.168.1.2:4004", None, None, Duration::from_secs(1)), Err(Error::Config(_))));
        assert_eq!(Client::new("http://127.0.0.1:4004/", None, None, Duration::from_secs(1)).unwrap().base, "http://127.0.0.1:4004");
    }

    #[test]
    fn the_decision_is_read_with_its_token_in_a_header_only() {
        let f = fixture(vec![("GET /api/openttd/decision", 200, DECISION)]);
        let c = client(&f);
        let d = c.decision(Some(1)).unwrap();
        assert_eq!(d["chosen_aggressiveness"], 75);
        assert_eq!(c.decision(None).unwrap()["ai"], "PrismaticAI");
        let seen = f.seen.lock().unwrap().join("\n");
        assert!(seen.contains("GET /api/openttd/decision?company_id=1 "));
        assert!(seen.to_ascii_lowercase().contains("authorization: bearer ottd-secret"));
        assert!(!seen.lines().next().unwrap().contains("secret"), "no token in the request line");
    }

    #[test]
    fn a_decision_off_the_contract_is_invalid() {
        let good: Value = serde_json::from_str(DECISION).unwrap();
        assert_eq!(check_decision(&good), Ok(()));
        let broken = |path: &str, v: Value| { let mut d = good.clone(); d[path] = v; check_decision(&d) };
        for (field, v) in [("ai", json!(1)), ("settings", json!("aggressiveness")), ("settings", json!("")), ("settings", json!("=5")), ("settings", json!("a=x")),
                           ("chosen_aggressiveness", json!(101)), ("chosen_aggressiveness", json!("75")), ("expected_value", json!("x")),
                           ("candidates", json!([])), ("candidates", json!([{"aggressiveness": "a", "expected_value": 1}])), ("candidates", json!([{"aggressiveness": 1}]))] {
            assert!(matches!(broken(field, v.clone()), Err(Error::Invalid(_))), "{field} = {v}");
        }
        assert!(check_decision(&json!({"ai": "x", "settings": "a=1,b=-2", "chosen_aggressiveness": 0, "expected_value": 0, "candidates": [{"aggressiveness": 0, "expected_value": 0}]})).is_ok());
        let f = fixture(vec![("GET /api/openttd/decision", 200, r#"{"ai": "PrismaticAI"}"#)]);
        assert!(matches!(client(&f).decision(None), Err(Error::Invalid(m)) if m.contains("settings")));
    }

    #[test]
    fn refusals_carry_prismatics_code_and_never_a_token() {
        let f = fixture(vec![
            ("GET /api/openttd/decision", 502, r#"{"ok": false, "error": {"code": "BRIDGE_UNREACHABLE", "message": "no bridge"}}"#),
            ("GET /api/mcp/tools", 401, r#"{"error": "INVALID_TOKEN"}"#),
            ("POST /api/mcp/exec", 404, "not json"),
        ]);
        let c = client(&f);
        let e = c.decision(None).unwrap_err();
        assert_eq!(e, Error::Refused { status: 502, code: "BRIDGE_UNREACHABLE".into(), message: "no bridge".into() });
        assert!(e.to_string().starts_with("prismatic: refused (502 BRIDGE_UNREACHABLE)"));
        assert_eq!(c.tools().unwrap_err(), Error::Refused { status: 401, code: "HTTP_ERROR".into(), message: "INVALID_TOKEN".into() });
        assert_eq!(c.exec("blackboard.get", json!({})).unwrap_err(), Error::Refused { status: 404, code: "NOT_FOUND".into(), message: "no message".into() });
        for err in [e, c.tools().unwrap_err()] { assert!(!err.to_string().contains("secret"), "{err}"); }
    }

    #[test]
    fn a_read_only_profile_says_how_to_allow_writes() {
        let f = fixture(vec![("POST /api/mcp/exec", 403, r#"{"ok": false, "error": {"code": "PROFILE_READ_ONLY", "message": "tool is not read-only"}}"#)]);
        assert_eq!(client(&f).bb_put("openttd", "k", json!({"a": 1})).unwrap_err(), Error::Refused {
            status: 403, code: "PROFILE_READ_ONLY".into(),
            message: "tool is not read-only (Prismatic's MCP profile is read_only: its blackboard.* tools, reads included, need PRISMATIC_MCP_PROFILE=full_access there)".into(),
        });
    }

    #[test]
    fn tools_and_exec_read_the_envelope() {
        let f = fixture(vec![
            ("GET /api/mcp/tools", 200, r#"{"ok": true, "data": {"tools": [{"name": "blackboard.get"}]}}"#),
            ("POST /api/mcp/exec", 200, r#"{"ok": true, "data": {"value": {"a": 1}}}"#),
        ]);
        let c = client(&f);
        assert_eq!(c.tools().unwrap()[0]["name"], "blackboard.get");
        assert_eq!(c.bb_get("openttd", "k").unwrap()["value"]["a"], 1);
        assert_eq!(c.bb_put("openttd", "k", json!({"a": 2})).unwrap()["value"]["a"], 1);
        c.bb_query("openttd", 5000).unwrap();
        let seen = f.seen.lock().unwrap().join("\n");
        assert!(seen.contains(r#""tool":"blackboard.put""#) && seen.contains(r#""limit":1000"#), "{seen}");
        assert!(seen.to_ascii_lowercase().contains("authorization: bearer mcp-secret"));
        let bare = fixture(vec![("GET /api/mcp/tools", 200, r#"{"tools": [{"name": "x"}]}"#), ("POST /api/mcp/exec", 200, r#"{"ok": true}"#)]);
        assert_eq!(client(&bare).tools().unwrap()[0]["name"], "x");
        assert_eq!(client(&bare).exec("x", json!({})).unwrap(), Value::Null);
        let list = fixture(vec![("GET /api/mcp/tools", 200, r#"{"ok": true, "data": [{"name": "y"}]}"#)]);
        assert_eq!(client(&list).tools().unwrap()[0]["name"], "y");
    }

    #[test]
    fn answers_off_the_envelope_are_invalid_or_refused() {
        let f = fixture(vec![
            ("GET /api/mcp/tools", 200, r#"{"ok": true, "data": {"nope": 1}}"#),
            ("POST /api/mcp/exec", 200, r#"{"ok": false, "error": {"code": "UNKNOWN_TOOL", "message": "no such tool"}}"#),
            ("GET /api/openttd/decision", 200, "<html>"),
        ]);
        let c = client(&f);
        assert!(matches!(c.tools(), Err(Error::Invalid(_))));
        assert_eq!(c.exec("x", json!({})).unwrap_err(), Error::Refused { status: 200, code: "UNKNOWN_TOOL".into(), message: "no such tool".into() });
        assert!(matches!(c.decision(None), Err(Error::Invalid(m)) if m.contains("not JSON")));
        let g = fixture(vec![("POST /api/mcp/exec", 200, r#"{"ok": false}"#)]);
        assert_eq!(client(&g).exec("x", json!({})).unwrap_err(), Error::Refused { status: 200, code: "TOOL_ERROR".into(), message: "no message".into() });
        let h = fixture(vec![("POST /api/mcp/exec", 200, r#"{"data": 1}"#)]);
        assert!(matches!(client(&h).exec("x", json!({})), Err(Error::Invalid(_))));
    }

    #[test]
    fn bad_input_never_leaves_this_process() {
        let f = fixture(vec![]);
        let c = client(&f);
        for bad in ["", "rm -rf", "a/b", "x?y=1"] { assert!(matches!(c.exec(bad, json!({})), Err(Error::Config(_))), "{bad}"); }
        assert!(matches!(c.bb_put("f", "k", json!(5)), Err(Error::Config(_))));
        assert!(f.seen.lock().unwrap().is_empty(), "nothing was sent");
    }

    #[test]
    fn a_silent_or_slow_prismatic_is_unreachable() {
        let f = fixture(vec![("GET /api/mcp/tools", 0, ""), ("delay:GET /api/openttd/decision", 200, DECISION)]);
        let c = client(&f);
        assert!(matches!(c.tools(), Err(Error::Unreachable(_))));
        assert!(matches!(c.decision(None), Err(Error::Unreachable(_))));
        let closed = { let l = TcpListener::bind("127.0.0.1:0").unwrap(); l.local_addr().unwrap() };
        let gone = Client::new(&format!("http://{closed}"), None, None, Duration::from_millis(300)).unwrap();
        let e = gone.tools().unwrap_err();
        assert!(matches!(e, Error::Unreachable(_)) && e.to_string().starts_with("prismatic: unreachable"), "{e}");
        assert_eq!(Error::Config("x".into()).to_string(), "prismatic: configuration: x");
        assert_eq!(Error::Invalid("y".into()).to_string(), "prismatic: invalid answer: y");
    }

    /// Every Rhai function against one fixture, configured as a script finds it: by the environment.
    #[test]
    fn scripts_call_every_function_and_catch_its_errors() {
        let f = fixture(vec![
            ("GET /api/openttd/decision", 200, DECISION),
            ("GET /api/mcp/tools", 200, r#"{"ok": true, "data": {"tools": [{"name": "blackboard.get"}]}}"#),
            ("POST /api/mcp/exec", 200, r#"{"ok": true, "data": {"value": {"a": 1}}}"#),
        ]);
        let _env = ENV.lock().unwrap();
        std::env::set_var("PRISMATIC_URL", &f.base);
        std::env::set_var("PRISMATIC_MCP_TOKEN", "t");
        std::env::remove_var("PRISMATIC_OPENTTD_TOKEN");
        let mut e = Engine::new();
        register(&mut e);
        let out: i64 = e.eval(r#"
            let d = prismatic_decision();
            let d1 = prismatic_decision(1);
            let t = prismatic_tools();
            let x = prismatic_exec("blackboard.get", #{frame: "openttd", key: "k"});
            let g = prismatic_bb_get("openttd", "k");
            let p = prismatic_bb_put("openttd", "k", #{a: 2});
            let q = prismatic_bb_query("openttd", 10);
            d.chosen_aggressiveness + d1.chosen_aggressiveness + t.len() + x.value.a + g.value.a + p.value.a + q.value.a
        "#).unwrap();
        assert_eq!(out, 75 + 75 + 1 + 1 + 1 + 1 + 1);
        // try/catch is a statement in Rhai: the caught error is kept in a variable.
        let caught: String = e.eval(r#"let r = "no"; try { prismatic_exec("bad name", #{}); } catch (err) { r = err; } r"#).unwrap();
        assert!(caught.starts_with("prismatic: configuration"), "{caught}");
        std::env::set_var("PRISMATIC_URL", "http://example.com");
        let off: String = e.eval(r#"let r = "no"; try { prismatic_tools(); } catch (err) { r = err; } r"#).unwrap();
        assert!(off.contains("not a loopback"), "{off}");
        std::env::set_var("PRISMATIC_URL", "");
        assert_eq!(Client::from_env().unwrap().base, "http://127.0.0.1:4004");
        std::env::remove_var("PRISMATIC_URL");
        std::env::remove_var("PRISMATIC_MCP_TOKEN");
    }

    /// Tests that change the environment run one at a time.
    static ENV: Mutex<()> = Mutex::new(());
}
