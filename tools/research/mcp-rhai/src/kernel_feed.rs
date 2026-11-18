//! RESEARCH-ONLY / UNSTABLE: AlgoGlyph kernel inputs assembled from the panel's recorded history
//! (I0086, ADR 0034 point 4: "AlgoGlyph's kernels receive trailing data assembled from the store by
//! their caller").
//!
//! The split (decided 2026-11-18, research/mcp-control.md "AlgoGlyph fed from the store"): AlgoGlyph
//! stays a generic kernel runner behind its `flow serve` line protocol and learns nothing of OpenTTD,
//! ZeroMQ or DuckDB; this side knows the game, reads the store through the panel's read-only
//! `/api/db/query`, follows the ZeroMQ bus, and builds each kernel's input.
//!
//! A trailing window of the `samples` table (one row per company per game day, ADR 0033) is seeded
//! from the store with one statement (`seed_sql`) and kept current by the bus's `game.sample`
//! envelopes; `game.new_world` empties it. Which kernel takes what from the window is one table,
//! `FEEDS`; a kernel with no row there gets the caller's input unchanged and the event says
//! `fallback`. Every call is described by one `plan.kernel` event (`event`): {decision, version,
//! kernel, inputs: {live, stored | fallback}, choice}.
use serde_json::{json, Value};
use std::collections::{BTreeMap, VecDeque};

/// The version named in every event: this module's revision of the feeding rules.
pub const VERSION: &str = "kernel-feed/1";

/// One company's books on one game day: a row of `samples`.
#[derive(Clone, Debug, PartialEq)]
pub struct Sample {
    pub company: i64,
    pub game_tick: i64,
    pub game_date: String,
    pub money: f64,
    pub loan: f64,
    pub value: f64,
    pub net: f64,
    pub vehicles: i64,
}

/// Where a sample came from.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Source { Store, Bus }

/// The newest `days` samples of every company, in game-tick order.
#[derive(Debug)]
pub struct Window {
    pub days: usize,
    pub by_company: BTreeMap<i64, VecDeque<Sample>>,
    pub from_store: usize,
    pub from_bus: usize,
    /// The statement the window was seeded with, or why it was not.
    pub seed: Result<String, String>,
}

impl Window {
    pub fn new(days: usize) -> Window {
        Window { days: days.max(1), by_company: BTreeMap::new(), from_store: 0, from_bus: 0, seed: Err("not seeded".into()) }
    }

    /// Add a sample; a day already held is replaced, an older day than the newest is dropped, and a
    /// day far behind the newest (a new game whose new_world was missed) starts the company afresh.
    pub fn push(&mut self, s: Sample, src: Source) {
        let days = self.days;
        let q = self.by_company.entry(s.company).or_default();
        if let Some(last) = q.back() {
            if s.game_tick == last.game_tick { q.pop_back(); }
            else if s.game_tick < last.game_tick {
                // A tick far behind the newest is a new game; a slightly older one is a late duplicate.
                if last.game_tick - s.game_tick > 74 * 30 { q.clear(); } else { return; }
            }
        }
        q.push_back(s);
        while q.len() > days { q.pop_front(); }
        match src { Source::Store => self.from_store += 1, Source::Bus => self.from_bus += 1 }
    }

    /// Apply one bus envelope (prismatic-bus/v1): game.sample adds a day, game.new_world empties.
    pub fn apply(&mut self, env: &Value) {
        match env["kind"].as_str() {
            Some("game.new_world") => self.by_company.clear(),
            Some("game.sample") => for s in samples_of_event(&env["data"]) { self.push(s, Source::Bus); },
            _ => {}
        }
    }

    pub fn rows(&self) -> usize { self.by_company.values().map(VecDeque::len).sum() }

    /// The company a request means: the one asked for, else the lowest id held.
    pub fn company(&self, want: Option<i64>) -> Option<i64> {
        match want { Some(c) => self.by_company.contains_key(&c).then_some(c), None => self.by_company.keys().next().copied() }
    }
}

/// The rows of a `game.sample` event's data (`panel.rs:sample_of`), as `panel_store.rs:sample_rows`
/// stores them: per company id, money, loan, value, net and fleet.
pub fn samples_of_event(d: &Value) -> Vec<Sample> {
    let n = |c: &Value, k: &str| c.get(k).and_then(Value::as_f64).unwrap_or(0.0);
    let (Some(t), Some(date)) = (d.get("t").and_then(Value::as_i64), d.get("date").and_then(Value::as_str)) else { return vec![] };
    d["companies"].as_array().into_iter().flatten().filter_map(|c| Some(Sample {
        company: c.get("id")?.as_i64()?, game_tick: t, game_date: date.to_string(),
        money: n(c, "money"), loan: n(c, "loan"), value: n(c, "value"), net: n(c, "net"),
        vehicles: c.get("fleet").and_then(Value::as_i64).unwrap_or(0),
    })).collect()
}

/// An instance name as the registry writes it (letters, digits, `-`, `_`, `.`), so it can stand in a
/// quoted literal of the read-only statement.
pub fn valid_instance(i: &str) -> bool {
    !i.is_empty() && i.len() <= 64 && i.chars().all(|c| c.is_ascii_alphanumeric() || matches!(c, '-' | '_' | '.'))
}

/// The statement that seeds the window: the newest `days` samples of every company of the
/// instance's latest run (a store file keeps every run of its instance, and game ticks restart).
pub fn seed_sql(instance: &str, days: usize) -> Result<String, String> {
    if !valid_instance(instance) { return Err(format!("not an instance name: {instance:?}")); }
    Ok(format!(
        "SELECT company, game_tick, game_date, money, loan, value, net, vehicles FROM samples \
         WHERE instance = '{instance}' AND company IS NOT NULL AND game_tick IS NOT NULL \
         AND ts_ms >= (SELECT coalesce(max(started_ms), 0) FROM runs WHERE instance = '{instance}') \
         QUALIFY row_number() OVER (PARTITION BY company ORDER BY game_tick DESC) <= {days} \
         ORDER BY company, game_tick"))
}

/// The samples of a `/api/db/query` answer {columns, rows, count, capped}: rows are lists in the
/// order of `columns`, so each field is found by its column's name.
pub fn samples_of_answer(a: &Value) -> Result<Vec<Sample>, String> {
    if let Some(e) = a.get("error").filter(|e| !e.is_null()) { return Err(format!("the store refused: {e}")); }
    let cols: Vec<&str> = a["columns"].as_array().ok_or("no columns in the answer")?.iter().filter_map(Value::as_str).collect();
    let at = |name: &str| cols.iter().position(|c| *c == name).ok_or(format!("no column {name}"));
    let (c, t, d, m, l, v, n, h) = (at("company")?, at("game_tick")?, at("game_date")?, at("money")?, at("loan")?, at("value")?, at("net")?, at("vehicles")?);
    let f = |r: &Vec<Value>, i: usize| r.get(i).and_then(Value::as_f64).unwrap_or(0.0);
    a["rows"].as_array().ok_or("no rows in the answer")?.iter().filter_map(Value::as_array).map(|r| Ok(Sample {
        company: r.get(c).and_then(Value::as_i64).ok_or("a row without a company")?,
        game_tick: r.get(t).and_then(Value::as_i64).ok_or("a row without a game tick")?,
        game_date: r.get(d).and_then(Value::as_str).unwrap_or("").to_string(),
        money: f(r, m), loan: f(r, l), value: f(r, v), net: f(r, n), vehicles: r.get(h).and_then(Value::as_i64).unwrap_or(0),
    })).collect()
}

/// What a feed took from the window, for the event's `inputs.stored`.
pub type Fed = Result<Value, String>;

/// A kernel's feed: completes the caller's input from the window, or says why it could not.
type Feed = fn(&mut serde_json::Map<String, Value>, &Window, Option<i64>) -> Fed;

/// Which kernel is fed what from recorded history. A kernel not listed gets its input as given.
pub const FEEDS: &[(&str, Feed)] = &[("action-plan", books)];

/// action-plan's books: `cash` and `loan` (whichever the caller left out) from the company's newest
/// stored day, instead of a console `companies` line; the trailing days travel in the event only,
/// since the kernel refuses keys it does not declare.
fn books(input: &mut serde_json::Map<String, Value>, w: &Window, want: Option<i64>) -> Fed {
    if input.contains_key("cash") && input.contains_key("loan") {
        return Err("the caller gave cash and loan".into());
    }
    let company = w.company(want).ok_or_else(|| match want {
        Some(c) => format!("no stored day of company {c}"),
        None => "no stored day of any company".into(),
    })?;
    let q = &w.by_company[&company];
    let last = q.back().ok_or("an empty company window")?;
    let mut took = Vec::new();
    if !input.contains_key("cash") { input.insert("cash".into(), json!(last.money)); took.push("cash"); }
    if !input.contains_key("loan") { input.insert("loan".into(), json!(last.loan)); took.push("loan"); }
    let cash: Vec<f64> = q.iter().map(|s| s.money).collect();
    let first = q.front().unwrap_or(last);
    Ok(json!({"company": company, "took": took, "game_date": last.game_date, "game_tick": last.game_tick,
              "cash": last.money, "loan": last.loan,
              "trailing": {"days": q.len(), "from": first.game_date, "to": last.game_date,
                           "min_cash": cash.iter().copied().fold(f64::INFINITY, f64::min),
                           "max_cash": cash.iter().copied().fold(f64::NEG_INFINITY, f64::max),
                           "net_change": last.net - first.net}}))
}

/// The kernel's input for a request {kernel, input?, company?}, and what was taken from the window
/// (Ok) or why nothing was (Err: the fallback the event names).
pub fn feed(kernel: &str, input: &Value, company: Option<i64>, w: &Window, store_on: bool) -> (Value, Fed) {
    let mut m = input.as_object().cloned().unwrap_or_default();
    let Some((_, f)) = FEEDS.iter().find(|(k, _)| *k == kernel) else {
        return (Value::Object(m), Err(format!("no stored input is defined for {kernel} yet (FEEDS in src/kernel_feed.rs)")));
    };
    if !store_on { return (Value::Object(m), Err("the store is switched off for this feeder (--no-store)".into())); }
    let fed = f(&mut m, w, company);
    (Value::Object(m), fed)
}

/// The `plan.kernel` event of one call (ADR 0034 point 3: {decision, version, inputs, choice}).
pub fn event(kernel: &str, kernel_input: &Value, fed: &Fed, w: &Window, live: Value, answer: &Value, runner: Value) -> Value {
    let stored = match fed {
        Ok(v) => json!({"query": w.seed.as_ref().ok(), "rows": w.rows(), "from_store": w.from_store, "from_bus": w.from_bus, "used": v}),
        Err(why) => json!({"fallback": why, "query": w.seed.as_ref().ok(), "seed_error": w.seed.as_ref().err(), "rows": w.rows()}),
    };
    json!({"kind": "plan.kernel", "data": {
        "decision": "algoglyph.kernel", "version": VERSION, "kernel": kernel,
        "inputs": {"live": live, "stored": stored, "kernel_input": kernel_input},
        "runner": runner, "choice": answer}})
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Store rows as /api/db/query answers them (the live panel's columns, 2026-11-18).
    fn answer() -> Value {
        json!({"capped": false, "count": 4,
               "columns": ["company", "game_tick", "game_date", "money", "loan", "value", "net", "vehicles"],
               "rows": [[0, 22376, "1950-10-13", 300000.0, 300000.0, 1.0, 0.0, 0],
                        [0, 22456, "1950-10-14", 298290.0, 300000.0, 790.0, -1710.0, 0],
                        [0, 22536, "1950-10-15", 297300.0, 300000.0, 2300.0, -2700.0, 0],
                        [1, 22536, "1950-10-15", 120000.0, 100000.0, 5000.0, 900.0, 3]]})
    }

    fn window() -> Window {
        let mut w = Window::new(30);
        for s in samples_of_answer(&answer()).unwrap() { w.push(s, Source::Store); }
        w.seed = Ok("SELECT ...".into());
        w
    }

    #[test]
    fn action_plan_books_come_from_the_newest_stored_day() {
        let w = window();
        let (input, fed) = feed("action-plan", &json!({"actions": [{"id": "a", "kind": "repay_loan", "amount": 50000}]}), None, &w, true);
        assert_eq!(input["cash"], 297300.0);
        assert_eq!(input["loan"], 300000.0);
        assert!(input.get("company").is_none() && input.get("trailing").is_none(), "only keys the kernel declares");
        let fed = fed.unwrap();
        assert_eq!(fed["company"], 0);
        assert_eq!(fed["trailing"]["days"], 3);
        assert_eq!(fed["trailing"]["min_cash"], 297300.0);
        assert_eq!(fed["trailing"]["from"], "1950-10-13");
        let e = event("action-plan", &input, &Ok(fed), &w, json!({}), &json!({"ok": true}), json!({}));
        assert_eq!(e["kind"], "plan.kernel");
        assert_eq!(e["data"]["inputs"]["stored"]["rows"], 4);
        assert_eq!(e["data"]["inputs"]["stored"]["from_store"], 4);
    }

    #[test]
    fn a_named_company_is_fed_its_own_books() {
        let (input, fed) = feed("action-plan", &json!({"actions": []}), Some(1), &window(), true);
        assert_eq!((input["cash"].as_f64(), input["loan"].as_f64()), (Some(120000.0), Some(100000.0)));
        assert_eq!(fed.unwrap()["company"], 1);
    }

    #[test]
    fn the_callers_books_win_and_the_event_says_fallback() {
        let (input, fed) = feed("action-plan", &json!({"actions": [], "cash": 5, "loan": 0}), None, &window(), true);
        assert_eq!(input["cash"], 5);
        assert_eq!(fed.unwrap_err(), "the caller gave cash and loan");
    }

    #[test]
    fn without_a_store_or_a_feed_the_input_is_passed_as_given_and_named_fallback() {
        let w = window();
        let (input, fed) = feed("action-plan", &json!({"actions": []}), None, &w, false);
        assert!(input.get("cash").is_none());
        assert!(fed.unwrap_err().contains("--no-store"));
        let (_, fed) = feed("action-plan", &json!({"actions": []}), Some(7), &w, true);
        assert_eq!(fed.unwrap_err(), "no stored day of company 7");
        let (input, fed) = feed("knapsack-dp", &json!({"weights": [1], "values": [2], "capacity": 1}), None, &w, true);
        assert_eq!(input["capacity"], 1);
        assert!(fed.unwrap_err().starts_with("no stored input is defined for knapsack-dp"));
        let e = event("knapsack-dp", &input, &Err("x".into()), &w, json!({}), &json!({}), json!({}));
        assert_eq!(e["data"]["inputs"]["stored"]["fallback"], "x");
    }

    #[test]
    fn the_bus_keeps_the_window_current_and_a_new_world_empties_it() {
        let mut w = window();
        let env = json!({"schema": "prismatic-bus/v1", "kind": "game.sample", "data": {"t": 22616, "date": "1950-10-16",
                         "companies": [{"id": 0, "money": 296000.0, "loan": 290000.0, "value": 2400.0, "net": -2800.0, "fleet": 1}]}});
        w.apply(&env);
        w.apply(&env); // the same day again replaces, never doubles
        assert_eq!(w.by_company[&0].len(), 4);
        assert_eq!(w.from_bus, 2);
        let (input, _) = feed("action-plan", &json!({"actions": []}), Some(0), &w, true);
        assert_eq!(input["loan"], 290000.0);
        // a late day older than the newest is dropped; a far older one is a new game
        w.push(Sample { game_tick: 22500, ..w.by_company[&0][0].clone() }, Source::Bus);
        assert_eq!(w.by_company[&0].back().unwrap().game_tick, 22616);
        w.push(Sample { game_tick: 10, ..w.by_company[&0][0].clone() }, Source::Bus);
        assert_eq!(w.by_company[&0].len(), 1);
        w.apply(&json!({"kind": "game.new_world", "data": {}}));
        assert_eq!(w.rows(), 0);
    }

    #[test]
    fn the_window_keeps_only_its_days() {
        let mut w = Window::new(2);
        for s in samples_of_answer(&answer()).unwrap() { w.push(s, Source::Store); }
        assert_eq!(w.by_company[&0].iter().map(|s| s.game_tick).collect::<Vec<_>>(), vec![22456, 22536]);
    }

    #[test]
    fn the_seed_reads_the_latest_run_of_a_well_formed_instance_only() {
        let q = seed_sql("panel-8800", 30).unwrap();
        assert!(q.starts_with("SELECT ") && q.contains("instance = 'panel-8800'") && q.contains("<= 30"));
        assert!(q.contains("FROM runs"));
        assert!(seed_sql("x'; DROP TABLE samples; --", 30).is_err());
        assert!(samples_of_answer(&json!({"error": "no such table"})).is_err());
    }
}
