//! RESEARCH-ONLY: Rhai reads and writes the typed coordination records of the Majordomus mesh
//! (research/agent-loop.md, Coordinate; prismatic-majordomus I4665, `mesh.records` and
//! `mesh.record.write`, released in Majordomus 1.2.0).
//!
//! The mesh journal is the one blackboard (the owner's answer of 2026-11-20): this client keeps
//! no records of its own. Every answer is checked against the shape Majordomus documents before a
//! script sees it, every failure is one typed error a script can catch, and nothing is invented.
//!
//! - The address is `$MAJORDOMUS_URL`, the server `majordomus serve ensure` prints for this
//!   checkout, and must be loopback. Unset is a configuration error, never a guess.
//! - Over HTTP a write names its session; it is the record's `session`, else
//!   `$MAJORDOMUS_SESSION`. Majordomus admits it only inside that session's live claims, and the
//!   author it records is the journal's `<stream>/<session>`, never a name in the input.
//! - What Majordomus refuses (`out_of_scope`, `not_own`, `stale_revision`, `invalid_input`,
//!   `idempotency_conflict`) reaches the script with its code and message.

use std::time::Duration;

use rhai::{Dynamic, Engine, EvalAltResult, Map};
use serde_json::{json, Value};

use super::prismatic::is_loopback;

/// The closed set of record kinds Majordomus admits.
pub const KINDS: &[&str] = &["observation", "hypothesis", "plan", "decision", "blocker", "handoff", "outcome"];
/// The parts a handoff must carry.
pub const HANDOFF_PARTS: &[&str] = &["objective", "current_state", "evidence", "blockers", "next_action"];
/// The filters `mesh.records` takes.
pub const FILTERS: &[&str] = &["record", "task", "kind", "author", "path", "live"];

/// Why a call to the mesh did not give a usable answer.
#[derive(Debug, PartialEq)]
pub enum Error {
    /// The configuration or the input is not usable; nothing was sent.
    Config(String),
    /// Nothing answered: refused, timed out, or the connection broke.
    Unreachable(String),
    /// Majordomus refused: the HTTP status, its code and message.
    Refused { status: u16, code: String, message: String },
    /// Majordomus answered, but not with the documented shape.
    Invalid(String),
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Config(m) => write!(f, "mesh: configuration: {m}"),
            Error::Unreachable(m) => write!(f, "mesh: unreachable: {m}"),
            Error::Refused { status, code, message } => write!(f, "mesh: refused ({status} {code}): {message}"),
            Error::Invalid(m) => write!(f, "mesh: invalid answer: {m}"),
        }
    }
}

#[derive(Clone, Debug)]
pub struct Client {
    base: String,
    session: Option<String>,
    timeout: Duration,
}

impl Client {
    /// A client for `base`, refused unless it is loopback.
    pub fn new(base: &str, session: Option<String>, timeout: Duration) -> Result<Client, Error> {
        let base = base.trim_end_matches('/').to_string();
        if !is_loopback(&base) { return Err(Error::Config(format!("{base} is not a loopback http address"))); }
        Ok(Client { base, session: session.filter(|s| !s.is_empty()), timeout })
    }

    /// The client the environment configures: `$MAJORDOMUS_URL` and `$MAJORDOMUS_SESSION`.
    pub fn from_env() -> Result<Client, Error> {
        let base = std::env::var("MAJORDOMUS_URL").ok().filter(|u| !u.is_empty())
            .ok_or_else(|| Error::Config("MAJORDOMUS_URL is not set (`majordomus serve ensure` prints this checkout's server)".into()))?;
        Client::new(&base, std::env::var("MAJORDOMUS_SESSION").ok(), Duration::from_secs(30))
    }

    fn call(&self, method: &str, path: &str, body: Option<Value>) -> Result<Value, Error> {
        let req = ureq::request(method, &format!("{}{path}", self.base)).timeout(self.timeout);
        let resp = match body { Some(b) => req.send_json(b), None => req.call() };
        match resp {
            Ok(r) => r.into_json::<Value>().map_err(|e| Error::Invalid(format!("{method} {path}: not JSON ({e})"))),
            Err(ureq::Error::Status(status, r)) => {
                // Majordomus answers {error: {code, message}}.
                let v: Value = r.into_json().unwrap_or(Value::Null);
                let e = v.get("error").cloned().unwrap_or(Value::Null);
                Err(Error::Refused {
                    status,
                    code: e.get("code").and_then(Value::as_str).unwrap_or("http_error").to_string(),
                    message: e.get("message").and_then(Value::as_str).unwrap_or("no message").to_string(),
                })
            }
            Err(e) => Err(Error::Unreachable(format!("{method} {path}: {}", e.kind()))),
        }
    }

    /// The records (`GET /api/v1/mesh/records`), narrowed by `query` (the `FILTERS`, strings or
    /// `live` as a boolean). Where the server runs no mesh the answer is `active: false` with
    /// its reason, passed through as it is.
    pub fn records(&self, query: &Value) -> Result<Value, Error> {
        let mut params = Vec::new();
        if let Some(q) = query.as_object() {
            for (k, v) in q {
                if !FILTERS.contains(&k.as_str()) { return Err(Error::Config(format!("{k:?} is not a filter (one of {})", FILTERS.join(", ")))); }
                let s = match v { Value::String(s) => s.clone(), Value::Bool(b) => b.to_string(), _ => return Err(Error::Config(format!("filter {k} is not a string or boolean"))) };
                if k == "kind" && !KINDS.contains(&s.as_str()) { return Err(Error::Config(format!("{s:?} is not a record kind"))); }
                params.push(format!("{k}={}", encode(&s)));
            }
        } else if !query.is_null() {
            return Err(Error::Config("the query is a map of filters".into()));
        }
        let path = if params.is_empty() { "/api/v1/mesh/records".to_string() } else { format!("/api/v1/mesh/records?{}", params.join("&")) };
        let v = self.call("GET", &path, None)?;
        check_list(&v)?;
        Ok(v)
    }

    /// Write a record or revise one of the session's own (`POST /api/v1/mesh/records`). The
    /// record is checked for what Majordomus would refuse as malformed before anything is sent.
    pub fn write(&self, record: Value) -> Result<Value, Error> {
        let mut r = record;
        let Some(m) = r.as_object_mut() else { return Err(Error::Config("a record is a map".into())) };
        if !m.get("session").and_then(Value::as_str).is_some_and(|s| !s.is_empty()) {
            let Some(s) = &self.session else { return Err(Error::Config("no session: name it in the record or set MAJORDOMUS_SESSION".into())) };
            m.insert("session".into(), json!(s));
        }
        check_write(&r)?;
        let v = self.call("POST", "/api/v1/mesh/records", Some(r))?;
        let ok = v.get("key").and_then(Value::as_str).is_some_and(|k| k.contains('/')) && v.get("revision").is_some_and(Value::is_u64);
        if !ok { return Err(Error::Invalid("POST /api/v1/mesh/records: no key and revision".into())); }
        Ok(v)
    }
}

/// Percent-encode a query value (everything but unreserved characters).
fn encode(s: &str) -> String {
    s.bytes().map(|b| if b.is_ascii_alphanumeric() || b"-_.~/".contains(&b) { (b as char).to_string() } else { format!("%{b:02X}") }).collect()
}

/// The answer of `mesh.records`: `active`; when active, `records`, `conflicts` and `ignored` lists.
pub fn check_list(v: &Value) -> Result<(), Error> {
    let bad = |m: &str| Err(Error::Invalid(format!("records: {m}")));
    match v.get("active").and_then(Value::as_bool) {
        None => bad("no active field"),
        Some(false) => if v.get("reason").is_some_and(Value::is_string) { Ok(()) } else { bad("inactive without a reason") },
        Some(true) => {
            for f in ["records", "conflicts", "ignored"] {
                if !v.get(f).is_some_and(Value::is_array) { return bad(&format!("{f} is not a list")); }
            }
            let rec_ok = |r: &Value| ["key", "author", "kind", "task"].iter().all(|f| r.get(*f).is_some_and(Value::is_string)) && r.get("revision").is_some_and(Value::is_u64);
            if v["records"].as_array().is_some_and(|rs| rs.iter().all(rec_ok)) { Ok(()) } else { bad("a record lacks key, author, kind, task or revision") }
        }
    }
}

/// What Majordomus refuses as `invalid_input`, checked here so a malformed record never leaves:
/// a kind outside the set, an empty task or title, a revision without the record it revises, a
/// handoff without its five parts (evidence at least one, blockers a list).
pub fn check_write(v: &Value) -> Result<(), Error> {
    let bad = |m: String| Err(Error::Config(format!("record: {m}")));
    let kind = v.get("kind").and_then(Value::as_str).unwrap_or("");
    if v.get("record").is_none() && !KINDS.contains(&kind) { return bad(format!("kind {kind:?} is not one of {}", KINDS.join(", "))); }
    if v.get("record").is_some() != v.get("revision").is_some() { return bad("a revision names both record and revision".into()); }
    if v.get("record").is_none() {
        for f in ["task", "title"] {
            if !v.get(f).and_then(Value::as_str).is_some_and(|s| !s.trim().is_empty()) { return bad(format!("{f} is empty")); }
        }
    }
    if kind == "handoff" {
        let h = v.get("handoff").cloned().unwrap_or(Value::Null);
        for p in HANDOFF_PARTS {
            let ok = match *p {
                "evidence" => h.get(p).and_then(Value::as_array).is_some_and(|a| !a.is_empty()),
                "blockers" => h.get(p).is_some_and(Value::is_array),
                _ => h.get(p).and_then(Value::as_str).is_some_and(|s| !s.trim().is_empty()),
            };
            if !ok { return bad(format!("a handoff needs {p}")); }
        }
    }
    Ok(())
}

fn to_rhai(r: Result<Value, Error>) -> Result<Dynamic, Box<EvalAltResult>> {
    let v = r.map_err(|e| e.to_string())?;
    rhai::serde::to_dynamic(v)
}

fn from_map(m: Map) -> Result<Value, Box<EvalAltResult>> {
    rhai::serde::from_dynamic(&Dynamic::from_map(m))
}

/// The `mesh_*` functions a script calls, each with the client the environment configures at the
/// call. A failure is a Rhai error whose text says which kind it is.
pub fn register(e: &mut Engine) {
    fn client() -> Result<Client, Box<EvalAltResult>> { Client::from_env().map_err(|e| e.to_string().into()) }
    e.register_fn("mesh_records", || to_rhai(client()?.records(&Value::Null)));
    e.register_fn("mesh_records", |query: Map| to_rhai(client()?.records(&from_map(query)?)));
    e.register_fn("mesh_record_write", |record: Map| to_rhai(client()?.write(from_map(record)?)));
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Read, Write};
    use std::net::TcpListener;
    use std::sync::{Arc, Mutex};

    /// A fixture of the Majordomus HTTP API on loopback: each request answered by `METHOD /path`,
    /// every request recorded; status 0 closes the connection without an answer.
    fn fixture(answers: Vec<(&'static str, u16, &'static str)>) -> (String, Arc<Mutex<Vec<String>>>) {
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
                    let text = String::from_utf8_lossy(&buf[..n]).to_string();
                    let len = text.lines().find_map(|l| l.to_ascii_lowercase().strip_prefix("content-length: ").map(|v| v.trim().parse::<usize>().unwrap_or(0))).unwrap_or(0);
                    if k == 0 || text.find("\r\n\r\n").is_some_and(|end| n >= end + 4 + len) { break; }
                }
                let text = String::from_utf8_lossy(&buf[..n]).to_string();
                log.lock().unwrap().push(text.clone());
                let mut parts = text.lines().next().unwrap_or("").split(' ');
                let key = format!("{} {}", parts.next().unwrap_or(""), parts.next().unwrap_or("").split('?').next().unwrap_or(""));
                let Some((_, status, body)) = answers.iter().find(|(k, _, _)| *k == key) else { continue };
                if *status == 0 { continue; }
                let _ = write!(s, "HTTP/1.1 {status} X\r\ncontent-type: application/json\r\ncontent-length: {}\r\nconnection: close\r\n\r\n{body}", body.len());
            }
        });
        (base, seen)
    }

    const LIST: &str = r#"{"active": true, "digest": "d", "records": [{"key": "s1/r1", "author": "s1/board-p1", "kind": "decision",
        "task": "I0094", "scope": ["research/x.md"], "status": "open", "title": "t", "revision": 2, "evidence": "evidenced"}],
        "conflicts": [], "ignored": []}"#;
    const WROTE: &str = r#"{"key": "s1/r2", "revision": 1, "event": "e", "lamport": 7, "durability": "journal", "evidence": "asserted", "repeated": false, "conflicts": []}"#;

    fn client(base: &str) -> Client { Client::new(base, Some("board-p1".into()), Duration::from_millis(800)).unwrap() }

    #[test]
    fn records_are_read_with_their_filters_encoded() {
        let (base, seen) = fixture(vec![("GET /api/v1/mesh/records", 200, LIST)]);
        let c = client(&base);
        assert_eq!(c.records(&Value::Null).unwrap()["records"][0]["revision"], 2);
        c.records(&json!({"task": "I0094 a&b", "kind": "decision", "live": true, "path": "research/x.md"})).unwrap();
        let seen = seen.lock().unwrap().join("\n");
        assert!(seen.contains("GET /api/v1/mesh/records "), "{seen}");
        for want in ["task=I0094%20a%26b", "kind=decision", "live=true", "path=research/x.md"] { assert!(seen.contains(want), "{want}: {seen}"); }
    }

    #[test]
    fn an_inactive_mesh_is_passed_through_and_a_malformed_list_is_invalid() {
        let (base, _) = fixture(vec![("GET /api/v1/mesh/records", 200, r#"{"active": false, "reason": "no mesh here"}"#)]);
        assert_eq!(client(&base).records(&Value::Null).unwrap()["reason"], "no mesh here");
        for bad in [json!({}), json!({"active": false}), json!({"active": true, "records": [], "conflicts": []}),
                    json!({"active": true, "records": [{"key": "k"}], "conflicts": [], "ignored": []})] {
            assert!(matches!(check_list(&bad), Err(Error::Invalid(_))), "{bad}");
        }
        let (b2, _) = fixture(vec![("GET /api/v1/mesh/records", 200, "<html>")]);
        assert!(matches!(client(&b2).records(&Value::Null), Err(Error::Invalid(m)) if m.contains("not JSON")));
    }

    #[test]
    fn a_write_names_its_session_and_returns_key_and_revision() {
        let (base, seen) = fixture(vec![("POST /api/v1/mesh/records", 200, WROTE)]);
        let c = client(&base);
        let a = c.write(json!({"kind": "observation", "task": "I0094", "scope": ["research/x.md"], "title": "seen",
                               "sources": [{"type": "tool", "command": "GET /api/bridge/observation", "result": "ok"}]})).unwrap();
        assert_eq!((a["key"].clone(), a["revision"].clone()), (json!("s1/r2"), json!(1)));
        c.write(json!({"record": "s1/r2", "revision": 1, "session": "board-p9", "title": "revised"})).unwrap();
        let seen = seen.lock().unwrap().join("\n");
        assert!(seen.contains(r#""session":"board-p1""#) && seen.contains(r#""session":"board-p9""#), "{seen}");
        let (b2, _) = fixture(vec![("POST /api/v1/mesh/records", 200, r#"{"ok": true}"#)]);
        assert!(matches!(client(&b2).write(json!({"kind": "plan", "task": "t", "title": "x"})), Err(Error::Invalid(_))));
    }

    #[test]
    fn majordomus_refusals_reach_the_script_with_their_code() {
        let (base, _) = fixture(vec![("POST /api/v1/mesh/records", 422, r#"{"error": {"code": "refused", "message": "stale_revision: the current revision is 3"}}"#),
                                     ("GET /api/v1/mesh/records", 500, "oops")]);
        let c = client(&base);
        let e = c.write(json!({"record": "s1/r1", "revision": 2, "title": "x"})).unwrap_err();
        assert_eq!(e, Error::Refused { status: 422, code: "refused".into(), message: "stale_revision: the current revision is 3".into() });
        assert!(e.to_string().starts_with("mesh: refused (422 refused): stale_revision"));
        assert_eq!(c.records(&Value::Null).unwrap_err(), Error::Refused { status: 500, code: "http_error".into(), message: "no message".into() });
    }

    #[test]
    fn malformed_records_and_queries_never_leave_this_process() {
        let (base, seen) = fixture(vec![]);
        let c = client(&base);
        let bad = [
            json!(5),
            json!({"kind": "rumour", "task": "t", "title": "x"}),
            json!({"kind": "plan", "task": "", "title": "x"}),
            json!({"kind": "plan", "task": "t", "title": " "}),
            json!({"record": "s/r", "title": "x"}),
            json!({"kind": "plan", "task": "t", "title": "x", "revision": 1}),
            json!({"kind": "handoff", "task": "t", "title": "x"}),
            json!({"kind": "handoff", "task": "t", "title": "x", "handoff": {"objective": "o", "current_state": "c", "evidence": [], "blockers": [], "next_action": "n"}}),
            json!({"kind": "handoff", "task": "t", "title": "x", "handoff": {"objective": "o", "current_state": "c", "evidence": ["e"], "next_action": "n"}}),
        ];
        for b in bad { assert!(matches!(c.write(b.clone()), Err(Error::Config(_))), "{b}"); }
        for q in [json!({"owner": "x"}), json!({"kind": "rumour"}), json!({"task": 5}), json!([1])] {
            assert!(matches!(c.records(&q), Err(Error::Config(_))), "{q}");
        }
        let anonymous = Client::new(&base, None, Duration::from_millis(500)).unwrap();
        assert!(matches!(anonymous.write(json!({"kind": "plan", "task": "t", "title": "x"})), Err(Error::Config(m)) if m.contains("MAJORDOMUS_SESSION")));
        assert!(check_write(&json!({"kind": "handoff", "task": "t", "title": "x", "handoff": {"objective": "o", "current_state": "c", "evidence": [{"type": "record", "record": "s/r", "revision": 1}], "blockers": [], "next_action": "n"}})).is_ok());
        assert!(seen.lock().unwrap().is_empty(), "nothing was sent");
    }

    #[test]
    fn only_loopback_and_a_configured_address_are_used() {
        assert!(matches!(Client::new("http://10.0.0.2:9000", None, Duration::from_secs(1)), Err(Error::Config(_))));
        let closed = { let l = TcpListener::bind("127.0.0.1:0").unwrap(); l.local_addr().unwrap() };
        let gone = Client::new(&format!("http://{closed}/"), None, Duration::from_millis(300)).unwrap();
        assert!(matches!(gone.records(&Value::Null), Err(Error::Unreachable(_))));
        let (silent, _) = fixture(vec![("GET /api/v1/mesh/records", 0, "")]);
        assert!(matches!(client(&silent).records(&Value::Null), Err(Error::Unreachable(_))));
        assert_eq!(Error::Invalid("y".into()).to_string(), "mesh: invalid answer: y");
    }

    #[test]
    fn scripts_read_write_and_catch_errors() {
        let (base, _) = fixture(vec![("GET /api/v1/mesh/records", 200, LIST), ("POST /api/v1/mesh/records", 200, WROTE)]);
        let _env = ENV.lock().unwrap();
        std::env::set_var("MAJORDOMUS_URL", &base);
        std::env::set_var("MAJORDOMUS_SESSION", "board-p1");
        let mut e = Engine::new();
        register(&mut e);
        let out: i64 = e.eval(r#"
            let all = mesh_records();
            let some = mesh_records(#{task: "I0094", live: true});
            let w = mesh_record_write(#{kind: "decision", task: "I0094", scope: ["research/x.md"], title: "apply aggressiveness 75"});
            all.records[0].revision + some.records.len() + w.revision
        "#).unwrap();
        assert_eq!(out, 2 + 1 + 1);
        let caught: String = e.eval(r#"let r = "no"; try { mesh_record_write(#{kind: "rumour", task: "t", title: "x"}); } catch (err) { r = err; } r"#).unwrap();
        assert!(caught.starts_with("mesh: configuration: record: kind"), "{caught}");
        std::env::remove_var("MAJORDOMUS_URL");
        let unset: String = e.eval(r#"let r = "no"; try { mesh_records(); } catch (err) { r = err; } r"#).unwrap();
        assert!(unset.contains("MAJORDOMUS_URL is not set"), "{unset}");
        std::env::remove_var("MAJORDOMUS_SESSION");
    }

    /// Tests that change the environment run one at a time.
    static ENV: Mutex<()> = Mutex::new(());
}
