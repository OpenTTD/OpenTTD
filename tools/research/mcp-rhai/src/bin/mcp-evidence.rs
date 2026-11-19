//! RESEARCH-ONLY: evidence with provenance from this repository, on the command line
//! (src/evidence.rs; research/agent-loop.md, Retrieve). The panel answers the same at GET /api/evidence.
//!
//! Usage: mcp-evidence [--k N] <question...>          the passages that answer it, as JSON
//!        mcp-evidence --benchmark <file.json>        recall@5 and MRR over the benchmark's questions
//!        mcp-evidence -h | --help
//! Exit: 0 answered (or the benchmark ran), 1 insufficient evidence, 2 usage, 3 not a git checkout.

#[path = "../evidence.rs"]
mod evidence;

use serde_json::{json, Value};
use std::path::{Path, PathBuf};

fn root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..")
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.is_empty() || args.iter().any(|a| a == "-h" || a == "--help") {
        println!("{}", include_str!("mcp-evidence.rs").lines().skip(4).take(4).map(|l| l.trim_start_matches("//! ").trim_start_matches("//!")).collect::<Vec<_>>().join("\n"));
        std::process::exit(if args.is_empty() { 2 } else { 0 });
    }
    let root = root();
    let index = match evidence::index_of(&root) {
        Ok(i) => i,
        Err(e) => { eprintln!("mcp-evidence: {e}"); std::process::exit(3); }
    };
    let now = |p: &str| std::fs::read_to_string(root.join(p)).ok();
    if args[0] == "--benchmark" {
        let Some(file) = args.get(1) else { eprintln!("mcp-evidence: --benchmark needs a file"); std::process::exit(2) };
        let bench: Value = match std::fs::read_to_string(file).ok().and_then(|t| serde_json::from_str(&t).ok()) {
            Some(b) => b,
            None => { eprintln!("mcp-evidence: {file} is not a benchmark JSON"); std::process::exit(2); }
        };
        let report = run_benchmark(&index, &bench, &now);
        println!("{}", serde_json::to_string_pretty(&report).unwrap_or_default());
        return;
    }
    let (k, question) = match args.iter().position(|a| a == "--k") {
        Some(i) => (args.get(i + 1).and_then(|v| v.parse().ok()).unwrap_or(5),
                    args.iter().enumerate().filter(|(j, _)| *j != i && *j != i + 1).map(|(_, a)| a.as_str()).collect::<Vec<_>>().join(" ")),
        None => (5, args.join(" ")),
    };
    if k == 0 { eprintln!("mcp-evidence: --k is a whole number from 1"); std::process::exit(2); }
    let a = index.answer(&question, k, &now);
    println!("{}", serde_json::to_string_pretty(&a).unwrap_or_default());
    std::process::exit(if a["status"] == "answered" { 0 } else { 1 });
}

/// Each question with its expected evidence: `expect` (paths, any of which counts) or
/// `"insufficient": true`. Reports recall@5, MRR and the insufficient-evidence accuracy.
fn run_benchmark(index: &evidence::Index, bench: &Value, now: &dyn Fn(&str) -> Option<String>) -> Value {
    let mut rows = Vec::new();
    let (mut hits, mut rr, mut answerable, mut insuff_ok, mut insuff_n) = (0, 0.0, 0, 0, 0);
    for q in bench["questions"].as_array().into_iter().flatten() {
        let question = q["q"].as_str().unwrap_or("");
        let a = index.answer(question, 5, now);
        let got: Vec<String> = a["results"].as_array().into_iter().flatten().filter_map(|r| r["path"].as_str().map(str::to_string)).collect();
        if q["insufficient"] == json!(true) {
            insuff_n += 1;
            let ok = a["status"] == "insufficient evidence";
            if ok { insuff_ok += 1; }
            rows.push(json!({"q": question, "want": "insufficient evidence", "status": a["status"], "pass": ok}));
            continue;
        }
        answerable += 1;
        let want: Vec<&str> = q["expect"].as_array().into_iter().flatten().filter_map(Value::as_str).collect();
        let rank = got.iter().position(|g| want.contains(&g.as_str()));
        if let Some(r) = rank { hits += 1; rr += 1.0 / (r as f64 + 1.0); }
        rows.push(json!({"q": question, "want": want, "rank": rank.map(|r| r + 1), "status": a["status"], "top": got.first()}));
    }
    json!({"benchmark": bench["name"], "version": bench["version"], "commit": index.commit, "files": index.files, "passages": index.passages.len(),
           "recall_at_5": if answerable > 0 { hits as f64 / answerable as f64 } else { 0.0 },
           "mrr": if answerable > 0 { (rr / answerable as f64 * 1000.0).round() / 1000.0 } else { 0.0 },
           "answerable": answerable, "found": hits, "insufficient_correct": insuff_ok, "insufficient_total": insuff_n, "questions": rows})
}
