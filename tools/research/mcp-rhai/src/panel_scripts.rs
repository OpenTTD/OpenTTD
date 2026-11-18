//! RESEARCH-ONLY: the panel's Rhai scripts as a workspace (the Scripts page, static/scripts.html).
//!
//! Every `.rhai` file of the script library (`rhai_host::rhai_dir()`: $MCP_RHAI_PATH, else
//! tools/research/mcp-panel/rhai) is listed with what its own text says: the header comment as its
//! documentation, the libraries it imports, whether another script imports it (a library) and
//! whether it runs as a service now. Nothing here names a script.
//!
//! - read, check (the Rhai parser's error with its line and column, nothing run), save (only what
//!   parses; the file is written in place, so the repository's diff shows the change and committing
//!   stays the owner's);
//! - services: a script started in the background in this process (`spawn_script`), its printed
//!   lines kept (the newest SERVICE_LINES) and readable after a sequence number;
//! - hot reload: a watcher looks at the library's files every RELOAD_EVERY; a running service whose
//!   own file, or any library it imports, changed (by the page's Save or by an editor outside) is
//!   stopped and started again on the new text, and says so in its log.
//! Stopping a service sets its abort flag, which ends it at its next operation (spawn_script).

use serde_json::{json, Value};
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{mpsc, Arc, Mutex, OnceLock};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use crate::panel::{spawn_script, Game, Input, ScriptLine};
use crate::rhai_host;

/// The lines a service keeps, newest last.
pub const SERVICE_LINES: usize = 2000;
/// How often the watcher looks for changed files.
pub const RELOAD_EVERY: Duration = Duration::from_millis(1000);

/// Whether `name` may name a script: letters, digits and underscores, so a name never leaves the
/// library's folder.
pub fn valid_name(name: &str) -> bool {
    !name.is_empty() && name.len() <= 64 && name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_')
}

fn path_of(dir: &Path, name: &str) -> Result<PathBuf, String> {
    if !valid_name(name) { return Err(format!("{name:?} is not a script name (letters, digits, _)")); }
    Ok(dir.join(format!("{name}.rhai")))
}

/// The header comment of a script: its leading `//` lines, the comment marks taken off.
pub fn doc_of(text: &str) -> String {
    let mut out = Vec::new();
    for line in text.lines() {
        let t = line.trim_start();
        if let Some(rest) = t.strip_prefix("//") {
            out.push(rest.strip_prefix(' ').unwrap_or(rest).to_string());
        } else if t.is_empty() && out.is_empty() {
            continue;
        } else {
            break;
        }
    }
    out.join("\n").trim().to_string()
}

/// A line without its `//` comment (a `//` inside a string literal is kept).
fn code_of(line: &str) -> &str {
    let b = line.as_bytes();
    let mut in_str = false;
    let mut i = 0;
    while i < b.len() {
        match b[i] {
            b'\\' if in_str => i += 1,
            b'"' => in_str = !in_str,
            b'/' if !in_str && b.get(i + 1) == Some(&b'/') => return &line[..i],
            _ => {}
        }
        i += 1;
    }
    line
}

/// The libraries a script imports (`import "name"`), in the order it names them; an import in a
/// comment is not one.
pub fn imports_of(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    for line in text.lines() {
        let mut rest = code_of(line);
        while let Some(i) = rest.find("import \"") {
            let after = &rest[i + 8..];
            if let Some(end) = after.find('"') {
                let name = &after[..end];
                if !out.iter().any(|n| n == name) { out.push(name.to_string()); }
                rest = &after[end..];
            } else {
                break;
            }
        }
    }
    out
}

/// The Rhai parser's verdict on a text: null when it parses, else {message, line, column}.
pub fn check(text: &str) -> Value {
    let engine = rhai::Engine::new();
    match engine.compile(text) {
        Ok(_) => Value::Null,
        Err(e) => {
            let pos = e.position();
            json!({"message": e.to_string(), "line": pos.line(), "column": pos.position()})
        }
    }
}

fn modified(p: &Path) -> Option<SystemTime> {
    std::fs::metadata(p).and_then(|m| m.modified()).ok()
}

fn secs(t: Option<SystemTime>) -> Value {
    t.and_then(|t| t.duration_since(UNIX_EPOCH).ok()).map_or(Value::Null, |d| json!(d.as_secs_f64()))
}

/// Every script in `dir`: {name, file, doc, imports, imported_by, library, bytes, modified}, by name.
pub fn list(dir: &Path) -> Vec<Value> {
    let mut texts: BTreeMap<String, (String, Option<SystemTime>, u64)> = BTreeMap::new();
    for entry in std::fs::read_dir(dir).into_iter().flatten().flatten() {
        let p = entry.path();
        if p.extension().and_then(|e| e.to_str()) != Some("rhai") { continue; }
        let Some(name) = p.file_stem().and_then(|s| s.to_str()).map(str::to_string) else { continue };
        if !valid_name(&name) { continue; }
        let text = std::fs::read_to_string(&p).unwrap_or_default();
        let bytes = text.len() as u64;
        texts.insert(name, (text, modified(&p), bytes));
    }
    let mut imported_by: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    for (name, (text, _, _)) in &texts {
        for lib in imports_of(text) { imported_by.entry(lib).or_default().insert(name.clone()); }
    }
    texts.iter().map(|(name, (text, m, bytes))| {
        let by: Vec<String> = imported_by.get(name).map(|s| s.iter().cloned().collect()).unwrap_or_default();
        json!({"name": name, "file": format!("{name}.rhai"), "doc": doc_of(text), "imports": imports_of(text),
               "imported_by": by, "library": !by.is_empty(), "bytes": bytes, "modified": secs(*m)})
    }).collect()
}

// ------------------------------------------------------------------------------------- services

struct Service {
    abort: Arc<AtomicBool>,
    lines: Arc<Mutex<VecDeque<(u64, String)>>>,
    seq: Arc<Mutex<u64>>,
    running: Arc<AtomicBool>,
    started: SystemTime,
    reloads: u64,
    /// The modification times the service was started on: its own file and every library it imports.
    stamps: Vec<(PathBuf, Option<SystemTime>)>,
}

fn services() -> &'static Mutex<BTreeMap<String, Service>> {
    static S: OnceLock<Mutex<BTreeMap<String, Service>>> = OnceLock::new();
    S.get_or_init(|| Mutex::new(BTreeMap::new()))
}

fn push(lines: &Mutex<VecDeque<(u64, String)>>, seq: &Mutex<u64>, text: String) {
    let mut n = seq.lock().unwrap();
    *n += 1;
    let mut l = lines.lock().unwrap();
    l.push_back((*n, text));
    while l.len() > SERVICE_LINES { l.pop_front(); }
}

/// The files a service depends on: its own and, transitively, every library it imports.
fn stamps(dir: &Path, name: &str) -> Vec<(PathBuf, Option<SystemTime>)> {
    let mut seen = BTreeSet::new();
    let mut todo = vec![name.to_string()];
    let mut out = Vec::new();
    while let Some(n) = todo.pop() {
        if !seen.insert(n.clone()) { continue; }
        let Ok(p) = path_of(dir, &n) else { continue };
        let text = std::fs::read_to_string(&p).unwrap_or_default();
        todo.extend(imports_of(&text));
        out.push((p.clone(), modified(&p)));
    }
    out
}

/// Start `name` as a service on the panel at `base` (replacing a run of it that ended); its lines
/// carry `reloads` restarts already.
fn start_in(base: &str, dir: &Path, name: &str, reloads: u64, carry: Option<(Arc<Mutex<VecDeque<(u64, String)>>>, Arc<Mutex<u64>>)>) -> Result<Service, String> {
    let p = path_of(dir, name)?;
    let text = std::fs::read_to_string(&p).map_err(|e| format!("{}: {e}", p.display()))?;
    if let Some(err) = check(&text).as_object() {
        return Err(format!("{name} does not parse: {}", err.get("message").and_then(Value::as_str).unwrap_or("?")));
    }
    let (lines, seq) = carry.unwrap_or_else(|| (Arc::new(Mutex::new(VecDeque::new())), Arc::new(Mutex::new(0))));
    let abort = Arc::new(AtomicBool::new(false));
    let running = Arc::new(AtomicBool::new(true));
    let (tx, rx) = mpsc::channel();
    let st = stamps(dir, name);
    push(&lines, &seq, format!("== {} {name} ({} file(s) watched)", if reloads == 0 { "started" } else { "reloaded" }, st.len()));
    spawn_script(base.to_string(), text, abort.clone(), tx);
    let (l2, s2, r2) = (lines.clone(), seq.clone(), running.clone());
    thread::spawn(move || {
        for msg in rx {
            match msg {
                ScriptLine::Out(t) => push(&l2, &s2, t),
                ScriptLine::Err(t) => push(&l2, &s2, format!("error: {t}")),
                ScriptLine::Exit(code) => { push(&l2, &s2, format!("== exit {code}")); break; }
            }
        }
        r2.store(false, Ordering::SeqCst);
    });
    Ok(Service { abort, lines, seq, running, started: SystemTime::now(), reloads, stamps: st })
}

/// Start a service (no-op answer when it already runs).
pub fn start(base: &str, dir: &Path, name: &str) -> Result<Value, String> {
    let mut all = services().lock().unwrap();
    if let Some(s) = all.get(name) {
        if s.running.load(Ordering::SeqCst) { return Ok(json!({"name": name, "running": true, "already": true})); }
    }
    let carry = all.get(name).map(|s| (s.lines.clone(), s.seq.clone()));
    let svc = start_in(base, dir, name, 0, carry)?;
    all.insert(name.to_string(), svc);
    Ok(json!({"name": name, "running": true}))
}

/// Stop a service; its lines stay readable.
pub fn stop(name: &str) -> Result<Value, String> {
    let all = services().lock().unwrap();
    let s = all.get(name).ok_or(format!("{name} is not a service here"))?;
    s.abort.store(true, Ordering::SeqCst);
    push(&s.lines, &s.seq, "== stop requested".into());
    Ok(json!({"name": name, "stopping": true}))
}

/// Every service: {name, running, started, reloads, last_seq, watched}.
pub fn status() -> Vec<Value> {
    services().lock().unwrap().iter().map(|(n, s)| json!({
        "name": n, "running": s.running.load(Ordering::SeqCst), "started": secs(Some(s.started)),
        "reloads": s.reloads, "last_seq": *s.seq.lock().unwrap(),
        "watched": s.stamps.iter().map(|(p, _)| p.file_name().map(|f| f.to_string_lossy().to_string()).unwrap_or_default()).collect::<Vec<_>>(),
    })).collect()
}

/// A service's lines after sequence number `since`: {name, running, lines: [{seq, text}], last_seq}.
pub fn log(name: &str, since: u64) -> Result<Value, String> {
    let all = services().lock().unwrap();
    let s = all.get(name).ok_or(format!("{name} has not run as a service here"))?;
    let lines: Vec<Value> = s.lines.lock().unwrap().iter().filter(|(n, _)| *n > since).map(|(n, t)| json!({"seq": n, "text": t})).collect();
    Ok(json!({"name": name, "running": s.running.load(Ordering::SeqCst), "lines": lines, "last_seq": *s.seq.lock().unwrap()}))
}

/// One look of the watcher: every running service whose watched files changed is restarted on the
/// new text (one that no longer parses keeps running on the old one and says why). Returns the
/// names reloaded.
pub fn reload_changed(base: &str, dir: &Path) -> Vec<String> {
    let mut all = services().lock().unwrap();
    let mut done = Vec::new();
    let names: Vec<String> = all.keys().cloned().collect();
    for name in names {
        let changed = {
            let s = &all[&name];
            s.running.load(Ordering::SeqCst) && s.stamps.iter().any(|(p, t)| modified(p) != *t)
        };
        if !changed { continue; }
        let (lines, seq, reloads) = { let s = &all[&name]; (s.lines.clone(), s.seq.clone(), s.reloads) };
        // Every file it would load must parse: its own and each library it imports.
        let broken = stamps(dir, &name).into_iter().find_map(|(p, _)| {
            let err = check(&std::fs::read_to_string(&p).unwrap_or_default());
            err.as_object().map(|e| format!("{}: {}", p.file_name().map(|f| f.to_string_lossy().to_string()).unwrap_or_default(),
                                            e.get("message").and_then(Value::as_str).unwrap_or("?")))
        });
        if let Some(why) = broken {
            push(&lines, &seq, format!("== not reloaded, the new text does not parse: {why}"));
            if let Some(s) = all.get_mut(&name) { s.stamps = stamps(dir, &name); }
            continue;
        }
        all[&name].abort.store(true, Ordering::SeqCst);
        match start_in(base, dir, &name, reloads + 1, Some((lines.clone(), seq.clone()))) {
            Ok(svc) => { all.insert(name.clone(), svc); done.push(name); }
            Err(e) => push(&lines, &seq, format!("== reload failed: {e}")),
        }
    }
    done
}

/// The watcher thread (started once by the panel): reload_changed every RELOAD_EVERY.
pub fn watch(g: Arc<Game>) {
    static STARTED: OnceLock<()> = OnceLock::new();
    if STARTED.set(()).is_err() { return; }
    thread::spawn(move || loop {
        thread::sleep(RELOAD_EVERY);
        for name in reload_changed(&g.panel_url(), &rhai_host::rhai_dir()) {
            g.event("rhai", &format!("service {name} reloaded: its file or a library it imports changed"));
        }
    });
}

// ------------------------------------------------------------------------------------- handlers

fn q<'a>(i: &'a Input, key: &str) -> Option<String> {
    i.body.get(key).and_then(Value::as_str).map(str::to_string).or_else(|| i.query.get(key).map(|s| s.to_string()))
}

pub fn h_scripts(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    watch(g.clone());
    let dir = rhai_host::rhai_dir();
    let running: BTreeMap<String, Value> = status().into_iter().map(|s| (s["name"].as_str().unwrap_or("").to_string(), s)).collect();
    let scripts: Vec<Value> = list(&dir).into_iter().map(|mut s| {
        let n = s["name"].as_str().unwrap_or("").to_string();
        s["service"] = running.get(&n).cloned().unwrap_or(Value::Null);
        s
    }).collect();
    // Shown relative to the repository (tools/research/mcp-panel/rhai), absolute only outside it.
    let repo = crate::panel_results::repo();
    let shown = dir.strip_prefix(repo.canonicalize().unwrap_or(repo.clone())).map(|p| p.display().to_string()).unwrap_or_else(|_| dir.display().to_string());
    Ok((200, json!({"dir": shown, "dir_abs": dir.display().to_string(), "scripts": scripts})))
}

pub fn h_script(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = q(i, "name").ok_or("?name= names the script")?;
    let p = path_of(&rhai_host::rhai_dir(), &name)?;
    let text = std::fs::read_to_string(&p).map_err(|e| format!("{name}: {e}"))?;
    Ok((200, json!({"name": name, "text": text, "doc": doc_of(&text), "imports": imports_of(&text),
                    "modified": secs(modified(&p)), "error": check(&text)})))
}

pub fn h_script_check(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let text = q(i, "text").unwrap_or_default();
    Ok((200, json!({"error": check(&text), "imports": imports_of(&text)})))
}

pub fn h_script_save(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = q(i, "name").ok_or("name names the script")?;
    let text = q(i, "text").ok_or("text is the script")?;
    let err = check(&text);
    if !err.is_null() { return Ok((422, json!({"saved": false, "error": err}))); }
    let p = path_of(&rhai_host::rhai_dir(), &name)?;
    std::fs::write(&p, &text).map_err(|e| format!("{}: {e}", p.display()))?;
    g.event("rhai", &format!("script {name} saved ({} bytes)", text.len()));
    Ok((200, json!({"saved": true, "name": name, "modified": secs(modified(&p)), "error": Value::Null})))
}

pub fn h_services(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    Ok((200, json!({"services": status(), "reload_every_ms": RELOAD_EVERY.as_millis() as u64})))
}

pub fn h_service_start(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    watch(g.clone());
    let name = q(i, "name").ok_or("name names the script")?;
    let out = start(&g.panel_url(), &rhai_host::rhai_dir(), &name)?;
    g.event("rhai", &format!("service {name} started"));
    Ok((200, out))
}

pub fn h_service_stop(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = q(i, "name").ok_or("name names the script")?;
    let out = stop(&name)?;
    g.event("rhai", &format!("service {name} stopped"));
    Ok((200, out))
}

pub fn h_service_log(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = q(i, "name").ok_or("?name= names the service")?;
    let since = q(i, "since").and_then(|s| s.parse().ok()).unwrap_or(0);
    Ok((200, log(&name, since)?))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tmp(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("panel-scripts-{tag}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    #[test]
    fn a_name_never_leaves_the_library() {
        assert!(valid_name("rail_lib"));
        for bad in ["", "../x", "a/b", "a.rhai", "a b", &"x".repeat(65)] { assert!(!valid_name(bad), "{bad}"); }
        assert!(path_of(Path::new("/l"), "../etc").is_err());
    }

    #[test]
    fn a_scripts_doc_and_imports_come_from_its_own_text() {
        let t = "// Rail steward: keeps lines paying.\n//\n// More.\nimport \"rail_lib\" as rl;\nimport \"ttd\" as ttd; // import \"no\"\n// import \"commented\"\nlet x = 1;\n";
        assert_eq!(doc_of(t), "Rail steward: keeps lines paying.\n\nMore.");
        assert_eq!(imports_of(t), vec!["rail_lib", "ttd"]);
    }

    #[test]
    fn check_names_the_line_and_column_of_a_parse_error() {
        assert!(check("let a = 1; a + 2").is_null());
        let e = check("let a = 1;\nlet b = ;\n");
        assert_eq!(e["line"], json!(2), "{e}");
        assert!(e["message"].as_str().unwrap().len() > 3);
    }

    #[test]
    fn a_library_is_a_script_another_one_imports() {
        let d = tmp("list");
        std::fs::write(d.join("lib_a.rhai"), "// A library.\nfn f() { 1 }\n").unwrap();
        std::fs::write(d.join("main_b.rhai"), "// Uses a.\nimport \"lib_a\" as a;\n").unwrap();
        std::fs::write(d.join("notes.txt"), "not a script").unwrap();
        let l = list(&d);
        assert_eq!(l.len(), 2);
        assert_eq!(l[0]["name"], "lib_a");
        assert_eq!(l[0]["library"], true);
        assert_eq!(l[0]["imported_by"], json!(["main_b"]));
        assert_eq!(l[1]["library"], false);
        assert_eq!(l[1]["doc"], "Uses a.");
        let st = stamps(&d, "main_b");
        assert_eq!(st.len(), 2, "its own file and the library it imports are watched");
    }

    /// A service runs, its lines are readable after a sequence number, an edit of a library it
    /// imports reloads it, an edit that does not parse keeps the old one running, and stop ends it.
    #[test]
    fn a_service_reloads_when_its_library_changes_and_stops_on_request() {
        let d = tmp("svc");
        std::fs::write(d.join("svc_lib.rhai"), "fn word() { \"one\" }\n").unwrap();
        std::fs::write(d.join("svc_main.rhai"), "import \"svc_lib\" as l;\nprint(l::word());\nloop { sleep(20); }\n").unwrap();
        std::env::set_var("MCP_RHAI_PATH", &d);
        let base = "http://127.0.0.1:9";
        start(base, &d, "svc_main").unwrap();
        let wait = |needle: &str| {
            for _ in 0..200 {
                let l = log("svc_main", 0).unwrap();
                if l["lines"].as_array().unwrap().iter().any(|x| x["text"].as_str().unwrap_or("").contains(needle)) { return true; }
                thread::sleep(Duration::from_millis(25));
            }
            false
        };
        assert!(wait("one"), "{}", log("svc_main", 0).unwrap());
        thread::sleep(Duration::from_millis(1100)); // a new modification time on coarse file systems
        std::fs::write(d.join("svc_lib.rhai"), "fn word() { \"two\" }\n").unwrap();
        assert_eq!(reload_changed(base, &d), vec!["svc_main".to_string()]);
        assert!(wait("two") && wait("reloaded"));
        thread::sleep(Duration::from_millis(1100));
        std::fs::write(d.join("svc_lib.rhai"), "fn word( { \"three\" }\n").unwrap();
        assert!(reload_changed(base, &d).is_empty());
        assert!(wait("not reloaded"));
        assert_eq!(status()[0]["reloads"], json!(1));
        stop("svc_main").unwrap();
        for _ in 0..200 { if !log("svc_main", 0).unwrap()["running"].as_bool().unwrap() { break; } thread::sleep(Duration::from_millis(25)); }
        assert_eq!(log("svc_main", 0).unwrap()["running"], false);
        assert!(start(base, &d, "nope_missing").is_err());
    }
}
