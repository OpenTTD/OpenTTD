//! RESEARCH-ONLY: the panel's own npm packages as ES modules for the browser (I0089,
//! research/rhai-ide.md). `npm ci` in tools/research/mcp-panel installs them; nothing is fetched
//! from a CDN and no copy is committed.
//!
//! - `GET /vendor/npm/<package>/<file>` serves a file of an installed package (scripts, styles,
//!   source maps and JSON only; no parent steps).
//! - The import map a page needs for bare specifiers (`import {EditorView} from
//!   '@codemirror/view'`) is derived from npm's own record of what it installed
//!   (node_modules/.package-lock.json) and each package's package.json, never listed by hand.

use std::path::{Path, PathBuf};
use std::sync::OnceLock;

use serde_json::{json, Map, Value};

/// The file `GET /vendor/npm/<rel>` names under `node_modules`, with its content type, or None for
/// a path that is not one (another prefix, a parent step, a type the browser does not load).
pub fn npm_file(node_modules: &Path, path: &str) -> Option<(PathBuf, &'static str)> {
    let rel = path.strip_prefix("/vendor/npm/")?;
    if rel.is_empty() || rel.starts_with('/') || rel.split('/').any(|p| p == ".." || p == "." || p.is_empty()) {
        return None;
    }
    let ctype = match Path::new(rel).extension().and_then(|e| e.to_str())? {
        "js" | "mjs" | "cjs" => "text/javascript",
        "css" => "text/css",
        "map" | "json" => "application/json",
        _ => return None,
    };
    let f = node_modules.join(rel);
    f.is_file().then_some((f, ctype))
}

/// The ES module entry of a package.json: `exports["."]` (a string, or its `import`, `module`,
/// `browser` or `default` condition, nested conditions followed), else `module`, else `main`.
pub fn esm_entry(pkg: &Value) -> Option<String> {
    fn pick(v: &Value) -> Option<String> {
        match v {
            Value::String(s) => Some(s.clone()),
            Value::Object(m) => ["import", "module", "browser", "default"].iter().find_map(|k| m.get(*k).and_then(pick)),
            Value::Array(a) => a.iter().find_map(pick),
            _ => None,
        }
    }
    let exports = pkg.get("exports");
    let dot = match exports {
        Some(Value::Object(m)) if m.contains_key(".") => m.get(".").and_then(pick),
        Some(Value::Object(m)) if !m.keys().any(|k| k.starts_with('.')) => exports.and_then(pick),
        Some(Value::String(s)) => Some(s.clone()),
        _ => None,
    };
    dot.or_else(|| pkg.get("module").and_then(Value::as_str).map(str::to_string))
        .or_else(|| pkg.get("main").and_then(Value::as_str).map(str::to_string))
        .map(|e| e.trim_start_matches("./").to_string())
}

/// The import map of every top-level installed package with an entry, from npm's
/// node_modules/.package-lock.json: `{"imports": {"<name>": "/vendor/npm/<name>/<entry>"}}`.
/// Nested copies (`node_modules/a/node_modules/b`) are left out: one version per name.
pub fn import_map(node_modules: &Path) -> Value {
    let lock: Value = std::fs::read_to_string(node_modules.join(".package-lock.json")).ok()
        .and_then(|t| serde_json::from_str(&t).ok()).unwrap_or(Value::Null);
    let mut imports = Map::new();
    for key in lock.get("packages").and_then(Value::as_object).into_iter().flatten().map(|(k, _)| k) {
        let Some(name) = key.strip_prefix("node_modules/") else { continue };
        if name.contains("/node_modules/") { continue; }
        let pkg: Value = std::fs::read_to_string(node_modules.join(name).join("package.json")).ok()
            .and_then(|t| serde_json::from_str(&t).ok()).unwrap_or(Value::Null);
        if let Some(entry) = esm_entry(&pkg) {
            imports.insert(name.to_string(), Value::String(format!("/vendor/npm/{name}/{entry}")));
        }
    }
    json!({"imports": imports})
}

/// The import map of this panel's node_modules as a `<script type="importmap">` element, derived
/// once per process (npm ci does not run under a running panel).
pub fn import_map_tag(node_modules: &Path) -> &'static str {
    static TAG: OnceLock<String> = OnceLock::new();
    TAG.get_or_init(|| {
        let map = import_map(node_modules).to_string().replace("</", "<\\/");
        format!("<script type=\"importmap\">{map}</script>")
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fixture() -> PathBuf {
        let d = std::env::temp_dir().join(format!("panel-npm-{}-{:?}", std::process::id(), std::thread::current().id()));
        let _ = std::fs::remove_dir_all(&d);
        let nm = d.join("node_modules");
        let write = |p: &str, t: &str| {
            let f = nm.join(p);
            std::fs::create_dir_all(f.parent().unwrap()).unwrap();
            std::fs::write(f, t).unwrap();
        };
        write(".package-lock.json", r#"{"packages": {"": {}, "node_modules/@codemirror/view": {}, "node_modules/style-mod": {}, "node_modules/cjs-only": {}, "node_modules/nothing": {}, "node_modules/a/node_modules/style-mod": {}}}"#);
        write("@codemirror/view/package.json", r#"{"exports": {".": {"import": "./dist/index.js", "require": "./dist/index.cjs"}}}"#);
        write("@codemirror/view/dist/index.js", "export {}");
        write("style-mod/package.json", r#"{"module": "src/style-mod.js", "main": "dist/style-mod.cjs"}"#);
        write("cjs-only/package.json", r#"{"main": "index.js"}"#);
        write("nothing/package.json", r#"{}"#);
        write("@codemirror/view/README.md", "no");
        nm
    }

    #[test]
    fn the_import_map_is_derived_from_npms_lock() {
        let nm = fixture();
        let m = import_map(&nm);
        assert_eq!(m["imports"]["@codemirror/view"], "/vendor/npm/@codemirror/view/dist/index.js");
        assert_eq!(m["imports"]["style-mod"], "/vendor/npm/style-mod/src/style-mod.js");
        assert_eq!(m["imports"]["cjs-only"], "/vendor/npm/cjs-only/index.js");
        assert!(m["imports"].get("nothing").is_none());
        assert_eq!(m["imports"].as_object().unwrap().len(), 3, "nested copies are left out");
        assert_eq!(import_map(Path::new("/nonexistent")), json!({"imports": {}}));
    }

    #[test]
    fn only_files_of_installed_packages_are_served() {
        let nm = fixture();
        assert_eq!(npm_file(&nm, "/vendor/npm/@codemirror/view/dist/index.js").map(|(_, c)| c), Some("text/javascript"));
        for bad in ["/vendor/npm/../secret.js", "/vendor/npm/@codemirror/../x.js", "/vendor/npm//etc/x.js", "/vendor/npm/",
                    "/vendor/npm/@codemirror/view/README.md", "/vendor/npm/@codemirror/view/dist/missing.js", "/static/x.js",
                    "/vendor/npm/a/./b.js"] {
            assert!(npm_file(&nm, bad).is_none(), "{bad}");
        }
        assert_eq!(npm_file(&nm, "/vendor/npm/style-mod/package.json").map(|(_, c)| c), Some("application/json"));
    }

    #[test]
    fn esm_entries_follow_the_package_conventions() {
        assert_eq!(esm_entry(&json!({"exports": "./x.js"})).as_deref(), Some("x.js"));
        assert_eq!(esm_entry(&json!({"exports": {"import": "./i.js", "require": "./r.cjs"}})).as_deref(), Some("i.js"));
        assert_eq!(esm_entry(&json!({"exports": {".": {"import": {"types": "./t.d.ts", "default": "./d.js"}}}})).as_deref(), Some("d.js"));
        assert_eq!(esm_entry(&json!({"exports": {".": [{"import": "./a.js"}, "./b.js"]}})).as_deref(), Some("a.js"));
        assert_eq!(esm_entry(&json!({"exports": {"./sub": "./s.js"}, "module": "m.js"})).as_deref(), Some("m.js"));
        assert_eq!(esm_entry(&json!({})), None);
    }

    #[test]
    fn the_tag_cannot_close_its_script_early() {
        let nm = fixture();
        let tag = import_map_tag(&nm);
        assert!(tag.starts_with("<script type=\"importmap\">") && tag.ends_with("</script>"));
        assert_eq!(tag.matches("</").count(), 1);
    }
}
