//! RESEARCH-ONLY: every function a Rhai script can call in the panel's engine (I0088,
//! research/rhai-ide.md), for completion, hover and the docs of the Scripts page.
//!
//! Nothing here names a function. The signatures come from the engine itself
//! (`Engine::gen_fn_signatures`), the documentation of a host function from the `//!` header of
//! src/main.rs, a derived function's from what it was derived from (an MCP tool's description and
//! input schema, an OpenAPI operation's summary), and a library function's from its script's AST
//! (its `///` comments and parameters).

use std::collections::BTreeMap;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use rhai::{Engine, FnAccess};
use serde_json::{json, Value};

/// The `//!` header of src/main.rs: where every host function is documented.
const HOST_DOC: &str = include_str!("main.rs");

/// What a catalogue is built from.
pub struct Sources<'a> {
    pub engine: &'a Engine,
    /// The `//!` documentation listing the host functions (src/main.rs).
    pub host_doc: &'a str,
    /// The panel's `/api/catalogue` answer (`tools.result.tools`).
    pub catalogue: &'a Value,
    /// The panel's `/openapi.json`.
    pub openapi: &'a Value,
    /// Every library script: (name without `.rhai`, text).
    pub library: &'a [(String, String)],
}

/// The Rhai identifier of a tool or operation name, as src/main.rs `ident` makes it.
pub fn ident(name: &str) -> String {
    name.chars().map(|c| if c.is_ascii_alphanumeric() { c } else { '_' }).collect()
}

/// The function name of a signature: the text before its first `(`, without a leading `fn `.
pub fn name_of(signature: &str) -> &str {
    let s = signature.trim();
    let s = s.strip_prefix("fn ").unwrap_or(s);
    s.split('(').next().unwrap_or(s).trim()
}

/// A signature as a script writer reads it: the engine's Rust paths and its error wrapper taken
/// off, unnamed parameters shown by type. `rpc(_: string) -> core::result::Result<rhai::types::
/// dynamic::Dynamic, alloc::boxed::Box<rhai::types::error::EvalAltResult>>` is
/// `rpc(string) -> Dynamic`.
pub fn clean_signature(sig: &str) -> String {
    let s = sig.trim();
    let Some(open) = s.find('(') else { return s.to_string() };
    let (head, tail) = s.split_at(open);
    let mut t = String::with_capacity(tail.len());
    // Drop every module path (`a::b::Type` -> `Type`) inside the parameters and the result.
    let mut word = String::new();
    let mut chars = tail.chars().peekable();
    while let Some(c) = chars.next() {
        if c.is_ascii_alphanumeric() || c == '_' {
            word.push(c);
        } else if c == ':' && chars.peek() == Some(&':') && !word.is_empty() {
            chars.next();
            word.clear();
        } else {
            t.push_str(&word);
            word.clear();
            t.push(c);
        }
    }
    t.push_str(&word);
    let t = t.replace("_: ", "");
    let t = unwrap_result(&t);
    format!("{head}{t}")
}

/// `Result<T, Box<EvalAltResult>>` (with or without spaces) as `T`, wherever it stands.
fn unwrap_result(s: &str) -> String {
    let mut out = s.to_string();
    while let Some(at) = out.find("Result<") {
        let start = at + "Result<".len();
        let mut depth = 1usize;
        let mut comma = None;
        let mut end = None;
        for (i, c) in out[start..].char_indices() {
            match c {
                '<' => depth += 1,
                '>' => { depth -= 1; if depth == 0 { end = Some(start + i); break; } }
                ',' if depth == 1 && comma.is_none() => comma = Some(start + i),
                _ => {}
            }
        }
        let (Some(end), Some(comma)) = (end, comma) else { break };
        let inner = out[start..comma].trim().to_string();
        out.replace_range(at..=end, &inner);
    }
    out
}

/// The documentation of each host function, read from `//!` lines that list them:
/// `//!   name(args)  other(args)   -> type  text`, continued on deeper-indented lines.
/// A line naming several functions documents each of them.
pub fn host_docs(text: &str) -> BTreeMap<String, String> {
    let mut out: BTreeMap<String, String> = BTreeMap::new();
    let mut current: Vec<String> = Vec::new();
    let mut entry = String::new();
    let flush = |names: &mut Vec<String>, entry: &mut String, out: &mut BTreeMap<String, String>| {
        for n in names.drain(..) {
            out.entry(n).or_insert_with(|| entry.trim().to_string());
        }
        entry.clear();
    };
    for line in text.lines() {
        let Some(rest) = line.strip_prefix("//!") else {
            if !line.trim().is_empty() && !line.starts_with("//") { break; }
            continue;
        };
        let indent = rest.len() - rest.trim_start().len();
        let body = rest.trim();
        if indent >= 3 && indent < 8 && starts_with_call(body) {
            flush(&mut current, &mut entry, &mut out);
            current = calls_in(body);
            entry = body.to_string();
        } else if indent >= 8 && !current.is_empty() {
            entry.push(' ');
            entry.push_str(body);
        } else {
            flush(&mut current, &mut entry, &mut out);
        }
    }
    flush(&mut current, &mut entry, &mut out);
    out
}

fn is_ident_char(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}

fn starts_with_call(s: &str) -> bool {
    let head: String = s.chars().take_while(|c| is_ident_char(*c)).collect();
    !head.is_empty() && s[head.len()..].starts_with('(')
}

/// The names of the calls an entry line starts with: `tools()  commands()  help()  text` gives
/// tools, commands and help; the text after the calls is not read.
fn calls_in(s: &str) -> Vec<String> {
    let mut names = Vec::new();
    let mut rest = s;
    while starts_with_call(rest) {
        let name: String = rest.chars().take_while(|c| is_ident_char(*c)).collect();
        let Some(close) = matching_paren(rest, name.len()) else { break };
        names.push(name);
        rest = rest[close + 1..].trim_start();
    }
    names
}

fn matching_paren(s: &str, open: usize) -> Option<usize> {
    let mut depth = 0usize;
    for (i, c) in s.char_indices().skip(open) {
        match c {
            '(' => depth += 1,
            ')' => {
                depth = depth.checked_sub(1)?;
                if depth == 0 { return Some(i); }
            }
            _ => {}
        }
    }
    None
}

/// The tools of a catalogue, by their Rhai name: (description, input schema).
fn tools_of(catalogue: &Value) -> BTreeMap<String, (String, Value)> {
    catalogue.pointer("/tools/result/tools").and_then(Value::as_array).into_iter().flatten()
        .filter_map(|t| {
            let name = t.get("name")?.as_str()?;
            let doc = t.get("description").and_then(Value::as_str).unwrap_or("").to_string();
            Some((ident(name), (doc, t.get("inputSchema").cloned().unwrap_or(Value::Null))))
        })
        .collect()
}

/// The operations of an OpenAPI document, by their Rhai name `api_<operationId>`:
/// (method and path, summary or description).
fn operations_of(openapi: &Value) -> BTreeMap<String, (String, String)> {
    let mut out = BTreeMap::new();
    for (path, ops) in openapi.get("paths").and_then(Value::as_object).into_iter().flatten() {
        for (method, op) in ops.as_object().into_iter().flatten() {
            let Some(id) = op.get("operationId").and_then(Value::as_str) else { continue };
            let doc = op.get("summary").or_else(|| op.get("description")).and_then(Value::as_str).unwrap_or("");
            out.insert(format!("api_{}", ident(id)), (format!("{} {path}", method.to_uppercase()), doc.to_string()));
        }
    }
    out
}

/// The functions of one library script that another script can call after `import "<name>"`:
/// every function that is not private, with its parameters and `///` comments. A script that does
/// not parse gives its error instead.
pub fn library_functions(engine: &Engine, name: &str, text: &str) -> Result<Vec<Value>, String> {
    let ast = engine.compile(text).map_err(|e| format!("{name}: {e}"))?;
    Ok(ast.iter_functions()
        .filter(|f| f.access != FnAccess::Private)
        .map(|f| {
            let doc: Vec<String> = f.comments.iter()
                .map(|c| c.trim_start_matches("///").trim_start_matches("/**").trim_end_matches("*/").trim().to_string())
                .filter(|c| !c.is_empty())
                .collect();
            json!({
                "name": format!("{name}::{}", f.name),
                "signature": format!("{name}::{}({})", f.name, f.params.join(", ")),
                "kind": "library",
                "module": name,
                "params": f.params,
                "doc": doc.join("\n"),
            })
        })
        .collect())
}

/// The whole catalogue: `{functions: [...], counts: {kind: n}, errors: [...]}`, every function once
/// per name with all its signatures (Rhai overloads by arity), sorted by name.
pub fn catalogue(src: &Sources) -> Value {
    let docs = host_docs(src.host_doc);
    let tools = tools_of(src.catalogue);
    let ops = operations_of(src.openapi);
    let mut by_name: BTreeMap<String, Value> = BTreeMap::new();
    for sig in src.engine.gen_fn_signatures(false) {
        let name = name_of(&sig).to_string();
        if name.is_empty() || name.starts_with("anon$") { continue; }
        let entry = by_name.entry(name.clone()).or_insert_with(|| {
            let (kind, doc, extra) = if let Some((doc, schema)) = tools.get(&name) {
                ("tool", doc.clone(), json!({"schema": schema}))
            } else if let Some((route, doc)) = ops.get(&name) {
                ("route", doc.clone(), json!({"route": route}))
            } else if name.starts_with("con_") {
                ("console", format!("the OpenTTD console command `{}`; with a string, its arguments", &name[4..]), json!({}))
            } else if name.starts_with("hub_") {
                ("hub", docs.get(&name).cloned().unwrap_or_else(|| "a method of the MCP hub (POST /hub/rpc)".into()), json!({}))
            } else {
                ("host", docs.get(&name).cloned().unwrap_or_default(), json!({}))
            };
            let mut v = json!({"name": name, "kind": kind, "doc": doc, "signatures": []});
            if let (Some(o), Some(e)) = (v.as_object_mut(), extra.as_object()) {
                for (k, x) in e { o.insert(k.clone(), x.clone()); }
            }
            v
        });
        if let Some(a) = entry.get_mut("signatures").and_then(Value::as_array_mut) {
            let s = Value::String(clean_signature(&sig));
            if !a.contains(&s) { a.push(s); }
        }
    }
    let mut errors = Vec::new();
    for (name, text) in src.library {
        match library_functions(src.engine, name, text) {
            Ok(fns) => for mut f in fns {
                let n = f["name"].as_str().unwrap_or("").to_string();
                let sig = f["signature"].clone();
                f["signatures"] = json!([sig]);
                if let Some(o) = f.as_object_mut() { o.remove("signature"); }
                by_name.entry(n).or_insert(f);
            },
            Err(e) => errors.push(Value::String(e)),
        }
    }
    let mut counts: BTreeMap<String, u64> = BTreeMap::new();
    for f in by_name.values() {
        *counts.entry(f["kind"].as_str().unwrap_or("").to_string()).or_default() += 1;
    }
    json!({"functions": by_name.into_values().collect::<Vec<_>>(), "counts": counts, "errors": errors})
}

/// The library as (name, text), read from the script directory.
pub fn read_library(dir: &std::path::Path) -> Vec<(String, String)> {
    let mut out: Vec<(String, String)> = std::fs::read_dir(dir).into_iter().flatten().flatten()
        .filter_map(|e| {
            let p = e.path();
            let name = p.file_name()?.to_str()?.strip_suffix(".rhai")?.to_string();
            Some((name, std::fs::read_to_string(&p).ok()?))
        })
        .collect();
    out.sort();
    out
}

fn cache() -> &'static Mutex<Option<(Instant, Arc<Value>)>> {
    static C: std::sync::OnceLock<Mutex<Option<(Instant, Arc<Value>)>>> = std::sync::OnceLock::new();
    C.get_or_init(|| Mutex::new(None))
}

/// How long one catalogue answers before it is derived again (a new tool, route or script shows
/// up within this time).
const FRESH: Duration = Duration::from_secs(30);

fn get_json(url: &str) -> Value {
    ureq::get(url).timeout(Duration::from_secs(20)).call().ok()
        .and_then(|r| r.into_json::<Value>().ok())
        .unwrap_or(Value::Null)
}

/// GET /api/rhai/functions: the catalogue of the engine a script of this panel runs in, derived
/// at most every 30 seconds; `?fresh=1` derives it now.
pub fn h_functions(g: &Arc<crate::panel::Game>, i: &crate::panel::Input) -> Result<(u16, Value), String> {
    let fresh = i.query.get("fresh").map(|v| v == "1" || v == "true").unwrap_or(false);
    if !fresh {
        if let Ok(slot) = cache().lock() {
            if let Some((at, v)) = slot.as_ref() {
                if at.elapsed() < FRESH { return Ok((200, (**v).clone())); }
            }
        }
    }
    let base = g.panel_url();
    let hub = std::env::var("MCP_HUB").ok().filter(|h| !h.is_empty());
    let engine = crate::rhai_host::host(&base, hub.as_deref());
    let catalogue = get_json(&format!("{base}/api/catalogue"));
    let openapi = get_json(&format!("{base}/openapi.json"));
    let library = read_library(&crate::rhai_host::rhai_dir());
    let v = catalogue_with_time(&Sources { engine: &engine, host_doc: HOST_DOC, catalogue: &catalogue, openapi: &openapi, library: &library });
    if let Ok(mut slot) = cache().lock() { *slot = Some((Instant::now(), Arc::new(v.clone()))); }
    Ok((200, v))
}

fn catalogue_with_time(src: &Sources) -> Value {
    let mut v = catalogue(src);
    let secs = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    v["derived_at"] = json!(secs);
    v
}

#[cfg(test)]
mod tests {
    use super::*;

    const DOC: &str = "//! Usage: mcp-rhai\n//!\n//! Functions a script gets:\n//!   rpc(method)  rpc(method, params)  -> map   raw MCP JSON-RPC answer\n//!   kernel(id, input)                  -> map   one AlgoGlyph kernel on that input, through\n//!                                               the flow CLI\n//!   tools()  commands()  help()          the derived functions below, by name\n//!\n//! Derived at start-up\nuse std::x;\n";

    #[test]
    fn host_docs_read_each_function_once_with_continuations() {
        let d = host_docs(DOC);
        assert_eq!(d.get("rpc").map(String::as_str), Some("rpc(method)  rpc(method, params)  -> map   raw MCP JSON-RPC answer"));
        assert!(d["kernel"].ends_with("on that input, through the flow CLI"), "{}", d["kernel"]);
        for n in ["tools", "commands", "help"] { assert!(d[n].contains("the derived functions"), "{n}"); }
        assert!(!d.contains_key("Usage"));
    }

    #[test]
    fn host_docs_cover_the_real_header() {
        let d = host_docs(HOST_DOC);
        for n in ["rpc", "read", "board_post", "kernel", "loopback_get", "env", "sleep", "to_json", "tools"] {
            assert!(d.contains_key(n), "main.rs documents {n}");
        }
    }

    #[test]
    fn signatures_read_as_rhai() {
        assert_eq!(clean_signature("rpc(_: string) -> core::result::Result<rhai::types::dynamic::Dynamic,alloc::boxed::Box<rhai::types::error::EvalAltResult>>"),
                   "rpc(string) -> Dynamic");
        assert_eq!(clean_signature("board_post(claim: &str, text: &str) -> Result<bool, Box<EvalAltResult>>"), "board_post(claim: &str, text: &str) -> bool");
        assert_eq!(clean_signature("sleep(_: i64)"), "sleep(i64)");
        assert_eq!(clean_signature("f(a: Result<i64>) -> i64"), "f(a: Result<i64>) -> i64");
        assert_eq!(clean_signature("plain"), "plain");
    }

    #[test]
    fn names_and_identifiers() {
        assert_eq!(name_of("fn board_post(claim: &str, text: &str) -> bool"), "board_post");
        assert_eq!(name_of("rpc(method: &str) -> Dynamic"), "rpc");
        assert_eq!(ident("openttd.game.set_pause"), "openttd_game_set_pause");
        assert!(calls_in("plain text").is_empty());
        assert_eq!(calls_in("a(b(c))  d()  rest"), vec!["a", "d"]);
        assert_eq!(matching_paren("a(b", 1), None);
    }

    #[test]
    fn catalogue_derives_host_tool_route_console_and_library() {
        let mut e = Engine::new_raw();
        e.register_fn("rpc", |_m: &str| 1_i64);
        e.register_fn("rpc", |_m: &str, _p: rhai::Map| 1_i64);
        e.register_fn("openttd_game_set_pause", |_a: rhai::Map| 1_i64);
        e.register_fn("api_bots", || 1_i64);
        e.register_fn("con_date", || 1_i64);
        e.register_fn("hub_audit", || 1_i64);
        let cat = json!({"tools": {"result": {"tools": [{"name": "openttd.game.set_pause", "description": "Pause the game", "inputSchema": {"type": "object"}}]}}});
        let api = json!({"paths": {"/api/bots": {"get": {"operationId": "bots", "summary": "Every bot"}}}});
        let lib = vec![
            ("ttd".to_string(), "/// Start a new game.\nfn new_game(seed) { seed }\nprivate fn hidden() {}\n".to_string()),
            ("broken".to_string(), "fn (".to_string()),
        ];
        let v = catalogue(&Sources { engine: &e, host_doc: DOC, catalogue: &cat, openapi: &api, library: &lib });
        let f = |n: &str| v["functions"].as_array().unwrap().iter().find(|f| f["name"] == n).cloned().unwrap_or(Value::Null);
        assert_eq!(f("rpc")["kind"], "host");
        assert_eq!(f("rpc")["signatures"].as_array().unwrap().len(), 2);
        assert!(f("rpc")["doc"].as_str().unwrap().contains("raw MCP JSON-RPC"));
        assert_eq!(f("openttd_game_set_pause")["kind"], "tool");
        assert_eq!(f("openttd_game_set_pause")["doc"], "Pause the game");
        assert_eq!(f("openttd_game_set_pause")["schema"]["type"], "object");
        assert_eq!(f("api_bots")["kind"], "route");
        assert_eq!(f("api_bots")["route"], "GET /api/bots");
        assert_eq!(f("con_date")["kind"], "console");
        assert_eq!(f("hub_audit")["kind"], "hub");
        assert_eq!(f("ttd::new_game")["kind"], "library");
        assert_eq!(f("ttd::new_game")["doc"], "Start a new game.");
        assert_eq!(f("ttd::new_game")["signatures"][0], "ttd::new_game(seed)");
        assert!(f("ttd::hidden").is_null());
        assert_eq!(v["errors"].as_array().unwrap().len(), 1);
        assert_eq!(v["counts"]["library"], 1);
    }

    #[test]
    fn read_library_reads_rhai_files_sorted() {
        let dir = std::env::temp_dir().join(format!("rhai-cat-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("b.rhai"), "fn x() {}").unwrap();
        std::fs::write(dir.join("a.rhai"), "fn y() {}").unwrap();
        std::fs::write(dir.join("note.txt"), "no").unwrap();
        let lib = read_library(&dir);
        assert_eq!(lib.iter().map(|(n, _)| n.as_str()).collect::<Vec<_>>(), vec!["a", "b"]);
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
