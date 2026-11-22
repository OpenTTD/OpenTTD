//! RESEARCH-ONLY / UNSTABLE: every event kind a bus carries, declared once (issue I0081, ADR 0034
//! point 3), and the payload every planning decision publishes.
//!
//! `KINDS` is the one table: the panel's kinds (its bus, src/panel*.rs) and the hub's (the events
//! src/hub.rs raises itself and publishes on its own bus, I0084). A panel's `GET /api/bus` lists the
//! panel's rows and the hub's `GET /api/bus` the hub's (`table`); the knowledge note
//! `.ai/repo/knowledge/curated/the-panel-bus-carries-these-event-kinds.md` holds all of them as a
//! region written from it (`markdown`, `mcp-panel --write-event-kinds`). A cargo test fails when a
//! publish call in the panel's sources, or a kind the hub raises, names a kind outside it, when a
//! declared kind has no producer, or when the note's region differs from the table.
//!
//! A planning decision publishes one `plan.*` event beside its `panel.*` sentence, with the data
//! `{decision, version, inputs: {live, stored}, choice}` (`plan`): `version` is the code revision
//! this binary was built from and the digest of the parameters the decision used, `inputs.live`
//! names what was read live, `inputs.stored` the store query with its row count or the fallback taken
//! instead, and `choice` is what was decided, as data. This module depends on nothing else of the
//! panel, so other binaries (mcp-kernel-feed) build the same payload.

use serde_json::{json, Value};

/// Who puts a kind on the bus.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum By {
    /// The panel itself (src/panel*.rs).
    Panel,
    /// Another process, through `POST /api/bus/publish`; only these kinds are accepted there.
    External,
    /// The hub (src/hub.rs), on its own bus and store, never on a panel's (I0084). A kind may be a
    /// panel's and the hub's both (game.month), each row with its own producer and data.
    Hub,
}

impl By {
    pub fn label(self) -> &'static str {
        match self { By::Panel => "panel", By::External => "external", By::Hub => "hub" }
    }

    /// Is a kind of this producer on a panel's bus?
    pub fn on_panel_bus(self) -> bool {
        self != By::Hub
    }
}

/// One declared event kind.
pub struct Kind {
    pub kind: &'static str,
    pub by: By,
    /// The producer: the function that publishes it.
    pub producer: &'static str,
    /// What `data` carries.
    pub data: &'static str,
    /// When it is published.
    pub when: &'static str,
}

const fn k(kind: &'static str, by: By, producer: &'static str, data: &'static str, when: &'static str) -> Kind {
    Kind { kind, by, producer, data, when }
}

/// The payload of every plan.* kind, named once.
const PLAN: &str = "`{decision, version: {revision, params, digest}, inputs: {live, stored: {query, rows} | {fallback}}, choice}`";

/// Every kind a bus carries: the panel's, then the hub's. Order is the documentation's order.
pub const KINDS: &[Kind] = &[
    // Game state, derived by difference between two MCP reads (panel.rs:watch, derive_game_events).
    k("game.observed", By::Panel, "panel.rs:derive_game_events", "`{date, tick, vehicles, companies (count), towns}`", "the first read after the MCP server is up"),
    k("company.observed", By::Panel, "panel.rs:derive_game_events", "the company object of `openttd://companies`", "the first read, once per company"),
    k("game.new_world", By::Panel, "panel.rs:derive_game_events", "`{date, towns}`", "the tick went back"),
    k("game.month", By::Panel, "panel.rs:derive_game_events", "`{date, vehicles, companies: {id: {money, value}}}`", "the date's month changed"),
    k("game.paused", By::Panel, "panel.rs:derive_game_events", "`{date, tick}`", "the paused flag went on"),
    k("game.resumed", By::Panel, "panel.rs:derive_game_events", "`{date, tick}`", "the paused flag went off"),
    k("vehicles.count", By::Panel, "panel.rs:derive_game_events", "`{from, to, date}`", "the fleet total changed"),
    k("company.founded", By::Panel, "panel.rs:derive_game_events", "the company object", "a company id appeared"),
    k("company.removed", By::Panel, "panel.rs:derive_game_events", "the company object", "a company id went"),
    k("company.renamed", By::Panel, "panel.rs:derive_game_events", "`{id, from, to}`", "a company's name changed to a non-empty one"),
    k("decision.recorded", By::Panel, "panel.rs:derive_game_events", "the decision object of `openttd://decisions`", "an MCP decision appeared"),
    k("decision.updated", By::Panel, "panel.rs:derive_game_events", "the decision object of `openttd://decisions`", "an MCP decision changed"),
    k("game.sample", By::Panel, "panel.rs:record_sample", "`{t, date, vehicles, fleet, towns, paused, companies: [{id, name, is_ai, money, value, loan, net, fleet}]}`", "once per new game day (fills the store's `samples`)"),
    // The server console and the GUI client.
    k("ai.log", By::Panel, "panel.rs:read_log", "`{company, level, text}`", "a console line `[script:N] [C] [L] text`, as the server prints it"),
    k("console.line", By::Panel, "panel.rs:read_log", "`{line}`", "every other console line"),
    k("news.shown", By::Panel, "panel.rs:watch_news", "the news item object", "each news item once, read every 2 s from the GUI client"),
    k("ask.answered", By::Panel, "panel.rs:ask_answers", "the question with its answer", "once per answer"),
    // The panel's own sentences (Game::event): prose for the page and GET /api/status, never a contract.
    k("panel.server", By::Panel, "panel.rs:Game::event", "`{text}`", "the server starts, exits, reports its script budget"),
    k("panel.mcp", By::Panel, "panel.rs:Game::event", "`{text}`", "the MCP server comes up; a tool call waits for approval"),
    k("panel.operator", By::Panel, "panel.rs:Game::event", "`{text}`", "the operator approves, switches auto-approve, downloads content, is refused a console line"),
    k("panel.console", By::Panel, "panel.rs:Game::event", "`{text}`", "a console line sent through `POST /api/console`"),
    k("panel.rhai", By::Panel, "panel.rs:Game::event", "`{text}`", "a Rhai script ran"),
    k("panel.view", By::Panel, "panel.rs:Game::event", "`{text}`", "a GUI client attached"),
    k("panel.ai", By::Panel, "panel.rs:Game::event", "`{text}`", "the bot's `planner:` lines, its start, the held start, calibration, Prismatic's directive"),
    k("panel.mesh", By::Panel, "panel.rs:Game::event", "`{text}`", "a plan.directive or plan.directive_outcome was mirrored to the Majordomus mesh as a typed record, or not (src/panel_mesh.rs; only with $MAJORDOMUS_URL and $MAJORDOMUS_SESSION)"),
    k("panel.plan", By::Panel, "panel.rs:Game::event", "`{text}`", "an action plan or an investment was decided"),
    k("panel.intent", By::Panel, "panel.rs:Game::event", "`{text}`", "a sentence was answered (`POST /api/intent`, `~ plan` in the console)"),
    k("panel.duel", By::Panel, "panel_results.rs", "`{text}`", "a duel started from the panel; the README scoreboard was regenerated"),
    // The memory Rhai scripts and services share (rhai_shared.rs, I0091): the audit trail of every write.
    k("rhai.shared", By::Panel, "rhai_shared.rs:h_shared_set, h_shared_delete", "`{op: set|delete, key, rev, writer, value}`", "a script, a service or the operator wrote or deleted a shared key"),
    // Planning decisions as data (ADR 0034 point 3), each beside its panel.* sentence.
    k("plan.portfolio", By::Panel, "panel_bot.rs:planned", PLAN, "the planner's portfolio for the bot was read (the bot start, `POST /api/bots/planned`)"),
    k("plan.bot_settings", By::Panel, "panel_bot.rs:planned", PLAN, "the bot's settings were chosen from that portfolio"),
    k("plan.calibration", By::Panel, "panel.rs:h_calibration_record", PLAN, "a calibration reading was measured (`POST /api/calibration`); the planned bot reads every stored one back (I0082)"),
    k("plan.investment", By::Panel, "panel.rs:h_investment", PLAN, "AlgoGlyph's transport-investment chose what to build (`GET /api/investment`)"),
    k("plan.action", By::Panel, "panel.rs:h_action_plan", PLAN, "AlgoGlyph's action-plan answered (`POST /api/actionplan`)"),
    k("plan.intent", By::Panel, "panel.rs:h_intent, answer_plan", PLAN, "a sentence was resolved to an intent and candidates"),
    k("plan.directive", By::Panel, "panel_decision.rs:pull", PLAN, "Prismatic's directive was pulled: refused by the contract, or each step with its state (applied, pending approval, refused, error) (`POST /api/decision/pull`); `inputs.stored` is the fit it sent (its samples query or bus, rows, model, I0085) or the fallback"),
    k("plan.directive_outcome", By::Panel, "panel.rs:watch, panel_decision.rs:on_decision", PLAN, "a directive that waited (plan.directive outcome pending) has the game's verdict for every step, or waited an hour without one (`expired`); `inputs.live.directive_seq` names its plan.directive"),
    k("plan.kernel", By::External, "kernel_feed.rs:event (mcp-kernel-feed --publish)", PLAN, "an AlgoGlyph kernel answered a request fed from the store (I0086)"),
    k("plan.duel", By::External, "tools/research/mcp-duel:publish_verdict", PLAN, "a duel ended; `choice` is its verdict `{seed, months, held_start_tick, a, b, rival_spread, rival, winner}`, read from the store (I0083)"),
    // The hub's own events (I0084): derived for a game's MCP server that has no panel, every
    // MCP_STATE_EVERY = 10 fan-in cycles (5 s), from two reads of openttd://game/state ...
    k("game.new_world", By::Hub, "hub.rs:derive_state_events", "`{date}`", "an MCP-only instance's tick went back"),
    k("game.month", By::Hub, "hub.rs:derive_state_events", "`{date}`", "an MCP-only instance's month changed"),
    k("game.paused", By::Hub, "hub.rs:derive_state_events", "`{date}`", "an MCP-only instance's paused flag went on"),
    k("game.resumed", By::Hub, "hub.rs:derive_state_events", "`{date}`", "an MCP-only instance's paused flag went off"),
    k("vehicles.count", By::Hub, "hub.rs:derive_state_events", "`{from, to}`", "an MCP-only instance's vehicle count changed"),
    k("companies.count", By::Hub, "hub.rs:derive_state_events", "`{from, to}`", "an MCP-only instance's company count changed"),
    // ... and the audit watch over every instance, every --audit-every seconds (default 60).
    k("audit.duplicate_station", By::Hub, "hub.rs:audit_findings", "a pair of `hub.audit`'s `duplicates`: `{owner, facilities, a, b, distance}`", "a duplicate station pair appeared"),
    k("audit.duplicate_depot", By::Hub, "hub.rs:audit_findings", "a pair of `hub.audit`'s `depots.duplicates`: `{owner, type, a, b, distance, by, reach}`", "a duplicate depot pair appeared"),
    k("audit.over_capacity", By::Hub, "hub.rs:audit_findings", "a route of `hub.audit`'s `capacity.over`: `{a, b, cargo, vehicles, capacity, usage, needed, too_many}`", "a route over capacity appeared (advisory)"),
    k("audit.resolved", By::Hub, "hub.rs:audit_changes", "`{was, key, finding}`: the finding's kind, its key and its data", "a finding the last audit announced went away"),
];

/// The declared kind of a panel's bus, if `kind` is one.
pub fn find(kind: &str) -> Option<&'static Kind> {
    KINDS.iter().find(|k| k.kind == kind && k.by.on_panel_bus())
}

/// Is `kind` declared on a panel's bus?
pub fn declared(kind: &str) -> bool {
    find(kind).is_some()
}

/// The declared kind the hub raises, if `kind` is one.
pub fn hub_kind(kind: &str) -> Option<&'static Kind> {
    KINDS.iter().find(|k| k.kind == kind && k.by == By::Hub)
}

/// The rows of the producers `of`, as `GET /api/bus` answers them.
pub fn table(of: &[By]) -> Value {
    Value::Array(KINDS.iter().filter(|k| of.contains(&k.by)).map(|k| json!({"kind": k.kind, "by": k.by.label(),
                                               "producer": k.producer, "data": k.data, "when": k.when})).collect())
}

/// The kinds on a panel's bus.
pub const PANEL_BUS: &[By] = &[By::Panel, By::External];
/// The kinds on the hub's bus.
pub const HUB_BUS: &[By] = &[By::Hub];

/// Where the knowledge note's region starts and ends.
pub const REGION_BEGIN: &str = "<!-- event-kinds:begin (generated from tools/research/mcp-rhai/src/panel_events.rs KINDS: mcp-panel --write-event-kinds) -->";
pub const REGION_END: &str = "<!-- event-kinds:end -->";

/// The table as the knowledge note's region holds it.
pub fn markdown() -> String {
    let mut s = String::from("| Kind | By | Producer | Data | When |\n|---|---|---|---|---|\n");
    for k in KINDS {
        let by = match k.by {
            By::Panel => "panel",
            By::External => "external (`POST /api/bus/publish`)",
            By::Hub => "hub (its own bus and store)",
        };
        s.push_str(&format!("| `{}` | {by} | `{}` | {} | {} |\n", k.kind, k.producer, k.data.replace('|', "\\|"), k.when.replace('|', "\\|")));
    }
    s
}

/// `text` with its region replaced by `markdown()`; an error when the markers are missing.
pub fn with_region(text: &str) -> Result<String, String> {
    let b = text.find(REGION_BEGIN).ok_or("no event-kinds:begin marker")?;
    let e = text[b..].find(REGION_END).ok_or("no event-kinds:end marker after the begin marker")? + b;
    Ok(format!("{}{REGION_BEGIN}\n{}{}", &text[..b], markdown(), &text[e..]))
}

/// The revision this binary was built from (build.rs; `-dirty` when tools/research/mcp-rhai had
/// uncommitted changes then).
pub const REVISION: &str = env!("MCP_RHAI_REVISION");

/// FNV-1a 64 of the parameters' JSON (serde_json orders object keys, so equal parameters give equal
/// digests).
pub fn digest(params: &Value) -> String {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in params.to_string().bytes() {
        h ^= b as u64;
        h = h.wrapping_mul(0x100000001b3);
    }
    format!("fnv1a64:{h:016x}")
}

/// `version` of a decision: the code revision, the parameters and their digest.
pub fn version(params: Value) -> Value {
    json!({"revision": REVISION, "digest": digest(&params), "params": params})
}

/// What a decision read from recorded history: a store query with its row count, or the fallback it
/// took instead (ADR 0034 point 2: never a silent default).
pub enum Stored {
    /// The calibration reads it (I0082); the decisions of I0083 to I0087 will.
    Query { sql: String, rows: usize },
    Fallback(String),
}

impl Stored {
    pub fn to_json(&self) -> Value {
        match self {
            Stored::Query { sql, rows } => json!({"query": sql, "rows": rows}),
            Stored::Fallback(why) => json!({"fallback": why}),
        }
    }
}

/// The fallback of a decision that reads the live game only, until its issue gives it a query.
pub fn live_only(issue: &str) -> Stored {
    Stored::Fallback(format!("live only: this decision reads no recorded history yet ({issue})"))
}

/// The data of one plan.* event.
pub fn plan(decision: &str, params: Value, live: Value, stored: &Stored, choice: Value) -> Value {
    json!({"decision": decision, "version": version(params), "inputs": {"live": live, "stored": stored.to_json()}, "choice": choice})
}

/// Why an event offered to `POST /api/bus/publish` is refused, if it is.
pub fn refuse_external(kind: &str, data: &Value) -> Option<String> {
    match find(kind) {
        None => return Some(format!("{kind} is not a declared kind (GET /api/bus lists them)")),
        Some(k) if k.by != By::External => return Some(format!("{kind} is published by the panel itself, not through this route")),
        _ => {}
    }
    let missing: Vec<&str> = ["decision", "version", "inputs", "choice"].into_iter().filter(|f| data.get(*f).is_none()).collect();
    if !missing.is_empty() {
        return Some(format!("a {kind} event carries {{decision, version, inputs, choice}}; missing: {}", missing.join(", ")));
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeSet;

    /// The panel's sources: every file whose code publishes on the bus.
    const SOURCES: &[(&str, &str)] = &[
        ("panel.rs", include_str!("panel.rs")),
        ("panel_bot.rs", include_str!("panel_bot.rs")),
        ("panel_decision.rs", include_str!("panel_decision.rs")),
        ("panel_results.rs", include_str!("panel_results.rs")),
        ("panel_store.rs", include_str!("panel_store.rs")),
        ("rhai_shared.rs", include_str!("rhai_shared.rs")),
    ];

    /// The string literal at the start of `s` (after `"`), if `s` starts with one.
    fn literal(s: &str) -> Option<&str> {
        let s = s.strip_prefix('"')?;
        Some(&s[..s.find('"')?])
    }

    /// The code of a source without its test module: tests publish what they like. Only the
    /// module is cut, not the first `#[cfg(test)]`: a function can be gated with one, as
    /// bus_sinks.rs gates open_in_memory (in panel_store.rs until I0084).
    fn code(text: &str) -> &str {
        text.find("#[cfg(test)]\nmod tests").map_or(text, |i| &text[..i])
    }

    /// Every kind the panel's code names in a publish call, and every call that names none.
    fn published() -> (BTreeSet<String>, Vec<String>) {
        let (mut kinds, mut unnamed) = (BTreeSet::new(), Vec::new());
        for (file, text) in SOURCES {
            let text = code(text);
            for (needle, prefix) in [(".publish(", ""), (".event(", "panel."), (".plan(", "")] {
                for (at, _) in text.match_indices(needle) {
                    let arg = text[at + needle.len()..].trim_start();
                    match literal(arg) {
                        Some(kind) => { kinds.insert(format!("{prefix}{kind}")); }
                        None => unnamed.push(format!("{file}: {needle}{}", &arg[..arg.len().min(40)])),
                    }
                }
            }
        }
        // derive_game_events returns its kinds as literals; watch publishes them as they come.
        let panel = code(SOURCES[0].1);
        let at = panel.find("pub fn derive_game_events").expect("derive_game_events");
        let body = &panel[at..at + panel[at..].find("\n}\n").expect("its end")];
        for part in body.split('"').skip(1).step_by(2) {
            if part.contains('.') && part.chars().all(|c| c.is_ascii_lowercase() || c == '.' || c == '_') {
                kinds.insert(part.to_string());
            }
        }
        (kinds, unnamed)
    }

    #[test]
    fn the_scan_reads_code_past_a_gated_function_and_stops_at_the_test_module() {
        let text = "fn a() {}\n#[cfg(test)]\nfn only_in_tests() {}\ng.plan(\"plan.x\", d);\n#[cfg(test)]\nmod tests {\n g.plan(\"plan.y\", d);\n}\n";
        assert!(code(text).contains("plan.x"), "code after a #[cfg(test)] function is scanned");
        assert!(!code(text).contains("plan.y"), "the test module is not");
        assert!(code(include_str!("bus_sinks.rs")).contains("pub fn send(&self, env: Value)"), "a source is scanned past open_in_memory");
    }

    #[test]
    fn the_table_is_unique_and_every_plan_kind_carries_the_plan_payload() {
        // Every kind once per bus: the panel's (panel and external rows) and the hub's.
        let names: Vec<(&str, bool)> = KINDS.iter().map(|k| (k.kind, k.by.on_panel_bus())).collect();
        assert_eq!(names.iter().collect::<BTreeSet<_>>().len(), names.len(), "every kind once per bus");
        for k in KINDS {
            assert!(k.kind.contains('.') && k.kind.chars().all(|c| c.is_ascii_lowercase() || c == '.' || c == '_'), "{}", k.kind);
            assert_eq!(k.kind.starts_with("plan."), k.data == PLAN, "{}: plan.* and only plan.* carry the plan payload", k.kind);
        }
        for want in ["plan.portfolio", "plan.bot_settings", "plan.calibration", "plan.investment", "plan.action", "plan.intent", "plan.directive"] {
            assert!(find(want).is_some_and(|k| k.by == By::Panel), "{want} is the panel's (I0081)");
        }
        let all = [By::Panel, By::External, By::Hub];
        assert_eq!(table(&all).as_array().unwrap().len(), KINDS.len());
        assert!(table(&all).as_array().unwrap().iter().all(|r| r["kind"].is_string() && r["by"].is_string() && r["producer"].is_string()));
        // A panel's bus never lists the hub's rows, and the hub's lists only its own.
        assert!(table(PANEL_BUS).as_array().unwrap().iter().all(|r| r["by"] != "hub"));
        assert!(table(HUB_BUS).as_array().unwrap().iter().all(|r| r["by"] == "hub" && r["producer"].as_str().unwrap().starts_with("hub.rs:")));
        assert!(find("audit.resolved").is_none() && hub_kind("audit.resolved").is_some(), "an audit kind is the hub's only");
        assert_eq!(find("game.month").map(|k| k.by), Some(By::Panel));
        assert_eq!(hub_kind("game.month").map(|k| k.data), Some("`{date}`"), "the hub's game.month is its own row");
    }

    #[test]
    fn no_publish_call_names_a_kind_outside_the_table_and_every_kind_has_its_producer() {
        let (kinds, unnamed) = published();
        // The calls that pass a kind on rather than naming one: watch (derive_game_events' kinds, read
        // above), Game::event (panel.<kind>, its callers read above), Game::plan (its callers, and it
        // refuses a kind outside the table) and POST /api/bus/publish (refuse_external, then Game::plan).
        // Each with how many calls of it there are: Bus::publish is called with a kind passed on by
        // Game::plan and by watch.
        let passed_on = [("panel.rs: .publish(kind, data)", 2), ("panel.rs: .publish(&format!(\"panel.{kind}\")", 1),
                         ("panel.rs: .plan(&kind, data)", 1)];
        for u in &unnamed {
            assert!(passed_on.iter().any(|(p, _)| u.starts_with(p)), "a publish call that names no kind: {u}");
        }
        for (p, n) in passed_on {
            assert_eq!(unnamed.iter().filter(|u| u.starts_with(p)).count(), n, "calls of {p}: {unnamed:#?}");
        }
        let outside: Vec<&String> = kinds.iter().filter(|k| !declared(k)).collect();
        assert!(outside.is_empty(), "published but not declared in KINDS: {outside:?}");
        let silent: Vec<&str> = KINDS.iter().filter(|k| k.by == By::Panel && !kinds.contains(k.kind)).map(|k| k.kind).collect();
        assert!(silent.is_empty(), "declared but no panel code publishes it: {silent:?}");
        assert!(kinds.len() >= 30, "the scan found the calls ({})", kinds.len());
    }

    #[test]
    fn the_knowledge_note_holds_the_table() {
        let note = include_str!("../../../../.ai/repo/knowledge/curated/the-panel-bus-carries-these-event-kinds.md");
        assert_eq!(with_region(note).unwrap(), note,
                   "the note's event-kinds region is stale: run `mcp-panel --write-event-kinds` (tools/research/mcp-rhai/target/release)");
        assert!(with_region("no markers").is_err());
        let md = markdown();
        assert_eq!(md.lines().count(), KINDS.len() + 2);
        assert!(md.contains("| `plan.kernel` | external (`POST /api/bus/publish`) |"));
        assert!(md.contains("| `audit.resolved` | hub (its own bus and store) | `hub.rs:audit_changes` |"));
        assert!(md.contains("| `plan.duel` | external (`POST /api/bus/publish`) | `tools/research/mcp-duel:publish_verdict` |"));
    }

    #[test]
    fn a_plan_event_carries_decision_version_inputs_and_choice() {
        let d = plan("bot.settings", json!({"own_lines": 8, "b": 1}), json!({"planner": "mcp-plan"}),
                     &Stored::Fallback("store off".into()), json!({"rail_after": 2}));
        assert_eq!(d["decision"], "bot.settings");
        assert_eq!(d["version"]["revision"], REVISION);
        assert!(!REVISION.is_empty());
        assert_eq!(d["version"]["params"], json!({"b": 1, "own_lines": 8}));
        assert_eq!(digest(&json!({"b": 1, "own_lines": 8})), digest(&json!({"own_lines": 8, "b": 1})), "key order does not change the digest");
        assert_eq!(d["version"]["digest"], digest(&json!({"b": 1, "own_lines": 8})));
        assert_ne!(digest(&json!({"own_lines": 8})), digest(&json!({"own_lines": 0})));
        // The same digest tools/research/mcp-duel computes in Python (test_mcp_duel.py pins it too).
        assert_eq!(digest(&json!({"own_lines": 8, "b": 1})), "fnv1a64:846dbf03f70f43fa");
        assert_eq!(d["inputs"], json!({"live": {"planner": "mcp-plan"}, "stored": {"fallback": "store off"}}));
        assert_eq!(d["choice"], json!({"rail_after": 2}));
        assert_eq!(Stored::Query { sql: "SELECT 1".into(), rows: 3 }.to_json(), json!({"query": "SELECT 1", "rows": 3}));
        assert!(live_only("I0082").to_json()["fallback"].as_str().unwrap().contains("I0082"));
    }

    #[test]
    fn only_a_declared_external_kind_with_the_plan_payload_enters_from_outside() {
        let good = plan("algoglyph.kernel", json!({}), json!({}), &Stored::Fallback("x".into()), json!({}));
        assert_eq!(refuse_external("plan.kernel", &good), None);
        assert_eq!(refuse_external("plan.duel", &good), None, "mcp-duel's verdict (I0083)");
        assert!(refuse_external("plan.kernel", &json!({"decision": "x"})).unwrap().contains("missing: version, inputs, choice"));
        assert!(refuse_external("plan.portfolio", &good).unwrap().contains("by the panel itself"));
        assert!(refuse_external("game.month", &good).is_some());
        assert!(refuse_external("plan.whatever", &good).unwrap().contains("not a declared kind"));
        assert!(refuse_external("audit.resolved", &good).unwrap().contains("not a declared kind"), "the hub's kinds are not on a panel's bus");
    }
}
