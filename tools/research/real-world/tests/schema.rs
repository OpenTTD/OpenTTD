//! The fixture snapshot and a compiled scenario against the contract
//! (tools/research/schema/real-world-scenario.v1.schema.json), with a small validator of the JSON
//! Schema keywords that schema uses: $ref (local), oneOf, type, enum, const, required, properties,
//! additionalProperties, items, minItems, maxItems, minLength, minimum, maximum, exclusiveMinimum,
//! pattern. An unknown keyword fails the test, so the validator cannot silently skip a rule.

use serde_json::Value;
use std::path::PathBuf;

const KNOWN: &[&str] = &[
    "$schema", "$id", "$ref", "$defs", "title", "description", "oneOf", "type", "enum", "const", "required", "properties",
    "additionalProperties", "items", "minItems", "maxItems", "minLength", "minimum", "maximum", "exclusiveMinimum", "pattern",
];

fn root() -> Value {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../schema/real-world-scenario.v1.schema.json");
    serde_json::from_str(&std::fs::read_to_string(p).unwrap()).unwrap()
}

fn type_ok(t: &str, v: &Value) -> bool {
    match t {
        "object" => v.is_object(),
        "array" => v.is_array(),
        "string" => v.is_string(),
        "number" => v.is_number(),
        "integer" => v.as_i64().is_some() || v.as_u64().is_some(),
        "boolean" => v.is_boolean(),
        "null" => v.is_null(),
        _ => panic!("unknown type {t}"),
    }
}

fn check(root: &Value, schema: &Value, v: &Value, path: &str, errs: &mut Vec<String>) {
    let o = schema.as_object().expect("a schema is an object");
    for k in o.keys() {
        assert!(KNOWN.contains(&k.as_str()), "the validator does not implement keyword {k} (at {path})");
    }
    if let Some(r) = o.get("$ref").and_then(Value::as_str) {
        let name = r.strip_prefix("#/$defs/").expect("local $ref");
        check(root, &root["$defs"][name], v, path, errs);
    }
    if let Some(alts) = o.get("oneOf").and_then(Value::as_array) {
        let ok = alts.iter().filter(|a| { let mut e = vec![]; check(root, a, v, path, &mut e); e.is_empty() }).count();
        if ok != 1 {
            errs.push(format!("{path}: matches {ok} of oneOf, needs exactly 1"));
        }
    }
    if let Some(t) = o.get("type") {
        let ts: Vec<&str> = match t { Value::String(s) => vec![s.as_str()], Value::Array(a) => a.iter().filter_map(Value::as_str).collect(), _ => vec![] };
        if !ts.iter().any(|t| type_ok(t, v)) {
            errs.push(format!("{path}: not of type {ts:?}"));
            return;
        }
    }
    if let Some(e) = o.get("enum").and_then(Value::as_array) {
        if !e.contains(v) { errs.push(format!("{path}: {v} not in enum")); }
    }
    if let Some(c) = o.get("const") {
        if c != v { errs.push(format!("{path}: {v} is not {c}")); }
    }
    if let Some(m) = o.get("minimum").and_then(Value::as_f64) {
        if v.as_f64().map(|x| x < m).unwrap_or(false) { errs.push(format!("{path}: below {m}")); }
    }
    if let Some(m) = o.get("maximum").and_then(Value::as_f64) {
        if v.as_f64().map(|x| x > m).unwrap_or(false) { errs.push(format!("{path}: above {m}")); }
    }
    if let Some(m) = o.get("exclusiveMinimum").and_then(Value::as_f64) {
        if v.as_f64().map(|x| x <= m).unwrap_or(false) { errs.push(format!("{path}: not above {m}")); }
    }
    if let (Some(s), Some(m)) = (v.as_str(), o.get("minLength").and_then(Value::as_u64)) {
        if (s.chars().count() as u64) < m { errs.push(format!("{path}: shorter than {m}")); }
    }
    if let (Some(s), Some(p)) = (v.as_str(), o.get("pattern").and_then(Value::as_str)) {
        if !regex::Regex::new(p).unwrap().is_match(s) { errs.push(format!("{path}: {s:?} does not match {p}")); }
    }
    if let Some(obj) = v.as_object() {
        if let Some(req) = o.get("required").and_then(Value::as_array) {
            for r in req.iter().filter_map(Value::as_str) {
                if !obj.contains_key(r) { errs.push(format!("{path}: missing {r}")); }
            }
        }
        let props = o.get("properties").and_then(Value::as_object);
        for (k, x) in obj {
            if let Some(ps) = props.and_then(|p| p.get(k)) {
                check(root, ps, x, &format!("{path}.{k}"), errs);
            } else if let Some(ap) = o.get("additionalProperties").filter(|a| a.is_object()) {
                check(root, ap, x, &format!("{path}.{k}"), errs);
            }
        }
    }
    if let Some(arr) = v.as_array() {
        if let Some(m) = o.get("minItems").and_then(Value::as_u64) {
            if (arr.len() as u64) < m { errs.push(format!("{path}: fewer than {m} items")); }
        }
        if let Some(m) = o.get("maxItems").and_then(Value::as_u64) {
            if (arr.len() as u64) > m { errs.push(format!("{path}: more than {m} items")); }
        }
        if let Some(it) = o.get("items") {
            for (i, x) in arr.iter().enumerate() {
                check(root, it, x, &format!("{path}[{i}]"), errs);
            }
        }
    }
}

fn validate(doc: &Value) -> Vec<String> {
    let r = root();
    let mut errs = vec![];
    check(&r, &r, doc, "$", &mut errs);
    errs
}

#[test]
fn the_brno_snapshot_satisfies_the_schema() {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("fixtures/brno/snapshot.json");
    let doc: Value = serde_json::from_str(&std::fs::read_to_string(p).unwrap()).unwrap();
    assert_eq!(validate(&doc), Vec::<String>::new());
}

#[test]
fn a_compiled_scenario_satisfies_the_schema_and_a_broken_one_does_not() {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let s = rw_scenario::snapshot::read(&dir.join("fixtures/brno/snapshot.json")).unwrap();
    let (r, b) = rw_scenario::rules::Rules::load(&dir.join("rules/temperate-default.v1.json")).unwrap();
    let out = std::env::temp_dir().join(format!("rw-schema-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&out);
    std::fs::create_dir_all(&out).unwrap();
    let sc = rw_scenario::write_outputs(&out, rw_scenario::compile::compile(&s, &r, &b).unwrap(), &s, &r).unwrap();
    let mut doc = serde_json::to_value(&sc).unwrap();
    assert_eq!(validate(&doc), Vec::<String>::new());
    doc["entities"][0]["evidence"] = Value::String("OBSERVED".into());
    assert!(!validate(&doc).is_empty(), "a game entity marked OBSERVED must fail the schema");
    let _ = std::fs::remove_dir_all(&out);
}
