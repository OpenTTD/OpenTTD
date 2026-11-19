//! RESEARCH-ONLY: memory the Rhai scripts and services of one panel share (I0091,
//! research/rhai-ide.md).
//!
//! A key holds a JSON value with a revision, the writer that declared itself and the time. A write
//! may name the revision it expects (compare-and-swap; 0 = the key must not exist yet), so two
//! services updating one key never lose each other's write silently: the second one is refused
//! with the current revision and reads again. Every write and delete is published as a `rhai.shared`
//! event, so it reaches the ZeroMQ bus and the DuckDB store (ADR 0033) as an audit trail.
//!
//! Limits, stated where they apply:
//! - the memory lives in the panel process: a restarted panel starts empty (the store keeps the
//!   history of every write, not the current map);
//! - `writer` is what the caller declares; the panel's loopback API authenticates no script.

use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, OnceLock};

use serde_json::{json, Value};

/// The largest value a key holds, as JSON text.
pub const MAX_VALUE: usize = 64 * 1024;
/// The most keys one panel holds.
pub const MAX_KEYS: usize = 4096;

#[derive(Clone, Debug, PartialEq)]
pub struct Entry {
    pub value: Value,
    pub rev: u64,
    pub writer: String,
    pub at: u64,
}

/// Why a write was refused.
#[derive(Debug, PartialEq)]
pub enum Refused {
    /// The key's revision is not the one the write expected (0: the key does not exist).
    Conflict { rev: u64 },
    /// The key, the value or the writer is not acceptable; the text says why.
    Invalid(String),
}

#[derive(Default)]
pub struct Shared {
    map: Mutex<BTreeMap<String, Entry>>,
}

/// A key is 1 to 128 characters of letters, digits and `_ . : / -`, so it reads well in a log and
/// groups by prefix (`rail.line.3`).
pub fn valid_key(key: &str) -> bool {
    (1..=128).contains(&key.len()) && key.bytes().all(|b| b.is_ascii_alphanumeric() || b"_.:/-".contains(&b))
}

fn now() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0)
}

fn writer_of(w: &str) -> Result<String, Refused> {
    let w = if w.trim().is_empty() { "unnamed" } else { w.trim() };
    if w.len() > 64 || w.chars().any(char::is_control) { return Err(Refused::Invalid("writer: at most 64 printable characters".into())); }
    Ok(w.to_string())
}

impl Shared {
    pub fn get(&self, key: &str) -> Option<Entry> {
        self.map.lock().ok()?.get(key).cloned()
    }

    /// Every entry whose key starts with `prefix`, in key order.
    pub fn list(&self, prefix: &str) -> Vec<(String, Entry)> {
        self.map.lock().map(|m| m.range(prefix.to_string()..).take_while(|(k, _)| k.starts_with(prefix)).map(|(k, e)| (k.clone(), e.clone())).collect())
            .unwrap_or_default()
    }

    /// Write `value` under `key`; with `expect`, only if the key's revision is that one (0: absent).
    /// Answers the new revision.
    pub fn set(&self, key: &str, value: Value, writer: &str, expect: Option<u64>) -> Result<u64, Refused> {
        if !valid_key(key) { return Err(Refused::Invalid(format!("key {key:?}: 1-128 characters of letters, digits and _ . : / -"))); }
        let size = value.to_string().len();
        if size > MAX_VALUE { return Err(Refused::Invalid(format!("value: {size} bytes as JSON, at most {MAX_VALUE}"))); }
        let writer = writer_of(writer)?;
        let mut m = self.map.lock().map_err(|_| Refused::Invalid("the shared memory's lock is poisoned".into()))?;
        let current = m.get(key).map(|e| e.rev).unwrap_or(0);
        if let Some(want) = expect {
            if want != current { return Err(Refused::Conflict { rev: current }); }
        }
        if current == 0 && m.len() >= MAX_KEYS { return Err(Refused::Invalid(format!("at most {MAX_KEYS} keys"))); }
        let rev = current + 1;
        m.insert(key.to_string(), Entry { value, rev, writer, at: now() });
        Ok(rev)
    }

    /// Delete `key`; with `expect`, only at that revision. Answers the revision it had.
    pub fn delete(&self, key: &str, writer: &str, expect: Option<u64>) -> Result<u64, Refused> {
        writer_of(writer)?;
        let mut m = self.map.lock().map_err(|_| Refused::Invalid("the shared memory's lock is poisoned".into()))?;
        let current = m.get(key).map(|e| e.rev).unwrap_or(0);
        if current == 0 || expect.is_some_and(|w| w != current) { return Err(Refused::Conflict { rev: current }); }
        m.remove(key);
        Ok(current)
    }
}

/// The one shared memory of this panel process.
pub fn shared() -> &'static Shared {
    static S: OnceLock<Shared> = OnceLock::new();
    S.get_or_init(Shared::default)
}

pub fn entry_json(key: &str, e: &Entry) -> Value {
    json!({"key": key, "value": e.value, "rev": e.rev, "writer": e.writer, "at": e.at})
}

fn refused(r: Refused) -> (u16, Value) {
    match r {
        Refused::Conflict { rev } => (409, json!({"error": format!("conflict: the key is at revision {rev}"), "rev": rev})),
        Refused::Invalid(why) => (400, json!({"error": why})),
    }
}

fn expect_of(body: &Value) -> Result<Option<u64>, String> {
    match body.get("rev") {
        None | Some(Value::Null) => Ok(None),
        Some(v) => v.as_u64().map(Some).ok_or_else(|| "rev: a whole number (0 = the key must not exist)".to_string()),
    }
}

/// GET /api/rhai/shared: one key (?key=) or every key under ?prefix= (all without one).
pub fn h_shared(_: &Arc<crate::panel::Game>, i: &crate::panel::Input) -> Result<(u16, Value), String> {
    if let Some(key) = i.query.get("key") {
        return Ok(match shared().get(key) {
            Some(e) => (200, entry_json(key, &e)),
            None => (404, json!({"error": format!("no shared key {key:?}"), "rev": 0})),
        });
    }
    let prefix = i.query.get("prefix").unwrap_or("");
    let entries: Vec<Value> = shared().list(prefix).iter().map(|(k, e)| entry_json(k, e)).collect();
    Ok((200, json!({"entries": entries, "count": entries.len(), "limits": {"keys": MAX_KEYS, "value_bytes": MAX_VALUE}})))
}

/// POST /api/rhai/shared {key, value, writer?, rev?}: write, published as `rhai.shared`.
pub fn h_shared_set(g: &Arc<crate::panel::Game>, i: &crate::panel::Input) -> Result<(u16, Value), String> {
    let key = i.body.get("key").and_then(Value::as_str).unwrap_or("");
    let value = i.body.get("value").cloned().unwrap_or(Value::Null);
    let writer = i.body.get("writer").and_then(Value::as_str).unwrap_or("");
    let expect = match expect_of(&i.body) { Ok(e) => e, Err(why) => return Ok((400, json!({"error": why}))) };
    Ok(match shared().set(key, value.clone(), writer, expect) {
        Ok(rev) => {
            let e = shared().get(key).map(|e| entry_json(key, &e)).unwrap_or(Value::Null);
            g.bus.publish("rhai.shared", json!({"op": "set", "key": key, "rev": rev, "writer": e["writer"], "value": value}));
            (200, e)
        }
        Err(r) => refused(r),
    })
}

/// POST /api/rhai/shared/delete {key, writer?, rev?}: delete, published as `rhai.shared`.
pub fn h_shared_delete(g: &Arc<crate::panel::Game>, i: &crate::panel::Input) -> Result<(u16, Value), String> {
    let key = i.body.get("key").and_then(Value::as_str).unwrap_or("");
    let writer = i.body.get("writer").and_then(Value::as_str).unwrap_or("");
    let expect = match expect_of(&i.body) { Ok(e) => e, Err(why) => return Ok((400, json!({"error": why}))) };
    Ok(match shared().delete(key, writer, expect) {
        Ok(rev) => {
            g.bus.publish("rhai.shared", json!({"op": "delete", "key": key, "rev": rev, "writer": writer_of(writer).unwrap_or_default()}));
            (200, json!({"key": key, "deleted": true, "rev": rev}))
        }
        Err(r) => refused(r),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_write_bumps_the_revision_and_keeps_its_writer() {
        let s = Shared::default();
        assert_eq!(s.set("rail.line.1", json!({"trains": 2}), "rail_steward", None), Ok(1));
        assert_eq!(s.set("rail.line.1", json!({"trains": 3}), "rail_steward", None), Ok(2));
        let e = s.get("rail.line.1").unwrap();
        assert_eq!((e.rev, e.writer.as_str(), e.value["trains"].as_i64()), (2, "rail_steward", Some(3)));
        assert_eq!(s.set("x", json!(1), "  ", None), Ok(1));
        assert_eq!(s.get("x").unwrap().writer, "unnamed");
    }

    #[test]
    fn compare_and_swap_refuses_a_stale_or_duplicate_write() {
        let s = Shared::default();
        assert_eq!(s.set("lock", json!("a"), "one", Some(0)), Ok(1), "0: create only");
        assert_eq!(s.set("lock", json!("b"), "two", Some(0)), Err(Refused::Conflict { rev: 1 }), "a second claim is refused");
        assert_eq!(s.set("lock", json!("b"), "two", Some(1)), Ok(2));
        assert_eq!(s.set("lock", json!("c"), "one", Some(1)), Err(Refused::Conflict { rev: 2 }), "a stale revision is refused");
        assert_eq!(s.get("lock").unwrap().value, json!("b"), "the refused write changed nothing");
    }

    #[test]
    fn deletes_need_the_key_and_the_revision() {
        let s = Shared::default();
        assert_eq!(s.delete("none", "w", None), Err(Refused::Conflict { rev: 0 }));
        s.set("k", json!(1), "w", None).unwrap();
        assert_eq!(s.delete("k", "w", Some(7)), Err(Refused::Conflict { rev: 1 }));
        assert_eq!(s.delete("k", "w", Some(1)), Ok(1));
        assert!(s.get("k").is_none());
        assert_eq!(s.set("k", json!(2), "w", Some(0)), Ok(1), "a deleted key starts again at revision 1");
    }

    #[test]
    fn keys_values_and_writers_are_bounded() {
        let s = Shared::default();
        for bad in ["", "has space", "ünï", &"x".repeat(129)] {
            assert!(matches!(s.set(bad, json!(1), "w", None), Err(Refused::Invalid(_))), "{bad:?}");
        }
        assert!(valid_key("rail.line/3:a-b_c"));
        assert!(matches!(s.set("big", json!("x".repeat(MAX_VALUE)), "w", None), Err(Refused::Invalid(_))));
        assert!(matches!(s.set("k", json!(1), &"w".repeat(65), None), Err(Refused::Invalid(_))));
        assert!(matches!(s.set("k", json!(1), "a\nb", None), Err(Refused::Invalid(_))));
    }

    #[test]
    fn listing_groups_by_prefix_in_key_order() {
        let s = Shared::default();
        for k in ["rail.2", "bus.1", "rail.1", "railway"] { s.set(k, json!(k), "w", None).unwrap(); }
        assert_eq!(s.list("rail.").iter().map(|(k, _)| k.as_str()).collect::<Vec<_>>(), vec!["rail.1", "rail.2"]);
        assert_eq!(s.list("").len(), 4);
        assert!(s.list("zzz").is_empty());
    }

    #[test]
    fn concurrent_compare_and_swap_increments_lose_no_update() {
        let s = std::sync::Arc::new(Shared::default());
        s.set("n", json!(0), "init", Some(0)).unwrap();
        let threads: Vec<_> = (0..8).map(|t| {
            let s = s.clone();
            std::thread::spawn(move || {
                for _ in 0..50 {
                    loop {
                        let e = s.get("n").unwrap();
                        let next = json!(e.value.as_i64().unwrap() + 1);
                        if s.set("n", next, &format!("t{t}"), Some(e.rev)).is_ok() { break; }
                    }
                }
            })
        }).collect();
        for t in threads { t.join().unwrap(); }
        let e = s.get("n").unwrap();
        assert_eq!((e.value.as_i64(), e.rev), (Some(400), 401));
    }

    #[test]
    fn the_key_limit_holds() {
        let s = Shared::default();
        for n in 0..MAX_KEYS { s.set(&format!("k{n}"), json!(n), "w", None).unwrap(); }
        assert!(matches!(s.set("one.more", json!(1), "w", None), Err(Refused::Invalid(_))));
        assert_eq!(s.set("k0", json!("again"), "w", None), Ok(2), "an existing key still updates");
    }
}
