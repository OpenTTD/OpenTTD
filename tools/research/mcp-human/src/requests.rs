//! What the owner asks of the bot from inside the game: the rows of the toolbar's own menu
//! (src/prismatic_mcp/prismatic_mcp_menu.cpp) queue requests in openttd://ui/requests, each with
//! the tile under the centre of the main view. The player takes the kinds it knows, does them
//! through the interface like everything else, and answers through openttd.ui.requests.answer,
//! which the game shows as a news item. Typed questions (kind "ask") belong to the relay, not here.

use crate::economy::Service;
use crate::hand::Hand;
use serde_json::{json, Value};

/// The kinds this player answers.
pub const KINDS: [&str; 3] = ["serve_here", "plan_rail", "watch"];

/// One open request of a kind this player answers.
#[derive(Debug, Clone, PartialEq)]
pub struct Request {
    pub id: u64,
    pub kind: String,
    pub at: Option<(usize, usize)>,
}

/// The open requests of our kinds, oldest first, from the resource's data.
pub fn parse(data: &Value) -> Vec<Request> {
    let mut out: Vec<Request> = data.get("requests").and_then(Value::as_array).cloned().unwrap_or_default().iter()
        .filter(|r| r.get("answered") != Some(&Value::Bool(true)))
        .filter_map(|r| {
            let kind = r.get("kind").and_then(Value::as_str)?;
            if !KINDS.contains(&kind) {
                return None;
            }
            let at = match (r.get("x").and_then(Value::as_u64), r.get("y").and_then(Value::as_u64)) {
                (Some(x), Some(y)) => Some((x as usize, y as usize)),
                _ => None,
            };
            Some(Request { id: r.get("id").and_then(Value::as_u64)?, kind: kind.to_string(), at })
        })
        .collect();
    out.sort_by_key(|r| r.id);
    out
}

impl Hand {
    /// The owner's open requests; none when this game has no menu (the resource is absent).
    pub fn owner_requests(&self) -> Vec<Request> {
        self.mcp.read("openttd://ui/requests").map(|d| parse(&d)).unwrap_or_default()
    }

    /// Answer a request: the game records it and shows it to the owner.
    pub fn answer(&self, id: u64, text: &str) -> Result<(), String> {
        self.tell(&format!("answer the owner: {text}"));
        self.mcp.call("openttd.ui.requests.answer", json!({"id": id, "answer": text})).map(|_| ())
    }

    /// The two towns nearest a tile.
    fn towns_near(&self, at: (usize, usize)) -> Result<Option<(Value, Value)>, String> {
        let towns = self.mcp.read("openttd://towns")?;
        let mut list: Vec<Value> = towns.get("towns").and_then(Value::as_array).cloned().unwrap_or_default();
        let d = |t: &Value| {
            let x = t.get("x").and_then(Value::as_i64).unwrap_or(0) - at.0 as i64;
            let y = t.get("y").and_then(Value::as_i64).unwrap_or(0) - at.1 as i64;
            x.abs() + y.abs()
        };
        list.sort_by_key(d);
        Ok(match list.as_slice() {
            [a, b, ..] => Some((a.clone(), b.clone())),
            _ => None,
        })
    }

    /// Do what one request asks, then answer it. A request that fails is answered with why.
    pub fn handle_request(&mut self, r: &Request, services: &mut Vec<Service>) -> Result<(), String> {
        self.say(&format!("the owner asks: {} at {:?}", r.kind, r.at));
        match r.kind.as_str() {
            "serve_here" => {
                let Some(at) = r.at else { return self.answer(r.id, "I need a place: centre the view on the towns first") };
                let Some((a, b)) = self.towns_near(at)? else { return self.answer(r.id, "there are no two towns to connect") };
                let n = |t: &Value| t.get("name").and_then(Value::as_str).unwrap_or("?").to_string();
                match self.serve_towns_at(&a, &b) {
                    Ok((stops, depot)) => {
                        services.push(Service { name: format!("{}-{}", n(&a), n(&b)), depot, stops, buses: 1 });
                        self.answer(r.id, &format!("done: a bus runs between {} and {}", n(&a), n(&b)))
                    }
                    Err(e) => {
                        let _ = self.escape();
                        let _ = self.tidy();
                        self.answer(r.id, &format!("I could not serve {} and {}: {e}", n(&a), n(&b)))
                    }
                }
            }
            "watch" => {
                self.answer(r.id, "watching one of our buses for a while")?;
                self.watch(25)
            }
            "plan_rail" => self.answer(r.id, "not yet: I build bus services so far; rail lines follow AlgoGlyph's track planner"),
            _ => Ok(()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn takes_open_requests_of_its_kinds_oldest_first() {
        let data = json!({"requests": [
            {"id": 3, "kind": "watch", "x": null, "y": null, "answered": false},
            {"id": 1, "kind": "serve_here", "x": 40, "y": 70, "answered": false},
            {"id": 2, "kind": "ask", "text": "why?", "answered": false},
            {"id": 4, "kind": "plan_rail", "x": 1, "y": 2, "answered": true},
        ]});
        let r = parse(&data);
        assert_eq!(r, vec![
            Request { id: 1, kind: "serve_here".into(), at: Some((40, 70)) },
            Request { id: 3, kind: "watch".into(), at: None },
        ]);
        assert!(parse(&json!({})).is_empty());
    }
}
