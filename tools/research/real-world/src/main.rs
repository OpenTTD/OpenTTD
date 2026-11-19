//! rw-scenario: the command line of the real-world scenario pipeline (RESEARCH-ONLY).

use rw_scenario::*;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const HELP: &str = "rw-scenario: real place -> OpenTTD heightmap, towns, industries and economy (RESEARCH-ONLY)

Commands:
  resolve  --query TEXT (--live | --nominatim FILE) [--pick ID]
           Ranked candidates and the choice of rule dominant-settlement/v1. Exit 4 when ambiguous.
  acquire  --query TEXT --live --out SNAPSHOT.json [--pick ID] [--map-size 256] [--grid 129]
           [--margin 0.15] [--rules FILE] [--cache DIR]
           The only command that uses the network, and only with --live. Bounded, paced, cached.
           Place search goes to Nominatim under its usage policy (read it before any live use):
           https://operations.osmfoundation.org/policies/nominatim/ (1 request/s, no bulk, cached).
  compile  --snapshot FILE --out DIR [--rules FILE]
           Offline and deterministic: heightmap.png, towns.json, game/RealWorldScenario/,
           openttd.cfg, preview.svg, scenario.json, SHA256SUMS.
  replay   --snapshot FILE [--rules FILE] [--expect SHA256SUMS]
           Compile twice in fresh directories; exit 10 unless both (and --expect) agree byte for byte.
  validate (--snapshot FILE | --scenario FILE)
           The contract checks; exit 10 on any finding.
  load-check --out DIR --openttd BIN [--baseset DIR] [--seed N] [--timeout SECONDS]
           Start a loopback dedicated server on the compiled heightmap with its GameScript, read
           what it placed, save the game, quit. Exit 6 partial, 7 did not load.

Exit codes: 0 ok, 2 usage, 3 missing prerequisite, 4 ambiguous place, 5 acquisition failed,
6 partial, 7 runtime failure, 10 validation or determinism failure.";

fn crate_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
}

fn default_rules() -> PathBuf {
    crate_dir().join("rules/temperate-default.v1.json")
}

struct Args {
    flags: BTreeMap<String, String>,
}

impl Args {
    fn parse(v: &[String]) -> Result<Args, String> {
        let mut flags = BTreeMap::new();
        let mut i = 0;
        while i < v.len() {
            let k = v[i].strip_prefix("--").ok_or_else(|| format!("unexpected argument {}", v[i]))?;
            if k == "live" {
                flags.insert(k.into(), "1".into());
                i += 1;
                continue;
            }
            let val = v.get(i + 1).ok_or_else(|| format!("--{k} needs a value"))?;
            flags.insert(k.to_string(), val.clone());
            i += 2;
        }
        Ok(Args { flags })
    }
    fn get(&self, k: &str) -> Option<&str> {
        self.flags.get(k).map(String::as_str)
    }
    fn need(&self, k: &str) -> Result<&str, String> {
        self.get(k).ok_or_else(|| format!("--{k} is required"))
    }
    fn num<T: std::str::FromStr>(&self, k: &str, d: T) -> Result<T, String> {
        match self.get(k) {
            None => Ok(d),
            Some(s) => s.parse().map_err(|_| format!("--{k}: not a number: {s}")),
        }
    }
    fn path(&self, k: &str) -> Option<PathBuf> {
        self.get(k).map(PathBuf::from)
    }
}

enum Fail {
    Usage(String),
    Code(u8, String),
}

impl From<String> for Fail {
    fn from(s: String) -> Self {
        Fail::Code(7, s)
    }
}

fn compile_to(snapshot: &Path, rules_path: &Path, out: &Path) -> Result<model::Scenario, Fail> {
    let snap = snapshot::read(snapshot).map_err(|e| Fail::Code(10, e))?;
    let (rules, bytes) = rules::Rules::load(rules_path).map_err(|e| Fail::Code(10, e))?;
    let c = compile::compile(&snap, &rules, &bytes).map_err(|e| Fail::Code(10, e))?;
    std::fs::create_dir_all(out).map_err(|e| Fail::Code(7, format!("{}: {e}", out.display())))?;
    let sc = write_outputs(out, c, &snap, &rules)?;
    let errs = compile::validate(&sc);
    if !errs.is_empty() {
        return Err(Fail::Code(10, format!("the compiled scenario fails its contract:\n  {}", errs.join("\n  "))));
    }
    Ok(sc)
}

fn summary(sc: &model::Scenario) -> serde_json::Value {
    let mut by: BTreeMap<String, usize> = BTreeMap::new();
    for e in &sc.entities {
        *by.entry(format!("{} {:?}", e.game.kind, e.evidence).to_uppercase()).or_insert(0) += 1;
    }
    let mut edges: BTreeMap<String, usize> = BTreeMap::new();
    for e in &sc.economy.edges {
        *edges.entry(format!("{:?}", e.evidence).to_uppercase()).or_insert(0) += 1;
    }
    serde_json::json!({
        "query": sc.query, "chosen": sc.chosen, "map_size": sc.extent.map_size,
        "metres_per_tile": sc.extent.metres_per_tile, "levels": sc.heightmap.levels,
        "heightmap_sha256": sc.heightmap.sha256, "entities": by, "edges": edges,
        "chains": sc.economy.chains, "isolated": sc.economy.isolated,
    })
}

fn run(argv: Vec<String>) -> Result<(), Fail> {
    let Some(cmd) = argv.first().cloned() else { return Err(Fail::Usage("no command".into())) };
    if cmd == "help" || cmd == "--help" || cmd == "-h" {
        println!("{HELP}");
        return Ok(());
    }
    let a = Args::parse(&argv[1..]).map_err(Fail::Usage)?;
    let rules_path = a.path("rules").unwrap_or_else(default_rules);
    match cmd.as_str() {
        "resolve" => {
            let query = a.need("query").map_err(Fail::Usage)?;
            let res = if let Some(f) = a.path("nominatim") {
                let body = std::fs::read(&f).map_err(|e| Fail::Code(3, format!("{}: {e}", f.display())))?;
                let c = resolve::candidates_from_nominatim(&body).map_err(|e| Fail::Code(10, e))?;
                // The file's name only: a host path never goes into a record.
                let name = f.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default();
                resolve::choose(c, a.get("pick"), "nominatim", &format!("file:{name}"))
            } else if a.get("live").is_some() {
                let mut client = net::Client::new(a.path("cache").unwrap_or_else(|| crate_dir().join(".cache")));
                acquire::resolve_live(&mut client, query, a.get("pick"))?.0
            } else {
                return Err(Fail::Usage("resolve needs --live or --nominatim FILE (no network without --live)".into()));
            };
            println!("{}", serde_json::to_string_pretty(&res).unwrap());
            eprintln!("{}\n{}", res.reason, acquire::list(&res.candidates));
            if res.chosen.is_none() {
                return Err(Fail::Code(4, "ambiguous: nothing chosen".into()));
            }
            Ok(())
        }
        "acquire" => {
            if a.get("live").is_none() {
                return Err(Fail::Usage("acquire uses the network and needs --live; offline work starts from a snapshot (compile --snapshot)".into()));
            }
            let (rules, _) = rules::Rules::load(&rules_path).map_err(|e| Fail::Code(10, e))?;
            let map_size: u32 = a.num("map-size", 256).map_err(Fail::Usage)?;
            if !map_size.is_power_of_two() || !(64..=4096).contains(&map_size) {
                return Err(Fail::Usage("--map-size must be a power of two in 64..=4096".into()));
            }
            let grid: u32 = a.num("grid", 129).map_err(Fail::Usage)?;
            if !(2..=257).contains(&grid) {
                return Err(Fail::Usage("--grid must be 2..=257 elevation samples a side".into()));
            }
            let req = model::Request {
                query: a.need("query").map_err(Fail::Usage)?.to_string(),
                map_size,
                elevation_grid: grid,
                margin: a.num("margin", 0.15).map_err(Fail::Usage)?,
                pick: a.get("pick").map(String::from),
            };
            let out = a.path("out").ok_or_else(|| Fail::Usage("--out is required".into()))?;
            let mut client = net::Client::new(a.path("cache").unwrap_or_else(|| crate_dir().join(".cache")));
            let snap = acquire::acquire_live(&mut client, &req, &rules).map_err(|e| {
                if e.contains("AMBIGUOUS") || e.contains("no candidate") || e.contains("--pick") {
                    Fail::Code(4, e)
                } else {
                    Fail::Code(5, e)
                }
            })?;
            snapshot::write(&out, &snap)?;
            let errs = snapshot::validate(&snap);
            eprintln!("rw-scenario: wrote {} ({} features; requests sent {:?})", out.display(), snap.features.len(), client.sent);
            for s in &snap.sources {
                eprintln!("  source {:<22} {:<9} {}", s.id, s.completeness, s.diagnostics.join("; "));
            }
            if !errs.is_empty() {
                return Err(Fail::Code(6, format!("snapshot written but not compilable:\n  {}", errs.join("\n  "))));
            }
            Ok(())
        }
        "compile" => {
            let snap = a.path("snapshot").ok_or_else(|| Fail::Usage("--snapshot is required".into()))?;
            let out = a.path("out").ok_or_else(|| Fail::Usage("--out is required".into()))?;
            let sc = compile_to(&snap, &rules_path, &out)?;
            println!("{}", serde_json::to_string_pretty(&summary(&sc)).unwrap());
            Ok(())
        }
        "replay" => {
            let snap = a.path("snapshot").ok_or_else(|| Fail::Usage("--snapshot is required".into()))?;
            let base = std::env::temp_dir().join(format!("rw-replay-{}", std::process::id()));
            let (d1, d2) = (base.join("a"), base.join("b"));
            compile_to(&snap, &rules_path, &d1)?;
            compile_to(&snap, &rules_path, &d2)?;
            let s1 = std::fs::read_to_string(d1.join("SHA256SUMS")).map_err(|e| e.to_string())?;
            let s2 = std::fs::read_to_string(d2.join("SHA256SUMS")).map_err(|e| e.to_string())?;
            let _ = std::fs::remove_dir_all(&base);
            print!("{s1}");
            if s1 != s2 {
                return Err(Fail::Code(10, "two compilations of the same snapshot differ".into()));
            }
            if let Some(e) = a.path("expect") {
                let want = std::fs::read_to_string(&e).map_err(|x| Fail::Code(3, format!("{}: {x}", e.display())))?;
                if want != s1 {
                    return Err(Fail::Code(10, format!("the compilation differs from {}", e.display())));
                }
                eprintln!("rw-scenario: identical to {}", e.display());
            }
            eprintln!("rw-scenario: replay deterministic (two compilations, identical SHA256SUMS)");
            Ok(())
        }
        "validate" => {
            let errs = if let Some(p) = a.path("snapshot") {
                snapshot::validate(&snapshot::read(&p).map_err(|e| Fail::Code(10, e))?)
            } else if let Some(p) = a.path("scenario") {
                let b = std::fs::read(&p).map_err(|e| Fail::Code(3, format!("{}: {e}", p.display())))?;
                let sc: model::Scenario = serde_json::from_slice(&b).map_err(|e| Fail::Code(10, format!("{}: {e}", p.display())))?;
                compile::validate(&sc)
            } else {
                return Err(Fail::Usage("validate needs --snapshot or --scenario".into()));
            };
            if errs.is_empty() {
                eprintln!("rw-scenario: valid");
                Ok(())
            } else {
                Err(Fail::Code(10, errs.join("\n")))
            }
        }
        "load-check" => {
            let out = a.path("out").ok_or_else(|| Fail::Usage("--out is required".into()))?;
            let bin = a.path("openttd").ok_or_else(|| Fail::Usage("--openttd is required".into()))?;
            if !bin.is_file() {
                return Err(Fail::Code(3, format!("{}: no such binary", bin.display())));
            }
            let baseset = a.path("baseset").or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join("Documents/OpenTTD/baseset")));
            let o = loadcheck::Options {
                out,
                openttd: bin,
                baseset,
                seed: a.num("seed", 31337).map_err(Fail::Usage)?,
                timeout: std::time::Duration::from_secs(a.num("timeout", 300).map_err(Fail::Usage)?),
            };
            let (report, code) = loadcheck::run(&o)?;
            println!("{}", serde_json::to_string_pretty(&serde_json::json!({
                "exit_code": report["exit_code"], "gamescript_finished": report["gamescript_finished"],
                "done": report["done"], "placed": report["placed"], "failures": report["failures"],
                "roles_unresolved": report["roles_unresolved"], "save": report["save"], "log": report["log"],
                "reload": report["reload"], "engine_generated": report["engine_generated"], "labelled": report["labelled"],
                "displaced_beyond_threshold": report["displaced_beyond_threshold"],
            })).unwrap());
            match code {
                0 => Ok(()),
                c => Err(Fail::Code(c as u8, format!("load-check: exit {c} (6 partial placement or the reload changed the counts, 7 did not load or finish)"))),
            }
        }
        _ => Err(Fail::Usage(format!("unknown command {cmd}"))),
    }
}

fn main() -> ExitCode {
    match run(std::env::args().skip(1).collect()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(Fail::Usage(m)) => {
            eprintln!("rw-scenario: {m}\n\n{HELP}");
            ExitCode::from(2)
        }
        Err(Fail::Code(c, m)) => {
            eprintln!("rw-scenario: {m}");
            ExitCode::from(c)
        }
    }
}
