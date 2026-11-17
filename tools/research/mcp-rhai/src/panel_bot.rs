//! RESEARCH-ONLY: the owner's bot in the control panel (src/panel.rs), ported from
//! tools/research/mcp-panel/server.py: the AIs the owner can run with their settings, what a setting
//! may name, starting a bot, changing a running bot's setting, and the routes the planner's portfolio
//! hands the bot when the panel starts it.

use serde_json::{json, Map, Value};
use std::collections::{BTreeMap, HashSet};
use std::path::PathBuf;
use std::process::Command;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::time::Duration;

use crate::panel::{py_int, py_json, py_str, run, Game};

fn ai_dir() -> PathBuf {
    PathBuf::from(std::env::var("HOME").unwrap_or_default()).join("Documents/OpenTTD/ai")
}

fn is_word(c: char) -> bool {
    c.is_alphanumeric() || c == '_'
}

/// The text between `open` and the first `close` after it, for every occurrence: (inside, rest).
/// A string literal at the start of `s`, `\"` escapes kept as written: (inside, rest).
fn quoted(s: &str) -> Option<(&str, &str)> {
    let s = s.strip_prefix('"')?;
    let mut escaped = false;
    let end = s.char_indices().find(|&(_, c)| {
        let close = c == '"' && !escaped;
        escaped = c == '\\' && !escaped;
        close
    })?.0;
    Some((&s[..end], &s[end + 1..]))
}

/// The value of `key = value` in a Squirrel table, as server.py's `field()` reads it: the first `key`
/// at a word boundary followed by `=`, its value a quoted string or the text up to a comma or line
/// end, stripped of blanks and quotes.
pub fn field(block: &str, key: &str) -> Option<String> {
    let mut from = 0;
    while let Some(p) = block[from..].find(key) {
        let at = from + p;
        from = at + key.len();
        if block[..at].chars().next_back().map_or(false, is_word) {
            continue;
        }
        let Some(rest) = block[at + key.len()..].trim_start().strip_prefix('=') else { continue };
        let rest = rest.trim_start();
        if let Some((inside, mut tail)) = quoted(rest) {
            // `"fr" + "2" + "_from"` (a loop unrolled by `unroll_loops`) is one string; a term that
            // is not a literal cannot be resolved here, so the value is unknown.
            let mut value = inside.to_string();
            while let Some(next) = tail.trim_start().strip_prefix('+') {
                let (part, after) = quoted(next.trim_start())?;
                value.push_str(part);
                tail = after;
            }
            return Some(value);
        }
        let end = rest.find([',', '\n']).unwrap_or(rest.len());
        if end == 0 {
            continue;
        }
        return Some(rest[..end].trim().trim_matches('"').to_string());
    }
    None
}

fn field_int(block: &str, key: &str) -> Option<i64> {
    field(block, key)?.parse().ok()
}

/// `AddLabels("name", {_0 = "off", _1 = "on"})` of an info.nut, by setting name.
fn labels(text: &str) -> BTreeMap<String, Map<String, Value>> {
    let mut out = BTreeMap::new();
    let mut from = 0;
    while let Some(p) = text[from..].find("AddLabels(") {
        let at = from + p + "AddLabels(".len();
        from = at;
        let Some((name, rest)) = quoted(text[at..].trim_start()).filter(|(n, _)| !n.is_empty()) else { continue };
        let Some(rest) = rest.trim_start().strip_prefix(',').map(str::trim_start).and_then(|r| r.strip_prefix('{')) else { continue };
        // The body ends at the first `}` that a `)` follows.
        let Some(end) = rest.match_indices('}').map(|(i, _)| i).find(|i| rest[i + 1..].trim_start().starts_with(')')) else { continue };
        let mut map = Map::new();
        let body = &rest[..end];
        let mut b = 0;
        while let Some(q) = body[b..].find('_') {
            let s = &body[b + q + 1..];
            b += q + 1;
            let n = s.chars().enumerate().take_while(|(i, c)| c.is_ascii_digit() || (*i == 0 && *c == '-')).count();
            if n == 0 || &s[..n] == "-" { continue; }
            let Some((label, _)) = s[n..].trim_start().strip_prefix('=').map(str::trim_start).and_then(quoted) else { continue };
            map.insert(s[..n].to_string(), json!(label));
        }
        out.insert(name.to_string(), map);
    }
    out
}

/// Walk `s` as Squirrel source: `f(byte index, char, outside a literal)` for every char, a string
/// or char literal (with `\` escapes) counted as inside. `f` returning false stops the walk.
fn walk_code(s: &str, mut f: impl FnMut(usize, char, bool) -> bool) {
    let (mut quote, mut escaped) = (None::<char>, false);
    for (i, c) in s.char_indices() {
        let outside = quote.is_none();
        match quote {
            Some(_) if escaped => escaped = false,
            Some(_) if c == '\\' => escaped = true,
            Some(q) if c == q => quote = None,
            None if c == '"' || c == '\'' => quote = Some(c),
            _ => {}
        }
        if !f(i, c, outside) { return; }
    }
}

/// `text` without `//` and `/* */` comments (each replaced by one blank), literals left as they are,
/// so a brace or quote in a comment cannot end a loop body or open a string.
pub fn strip_comments(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    let mut chars = text.char_indices().peekable();
    let (mut quote, mut escaped) = (None::<char>, false);
    while let Some((i, c)) = chars.next() {
        if let Some(q) = quote {
            out.push(c);
            if escaped { escaped = false; } else if c == '\\' { escaped = true; } else if c == q { quote = None; }
            continue;
        }
        let rest = &text[i..];
        if rest.starts_with("//") || rest.starts_with("/*") {
            let end = if rest.starts_with("//") { rest.find('\n').unwrap_or(rest.len()) } else { rest[2..].find("*/").map_or(rest.len(), |e| e + 4) };
            out.push(' ');
            while chars.peek().map_or(false, |&(k, _)| k < i + end) { chars.next(); }
            continue;
        }
        if c == '"' || c == '\'' { quote = Some(c); }
        out.push(c);
    }
    out
}

/// The most text unrolling may write, so nested loops cannot blow the panel's memory.
const UNROLL_LIMIT: usize = 4 << 20;

/// `text` with every `for (local i = a; i <= b; i++) { ... }` (or `<`) written out once per `i`,
/// the loop variable outside literals replaced by its value as a string literal, so settings an
/// info.nut declares in a loop (PrismaticAI's fr2_* to fr16_*) are read by name. Nested loops are
/// unrolled too. A loop of another shape is left as it is. Comments must be stripped first
/// (`strip_comments`).
pub fn unroll_loops(text: &str) -> String {
    let mut out = String::new();
    let mut rest = text;
    while let Some(p) = rest.find("for (local ") {
        out.push_str(&rest[..p]);
        let after = &rest[p..];
        match unroll_one(after) {
            Some((written, used)) => { out.push_str(&written); rest = &after[used..]; }
            None => { out.push_str("for (local "); rest = &after["for (local ".len()..]; }
        }
    }
    out.push_str(rest);
    out
}

/// One `for (local ...)` loop at the start of `s`, unrolled, and how many bytes of `s` it took.
fn unroll_one(s: &str) -> Option<(String, usize)> {
    let head_end = s.find(')')?;
    let head = &s["for (local ".len()..head_end];
    let parts: Vec<&str> = head.split(';').map(str::trim).collect();
    let [init, cond, step] = parts[..] else { return None };
    let (var, start) = init.split_once('=')?;
    let var = var.trim();
    if var.is_empty() || !var.chars().all(is_word) { return None; }
    let start: i64 = start.trim().parse().ok()?;
    let (end, inclusive) = if let Some(e) = cond.strip_prefix(var).map(str::trim_start).and_then(|c| c.strip_prefix("<=")) {
        (e.trim().parse::<i64>().ok()?, true)
    } else {
        (cond.strip_prefix(var)?.trim_start().strip_prefix('<')?.trim().parse::<i64>().ok()?, false)
    };
    if step != format!("{var}++") { return None; }
    let open = head_end + 1 + s[head_end + 1..].find('{')?;
    if !s[head_end + 1..open].trim().is_empty() { return None; }
    // The body's closing brace, braces inside literals not counted.
    let (mut depth, mut close) = (0usize, None);
    walk_code(&s[open..], |i, c, outside| {
        if outside && c == '{' { depth += 1; }
        if outside && c == '}' {
            depth -= 1;
            if depth == 0 { close = Some(open + i); return false; }
        }
        true
    });
    let close = close?;
    let body = &s[open + 1..close];
    let last = if inclusive { end } else { end.checked_sub(1)? };
    let count = last.checked_sub(start)?;
    if !(0..=1000).contains(&count) { return None; }
    let mut written = String::new();
    for i in start..=last {
        written.push_str(&substitute(body, var, &format!("\"{i}\"")));
        if written.len() > UNROLL_LIMIT { return None; }
    }
    let written = unroll_loops(&written);
    if written.len() > UNROLL_LIMIT { return None; }
    Some((written, close + 1))
}

/// `body` with the identifier `var` outside literals replaced by `with`.
fn substitute(body: &str, var: &str, with: &str) -> String {
    let mut out = String::with_capacity(body.len());
    let mut skip_to = 0usize;
    walk_code(body, |i, c, outside| {
        if i < skip_to { return true; }
        if outside && body[i..].starts_with(var)
            && (i == 0 || !is_word(body[..i].chars().next_back().unwrap()))
            && !body[i + var.len()..].chars().next().map_or(false, is_word) {
            out.push_str(with);
            skip_to = i + var.len();
            return true;
        }
        out.push(c);
        true
    });
    out
}

/// Settings as an AI's info.nut declares them, in openttd.ai.list's shape (flags, labels, value =
/// the default, editable), so the panel draws one kind of control for either source.
pub fn info_settings(text: &str) -> Vec<Value> {
    let text = &unroll_loops(&strip_comments(text));
    let labels = labels(text);
    let mut out = Vec::new();
    let mut from = 0;
    while let Some(p) = text[from..].find("AddSetting({") {
        let at = from + p + "AddSetting({".len();
        let Some(end) = text[at..].find("});") else { break };
        let block = &text[at..at + end];
        from = at + end;
        let flags_text = field(block, "flags").unwrap_or_default();
        let flags: Vec<&str> = [("boolean", "CONFIG_BOOLEAN"), ("in_game", "CONFIG_INGAME"), ("developer", "CONFIG_DEVELOPER")]
            .iter().filter(|(_, t)| flags_text.contains(t)).map(|(f, _)| *f).collect();
        // A name that is not a literal (or a loop unrolled into one) is skipped, not shown bare.
        let Some(name) = field(block, "name") else { continue };
        let name = Some(name);
        let default = field_int(block, "custom_value");
        let (mut mn, mut mx) = (field_int(block, "min_value"), field_int(block, "max_value"));
        if flags.contains(&"boolean") {
            (mn, mx) = (Some(0), Some(1));
        }
        let step = field_int(block, "step_size").filter(|s| *s != 0).unwrap_or(1);
        let lab = name.as_ref().and_then(|n| labels.get(n)).cloned().unwrap_or_default();
        out.push(json!({"name": name, "description": field(block, "description"), "min": mn, "max": mx, "step": step,
                        "default": default, "flags": flags, "labels": lab, "value": default, "editable": true,
                        "ingame": flags.contains(&"in_game")}));
    }
    out
}

/// The name an info.nut gives its AI: `GetName() { return "..."; }`.
pub fn info_name(text: &str) -> Option<String> {
    let mut from = 0;
    while let Some(p) = text[from..].find("GetName()") {
        let at = from + p + "GetName()".len();
        from = at;
        let Some(rest) = text[at..].trim_start().strip_prefix('{') else { continue };
        let Some(rest) = rest.trim_start().strip_prefix("return") else { continue };
        if let Some((name, _)) = quoted(rest.trim_start()).filter(|(n, _)| !n.is_empty()) {
            return Some(name.to_string());
        }
    }
    None
}

fn data_of(v: Value) -> Value {
    match v {
        Value::Object(mut m) if m.contains_key("data") => m.remove("data").unwrap_or(Value::Null),
        other => other,
    }
}

fn key(v: &Value) -> String {
    v.as_str().map(str::to_string).unwrap_or_else(|| v.to_string())
}

/// The AIs the owner can run as their representative, with their settings, and the running AI
/// companies with the value each setting has now. One shape whatever the source: from
/// openttd.ai.list when the game offers it (values read from the running scripts), else from each
/// AI's info.nut (defaults only, and no live change possible).
pub fn bots(g: &Arc<Game>) -> Result<Value, String> {
    let tools = g.tool_names();
    let mut infos: Vec<PathBuf> = std::fs::read_dir(ai_dir()).map(|d| d.filter_map(|e| e.ok().map(|e| e.path().join("info.nut")))
        .filter(|p| p.is_file()).collect()).unwrap_or_default();
    infos.sort();
    let mut ais = Vec::new();
    for info in infos {
        let text = String::from_utf8_lossy(&std::fs::read(&info).unwrap_or_default()).into_owned();
        let dir = info.parent().and_then(|d| d.file_name()).map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        ais.push(json!({"ai": info_name(&text).unwrap_or_else(|| dir.clone()), "dir": dir, "settings": info_settings(&text)}));
    }
    let mut companies = BTreeMap::new();
    for c in g.resource("openttd://companies")?.pointer("/data/companies").and_then(Value::as_array).cloned().unwrap_or_default() {
        if c.get("is_ai").map_or(false, |v| v.as_bool().unwrap_or(false)) {
            companies.insert(key(&c["id"]), c);
        }
    }
    let mut running = Vec::new();
    let mut source = "info.nut (openttd.ai.list not in tools/list yet)";
    let live_list = tools.contains("openttd.ai.list");
    if live_list {
        let data = g.tool("openttd.ai.list", json!({}))?;
        let data = if data.is_object() { data_of(data) } else { json!({}) };
        source = "mcp:openttd.ai.list";
        // An AI installed but without an info.nut under ~/Documents/OpenTTD/ai still appears, with no settings to set.
        let known: HashSet<String> = ais.iter().map(|a| py_str(a.get("ai"))).collect();
        for a in data["available"].as_array().into_iter().flatten() {
            if !known.contains(&py_str(a.get("name"))) {
                ais.push(json!({"ai": a.get("name"), "dir": null, "settings": []}));
            }
        }
        for c in data["companies"].as_array().into_iter().flatten() {
            let base = companies.get(&key(&c["company"])).cloned().unwrap_or(json!({}));
            let mut settings = c.get("settings").cloned().unwrap_or(json!([]));
            for st in settings.as_array_mut().into_iter().flatten() {
                let ingame = st["flags"].as_array().map_or(false, |f| f.contains(&json!("in_game")));
                if st.is_object() { st["ingame"] = json!(ingame); }
            }
            running.push(json!({"id": c.get("company"), "name": base.get("name").cloned().unwrap_or(json!("")), "money": base.get("money"),
                                "ai": c.get("ai"), "running": c.get("running"), "settings": settings}));
        }
    } else {
        for c in companies.values() {
            running.push(json!({"id": c["id"], "name": c.get("name").cloned().unwrap_or(json!("")), "money": c.get("money"),
                                "ai": null, "running": true, "settings": []}));
        }
    }
    Ok(json!({"source": source, "ais": ais, "running": running,
              "live": {"list": live_list, "set_setting": tools.contains("openttd.ai.set_setting")}}))
}

/// What a town, industry or cargo setting may name: towns from openttd://towns (by population), and
/// from openttd://world/graph where the game serves it, industries with what they produce and accept
/// and every cargo they name. A kind the game cannot list is empty, and the panel then shows a number
/// box for it instead of a picker with nothing in it.
pub fn bot_choices(g: &Arc<Game>) -> Result<Value, String> {
    let mut towns = Vec::new();
    let mut industries = Vec::new();
    let mut cargos: BTreeMap<i64, String> = BTreeMap::new();
    let t = data_of(g.resource("openttd://towns")?);
    for n in t["towns"].as_array().into_iter().flatten() {
        towns.push(json!({"id": n.get("id"), "name": n.get("name").cloned().unwrap_or_else(|| json!(format!("town {}", py_str(n.get("id"))))),
                          "population": n.get("population").cloned().unwrap_or(json!(0))}));
    }
    let gr = data_of(g.resource("openttd://world/graph")?);
    let had_towns = !towns.is_empty();
    for n in gr["nodes"].as_array().into_iter().flatten() {
        let id = py_str(n.get("id"));
        let (kind, idx) = id.split_once(':').unwrap_or((id.as_str(), ""));
        let Ok(idx) = idx.parse::<i64>() else { continue };
        let names = |k: &str| -> Vec<Value> { n[k].as_array().into_iter().flatten().map(|p| p.get("name").cloned().unwrap_or(Value::Null)).collect() };
        if kind == "town" && !had_towns {
            towns.push(json!({"id": idx, "name": n.get("name").cloned().unwrap_or_else(|| json!(format!("town {idx}"))),
                              "population": n.get("population").cloned().unwrap_or(json!(0))}));
        } else if kind == "industry" {
            industries.push(json!({"id": idx, "name": n.get("name").cloned().unwrap_or_else(|| json!(format!("industry {idx}"))),
                                   "produces": names("produces"), "accepts": names("accepts")}));
            for c in n["produces"].as_array().into_iter().flatten().chain(n["accepts"].as_array().into_iter().flatten()) {
                if let Some(id) = c.get("cargo").filter(|v| !v.is_null()).and_then(|v| py_int(v).ok()) {
                    cargos.insert(id, c.get("name").and_then(Value::as_str).map(str::to_string).unwrap_or_else(|| format!("cargo {id}")));
                }
            }
        }
    }
    towns.sort_by(|a, b| b["population"].as_f64().unwrap_or(0.0).total_cmp(&a["population"].as_f64().unwrap_or(0.0)));
    industries.sort_by(|a, b| py_str(a.get("name")).cmp(&py_str(b.get("name"))));
    let mut cargo: Vec<(i64, String)> = cargos.into_iter().collect();
    cargo.sort_by(|a, b| a.1.cmp(&b.1));
    Ok(json!({"town": towns, "industry": industries, "cargo": cargo.into_iter().map(|(id, name)| json!({"id": id, "name": name})).collect::<Vec<_>>()}))
}

pub fn start_bot(g: &Arc<Game>, ai: &str, settings: Value) -> Result<Value, String> {
    let mut flat = Vec::new();
    for (k, v) in settings.as_object().into_iter().flatten() {
        flat.push(format!("{k}={}", py_int(v)?));
    }
    let flat = flat.join(",");
    if g.tool_names().contains("openttd.ai.start") {
        return g.tool("openttd.ai.start", json!({"name": ai, "settings": settings}));
    }
    let cmd = format!("start_ai {ai} {flat}").trim().to_string();
    g.console(&cmd)?;
    g.ai_started.store(true, Ordering::SeqCst);
    g.event("ai", &format!("bot started: start_ai {ai} {flat}"));
    Ok(json!({"status": "started", "via": "console", "command": cmd}))
}

pub fn set_bot_setting(g: &Arc<Game>, company: &Value, name: &str, value: &Value) -> Result<Value, String> {
    if !g.tool_names().contains("openttd.ai.set_setting") {
        return Ok(json!({"error": "this game binary has no openttd.ai.set_setting: a running bot's settings cannot change; start a bot with the new settings instead",
                         "fallback": "start"}));
    }
    g.tool("openttd.ai.set_setting", json!({"company": py_int(company)?, "name": name, "value": py_int(value)?}))
}

/// Settings in the order they were set, a key set again keeping its place (a Python dict).
#[derive(Default)]
pub struct Settings(pub Vec<(String, Value)>);

impl Settings {
    fn set(&mut self, k: &str, v: Value) {
        match self.0.iter_mut().find(|(n, _)| n == k) {
            Some(e) => e.1 = v,
            None => self.0.push((k.to_string(), v)),
        }
    }
    fn has(&self, k: &str) -> bool {
        self.0.iter().any(|(n, _)| n == k)
    }
    pub fn iter(&self) -> impl Iterator<Item = &(String, Value)> {
        self.0.iter()
    }
}

/// A world-graph id ("industry:12") or a plain number, as the number.
fn num(v: &Value) -> Option<i64> {
    py_str(Some(v)).rsplit(':').next()?.trim().parse().ok()
}

/// `d.get(k, default)`: the value when the key is there (even null), else the default.
fn get_or<'a>(v: &'a Value, k: &str, default: &'a Value) -> &'a Value {
    v.get(k).unwrap_or(default)
}

/// Freight lines handed to the bot at most (fr_*, fr2_* .. fr16_*); bridge 2026-11-17 declares as many.
pub const MAX_FREIGHT_LINES: usize = 16;

/// Freight lines the planned bot may plan itself once the handed ones are built (bridge own_lines; it
/// defaults to 0 there, so a bot started without a plan, a duel's baseline, stays unplanned). On: over
/// ten years on seed 31337 eight own lines gave +10.4% (research/results duel-20261117T155736, 4811581,
/// against duel-20261117T180225, 4356412, without them). They cost value until about month 50, so a
/// 36-month duel shows their cost and not their return (duel-20261117T131347: -20%).
pub const OWN_LINES: usize = 8;

/// The bot's routes from the planner's portfolio within the budget, chosen from mcp-plan's answer:
/// up to eight freight rail lines (fr_*, fr2_* .. fr8_*), one passenger rail pair (rail_a, rail_b)
/// and one air pair (air_a, air_b, air_planes), with the events that explain each choice.
pub fn settings_from_plan(out: &Value, event: &mut dyn FnMut(String)) -> Settings {
    let empty = json!({});
    let pf = out.get("portfolio").filter(|v| v.is_object()).unwrap_or(&empty);
    let chosen: Vec<Value> = pf["chosen"].as_array().cloned().unwrap_or_default();
    event(format!("planner portfolio by {}: {} investment(s), total score {}, {}k of {}k",
                  pf.get("solver").map_or("no solver".to_string(), |v| py_str(Some(v))), chosen.len(),
                  py_str(pf.get("total_score")), py_str(pf.get("investment_k")), py_str(pf.get("budget_k"))));
    let priced: Vec<Value> = out.pointer("/line_costs/priced").and_then(Value::as_array).cloned().unwrap_or_default();
    if !priced.is_empty() {
        let found: Vec<&Value> = priced.iter().filter(|p| p["status"] == json!("found")).collect();
        event(format!("planner: {} rail line(s) priced on the map by track-planning, {} with a route: {}{}", priced.len(), found.len(),
                      found.iter().map(|p| format!("{} pieces {}", py_str(p.get("pieces")), p["money"].as_f64().unwrap_or(0.0).round() as i64))
                          .collect::<Vec<_>>().join(", "),
                      if found.len() == priced.len() { String::new() } else { format!("; {} without a route dropped their rail option", priced.len() - found.len()) }));
    }
    let priced_any = !priced.is_empty();
    // PrismaticAI lays rail over buildable land, level crossings, bridges and tunnels (rail.nut
    // FindRailPath), so any line track-planning found a route for is the bot's. When lines were
    // priced, an unpriced one is not handed over blind either: on seed 20208 an unpriced passenger
    // line needed a bridge over a river and failed. Without AlgoGlyph (nothing priced) every line is
    // tried as before.
    let bot_can_build = |opt: &Value| match opt.get("track").filter(|t| t.as_object().map_or(false, |m| !m.is_empty())) {
        None => !priced_any,
        Some(t) => t["status"] == json!("found"),
    };
    let score = |c: &Value| c["score"].as_f64().unwrap_or(0.0);
    // The portfolio first; a rail line the bot cannot build is replaced by the next priced or
    // unpriced line of its kind that it can, from the planner's lists (best score first).
    let mut spare: Vec<Value> = out["freight"].as_array().into_iter().flatten()
        .chain(out.pointer("/by_mode/rail").and_then(Value::as_array).into_iter().flatten())
        .filter(|c| !chosen.contains(c) && score(c) > 0.0).cloned().collect();
    spare.sort_by(|a, b| score(b).total_cmp(&score(a)));
    let mut settings = Settings::default();
    let (mut freight_lines, mut producers) = (0usize, HashSet::new());
    // The freight lines chosen (portfolio first, then spares), set in order of score after the loop.
    let mut lines: Vec<(f64, i64, i64, i64, Value)> = Vec::new();
    let mut pair_score: Option<f64> = None;
    for c in chosen.iter().chain(spare.iter()) {
        let mode = c["mode"].as_str().unwrap_or("");
        let in_chosen = chosen.contains(c);
        let opt = c["options"].as_array().into_iter().flatten().find(|o| o["mode"] == json!(mode)).cloned().unwrap_or(json!({}));
        let rail = mode == "rail" || mode == "freight_rail";
        if rail && !bot_can_build(&opt) {
            if in_chosen {
                let line = format!("{} - {}", py_str(get_or(c, "producer", get_or(c, "a", &empty)).get("name")),
                                   py_str(get_or(c, "acceptor", get_or(c, "b", &empty)).get("name")));
                let why = if opt.get("track").map_or(false, |t| t.as_object().map_or(false, |m| !m.is_empty())) {
                    format!("has no route on the map ({})", py_str(opt["track"].get("status")))
                } else {
                    "was not priced on the map, so it is not handed to the bot blind".to_string()
                };
                event(format!("planner: the {mode} line {line} {why}; the next line it can build goes instead"));
            }
            continue;
        }
        if !in_chosen && !rail {
            continue;
        }
        let freight = c["kind"] == json!("freight");
        if freight && mode == "freight_rail" && freight_lines < MAX_FREIGHT_LINES {
            // Up to MAX_FREIGHT_LINES freight lines, one per producer (a producer's cargo is counted
            // once): fr_* is built at once, the rest when money allows. After eight lines the best run
            // on seed 31337 held 308074 idle at month 36 with its loan repaid (research/results
            // duel-20261117T041705), so a longer list is handed over and built as money comes in. The
            // game ignores a setting the AI does not declare (ScriptConfig::SetSetting), so an older bot
            // uses the first ones.
            let (Some(producer), Some(acceptor), Ok(cargo)) = (num(&c["producer"]["id"]), num(&c["acceptor"]["id"]), py_int(&c["cargo_id"])) else { continue };
            if !producers.insert(producer) {
                continue;
            }
            freight_lines += 1;
            lines.push((score(c), producer, acceptor, cargo, json!({"cargo": c.get("cargo"), "from": c["producer"].get("name"),
                "to": c["acceptor"].get("name"), "distance": c.get("distance"), "profit": opt.get("monthly_profit"), "score": c.get("score")})));
        } else if !freight && mode == "rail" && !settings.has("rail_a") {
            settings.set("rail_a", c["a"]["id"].clone());
            settings.set("rail_b", c["b"]["id"].clone());
            pair_score = Some(score(c));
            event(format!("planner: passengers by rail {} - {}, score {}", py_str(c["a"].get("name")), py_str(c["b"].get("name")), py_str(c.get("score"))));
        } else if !freight && mode == "air" && !settings.has("air_a") {
            let count = opt.pointer("/vehicles/count").filter(|v| crate::panel::py_int(v).map_or(false, |n| n != 0))
                .and_then(|v| py_int(v).ok()).unwrap_or(2);
            settings.set("air_a", c["a"]["id"].clone());
            settings.set("air_b", c["b"]["id"].clone());
            settings.set("air_planes", json!(count.clamp(1, 8)));
            event(format!("planner: air {} - {}, {count} plane(s), score {}", py_str(c["a"].get("name")), py_str(c["b"].get("name")), py_str(c.get("score"))));
        }
    }
    // The bot builds its lines in the order it is given, borrowing for each: the best score first.
    // On seed 31337 a passenger pair scored 113003 was built second, before freight lines scored up
    // to 240547, and the company was a third behind at month 24 (research/results
    // duel-20261117T032759). Freight lines go in order of score, and rail_after tells the bot how many
    // of them score above the passenger pair, to build before it.
    lines.sort_by(|a, b| b.0.total_cmp(&a.0));
    for (i, (_, producer, acceptor, cargo, e)) in lines.iter().enumerate() {
        let prefix = if i == 0 { "fr_".to_string() } else { format!("fr{}_", i + 1) };
        settings.set(&format!("{prefix}from"), json!(producer));
        settings.set(&format!("{prefix}to"), json!(acceptor));
        settings.set(&format!("{prefix}cargo"), json!(cargo));
        event(format!("planner: {} by rail {} -> {}, {} tiles, profit {}/month, score {}{}", py_str(e.get("cargo")),
                      py_str(e.get("from")), py_str(e.get("to")), py_str(e.get("distance")), py_str(e.get("profit")), py_str(e.get("score")),
                      if i == 0 { String::new() } else { format!(" (line {}, built when the money allows)", i + 1) }));
    }
    if !lines.is_empty() && OWN_LINES > 0 {
        settings.set("own_lines", json!(OWN_LINES));
        event(format!("planner: after these lines the bot may plan up to {OWN_LINES} more itself (own_lines)"));
    }
    if let Some(ps) = pair_score {
        let after = lines.iter().filter(|f| f.0 > ps).count();
        settings.set("rail_after", json!(after));
        event(format!("planner: the passenger rail pair (score {ps}) is built after {after} freight line(s) that score more"));
    }
    settings
}

/// The bot's routes from the planner's portfolio within the budget (difficulty.max_loan), read live
/// with mcp-plan; no settings without a planner or a portfolio.
pub fn planned_air(g: &Arc<Game>) -> Settings {
    let exe = crate::panel::sibling("mcp-plan");
    if !g.args.plan_bot || !exe.exists() {
        return Settings::default();
    }
    let mut params = match g.max_loan().filter(|b| *b != 0) { Some(b) => json!({"budget": b}), None => json!({}) };
    // Back-propagation: with PRISMATIC_PLAN_CALIBRATION naming a calibration store (research/
    // calibration.json, relative to the repository), the plan starts from what earlier games earned
    // per mode instead of the model alone (src/calibrate.rs). Off unless a run asks for it: a duel's
    // calibrated: true, so its A/B decides whether it pays.
    if let Some(name) = std::env::var("PRISMATIC_PLAN_CALIBRATION").ok().filter(|v| !v.is_empty() && v != "off") {
        let path = crate::panel_results::repo().join(&name);
        match std::fs::read_to_string(&path).ok().and_then(|t| serde_json::from_str::<Value>(&t).ok()) {
            Some(store) if store.pointer("/params/calibration").is_some_and(|c| c.as_object().is_some_and(|m| !m.is_empty())) => {
                let cal = store["params"]["calibration"].clone();
                g.event("ai", &format!("planner: calibration from {name} ({} reading(s)): {cal}", store["readings"].as_array().map_or(0, Vec::len)));
                params["calibration"] = cal;
            }
            _ => g.event("ai", &format!("planner: calibration {name} asked for, but it holds no factor; the plan runs uncalibrated")),
        }
    }
    let mut cmd = Command::new(&exe);
    // Forty candidates, so up to sixteen freight lines with distinct producers can be found (ten gave
    // three on seed 20208, thirty six on seed 31337).
    cmd.arg("--panel").arg(g.panel_url()).arg("--limit").arg("40").arg("--params").arg(py_json(&params));
    // With AlgoGlyph present the best rail lines are priced on the real map (track-planning) before
    // the portfolio is chosen, so the bot builds what pays on this terrain; as many lines of each list
    // as the bot takes freight lines (at most two per producer, linecost.rs).
    if std::env::var("MCP_ALGOGLYPH_DIR").map_or(false, |d| !d.is_empty()) {
        cmd.arg("--track").arg(MAX_FREIGHT_LINES.to_string());
    }
    let out = match run(&mut cmd, None, Duration::from_secs(600), None) {
        Ok(r) if r.code == 0 => match serde_json::from_str::<Value>(&r.stdout) {
            Ok(v) => v,
            Err(e) => { g.event("ai", &format!("planner not reachable for the bot: {e}")); return Settings::default(); }
        },
        Ok(_) => json!({}),
        Err(e) => { g.event("ai", &format!("planner not reachable for the bot: {e}")); return Settings::default(); }
    };
    let mut settings = settings_from_plan(&out, &mut |text| g.event("ai", &text));
    // Extra settings for this bot from the run that started the stack (mcp-duel --bot-settings, the
    // duel form): an A/B of a bot feature without a code change. A malformed list is refused whole.
    match extra_settings(&std::env::var("PRISMATIC_BOT_SETTINGS").unwrap_or_default()) {
        Ok(extra) => for (k, v) in extra {
            g.event("ai", &format!("bot setting from the run: {k}={v}"));
            settings.set(&k, json!(v));
        },
        Err(e) => g.event("ai", &format!("bot settings from the run refused: {e}")),
    }
    settings
}

/// "name=value,name=value" with setting names of lower-case letters, digits and _ and whole values.
pub fn extra_settings(text: &str) -> Result<Vec<(String, i64)>, String> {
    text.split(',').map(str::trim).filter(|p| !p.is_empty()).map(|p| {
        let (k, v) = p.split_once('=').ok_or_else(|| format!("{p}: name=value"))?;
        let (k, v) = (k.trim(), v.trim());
        if k.is_empty() || !k.chars().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '_') {
            return Err(format!("{k}: a setting name (a-z, 0-9, _)"));
        }
        Ok((k.to_string(), v.parse::<i64>().map_err(|_| format!("{k}: a whole number, not {v}"))?))
    }).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    const INFO: &str = r#"class X extends AIInfo {
	function GetName()        { return "PrismaticAI"; }
	function GetSettings() {
		AddSetting({
			name = "aggressiveness",
			description = "How aggressively (0 = passive, 100 = maximal), e.g. 'aggressiveness=75'.",
			min_value = 0,
			max_value = 100,
			custom_value = 50,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "intercity",
			description = "1: connect towns; 0: one town only.",
			min_value = 0, max_value = 5, custom_value = 1, step_size = 0,
			flags = CONFIG_INGAME | CONFIG_BOOLEAN,
		});
		AddLabels("intercity", {_0 = "off", _1 = "on"});
		AddSetting({name = "reserve", description = "Cash", min_value = 0, max_value = 500000, custom_value = 20000, step_size = 5000, flags = 0});
	}
}"#;

    #[test]
    fn settings_are_read_from_info_nut() {
        assert_eq!(info_name(INFO).as_deref(), Some("PrismaticAI"));
        let s = info_settings(INFO);
        assert_eq!(s.len(), 3);
        assert_eq!(s[0], json!({"name": "aggressiveness", "description": "How aggressively (0 = passive, 100 = maximal), e.g. 'aggressiveness=75'.",
                                "min": 0, "max": 100, "step": 1, "default": 50, "flags": ["in_game"], "labels": {}, "value": 50,
                                "editable": true, "ingame": true}));
        assert_eq!((s[1]["min"].clone(), s[1]["max"].clone(), s[1]["step"].clone()), (json!(0), json!(1), json!(1)), "a boolean is 0..1");
        assert_eq!(s[1]["flags"], json!(["boolean", "in_game"]));
        assert_eq!(s[1]["labels"], json!({"0": "off", "1": "on"}));
        assert_eq!((s[2]["step"].clone(), s[2]["flags"].clone(), s[2]["ingame"].clone()), (json!(5000), json!([]), json!(false)));
        let real = ai_dir().join("PrismaticAI/info.nut");
        if let Ok(text) = std::fs::read_to_string(real) {
            let s = info_settings(&text);
            assert!(s.iter().all(|s| s["name"].is_string() && s["default"].is_i64()));
            let names: Vec<&str> = s.iter().filter_map(|s| s["name"].as_str()).collect();
            assert_eq!(names.iter().collect::<HashSet<_>>().len(), names.len(), "every name once");
            assert!(!names.contains(&"fr"), "no bare name from a loop");
        }
    }

    #[test]
    fn settings_declared_in_a_loop_are_read_by_name() {
        let text = r#"
		AddSetting({name = "fr1_from", description = "line 1", min_value = -1, max_value = 9, custom_value = -1, flags = 0});
		for (local i = 2; i <= 3; i++) {
			AddSetting({
				name = "fr" + i + "_from",
				description = "Producing industry of line " + i + " (id; -1: none).",
				min_value = -1, max_value = 65535, custom_value = -1, flags = CONFIG_INGAME,
			});
		}
		for (local n = 0; n < 2; n++) { AddSetting({name = "x" + n, custom_value = 0, flags = 0}); }
		AddSetting({name = "y" + unknown, custom_value = 0, flags = 0});"#;
        let s = info_settings(text);
        let names: Vec<&str> = s.iter().filter_map(|s| s["name"].as_str()).collect();
        assert_eq!(names, vec!["fr1_from", "fr2_from", "fr3_from", "x0", "x1"], "unrolled; an unresolvable name is skipped");
        assert_eq!(s[2]["description"], json!("Producing industry of line 3 (id; -1: none)."));
        assert_eq!((s[1]["min"].clone(), s[1]["max"].clone(), s[1]["ingame"].clone()), (json!(-1), json!(65535), json!(true)));
        assert_eq!(unroll_loops("for (local i = 0; i < n; i++) { f(i); }"), "for (local i = 0; i < n; i++) { f(i); }", "another shape stays");
    }

    #[test]
    fn a_loop_survives_comments_escapes_nesting_and_huge_bounds() {
        let text = r#"
		for (local i = 1; i <= 2; i++) { // a comment with a } and a " in it
			/* and { another " } */
			AddSetting({name = "a" + i, description = "say \"hi\" {" + i + "}", custom_value = 0, flags = 0});
			for (local j = 0; j < 2; j++) { AddSetting({name = "b" + i + "_" + j, custom_value = 0, flags = 0}); }
		}
		for (local k = -2; k <= 9223372036854775807; k++) { AddSetting({name = "huge" + k, custom_value = 0, flags = 0}); }
		for (local k = -9223372036854775808; k < -9223372036854775808; k++) { AddSetting({name = "min" + k, custom_value = 0, flags = 0}); }"#;
        let s = info_settings(text);
        let names: Vec<&str> = s.iter().filter_map(|s| s["name"].as_str()).collect();
        assert_eq!(names, vec!["a1", "b1_0", "b1_1", "a2", "b2_0", "b2_1"], "comments ignored, nested unrolled, huge loops left alone");
        assert_eq!(s[0]["description"], json!(r#"say \"hi\" {1}"#), "escapes kept as written");
        assert_eq!(strip_comments("x = \"// not a comment\"; // gone\ny"), "x = \"// not a comment\";  \ny");
    }

    #[test]
    fn extra_bot_settings_are_parsed_or_refused_whole() {
        assert_eq!(extra_settings("own_lines=8, line_budget=50000").unwrap(), vec![("own_lines".to_string(), 8), ("line_budget".to_string(), 50000)]);
        assert!(extra_settings("").unwrap().is_empty());
        assert!(extra_settings("own_lines").is_err());
        assert!(extra_settings("Own=1").is_err());
        assert!(extra_settings("own_lines=eight").is_err());
    }

    #[test]
    fn the_bot_gets_the_portfolio_with_rail_it_can_build() {
        let rail = |a: i64, score: f64, status: &str| json!({"kind": "passengers", "mode": "rail", "score": score,
            "a": {"id": a, "name": format!("T{a}")}, "b": {"id": a + 1, "name": "B"},
            "options": [{"mode": "rail", "track": {"status": status}}]});
        let fr = |p: i64, score: f64| json!({"kind": "freight", "mode": "freight_rail", "score": score, "cargo": "Coal", "cargo_id": 1,
            "distance": 40, "producer": {"id": format!("industry:{p}"), "name": "Mine"}, "acceptor": {"id": "industry:99", "name": "Plant"},
            "options": [{"mode": "freight_rail", "monthly_profit": 500, "track": {"status": "found"}}]});
        let air = json!({"kind": "passengers", "mode": "air", "score": 3.0, "a": {"id": 4, "name": "A"}, "b": {"id": 5, "name": "B"},
                         "options": [{"mode": "air", "vehicles": {"count": 12}}]});
        let out = json!({"portfolio": {"solver": "knapsack-dp", "chosen": [rail(1, 9.0, "no_route"), fr(7, 8.0), air.clone()], "total_score": 20,
                                       "investment_k": 250, "budget_k": 300},
                         "line_costs": {"priced": [{"status": "found", "pieces": 30, "money": 12345.6}, {"status": "no_route"}]},
                         "freight": [fr(7, 8.0), fr(8, 5.0), fr(7, 4.0)], "by_mode": {"rail": [rail(1, 9.0, "no_route"), rail(10, 2.0, "found")]}});
        let mut events = Vec::new();
        let s = settings_from_plan(&out, &mut |e| events.push(e));
        let flat: Vec<String> = s.iter().map(|(k, v)| format!("{k}={}", py_str(Some(v)))).collect();
        // Freight lines are set after the loop, in order of score; the passenger pair (score 2) waits
        // for both freight lines (8 and 5), so rail_after is 2.
        assert_eq!(flat, ["air_a=4", "air_b=5", "air_planes=8", "rail_a=10", "rail_b=11", "fr_from=7", "fr_to=99", "fr_cargo=1",
                          "fr2_from=8", "fr2_to=99", "fr2_cargo=1", "own_lines=8", "rail_after=2"]);
        assert!(events.iter().any(|e| e == "planner: the passenger rail pair (score 2) is built after 2 freight line(s) that score more"));
        // A spare that scores above a portfolio line is built first.
        let order = json!({"portfolio": {"chosen": [fr(7, 3.0), rail(10, 4.0, "found")]}, "freight": [fr(7, 3.0), fr(8, 6.0)]});
        let s2 = settings_from_plan(&order, &mut |_| {});
        let get = |k: &str| s2.iter().find(|(n, _)| n.as_str() == k).map(|(_, v)| py_str(Some(v)));
        assert_eq!(get("fr_from").as_deref(), Some("8"));
        assert_eq!(get("fr2_from").as_deref(), Some("7"));
        assert_eq!(get("rail_after").as_deref(), Some("1"), "only the line scoring 6 is above the pair's 4");
        assert_eq!(events[0], "planner portfolio by knapsack-dp: 3 investment(s), total score 20, 250k of 300k");
        assert_eq!(events[1], "planner: 2 rail line(s) priced on the map by track-planning, 1 with a route: 30 pieces 12346; 1 without a route dropped their rail option");
        assert!(events[2].starts_with("planner: the rail line T1 - B has no route on the map (no_route)"));
        assert!(events.iter().any(|e| e.ends_with("(line 2, built when the money allows)")));
        let none = settings_from_plan(&json!({}), &mut |_| {});
        assert!(none.0.is_empty());
    }
}
