//! RESEARCH-ONLY: the panel mirrors its directive decisions into the Majordomus mesh's typed
//! records (research/agent-loop.md, Coordinate; Majordomus I4665).
//!
//! A `plan.directive` becomes a `decision` record, its `plan.directive_outcome` an `outcome`
//! record that cites the decision, each citing the panel event it mirrors as a tool result, so
//! the mesh's evidence state is `evidenced`, not `asserted`. Opt-in: only when `$MAJORDOMUS_URL`
//! and `$MAJORDOMUS_SESSION` are set. Best effort and off the game's path: a write runs on its own
//! thread, and a refusal or an unreachable server is a panel.mesh line, never a failed directive.
//! The bus and the store stay the record of truth; the mesh carries what other agents read.

use std::collections::BTreeMap;
use std::sync::{Mutex, OnceLock};

use serde_json::{json, Value};

use crate::rhai_host::mesh_records::Client;

/// The task a directive's records are about: one per directive, by its plan.directive seq.
pub fn task_of(instance: &str, directive_seq: u64) -> String {
    format!("openttd:{instance}:directive-{directive_seq}")
}

/// The record that mirrors a plan event, or None for a kind that is not mirrored. `decision_key`
/// is the mesh key of the directive's decision record, when it was written.
pub fn record_of(kind: &str, seq: u64, data: &Value, instance: &str, at: &str, decision_key: Option<&str>) -> Option<Value> {
    let outcome = data.pointer("/choice/outcome").and_then(Value::as_str).unwrap_or("unknown");
    let tool = |command: String| json!({"type": "tool", "command": command, "result": outcome, "observed_at": at});
    match kind {
        "plan.directive" => {
            let url = data.pointer("/inputs/live/url").or_else(|| data.pointer("/inputs/url")).and_then(Value::as_str).unwrap_or("");
            let mut body = format!("Prismatic's directive{} was {outcome} by panel {instance}.", if url.is_empty() { String::new() } else { format!(" ({url})") });
            if let Some(k) = data.pointer("/inputs/live/key").and_then(Value::as_str) { body.push_str(&format!(" Idempotency key {k}.")); }
            Some(json!({"kind": "decision", "task": task_of(instance, seq), "scope": [],
                        "title": format!("directive {seq} on {instance}: {outcome}"), "body": body,
                        "sources": [tool(format!("{instance} plan.directive seq {seq}"))],
                        "idempotency": format!("{instance}:plan.directive:{seq}")}))
        }
        "plan.directive_outcome" => {
            let directive = data.pointer("/inputs/live/directive_seq").and_then(Value::as_u64)?;
            let mut sources = vec![tool(format!("{instance} plan.directive_outcome seq {seq}"))];
            if let Some(k) = decision_key { sources.push(json!({"type": "record", "record": k, "revision": 1})); }
            Some(json!({"kind": "outcome", "task": task_of(instance, directive), "scope": [],
                        "title": format!("directive {directive} on {instance} settled: {outcome}"),
                        "status": "resolved", "sources": sources,
                        "idempotency": format!("{instance}:plan.directive_outcome:{seq}")}))
        }
        _ => None,
    }
}

/// Now as RFC 3339 UTC (`2026-11-22T06:00:00Z`).
pub fn utc_now() -> String {
    let secs = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    utc_of(secs)
}

/// A Unix time as RFC 3339 UTC, by the civil-from-days algorithm.
pub fn utc_of(secs: u64) -> String {
    let (days, rem) = ((secs / 86400) as i64, secs % 86400);
    let z = days + 719468;
    let era = z.div_euclid(146097);
    let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = yoe + era * 400 + i64::from(m <= 2);
    format!("{y:04}-{m:02}-{d:02}T{:02}:{:02}:{:02}Z", rem / 3600, rem % 3600 / 60, rem % 60)
}

/// The decision record written for each directive seq, so its outcome can cite it.
fn decisions() -> &'static Mutex<BTreeMap<u64, String>> {
    static D: OnceLock<Mutex<BTreeMap<u64, String>>> = OnceLock::new();
    D.get_or_init(|| Mutex::new(BTreeMap::new()))
}

/// Whether mirroring is configured: both variables set and non-empty.
pub fn configured() -> bool {
    ["MAJORDOMUS_URL", "MAJORDOMUS_SESSION"].iter().all(|v| std::env::var(v).is_ok_and(|s| !s.is_empty()))
}

/// Write the record of one plan event with `client`; the line the panel logs about it.
pub fn write(client: &Client, kind: &str, seq: u64, data: &Value, instance: &str, at: &str) -> Option<String> {
    let directive = data.pointer("/inputs/live/directive_seq").and_then(Value::as_u64);
    let key = directive.and_then(|d| decisions().lock().ok()?.get(&d).cloned());
    let record = record_of(kind, seq, data, instance, at, key.as_deref())?;
    Some(match client.write(record) {
        Ok(a) => {
            let k = a["key"].as_str().unwrap_or_default().to_string();
            if kind == "plan.directive" {
                if let Ok(mut d) = decisions().lock() { d.insert(seq, k.clone()); if d.len() > 1000 { d.pop_first(); } }
            }
            format!("{kind} {seq} mirrored to the mesh as {k}{}", if a["repeated"] == json!(true) { " (repeated)" } else { "" })
        }
        Err(e) => format!("{kind} {seq} not mirrored to the mesh: {e}"),
    })
}

/// The lines the mirroring threads left for the panel to log, oldest first.
fn lines() -> &'static Mutex<Vec<String>> {
    static L: OnceLock<Mutex<Vec<String>>> = OnceLock::new();
    L.get_or_init(|| Mutex::new(Vec::new()))
}

/// Take the lines written so far; the panel logs each as a panel.mesh line.
pub fn take_lines() -> Vec<String> {
    lines().lock().map(|mut l| std::mem::take(&mut *l)).unwrap_or_default()
}

fn note(line: String) {
    if let Ok(mut l) = lines().lock() { l.push(line); if l.len() > 200 { l.remove(0); } }
}

/// Mirror one plan event when configured, on its own thread; its line waits in `take_lines`.
/// The thread's handle, or None when nothing is mirrored.
pub fn mirror(kind: &str, seq: u64, data: &Value, instance: &str) -> Option<std::thread::JoinHandle<()>> {
    if !configured() || record_of(kind, seq, data, instance, "", None).is_none() { return None; }
    let (kind, data, instance) = (kind.to_string(), data.clone(), instance.to_string());
    let at = utc_now();
    Some(std::thread::spawn(move || match Client::from_env() {
        Ok(c) => { if let Some(line) = write(&c, &kind, seq, &data, &instance, &at) { note(line); } }
        Err(e) => note(format!("{kind} {seq} not mirrored to the mesh: {e}")),
    }))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Read, Write};
    use std::net::TcpListener;
    use std::sync::Arc;
    use std::time::Duration;

    fn directive(outcome: &str) -> Value {
        json!({"decision": "directive", "inputs": {"live": {"url": "http://127.0.0.1:4004/api/openttd/decision", "key": "k1"}}, "choice": {"outcome": outcome}})
    }

    #[test]
    fn a_directive_is_a_decision_and_its_outcome_cites_it() {
        let d = record_of("plan.directive", 7, &directive("pending"), "panel-8800", "2026-11-22T06:00:00Z", None).unwrap();
        assert_eq!((d["kind"].clone(), d["task"].clone()), (json!("decision"), json!("openttd:panel-8800:directive-7")));
        assert_eq!(d["sources"][0], json!({"type": "tool", "command": "panel-8800 plan.directive seq 7", "result": "pending", "observed_at": "2026-11-22T06:00:00Z"}));
        assert!(d["body"].as_str().unwrap().contains("Idempotency key k1") && d["idempotency"] == "panel-8800:plan.directive:7");
        let o = record_of("plan.directive_outcome", 9, &json!({"inputs": {"live": {"directive_seq": 7}}, "choice": {"outcome": "applied"}}), "panel-8800", "t", Some("s/r1")).unwrap();
        assert_eq!((o["kind"].clone(), o["task"].clone(), o["status"].clone()), (json!("outcome"), json!("openttd:panel-8800:directive-7"), json!("resolved")));
        assert_eq!(o["sources"][1], json!({"type": "record", "record": "s/r1", "revision": 1}));
        assert!(record_of("plan.directive_outcome", 9, &json!({}), "p", "t", None).is_none(), "an outcome names its directive");
        assert!(record_of("plan.portfolio", 1, &json!({}), "p", "t", None).is_none());
        for r in [d, o] { crate::rhai_host::mesh_records::check_write(&r).unwrap(); }
    }

    /// Against a loopback fixture of the records API: the decision is written, its key is kept,
    /// the outcome cites it, and a refusal is a line, not a failure.
    #[test]
    fn records_are_written_and_refusals_are_logged() {
        let l = TcpListener::bind("127.0.0.1:0").unwrap();
        let base = format!("http://{}", l.local_addr().unwrap());
        let seen = Arc::new(Mutex::new(Vec::<String>::new()));
        let log = seen.clone();
        std::thread::spawn(move || {
            for (n, s) in l.incoming().enumerate() {
                let Ok(mut s) = s else { continue };
                let mut buf = vec![0u8; 65536];
                let k = s.read(&mut buf).unwrap_or(0);
                let mut text = String::from_utf8_lossy(&buf[..k]).to_string();
                if !text.contains("\r\n\r\n{") { let k2 = s.read(&mut buf).unwrap_or(0); text.push_str(&String::from_utf8_lossy(&buf[..k2])); }
                log.lock().unwrap().push(text);
                let (status, body) = match n {
                    0 => (200, r#"{"key": "s/r1", "revision": 1, "repeated": false}"#),
                    1 => (200, r#"{"key": "s/r2", "revision": 1, "repeated": true}"#),
                    _ => (422, r#"{"error": {"code": "refused", "message": "out_of_scope: claim first"}}"#),
                };
                let _ = write!(s, "HTTP/1.1 {status} X\r\ncontent-type: application/json\r\ncontent-length: {}\r\nconnection: close\r\n\r\n{body}", body.len());
            }
        });
        let c = Client::new(&base, Some("board-p1".into()), Duration::from_millis(800)).unwrap();
        assert_eq!(write(&c, "plan.directive", 70001, &directive("pending"), "panel-1", "t").unwrap(), "plan.directive 70001 mirrored to the mesh as s/r1");
        let outcome = json!({"inputs": {"live": {"directive_seq": 70001}}, "choice": {"outcome": "applied"}});
        assert_eq!(write(&c, "plan.directive_outcome", 70002, &outcome, "panel-1", "t").unwrap(), "plan.directive_outcome 70002 mirrored to the mesh as s/r2 (repeated)");
        let line = write(&c, "plan.directive", 70003, &directive("stale"), "panel-1", "t").unwrap();
        assert!(line.starts_with("plan.directive 70003 not mirrored to the mesh: mesh: refused (422 refused): out_of_scope"), "{line}");
        assert!(write(&c, "plan.portfolio", 1, &json!({}), "panel-1", "t").is_none());
        let seen = seen.lock().unwrap().join("\n");
        assert!(seen.contains(r#""record":"s/r1""#), "the outcome cites the decision: {seen}");
        assert!(seen.contains(r#""session":"board-p1""#));
    }

    #[test]
    fn times_are_rfc3339_utc() {
        assert_eq!(utc_of(0), "1970-01-01T00:00:00Z");
        assert_eq!(utc_of(951782400), "2000-02-29T00:00:00Z");
        assert_eq!(utc_of(1795324800), "2026-11-22T05:20:00Z");
        assert!(utc_now().ends_with('Z') && utc_now().len() == 20);
    }

    #[test]
    fn mirroring_is_off_unless_both_variables_are_set() {
        let _e = ENV.lock().unwrap();
        take_lines();
        std::env::remove_var("MAJORDOMUS_URL");
        std::env::set_var("MAJORDOMUS_SESSION", "s");
        assert!(!configured());
        assert!(mirror("plan.directive", 1, &directive("applied"), "p").is_none(), "nothing is written when off");
        std::env::set_var("MAJORDOMUS_URL", "http://10.1.2.3:9");
        assert!(configured());
        assert!(mirror("plan.portfolio", 2, &json!({}), "p").is_none(), "a kind not mirrored starts nothing");
        mirror("plan.directive", 3, &directive("applied"), "p").unwrap().join().unwrap();
        let got = take_lines();
        assert!(got.len() == 1 && got[0].contains("not mirrored") && got[0].contains("not a loopback"), "{got:?}");
        assert!(take_lines().is_empty(), "taken once");
        std::env::remove_var("MAJORDOMUS_URL");
        std::env::remove_var("MAJORDOMUS_SESSION");
    }

    static ENV: Mutex<()> = Mutex::new(());
}
