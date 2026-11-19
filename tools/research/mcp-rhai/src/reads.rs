//! RESEARCH-ONLY: the `openttd://` resources one run of a planner read (mcp-plan, mcp-intent), in
//! order, once each, with whether a read succeeded. Their answer carries it as `reads`, which the
//! panel's plan.* events name as their live inputs (I0081).

use std::sync::Mutex;

static READS: Mutex<Vec<(String, bool)>> = Mutex::new(Vec::new());

/// Note one read of resource `openttd://<name>`; a resource read twice is listed once, as read when
/// any of its reads succeeded.
pub fn note(name: &str, ok: bool) {
    note_in(&mut READS.lock().unwrap(), name, ok);
}

fn note_in(reads: &mut Vec<(String, bool)>, name: &str, ok: bool) {
    let uri = format!("openttd://{name}");
    match reads.iter_mut().find(|(u, _)| *u == uri) {
        Some(e) => e.1 |= ok,
        None => reads.push((uri, ok)),
    }
}

/// Every read so far as `[{uri, ok}]`.
pub fn json() -> serde_json::Value {
    to_json(&READS.lock().unwrap())
}

fn to_json(reads: &[(String, bool)]) -> serde_json::Value {
    serde_json::Value::Array(reads.iter().map(|(u, ok)| serde_json::json!({"uri": u, "ok": ok})).collect())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_resource_is_listed_once_in_the_order_first_read() {
        let mut r = Vec::new();
        note_in(&mut r, "towns", false);
        note_in(&mut r, "world/graph", true);
        note_in(&mut r, "towns", true);
        note_in(&mut r, "towns", false);
        assert_eq!(to_json(&r), serde_json::json!([{"uri": "openttd://towns", "ok": true}, {"uri": "openttd://world/graph", "ok": true}]));
    }
}
