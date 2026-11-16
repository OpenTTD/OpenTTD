//! RESEARCH-ONLY: how much of OpenTTD's public C++ API the MCP surfaces expose (rule
//! project.public-api-is-exposed).
//!
//! Derives the public API from the sources, never from a list: the `Commands` enum in
//! src/command_type.h (every game-state change), the console commands registered in
//! src/console_cmds.cpp, and the static functions of the Script API classes in
//! src/script/api/script_*.hpp. Compares each with what a running panel exposes: MCP tools/list
//! (`openttd.cmd.<Name>` for a command, `<Name>` its Cmd* function without the prefix, mapped from
//! the enum entry by the `DEF_CMD_TRAIT` declarations in src/*_cmd.h) and the panel's OpenAPI document (one route per tool, so
//! Swagger shows it). Console commands count as exposed when the panel's Rhai derivation sees them
//! (list_cmds through /api/console).
//!
//! Usage: mcp-api-coverage [--repo <path>] [--panel http://127.0.0.1:8800] [--json] [--report]
//! Exit status: 0 every dimension is fully exposed, 10 a dimension is not (unless --report),
//! 2 usage, 3 the sources or the panel cannot be read.
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};
use std::time::Duration;

use serde_json::{json, Value};

fn commands(repo: &Path) -> std::io::Result<BTreeSet<String>> {
    let src = std::fs::read_to_string(repo.join("src/command_type.h"))?;
    let start = src.find("enum class Commands").ok_or_else(|| std::io::Error::other("no enum class Commands"))?;
    let body = &src[start..];
    let open = body.find('{').unwrap_or(0) + 1;
    let close = body[open..].find("\n};").map(|i| open + i).unwrap_or(body.len());
    Ok(body[open..close]
        .lines()
        .filter_map(|l| {
            let t = l.trim();
            let name: String = t.chars().take_while(|c| c.is_ascii_alphanumeric() || *c == '_').collect();
            (!name.is_empty() && name.chars().next().is_some_and(|c| c.is_ascii_uppercase())
                && t[name.len()..].trim_start().starts_with([',', '=', '/']) && name != "End")
                .then_some(name)
        })
        .collect())
}

/// The `(enum entry, Cmd* function)` pairs a header declares with `DEF_CMD_TRAIT(Commands::X, CmdY, ...)`.
fn trait_pairs(text: &str) -> Vec<(String, String)> {
    text.split("DEF_CMD_TRAIT(Commands::").skip(1).filter_map(|rest| {
        let mut parts = rest.splitn(3, ',');
        let entry = parts.next()?.trim();
        let function = parts.next()?.trim();
        (!entry.is_empty() && function.starts_with("Cmd")).then(|| (entry.to_string(), function.to_string()))
    }).collect()
}

/// Every command's tool name, `openttd.cmd.<function without Cmd>`, by its enum entry: the
/// catalogue names a tool after the function the command runs, which the enum entry maps to in
/// the `DEF_CMD_TRAIT` declarations (every header under src/).
fn command_tools(repo: &Path) -> std::io::Result<BTreeMap<String, String>> {
    let mut out = BTreeMap::new();
    let mut dirs = vec![repo.join("src")];
    while let Some(dir) = dirs.pop() {
        for entry in std::fs::read_dir(&dir)? {
            let path = entry?.path();
            if path.is_dir() {
                dirs.push(path);
            } else if path.extension().is_some_and(|e| e == "h") {
                let text = std::fs::read_to_string(&path).unwrap_or_default();
                for (entry, function) in trait_pairs(&text) {
                    out.insert(entry, format!("openttd.cmd.{}", &function[3..]));
                }
            }
        }
    }
    Ok(out)
}

fn console_commands(repo: &Path) -> std::io::Result<BTreeSet<String>> {
    let src = std::fs::read_to_string(repo.join("src/console_cmds.cpp"))?;
    Ok(src
        .split("IConsole::CmdRegister(\"")
        .skip(1)
        .filter_map(|s| s.split('"').next().map(str::to_string))
        .collect())
}

fn script_api(repo: &Path) -> std::io::Result<BTreeSet<String>> {
    let mut out = BTreeSet::new();
    for entry in std::fs::read_dir(repo.join("src/script/api"))? {
        let path: PathBuf = entry?.path();
        let name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
        if !(name.starts_with("script_") && name.ends_with(".hpp")) {
            continue;
        }
        let text = std::fs::read_to_string(&path).unwrap_or_default();
        let mut class = String::new();
        for line in text.lines() {
            let t = line.trim();
            if let Some(rest) = t.strip_prefix("class Script") {
                class = format!("Script{}", rest.split([' ', ':', '{']).next().unwrap_or(""));
            }
            if let Some(rest) = t.strip_prefix("static ") {
                if t.ends_with(';') && t.contains('(') && !t.contains('=') {
                    let before = &rest[..rest.find('(').unwrap()];
                    if let Some(f) = before.split_whitespace().last() {
                        let f = f.trim_start_matches(['*', '&']);
                        if !f.is_empty() && !class.is_empty() {
                            out.insert(format!("{class}::{f}"));
                        }
                    }
                }
            }
        }
    }
    Ok(out)
}

fn get(panel: &str, path: &str) -> Result<Value, String> {
    ureq::get(&format!("{panel}{path}"))
        .timeout(Duration::from_secs(15))
        .call()
        .map_err(|e| e.to_string())?
        .into_json()
        .map_err(|e| e.to_string())
}

fn post(panel: &str, path: &str, body: Value) -> Result<Value, String> {
    ureq::post(&format!("{panel}{path}"))
        .timeout(Duration::from_secs(15))
        .send_json(body)
        .map_err(|e| e.to_string())?
        .into_json()
        .map_err(|e| e.to_string())
}

fn main() {
    let mut args = std::env::args().skip(1);
    let (mut repo, mut panel, mut as_json, mut report) =
        (PathBuf::from("."), "http://127.0.0.1:8800".to_string(), false, false);
    while let Some(a) = args.next() {
        match a.as_str() {
            "--repo" => repo = PathBuf::from(args.next().unwrap_or_default()),
            "--panel" => panel = args.next().unwrap_or_default(),
            "--json" => as_json = true,
            "--report" => report = true,
            _ => {
                eprintln!("usage: mcp-api-coverage [--repo PATH] [--panel URL] [--json] [--report]");
                std::process::exit(2);
            }
        }
    }
    let (cmds, cons, api, cmd_tools) = match (commands(&repo), console_commands(&repo), script_api(&repo), command_tools(&repo)) {
        (Ok(a), Ok(b), Ok(c), Ok(d)) => (a, b, c, d),
        (a, b, c, d) => {
            eprintln!("mcp-api-coverage: cannot read the sources under {}: {:?} {:?} {:?} {:?}", repo.display(),
                      a.err(), b.err(), c.err(), d.err());
            std::process::exit(3);
        }
    };
    let panel = panel.trim_end_matches('/').to_string();
    let (tools, spec, listed) = match (get(&panel, "/api/catalogue"), get(&panel, "/openapi.json"),
                                       post(&panel, "/api/console", json!({"command": "list_cmds", "wait": 1.0}))) {
        (Ok(c), Ok(s), Ok(l)) => (c, s, l),
        (c, s, l) => {
            eprintln!("mcp-api-coverage: cannot read the panel at {panel}: {:?} {:?} {:?}", c.err(), s.err(), l.err());
            std::process::exit(3);
        }
    };
    let tool_names: BTreeSet<String> = tools.pointer("/tools/result/tools").and_then(Value::as_array).into_iter().flatten()
        .filter_map(|t| t.get("name").and_then(Value::as_str).map(str::to_string)).collect();
    let routes: BTreeSet<String> = spec.get("paths").and_then(Value::as_object).map(|p| p.keys().cloned().collect()).unwrap_or_default();
    let live_console: BTreeSet<String> = listed.get("output").and_then(Value::as_array).into_iter().flatten()
        .filter_map(|l| l.as_str().map(str::trim).map(str::to_string)).collect();

    // A command is exposed when its tool (named after its Cmd* function) is an MCP tool and its
    // derived route is in OpenAPI. An entry no DEF_CMD_TRAIT maps is looked up by its own name.
    let cmd_exposed = |c: &String| {
        let tool = cmd_tools.get(c).cloned().unwrap_or_else(|| format!("openttd.cmd.{c}"));
        tool_names.contains(&tool) && routes.contains(&format!("/api/tools/{tool}"))
    };
    // A script API function is exposed when a tool names its class and function (openttd.api.<Class>.<Function>).
    let api_exposed = |f: &String| {
        let tool = format!("openttd.api.{}", f.replace("::", "."));
        tool_names.contains(&tool) && routes.contains(&format!("/api/tools/{tool}"))
    };
    let dims = vec![
        ("commands", "Commands enum (src/command_type.h)", cmds.iter().filter(|c| cmd_exposed(c)).count(), cmds.len(),
         cmds.iter().filter(|c| !cmd_exposed(c)).cloned().collect::<Vec<_>>()),
        ("console", "console commands (src/console_cmds.cpp)", cons.iter().filter(|c| live_console.contains(*c)).count(), cons.len(),
         cons.iter().filter(|c| !live_console.contains(*c)).cloned().collect()),
        ("script_api", "Script API static functions (src/script/api/script_*.hpp)", api.iter().filter(|f| api_exposed(f)).count(), api.len(),
         api.iter().filter(|f| !api_exposed(f)).cloned().collect()),
    ];
    let full = dims.iter().all(|d| d.2 == d.3);
    if as_json {
        let v: Vec<Value> = dims.iter().map(|(id, what, n, total, missing)| json!({
            "dimension": id, "source": what, "exposed": n, "total": total, "missing": missing})).collect();
        println!("{}", serde_json::to_string_pretty(&json!({"full": full, "dimensions": v,
            "mcp_tools": tool_names.len(), "openapi_paths": routes.len()})).unwrap());
    } else {
        println!("public C++ API exposed through MCP and OpenAPI ({} MCP tools, {} OpenAPI paths):", tool_names.len(), routes.len());
        for (id, what, n, total, missing) in &dims {
            let pct = if *total == 0 { 100.0 } else { *n as f64 * 100.0 / *total as f64 };
            println!("  {} {id:<11} {n:>4}/{total:<4} {pct:5.1}%  {what}", if n == total { "PASS" } else { "FAIL" });
            if !missing.is_empty() {
                let shown: Vec<_> = missing.iter().take(6).cloned().collect();
                println!("                missing e.g. {}{}", shown.join(", "), if missing.len() > 6 { ", ..." } else { "" });
            }
        }
    }
    if !full && !report {
        std::process::exit(10);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_command_is_named_after_the_function_its_trait_declares() {
        let header = "DEF_CMD_TRAIT(Commands::BuildRail, CmdBuildSingleRail, CommandFlags({CommandFlag::Auto}), CommandType::LandscapeConstruction)\n\
                      DEF_CMD_TRAIT(Commands::MoveWaypointNAme, CmdMoveWaypointName, {}, CommandType::OtherManagement)\n\
                      #define DEF_CMD_TRAIT(cmd_, proc_, flags_, type_)";
        assert_eq!(trait_pairs(header), vec![
            ("BuildRail".to_string(), "CmdBuildSingleRail".to_string()),
            ("MoveWaypointNAme".to_string(), "CmdMoveWaypointName".to_string()),
        ]);
    }

    #[test]
    fn every_command_of_this_checkout_maps_to_a_tool() {
        let repo = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..");
        let (cmds, tools) = (commands(&repo).unwrap(), command_tools(&repo).unwrap());
        let unmapped: Vec<_> = cmds.iter().filter(|c| !tools.contains_key(*c)).collect();
        assert!(unmapped.is_empty(), "no DEF_CMD_TRAIT for {unmapped:?}");
        assert_eq!(tools["BuildRail"], "openttd.cmd.BuildSingleRail");
    }
}
