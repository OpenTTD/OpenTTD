//! Chained actions: a plan first, then its execution. The owner's actions (repay 50,000, serve two
//! towns, add a bus, watch) go to AlgoGlyph's `action-plan` kernel together with the company's books
//! from openttd://companies (cash, loan, max_loan, loan_step). The kernel validates the DAG, refuses a
//! plan that would overdraw or repay a loan already repaid, and expands it into steps in dependency
//! order, one press each, with the cash after every step and a graph to draw. The player shows the
//! plan in the game, then does the steps in order through the interface, as it does everything.
//! Each step's parameters ({a, b} town ids for serve_towns, {seconds} for watch, {max_distance,
//! min_distance} for build_rail_line) come from the
//! step's own `params`, which the kernel copies from its action.

use crate::algoglyph::Flow;
use crate::economy::Service;
use crate::hand::Hand;
use serde_json::{json, Value};

/// The kinds this player executes today; others are refused before anything runs.
pub const EXECUTABLE: [&str; 8] = ["repay_loan", "borrow", "serve_towns", "add_bus", "watch", "build_rail_line", "renew_vehicle", "sell_vehicle"];

/// The kernel's input: the owner's actions and the company's books.
pub fn plan_input(actions: &Value, books: &crate::economy::Books) -> Value {
    json!({"actions": actions, "cash": books.money, "loan": books.loan, "max_loan": books.max_loan, "loan_step": books.loan_step})
}

/// The kernel's steps, refused as a whole when any step is of a kind this player cannot do.
pub fn steps(plan: &Value) -> Result<Vec<Value>, String> {
    let r = plan.get("result").unwrap_or(plan);
    if plan.get("ok") == Some(&Value::Bool(false)) || r.get("ok") == Some(&Value::Bool(false)) {
        return Err(format!("the plan was refused: {}", plan.get("errors").or(r.get("errors")).unwrap_or(plan)));
    }
    let steps = r.get("steps").and_then(Value::as_array).cloned().ok_or("the plan has no steps")?;
    if let Some(s) = steps.iter().find(|s| !EXECUTABLE.contains(&s.get("kind").and_then(Value::as_str).unwrap_or(""))) {
        return Err(format!("this player cannot do a '{}' step yet", s.get("kind").and_then(Value::as_str).unwrap_or("?")));
    }
    Ok(steps)
}

impl Hand {
    /// Plan the owner's actions with the kernel, show the plan, then do its steps in order.
    pub fn execute(&mut self, actions: &Value) -> Result<(), String> {
        let books = self.books()?;
        let mut flow = Flow::from_env().ok_or("set MCP_ALGOGLYPH_DIR to an AlgoGlyph checkout with the action-plan kernel")?;
        let plan = flow.kernel("action-plan", plan_input(actions, &books))?;
        let steps = steps(&plan)?;
        let r = plan.get("result").unwrap_or(&plan);
        let fin = r.get("final").cloned().unwrap_or(Value::Null);
        let summary = format!("Plan: {} step(s) in {} wave(s); cash {} -> {}, loan {} -> {}", steps.len(),
            r.get("waves").and_then(Value::as_array).map(Vec::len).unwrap_or(0), books.money, fin["cash"], books.loan, fin["loan"]);
        self.say(&summary);
        println!("{}", serde_json::to_string(r).unwrap_or_default());
        self.announce(&summary, None);
        let by_id = |id: &str| actions.as_array().and_then(|a| a.iter().find(|x| x.get("id").and_then(Value::as_str) == Some(id)).cloned()).unwrap_or(Value::Null);
        let mut services: Vec<Service> = Vec::new();
        for s in &steps {
            let kind = s.get("kind").and_then(Value::as_str).unwrap_or("");
            // A step carries its action's params (the kernel copies them); older plans name the action only.
            let input = by_id(s.get("action").and_then(Value::as_str).unwrap_or(""));
            let action = match s.get("params") {
                Some(p) if p.is_object() => { let mut m = p.clone(); m["label"] = input.get("label").cloned().unwrap_or(Value::Null); m }
                _ => input.get("params").filter(|p| p.is_object()).cloned().unwrap_or(input),
            };
            self.say(&format!("step {}: {kind} ({})", s.get("n").unwrap_or(&Value::Null), action.get("label").and_then(Value::as_str).unwrap_or("")));
            match kind {
                "repay_loan" => self.repay()?,
                "borrow" => self.borrow()?,
                "watch" => self.watch(action.get("seconds").and_then(Value::as_u64).unwrap_or(20))?,
                "serve_towns" => {
                    let (a, b) = self.towns_by_id(&action)?;
                    let (stops, depot) = self.serve_towns_at(&a, &b)?;
                    services.push(Service { name: format!("{}-{}", a["name"].as_str().unwrap_or("?"), b["name"].as_str().unwrap_or("?")), depot, stops, buses: 1 });
                }
                "build_rail_line" => {
                    // A whole rail service (stations, AlgoGlyph track, depot, train) from the
                    // planner's candidates within {max_distance, min_distance}.
                    let max = action.get("max_distance").and_then(Value::as_f64).unwrap_or(90.0);
                    let min = action.get("min_distance").and_then(Value::as_f64).unwrap_or(0.0);
                    let list: Vec<Value> = self.rail_candidates(max)?.into_iter()
                        .filter(|c| c["distance"].as_f64().unwrap_or(0.0) >= min).take(3).collect();
                    self.first_rail_service(&list)?;
                }
                "renew_vehicle" | "sell_vehicle" => {
                    // Through the depot, as fleet.rs does it: {vehicle} names the vehicle's id.
                    let id = action.get("vehicle").and_then(Value::as_u64).ok_or_else(|| format!("step '{kind}' needs params.vehicle"))?;
                    let fate = if kind == "renew_vehicle" { crate::fleet::Fate::Renew } else { crate::fleet::Fate::Sell };
                    self.retire(id, fate)?;
                }
                "add_bus" => {
                    let s = services.last().cloned().ok_or("add_bus needs a serve_towns step before it in this plan")?;
                    self.buy_bus(s.depot, s.stops)?;
                }
                _ => unreachable!("refused before running"),
            }
        }
        let after = self.books()?;
        self.tell(&format!("plan done: cash {}, loan {}", after.money, after.loan));
        Ok(())
    }

    /// The two towns a serve_towns action names by id ({a, b}).
    fn towns_by_id(&self, action: &Value) -> Result<(Value, Value), String> {
        let towns = self.mcp.read("openttd://towns")?;
        let list = towns.get("towns").and_then(Value::as_array).cloned().unwrap_or_default();
        let get = |k: &str| {
            let id = action.get(k).and_then(Value::as_u64).ok_or_else(|| "serve_towns needs town ids a and b".to_string())?;
            list.iter().find(|t| t.get("id").and_then(Value::as_u64) == Some(id)).cloned().ok_or(format!("no town {id}"))
        };
        Ok((get("a")?, get("b")?))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn takes_the_kernels_steps_and_refuses_what_it_cannot_do() {
        let ok = json!({"ok": true, "result": {"steps": [{"n": 1, "action": "repay", "kind": "repay_loan"}, {"n": 2, "action": "repay", "kind": "repay_loan"}]}});
        assert_eq!(steps(&ok).unwrap().len(), 2);
        let rail = json!({"ok": true, "result": {"steps": [{"n": 1, "action": "r", "kind": "build_rail_piece"}]}});
        assert!(steps(&rail).unwrap_err().contains("build_rail_piece"));
        assert!(steps(&json!({"ok": false, "errors": ["would overdraw"]})).unwrap_err().contains("overdraw"));
    }
}
