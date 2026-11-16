//! Keeping the fleet young and paying, the way a person does it: open the company's vehicle list
//! from the toolbar, open the vehicle, send it to its depot, and there either buy a copy of it
//! (the copy takes the old one's orders) and start the copy, or not, and drag the old vehicle onto
//! the depot's sell button. Which vehicles is read from openttd://vehicles (age, max_age,
//! profits); where they are on screen from the depot and list windows' own hit tests
//! (openttd.ui.windows `vehicles`).

use crate::hand::{Hand, Target};
use serde_json::{json, Value};
use std::thread::sleep;
use std::time::{Duration, Instant};

/// What to do with a vehicle that no longer belongs on the road as it is.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Fate {
    /// Old: buy a copy in its depot, start the copy, sell the old one.
    Renew,
    /// Losing money for two years: sell it and buy nothing.
    Sell,
}

/// When a vehicle counts as worn out or unprofitable.
#[derive(Debug, Clone, Copy)]
pub struct Rules {
    /// Renew at this age in days; None renews at the vehicle's own max_age (when the game calls it old).
    pub renew_age: Option<u64>,
    /// Sell a vehicle that lost money both last year and so far this year, once it is this old (days).
    pub sell_after: u64,
}

impl Default for Rules {
    fn default() -> Self {
        Rules { renew_age: None, sell_after: 730 }
    }
}

/// The company's vehicles that need a visit to the depot, oldest first, with the reason.
pub fn due(vehicles: &[Value], company: u64, rules: Rules) -> Vec<(u64, Fate, String)> {
    let mut out: Vec<(u64, u64, Fate, String)> = Vec::new();
    for v in vehicles {
        if v.get("owner").and_then(Value::as_u64) != Some(company) || v.get("crashed") == Some(&Value::Bool(true)) {
            continue;
        }
        let (Some(id), Some(age)) = (v.get("id").and_then(Value::as_u64), v.get("age").and_then(Value::as_u64)) else { continue };
        let max_age = v.get("max_age").and_then(Value::as_u64).unwrap_or(u64::MAX);
        let last = v.get("profit_last_year").and_then(Value::as_i64).unwrap_or(0);
        let this = v.get("profit_this_year").and_then(Value::as_i64).unwrap_or(0);
        let limit = rules.renew_age.unwrap_or(max_age);
        if age >= rules.sell_after && last < 0 && this < 0 {
            out.push((id, age, Fate::Sell, format!("lost {} last year and {} this year", -last, -this)));
        } else if age >= limit {
            out.push((id, age, Fate::Renew, format!("{age} days old (renewed at {limit})")));
        }
    }
    out.sort_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(&b.0)));
    out.into_iter().map(|(id, _, f, why)| (id, f, why)).collect()
}

/// The toolbar button that lists the company's vehicles of a type, as its tooltip says.
fn list_button(kind: &str) -> Result<&'static str, String> {
    match kind {
        "road" => Ok("open list of company's road vehicles"),
        "train" => Ok("open list of company's trains"),
        "ship" => Ok("open list of company's ships"),
        "aircraft" => Ok("open list of company's aircraft"),
        _ => Err(format!("no vehicle list for '{kind}'")),
    }
}

/// The depot button a vehicle is dragged to: a train is sold whole from the sell-chain button.
fn sell_button(kind: &str) -> &'static str {
    if kind == "train" { "sell the whole train" } else { "to here to sell it" }
}

const SEND_TO_DEPOT: &str = "to depot. ctrl+click to only service";
const CLONE: &str = "buy a copy of the";

/// The cell a window draws a vehicle in, for one action ("open", "drag", "start_stop").
fn cell(w: &Value, vehicle: u64, action: &str) -> Option<Target> {
    w.get("vehicles").and_then(Value::as_array)?.iter()
        .find(|c| c.get("vehicle").and_then(Value::as_u64) == Some(vehicle) && c.get("action").and_then(Value::as_str) == Some(action))
        .and_then(|c| c.get("rect")).and_then(Target::from_rect)
}

impl Hand {
    fn local_company(&self) -> Result<u64, String> {
        self.state()?.get("local_company").and_then(Value::as_u64).ok_or_else(|| "this client has no company".into())
    }

    /// The window of one vehicle (its view, whose window number is the vehicle id), if open.
    fn vehicle_view(&self, id: u64) -> Result<Option<Value>, String> {
        Ok(self.windows()?.into_iter().find(|w| {
            w.get("key").and_then(Value::as_str).is_some_and(|k| k.starts_with("view_vehicle") && !k.contains("details") && !k.contains("refit"))
                && w.get("number").and_then(Value::as_u64) == Some(id)
        }))
    }

    fn wait_vehicle_view(&self, id: u64) -> Result<Value, String> {
        let started = Instant::now();
        loop {
            if let Some(w) = self.vehicle_view(id)? {
                return Ok(w);
            }
            if started.elapsed() > Duration::from_secs(4) {
                return Err(format!("the window of vehicle {id} did not open"));
            }
            sleep(Duration::from_millis(120));
        }
    }

    /// Open a vehicle's window from the company's vehicle list (toolbar, then its row).
    pub fn open_vehicle(&mut self, id: u64) -> Result<Value, String> {
        if let Some(w) = self.vehicle_view(id)? {
            return Ok(w);
        }
        let kind = self.vehicle(id)?.get("type").and_then(Value::as_str).unwrap_or("").to_string();
        let me = self.local_company()? as i64;
        self.say(&format!("open the list of the company's {kind} vehicles"));
        self.menu_pick(list_button(&kind)?, |items| items.iter().position(|i| i.get("result").and_then(Value::as_i64) == Some(me)))?;
        let started = Instant::now();
        let (list, row) = loop {
            if let Some((w, t)) = self.windows()?.into_iter().find_map(|w| cell(&w, id, "open").map(|t| (w, t))) {
                break (w, t);
            }
            if started.elapsed() > Duration::from_secs(4) {
                return Err(format!("vehicle {id} is not in the list (scrolled out, or the list did not open)"));
            }
            sleep(Duration::from_millis(120));
        };
        self.say(&format!("click vehicle {id} in the list"));
        self.raise_if_covered(&list, row)?;
        self.click(row)?;
        let view = self.wait_vehicle_view(id)?;
        self.close(&list)?;
        Ok(view)
    }

    /// Send a vehicle to its depot from its window and wait until it stands stopped inside.
    pub fn send_to_depot(&mut self, id: u64, patience: Duration) -> Result<(usize, usize), String> {
        let v = self.vehicle(id)?;
        if v.get("in_depot") != Some(&Value::Bool(true)) {
            let view = self.open_vehicle(id)?;
            self.say(&format!("send vehicle {id} to its depot"));
            self.click_in(&view, SEND_TO_DEPOT)?;
            let started = Instant::now();
            let mut last_note = Instant::now();
            loop {
                let v = self.vehicle(id)?;
                if v.get("in_depot") == Some(&Value::Bool(true)) {
                    break;
                }
                if started.elapsed() > patience {
                    return Err(format!("vehicle {id} did not reach a depot in {}s", patience.as_secs()));
                }
                if last_note.elapsed() > Duration::from_secs(20) {
                    self.say(&format!("vehicle {id} is on its way to the depot"));
                    last_note = Instant::now();
                }
                sleep(Duration::from_millis(500));
            }
        }
        let v = self.vehicle(id)?;
        let at = |k: &str| v.get(k).and_then(Value::as_u64).map(|n| n as usize).ok_or_else(|| format!("vehicle {id} has no {k}"));
        Ok((at("x")?, at("y")?))
    }

    /// In the vehicle's window, buy a copy of it (orders copied), start the copy; its id.
    fn clone_and_start(&mut self, id: u64) -> Result<u64, String> {
        let view = self.open_vehicle(id)?;
        let before = self.own_vehicles()?;
        self.say(&format!("buy a copy of vehicle {id}"));
        self.click_in(&view, CLONE)?;
        let started = Instant::now();
        let copy = loop {
            if let Some(n) = self.own_vehicles()?.into_iter().find(|n| !before.contains(n)) {
                break n;
            }
            if started.elapsed() > Duration::from_secs(5) {
                return Err(self.game_error().ok().flatten().map(|e| format!("the copy was not bought: {e}")).unwrap_or_else(|| "the copy was not bought".into()));
            }
            sleep(Duration::from_millis(150));
        };
        let view = self.wait_vehicle_view(copy)?;
        self.say(&format!("start the copy, vehicle {copy}"));
        self.click_in(&view, crate::play::START_STOP)?;
        let started = Instant::now();
        while self.vehicle(copy)?.get("stopped") == Some(&Value::Bool(true)) {
            if started.elapsed() > Duration::from_secs(4) {
                return Err(format!("the copy {copy} did not start"));
            }
            sleep(Duration::from_millis(150));
        }
        if let Some(w) = self.vehicle_view(copy)? {
            self.close(&w)?;
        }
        Ok(copy)
    }

    /// Open the depot a vehicle stands in and drag the vehicle onto its sell button.
    fn sell_in_depot(&mut self, id: u64, depot: (usize, usize)) -> Result<(), String> {
        let kind = self.vehicle(id)?.get("type").and_then(Value::as_str).unwrap_or("").to_string();
        if let Some(w) = self.vehicle_view(id)? {
            self.close(&w)?;
        }
        let w = self.open_depot(depot)?;
        let started = Instant::now();
        let (w, grab) = loop {
            let w = self.window("depot")?.unwrap_or_else(|| w.clone());
            if let Some(t) = cell(&w, id, "drag") {
                break (w, t);
            }
            if started.elapsed() > Duration::from_secs(4) {
                return Err(format!("vehicle {id} is not drawn in the depot at {},{}", depot.0, depot.1));
            }
            sleep(Duration::from_millis(120));
        };
        let sell = Self::widget_saying(&w, sell_button(&kind)).and_then(|wd| wd.get("rect").and_then(Target::from_rect))
            .ok_or("the depot has no sell button")?;
        self.raise_if_covered(&w, grab)?;
        self.say(&format!("drag vehicle {id} onto the sell button"));
        self.drag(grab, sell)?;
        let started = Instant::now();
        while self.own_vehicles()?.contains(&id) {
            if started.elapsed() > Duration::from_secs(4) {
                return Err(self.game_error().ok().flatten().map(|e| format!("vehicle {id} was not sold: {e}")).unwrap_or_else(|| format!("vehicle {id} was not sold")));
            }
            sleep(Duration::from_millis(150));
        }
        if let Some(w) = self.window("depot")? {
            self.close(&w)?;
        }
        Ok(())
    }

    /// Renew or sell one vehicle through the interface. Answers the copy's id when renewed.
    pub fn retire(&mut self, id: u64, fate: Fate) -> Result<Option<u64>, String> {
        let orders = self.vehicle(id)?.get("orders").and_then(Value::as_array).map(Vec::len).unwrap_or(0);
        let depot = self.send_to_depot(id, Duration::from_secs(240))?;
        let copy = match fate {
            Fate::Renew => Some(self.clone_and_start(id)?),
            Fate::Sell => None,
        };
        self.sell_in_depot(id, depot)?;
        if let Some(c) = copy {
            let got = self.vehicle(c)?.get("orders").and_then(Value::as_array).map(Vec::len).unwrap_or(0);
            if got != orders {
                return Err(format!("the copy {c} has {got} orders, the old vehicle had {orders}"));
            }
        }
        self.tell(&match copy {
            Some(c) => format!("vehicle {id} renewed: the copy {c} runs its {orders} orders"),
            None => format!("vehicle {id} sold"),
        });
        let _ = self.tidy();
        Ok(copy)
    }

    /// The vehicles the rules say are due, for this client's company.
    pub fn due_vehicles(&self, rules: Rules) -> Result<Vec<(u64, Fate, String)>, String> {
        let me = self.local_company()?;
        let v = self.mcp.read("openttd://vehicles")?;
        Ok(due(v.get("vehicles").and_then(Value::as_array).map(Vec::as_slice).unwrap_or(&[]), me, rules))
    }
}

/// A plan step's own record of a retirement, for the e2e and the action log.
pub fn report(id: u64, fate: Fate, copy: Option<u64>) -> Value {
    json!({"vehicle": id, "fate": match fate { Fate::Renew => "renew", Fate::Sell => "sell" }, "copy": copy})
}

#[cfg(test)]
mod tests {
    use super::*;

    fn v(id: u64, owner: u64, age: u64, max_age: u64, last: i64, this: i64) -> Value {
        json!({"id": id, "owner": owner, "age": age, "max_age": max_age, "profit_last_year": last, "profit_this_year": this, "crashed": false})
    }

    #[test]
    fn old_vehicles_are_renewed_at_their_own_max_age() {
        let list = [v(1, 0, 5000, 5475, 900, 100), v(2, 0, 5475, 5475, 900, 100), v(3, 1, 9000, 5475, 900, 100)];
        let d = due(&list, 0, Rules::default());
        assert_eq!(d.len(), 1, "only company 0's vehicle past its max age: {d:?}");
        assert_eq!((d[0].0, d[0].1), (2, Fate::Renew));
    }

    #[test]
    fn a_vehicle_losing_money_for_two_years_is_sold_not_renewed() {
        let list = [v(4, 0, 6000, 5475, -300, -20), v(5, 0, 400, 5475, -300, -20), v(6, 0, 800, 5475, -300, 50)];
        let d = due(&list, 0, Rules::default());
        assert_eq!(d, vec![(4, Fate::Sell, "lost 300 last year and 20 this year".to_string())], "young or recovering vehicles stay");
    }

    #[test]
    fn a_renewal_age_overrides_max_age_and_the_oldest_comes_first() {
        let list = [v(7, 0, 90, 5475, 0, 0), v(8, 0, 120, 5475, 0, 0), v(9, 0, 30, 5475, 0, 0)];
        let d = due(&list, 0, Rules { renew_age: Some(60), sell_after: 730 });
        assert_eq!(d.iter().map(|x| x.0).collect::<Vec<_>>(), vec![8, 7]);
    }

    #[test]
    fn a_vehicle_without_an_age_is_never_touched() {
        let list = [json!({"id": 10, "owner": 0, "profit_last_year": -5, "profit_this_year": -5})];
        assert!(due(&list, 0, Rules { renew_age: Some(0), sell_after: 0 }).is_empty());
    }

    #[test]
    fn the_list_and_sell_buttons_follow_the_vehicle_type() {
        assert_eq!(list_button("train").unwrap(), "open list of company's trains");
        assert!(list_button("tram").is_err());
        assert_eq!(sell_button("train"), "sell the whole train");
        assert_eq!(sell_button("road"), "to here to sell it");
    }

    #[test]
    fn a_cell_is_found_by_vehicle_and_action() {
        let w = json!({"vehicles": [
            {"vehicle": 3, "action": "open", "rect": {"x": 10, "y": 20, "w": 30, "h": 10, "cx": 25, "cy": 25}},
            {"vehicle": 3, "action": "drag", "rect": {"x": 50, "y": 20, "w": 30, "h": 10, "cx": 65, "cy": 25}}]});
        assert!(cell(&w, 3, "drag").is_some());
        assert!(cell(&w, 4, "drag").is_none());
    }
}
