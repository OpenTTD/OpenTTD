//! RESEARCH-ONLY: evidence retrieval with provenance (research/agent-loop.md, Retrieve).
//!
//! An agent asks a question and gets passages of this repository it can inspect: the path, the
//! lines, the commit the index was built at, the file's content hash, the source class, and whether
//! the passage is still what the file says now. Nothing is summarised: a result is the text itself.
//! When no passage covers enough of the question, the answer says `insufficient evidence` instead.
//!
//! - Sources: the tracked files (`git ls-files`), each in one class derived from its path:
//!   implementation, test, policy (rules), design (ADRs), knowledge (curated records), plan
//!   (milestones, issues, intents), configuration (yaml, json, toml, txt, ini), documentation,
//!   candidate (unreviewed knowledge) and record (results, historical).
//!   Excluded: `.ai/local/`, vendored and generated trees, binaries, files over 512 KiB and any
//!   path that looks like a secret (`.env`, keys, tokens, credentials).
//! - Ranking: passages covering at least half the question first, among them by authority tier
//!   (implementation, tests and policy; then design, knowledge and documentation; then candidates and
//!   records), BM25 over identifiers and words within a tier, so an old plan never outranks the code
//!   or the rule in force.
//! - Provenance: every answer and every result names the repository (its `origin` remote without
//!   any user or password, else its directory's name), the commit, the path, the lines and the
//!   file's content digest.
//! - Freshness: at query time each passage is read again from the work tree: `current` when its
//!   file is unchanged or the passage is still there (its new lines given), `stale` when the passage
//!   is gone, `deleted` when the file is (a renamed file too, until a commit moves the index).
//!   The index is rebuilt when HEAD moves, so a commit never leaves an old one answering.

use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::path::Path;

use serde_json::{json, Value};

/// The largest file indexed, in bytes.
pub const MAX_FILE: usize = 512 * 1024;
/// Lines per passage, and the step between passages (they overlap).
pub const WINDOW: usize = 30;
pub const STEP: usize = 20;
/// A question needs passages covering at least this share of its terms to be answered.
pub const MIN_COVERAGE: f64 = 0.5;

/// The class of a source, from its path; None for a path that is never indexed.
pub fn class_of(path: &str) -> Option<&'static str> {
    let lower = path.to_ascii_lowercase();
    let name = lower.rsplit('/').next().unwrap_or(&lower);
    let secret = [".env", "credential", "secret", ".pem", ".key", "token", "password", "id_rsa"];
    if lower.starts_with(".ai/local/") || lower.starts_with("src/3rdparty/") || lower.contains("/node_modules/")
        || lower.starts_with("build") || lower.contains("/target/") || secret.iter().any(|s| name.contains(s)) {
        return None;
    }
    let text_ext = ["rs", "cpp", "h", "hpp", "c", "md", "rhai", "js", "mjs", "nut", "py", "sh", "toml", "yaml", "yml", "json", "txt", "ini", "cmake", "html", "css"];
    let ext = name.rsplit_once('.').map(|(_, e)| e).unwrap_or("");
    let extensionless_tool = path.starts_with("tools/") && !name.contains('.');
    if !text_ext.contains(&ext) && !extensionless_tool { return None; }
    Some(if lower.starts_with(".ai/repo/rules/") { "policy" }
        else if lower.starts_with(".ai/repo/adrs/") { "design" }
        else if lower.starts_with(".ai/repo/knowledge/curated/") { "knowledge" }
        else if lower.starts_with(".ai/repo/knowledge/candidates/") { "candidate" }
        else if lower.starts_with("research/results/") { "record" }
        else if lower.starts_with(".ai/repo/project/") { "plan" }
        else if lower.starts_with("src/tests/") || lower.contains("/test/") || name.starts_with("test_") || name.contains(".test.") { "test" }
        else if ext == "md" { "documentation" }
        else if ["json", "toml", "yaml", "yml", "txt", "ini"].contains(&ext) { "configuration" }
        else { "implementation" })
}

/// How a class ranks against the others at the same score: current truth first.
pub fn class_weight(class: &str) -> f64 {
    match class {
        "implementation" | "policy" => 1.0,
        "test" => 0.95,
        "design" | "knowledge" | "plan" | "configuration" => 0.85,
        "documentation" => 0.75,
        "candidate" => 0.5,
        _ => 0.4, // record
    }
}

/// The authority tier of a class: 0 current truth, 1 design and documentation, 2 history.
pub fn tier(class: &str) -> u8 {
    match class {
        "implementation" | "policy" | "test" => 0,
        "design" | "knowledge" | "documentation" | "plan" | "configuration" => 1,
        _ => 2,
    }
}

/// The terms of a text: identifiers and words, lowercased, with an identifier's parts too
/// (`panel_decision` gives `panel_decision`, `panel`, `decision`; `observedDigest` gives its two).
pub fn terms(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    for raw in text.split(|c: char| !(c.is_alphanumeric() || c == '_')) {
        if raw.len() < 2 { continue; }
        let whole = raw.to_lowercase();
        let mut parts: Vec<String> = Vec::new();
        let mut cur = String::new();
        let chars: Vec<char> = raw.chars().collect();
        for (i, c) in chars.iter().enumerate() {
            let boundary = *c == '_' || (c.is_uppercase() && i > 0 && chars[i - 1].is_lowercase());
            if boundary && !cur.is_empty() { parts.push(cur.to_lowercase()); cur.clear(); }
            if *c != '_' { cur.push(*c); }
        }
        if !cur.is_empty() { parts.push(cur.to_lowercase()); }
        if STOP.contains(&whole.as_str()) { continue; }
        if parts.len() > 1 { for p in parts { if p.len() >= 2 && !STOP.contains(&p.as_str()) { out.push(p); } } }
        out.push(whole);
    }
    out
}

const STOP: &[&str] = &["the", "and", "for", "is", "of", "to", "in", "a", "an", "it", "on", "be", "by", "or", "as", "at", "this", "that",
                         "with", "from", "are", "was", "its", "not", "no", "if", "do", "does", "how", "what", "which", "where", "when", "why",
                         "who", "can", "into", "than", "then", "there", "their", "one", "all", "any"];

/// FNV-1a 64 of a text, as hex: the change detector of a file or passage.
pub fn digest(text: &str) -> String {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in text.bytes() { h ^= b as u64; h = h.wrapping_mul(0x100000001b3); }
    format!("{h:016x}")
}

#[derive(Clone, Debug)]
pub struct Passage {
    pub path: String,
    pub class: &'static str,
    pub start: usize,
    pub end: usize,
    pub text: String,
    pub file_digest: String,
    tf: HashMap<String, u32>,
    len: usize,
}

/// An index of passages at one commit.
pub struct Index {
    pub repository: String,
    pub commit: String,
    pub indexed_at: u64,
    pub files: usize,
    pub passages: Vec<Passage>,
    df: HashMap<String, usize>,
    avg_len: f64,
}

/// The passages of one file: windows of WINDOW lines every STEP lines.
pub fn passages_of(path: &str, class: &'static str, text: &str) -> Vec<Passage> {
    let lines: Vec<&str> = text.lines().collect();
    let file_digest = digest(text);
    let mut out = Vec::new();
    let mut start = 0;
    loop {
        let end = (start + WINDOW).min(lines.len());
        let body = lines[start..end].join("\n");
        // The path's own terms belong to every passage of it: a file named for a thing is about it.
        let mut ts = terms(&body);
        ts.extend(terms(&path.replace(['/', '.', '-'], " ")));
        let mut tf = HashMap::new();
        for t in &ts { *tf.entry(t.clone()).or_insert(0) += 1; }
        if !body.trim().is_empty() {
            out.push(Passage { path: path.to_string(), class, start: start + 1, end, text: body, file_digest: file_digest.clone(), tf, len: ts.len() });
        }
        if end >= lines.len() { break; }
        start += STEP;
    }
    out
}

impl Index {
    /// An index of `files` ((path, text)) at `commit`; files of no class are left out, and a
    /// passage whose text another file already holds is kept once (the first path in order).
    pub fn build(commit: &str, indexed_at: u64, files: &[(String, String)]) -> Index {
        let mut passages = Vec::new();
        let mut seen = BTreeSet::new();
        let mut n = 0;
        for (path, text) in files {
            let Some(class) = class_of(path) else { continue };
            if text.len() > MAX_FILE || text.contains('\0') { continue; }
            n += 1;
            for p in passages_of(path, class, text) {
                if seen.insert(digest(&p.text)) { passages.push(p); }
            }
        }
        let mut df: HashMap<String, usize> = HashMap::new();
        for p in &passages { for t in p.tf.keys() { *df.entry(t.clone()).or_insert(0) += 1; } }
        let avg_len = if passages.is_empty() { 1.0 } else { passages.iter().map(|p| p.len).sum::<usize>() as f64 / passages.len() as f64 };
        Index { repository: String::new(), commit: commit.to_string(), indexed_at, files: n, passages, df, avg_len }
    }

    /// The passages that answer `question`, best first: (score, coverage of the question's terms,
    /// passage index).
    pub fn rank(&self, question: &str, k: usize) -> Vec<(f64, f64, usize)> {
        let q: Vec<String> = terms(question).into_iter().collect::<BTreeSet<_>>().into_iter().collect();
        if q.is_empty() { return Vec::new(); }
        let n = self.passages.len() as f64;
        let (k1, b) = (1.2, 0.75);
        let mut scored: Vec<(f64, f64, usize)> = self.passages.iter().enumerate().filter_map(|(i, p)| {
            let mut s = 0.0;
            let mut hit = 0;
            for t in &q {
                let Some(&f) = p.tf.get(t) else { continue };
                hit += 1;
                let df = *self.df.get(t).unwrap_or(&0) as f64;
                let idf = ((n - df + 0.5) / (df + 0.5) + 1.0).ln();
                let f = f as f64;
                s += idf * f * (k1 + 1.0) / (f + k1 * (1.0 - b + b * p.len as f64 / self.avg_len));
            }
            (hit > 0).then(|| (s * class_weight(p.class), hit as f64 / q.len() as f64, i))
        }).collect();
        // Authority before wording: passages that cover enough of the question come first, among
        // them current truth (code, rules, tests) before design and documentation, before
        // candidates and records; BM25 orders within a tier. An old plan is found, never first.
        let key = |r: &(f64, f64, usize)| ((r.1 < MIN_COVERAGE) as u8, tier(self.passages[r.2].class));
        scored.sort_by(|a, b| key(a).cmp(&key(b)).then(b.0.partial_cmp(&a.0).unwrap_or(std::cmp::Ordering::Equal)).then(a.2.cmp(&b.2)));
        scored.truncate(k);
        scored
    }

    /// The answer to `question`: the passages with their provenance and freshness against `now`
    /// (each path's current text, None when it is gone), or `insufficient evidence`.
    pub fn answer(&self, question: &str, k: usize, now: &dyn Fn(&str) -> Option<String>) -> Value {
        let ranked = self.rank(question, k);
        let best = ranked.iter().map(|r| r.1).fold(0.0, f64::max);
        let results: Vec<Value> = ranked.iter().map(|(score, coverage, i)| {
            let p = &self.passages[*i];
            let mut v = json!({"path": p.path, "class": p.class, "lines": [p.start, p.end], "score": (score * 1000.0).round() / 1000.0,
                               "coverage": (coverage * 100.0).round() / 100.0, "repository": self.repository, "commit": self.commit, "file_digest": p.file_digest,
                               "text": p.text});
            let (fresh, lines) = freshness(p, now(&p.path).as_deref());
            v["freshness"] = json!(fresh);
            if let Some((a, b)) = lines { v["now_lines"] = json!([a, b]); }
            v
        }).collect();
        let status = if results.is_empty() || best < MIN_COVERAGE { "insufficient evidence" } else { "answered" };
        json!({"question": question, "status": status, "repository": self.repository, "commit": self.commit, "indexed_at": self.indexed_at,
               "files": self.files, "passages": self.passages.len(), "min_coverage": MIN_COVERAGE, "results": results})
    }
}

/// Whether a passage still holds against a file's current text: `current` (with its lines now,
/// when it moved), `stale` (the passage is no longer in the file) or `deleted` (no file).
pub fn freshness(p: &Passage, now: Option<&str>) -> (&'static str, Option<(usize, usize)>) {
    let Some(text) = now else { return ("deleted", None) };
    if digest(text) == p.file_digest { return ("current", None); }
    match text.find(&p.text) {
        Some(at) => {
            let start = text[..at].matches('\n').count() + 1;
            ("current", Some((start, start + p.text.matches('\n').count())))
        }
        None => ("stale", None),
    }
}

/// The repository's identity: its `origin` remote with any `user[:password]@` removed and the
/// scheme and `.git` dropped (`github.com/korczis/OpenTTD`), else the name of its directory.
pub fn repository_of(root: &Path) -> String {
    let remote = std::process::Command::new("git").arg("-C").arg(root).args(["config", "--get", "remote.origin.url"]).output()
        .ok().filter(|o| o.status.success()).map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string()).unwrap_or_default();
    if !remote.is_empty() { return identity_of_remote(&remote); }
    let top = std::process::Command::new("git").arg("-C").arg(root).args(["rev-parse", "--show-toplevel"]).output()
        .ok().map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string()).unwrap_or_default();
    Path::new(&top).file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_else(|| "unknown".into())
}

/// A remote URL as an identity without credentials: `https://u:p@host/a/b.git`, `ssh://git@host/a/b`
/// and `git@host:a/b.git` all become `host/a/b`.
pub fn identity_of_remote(url: &str) -> String {
    let rest = url.split_once("://").map(|(_, r)| r).unwrap_or(url);
    let rest = rest.rsplit_once('@').map(|(_, r)| r).unwrap_or(rest);
    let rest = if url.contains("://") { rest.to_string() } else { rest.replacen(':', "/", 1) };
    rest.trim_end_matches('/').trim_end_matches(".git").to_string()
}

/// The tracked files of the repository at `root` with their text: (path, text).
pub fn tracked(root: &Path) -> Result<(String, Vec<(String, String)>), String> {
    let git = |args: &[&str]| std::process::Command::new("git").arg("-C").arg(root).args(args).output()
        .map_err(|e| format!("git: {e}")).and_then(|o| if o.status.success() { Ok(String::from_utf8_lossy(&o.stdout).to_string()) } else { Err(String::from_utf8_lossy(&o.stderr).to_string()) });
    let commit = git(&["rev-parse", "HEAD"])?.trim().to_string();
    let mut files = Vec::new();
    for path in git(&["ls-files"])?.lines() {
        if class_of(path).is_none() { continue; }
        let Ok(meta) = std::fs::metadata(root.join(path)) else { continue };
        if meta.len() as usize > MAX_FILE { continue; }
        if let Ok(text) = std::fs::read_to_string(root.join(path)) { files.push((path.to_string(), text)); }
    }
    Ok((commit, files))
}

/// The index of `root` at its HEAD, kept while HEAD stays the same (rebuilt after a commit).
pub fn index_of(root: &Path) -> Result<std::sync::Arc<Index>, String> {
    use std::sync::{Arc, Mutex, OnceLock};
    static CACHE: OnceLock<Mutex<BTreeMap<String, Arc<Index>>>> = OnceLock::new();
    let head = std::process::Command::new("git").arg("-C").arg(root).args(["rev-parse", "HEAD"]).output()
        .map_err(|e| format!("git: {e}"))?;
    let head = String::from_utf8_lossy(&head.stdout).trim().to_string();
    // One index per repository, replaced when its HEAD moves.
    let key = root.display().to_string();
    let cache = CACHE.get_or_init(|| Mutex::new(BTreeMap::new()));
    if let Some(i) = cache.lock().map_err(|_| "poisoned")?.get(&key).filter(|i| i.commit == head) { return Ok(i.clone()); }
    let (commit, files) = tracked(root)?;
    let at = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    let mut index = Index::build(&commit, at, &files);
    index.repository = repository_of(root);
    let index = Arc::new(index);
    cache.lock().map_err(|_| "poisoned")?.insert(key, index.clone());
    Ok(index)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn files() -> Vec<(String, String)> {
        vec![
            ("src/command.cpp".into(), "/** Post a command through the command system. */\nbool CommandHelperBase::InternalPost(Commands cmd) {\n  return DoCommandP(cmd);\n}\n".into()),
            (".ai/repo/rules/project/game-state-mutates-only-through-commands.v1.md".into(), "Change persistent game state only through a registered command issued with Command<>::Post, never by writing a pool.\n".into()),
            (".ai/repo/adrs/0016-lockstep.md".into(), "# ADR 0016\nThe game is deterministic lockstep: every client computes the same state from one command stream.\n".into()),
            ("research/old-plan.md".into(), "A plan from long ago: game state may be written directly into the pool from a hook.\n".into()),
            ("research/results/duel-1.md".into(), "game state pool written directly hook record\n".into()),
            ("src/tests/test_command.cpp".into(), "TEST_CASE(\"a command posts\") { CHECK(InternalPost(cmd)); }\n".into()),
            ("README.md".into(), "Post a command through the command system.\n".into()),
            (".ai/local/state/secret-notes.md".into(), "command system internal post\n".into()),
            (".env".into(), "TOKEN=abc command\n".into()),
            ("src/3rdparty/fmt/core.h".into(), "command command command\n".into()),
            ("media/logo.png".into(), "command\n".into()),
        ]
    }

    fn index() -> Index { Index::build("abc123", 1, &files()) }

    #[test]
    fn only_allowed_sources_are_indexed_in_their_classes() {
        assert_eq!(class_of("src/command.cpp"), Some("implementation"));
        assert_eq!(class_of(".ai/repo/rules/project/x.v1.md"), Some("policy"));
        assert_eq!(class_of(".ai/repo/adrs/0016-x.md"), Some("design"));
        assert_eq!(class_of(".ai/repo/knowledge/curated/x.md"), Some("knowledge"));
        assert_eq!(class_of(".ai/repo/knowledge/candidates/x.md"), Some("candidate"));
        assert_eq!(class_of("research/results/duel-1.json"), Some("record"));
        assert_eq!(class_of("src/tests/test_x.cpp"), Some("test"));
        assert_eq!(class_of("tools/research/mcp-panel/test/a.test.mjs"), Some("test"));
        assert_eq!(class_of("research/agent-loop.md"), Some("documentation"));
        assert_eq!(class_of("tools/research/mcp-decision-check"), Some("implementation"));
        assert_eq!(class_of(".ai/repo/project/issues/I0090.yaml"), Some("plan"));
        assert_eq!(class_of("tools/research/real-world/fixtures/brno/LICENSE-DATA.txt"), Some("configuration"));
        assert_eq!(class_of("surfaces.toml"), Some("configuration"));
        for never in [".ai/local/state/x.md", ".env", "config/credentials.json", "src/3rdparty/fmt/core.h", "media/logo.png",
                      "tools/x/node_modules/a.js", "build/x.cpp", "keys/id_rsa", "a/api_token.txt"] {
            assert_eq!(class_of(never), None, "{never}");
        }
        let ix = index();
        assert_eq!(ix.files, 7, "local state, secrets, vendored and binary files are left out");
        assert!(ix.passages.iter().all(|p| !p.path.starts_with(".ai/local") && p.path != ".env"));
    }

    #[test]
    fn an_answer_cites_path_lines_commit_and_digest() {
        let ix = index();
        let a = ix.answer("how is game state changed through a command", 3, &|p| files().into_iter().find(|f| f.0 == p).map(|f| f.1));
        assert_eq!(a["status"], "answered", "{a}");
        let first = &a["results"][0];
        assert_eq!(first["path"], ".ai/repo/rules/project/game-state-mutates-only-through-commands.v1.md");
        assert_eq!((first["class"].clone(), first["lines"].clone(), first["commit"].clone()), (json!("policy"), json!([1, 1]), json!("abc123")));
        assert_eq!(first["freshness"], "current");
        assert_eq!(first["file_digest"].as_str().unwrap().len(), 16);
        assert!(first["text"].as_str().unwrap().contains("Command<>::Post"));
    }

    #[test]
    fn current_code_and_policy_outrank_old_plans_and_records() {
        let ix = index();
        let ranked: Vec<&str> = ix.rank("game state written directly pool hook", 5).iter().map(|r| ix.passages[r.2].path.as_str()).collect();
        let pos = |p: &str| ranked.iter().position(|x| *x == p).unwrap_or(99);
        assert!(pos(".ai/repo/rules/project/game-state-mutates-only-through-commands.v1.md") < pos("research/results/duel-1.md"), "{ranked:?}");
        assert!(ranked.contains(&"research/old-plan.md"), "the old plan is found, only not first");
    }

    #[test]
    fn a_known_symbol_test_and_adr_are_found() {
        let ix = index();
        let top = |q: &str| ix.passages[ix.rank(q, 1)[0].2].path.clone();
        assert_eq!(top("InternalPost CommandHelperBase"), "src/command.cpp");
        assert_eq!(top("TEST_CASE command posts CHECK"), "src/tests/test_command.cpp");
        assert_eq!(top("deterministic lockstep client stream"), ".ai/repo/adrs/0016-lockstep.md");
    }

    #[test]
    fn duplicate_passages_are_kept_once() {
        let mut f = files();
        f.push(("docs/copy.md".into(), "Post a command through the command system.\n".into()));
        let ix = Index::build("c", 1, &f);
        assert_eq!(ix.passages.iter().filter(|p| p.text == "Post a command through the command system.").count(), 1);
    }

    #[test]
    fn a_question_without_evidence_says_so() {
        let ix = index();
        let none = |_: &str| None;
        assert_eq!(ix.answer("quantum chromodynamics of penguins", 5, &none)["status"], "insufficient evidence");
        assert_eq!(ix.answer("command penguins chromodynamics quantum", 5, &none)["status"], "insufficient evidence", "one term of four is not enough");
        assert_eq!(ix.answer("", 5, &none)["status"], "insufficient evidence");
        assert_eq!(ix.answer("the of and", 5, &none)["results"], json!([]));
    }

    #[test]
    fn freshness_follows_the_file_now() {
        let ix = index();
        let rule = ix.passages.iter().find(|p| p.class == "policy").unwrap().clone();
        let original = files().into_iter().find(|f| f.0 == rule.path).unwrap().1;
        assert_eq!(freshness(&rule, Some(&original)), ("current", None));
        let moved = format!("# A new heading\n\n{original}");
        assert_eq!(freshness(&rule, Some(&moved)), ("current", Some((3, 3))), "moved: its new lines");
        assert_eq!(freshness(&rule, Some("Something else entirely.\n")).0, "stale");
        assert_eq!(freshness(&rule, None).0, "deleted");
        let a = ix.answer("registered command pool", 1, &|_| None);
        assert_eq!(a["results"][0]["freshness"], "deleted");
    }

    #[test]
    fn terms_split_identifiers_and_drop_stop_words() {
        let t = terms("panel_decision observedDigest the of x");
        for want in ["panel_decision", "panel", "decision", "observeddigest", "observed", "digest"] { assert!(t.contains(&want.to_string()), "{want}: {t:?}"); }
        assert!(!t.contains(&"the".to_string()) && !t.contains(&"x".to_string()));
        assert_eq!(digest("a"), digest("a"));
        assert_ne!(digest("a"), digest("b"));
    }

    #[test]
    fn long_files_are_windowed_with_overlap_and_line_numbers() {
        let text: String = (1..=50).map(|n| format!("line {n}\n")).collect();
        let ps = passages_of("src/x.cpp", "implementation", &text);
        assert_eq!(ps.iter().map(|p| (p.start, p.end)).collect::<Vec<_>>(), vec![(1, 30), (21, 50)]);
        let big = "x".repeat(MAX_FILE + 1);
        assert_eq!(Index::build("c", 1, &[("src/big.cpp".into(), big)]).files, 0);
    }

    #[test]
    fn a_remote_is_an_identity_without_credentials() {
        assert_eq!(identity_of_remote("https://user:hunter2@github.com/korczis/OpenTTD.git"), "github.com/korczis/OpenTTD");
        assert_eq!(identity_of_remote("git@github.com:korczis/OpenTTD.git"), "github.com/korczis/OpenTTD");
        assert_eq!(identity_of_remote("ssh://git@gitlab.com/korczis/bridge"), "gitlab.com/korczis/bridge");
        assert_eq!(identity_of_remote("/srv/git/local.git/"), "/srv/git/local");
    }

    /// A real git repository in a temporary directory: an edit makes a passage stale, a rename or
    /// a deletion makes it deleted, and a commit rebuilds the index so the answer moves with it.
    #[test]
    fn edits_renames_deletions_and_commits_are_followed() {
        let dir = std::env::temp_dir().join(format!("evidence-fixture-{}-{}", std::process::id(), digest(&format!("{:?}", std::time::Instant::now()))));
        std::fs::create_dir_all(dir.join("src")).unwrap();
        let git = |args: &[&str]| { let o = std::process::Command::new("git").arg("-C").arg(&dir).args(args).output().unwrap(); assert!(o.status.success(), "git {args:?}: {}", String::from_utf8_lossy(&o.stderr)); };
        git(&["init", "-q"]);
        git(&["config", "user.email", "fixture@example.invalid"]);
        git(&["config", "user.name", "fixture"]);
        git(&["remote", "add", "origin", "https://someone:s3cret@example.invalid/lab/fixture.git"]);
        std::fs::write(dir.join("src/signal.cpp"), "/** Place a signal on a rail tile. */
CommandCost CmdBuildSingleSignal() {}
").unwrap();
        std::fs::write(dir.join("src/api.token"), "CmdBuildSingleSignal signal rail tile
").unwrap();
        git(&["add", "-A"]);
        git(&["commit", "-q", "-m", "one"]);
        let now = |p: &str| std::fs::read_to_string(dir.join(p)).ok();
        let q = "CmdBuildSingleSignal signal rail tile";

        let first = index_of(&dir).unwrap();
        let a = first.answer(q, 5, &now);
        assert_eq!(a["status"], "answered", "{a}");
        assert_eq!(a["repository"], "example.invalid/lab/fixture", "no credentials in the identity");
        assert!(!a.to_string().contains("s3cret"));
        assert_eq!(a["results"].as_array().unwrap().len(), 1, "the token file is never indexed: {a}");
        let r = &a["results"][0];
        assert_eq!((r["path"].clone(), r["freshness"].clone(), r["commit"].clone()), (json!("src/signal.cpp"), json!("current"), json!(first.commit)));

        std::fs::write(dir.join("src/signal.cpp"), "// rewritten
int nothing();
").unwrap();
        assert_eq!(first.answer(q, 5, &now)["results"][0]["freshness"], "stale", "an uncommitted edit");
        git(&["checkout", "-q", "--", "src/signal.cpp"]);
        git(&["mv", "src/signal.cpp", "src/signals.cpp"]);
        assert!(std::sync::Arc::ptr_eq(&first, &index_of(&dir).unwrap()), "same HEAD, same index");
        assert_eq!(first.answer(q, 5, &now)["results"][0]["freshness"], "deleted", "a rename before its commit");

        git(&["commit", "-q", "-m", "rename"]);
        let second = index_of(&dir).unwrap();
        assert_ne!(second.commit, first.commit, "a commit rebuilds the index");
        let b = second.answer(q, 5, &now);
        assert_eq!((b["results"][0]["path"].clone(), b["results"][0]["freshness"].clone()), (json!("src/signals.cpp"), json!("current")));

        git(&["rm", "-q", "src/signals.cpp"]);
        assert_eq!(second.answer(q, 5, &now)["results"][0]["freshness"], "deleted");
        git(&["commit", "-q", "-m", "delete"]);
        assert_eq!(index_of(&dir).unwrap().answer(q, 5, &now)["status"], "insufficient evidence", "nothing is left to cite");
        std::fs::remove_dir_all(&dir).ok();
    }

    /// The index of this repository itself, at its HEAD, answers a question about its own code.
    #[test]
    fn this_repository_is_indexed_at_its_head() {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..");
        let ix = index_of(&root).unwrap();
        assert!(ix.files > 500 && ix.commit.len() == 40, "{} files at {}", ix.files, ix.commit);
        assert!(!ix.repository.is_empty() && !ix.repository.contains('@'), "{}", ix.repository);
        let a = ix.answer("observation digest freshness directive stale", 5, &|p| std::fs::read_to_string(root.join(p)).ok());
        assert_eq!(a["status"], "answered");
        assert!(a["results"].as_array().unwrap().iter().any(|r| r["path"] == "tools/research/mcp-rhai/src/panel_decision.rs"), "{a}");
    }
}
