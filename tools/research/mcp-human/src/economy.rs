//! Running the company the way a person does over a whole game: serve new town pairs while
//! money allows, add buses to services whose buses earn, borrow through the finances window when
//! cash runs short, and between jobs watch a bus with the camera. Every decision reads published
//! state (openttd://companies, openttd://vehicles); every action goes through the interface.

use crate::hand::Hand;
use serde_json::Value;
use std::time::{Duration, Instant};

/// What the company has built: one entry per town pair it serves.
#[derive(Debug, Clone)]
pub struct Service {
    pub name: String,
    pub depot: (usize, usize),
    pub stops: [(usize, usize); 2],
    pub buses: usize,
}

/// The thresholds a cautious person keeps in mind, in the game's internal currency.
#[derive(Debug, Clone, Copy)]
pub struct Policy {
    /// Below this, borrow before building.
    pub borrow_below: i64,
    /// A new service is started only with at least this much.
    pub expand_above: i64,
    /// A service gets another bus only while it has fewer than this many.
    pub max_buses: usize,
    /// How many times the player borrows at most.
    pub max_borrows: usize,
}

impl Default for Policy {
    fn default() -> Self {
        Policy { borrow_below: 20_000, expand_above: 35_000, max_buses: 3, max_borrows: 3 }
    }
}

const FINANCES: &str = "open company finances information";
const BORROW: &str = "increase size of loan";

impl Hand {
    /// The money of this client's company.
    pub fn money(&self) -> Result<i64, String> {
        let me = self.state()?.get("local_company").and_then(Value::as_u64).unwrap_or(255);
        let c = self.mcp.read("openttd://companies")?;
        c.get("companies").and_then(Value::as_array)
            .and_then(|a| a.iter().find(|x| x.get("id").and_then(Value::as_u64) == Some(me)).cloned())
            .and_then(|x| x.get("money").and_then(Value::as_i64))
            .ok_or_else(|| "this client's company is not listed".to_string())
    }

    /// Borrow once through the finances window: open it, press Borrow, close it.
    pub fn borrow(&mut self) -> Result<(), String> {
        self.escape()?;
        // The finances button opens a menu of companies that closes on release: hold, glide to
        // this company's entry (its value is the company id), let go.
        let me = self.state()?.get("local_company").and_then(Value::as_i64).unwrap_or(0);
        self.menu_pick(FINANCES, |items| {
            items.iter().position(|i| i.get("result").and_then(Value::as_i64) == Some(me)).or(if items.is_empty() { None } else { Some(0) })
        })?;
        self.think(500.0, 900.0);
        let w = self.wait_window("finances")?;
        self.click_in(&w, BORROW)?;
        self.think(400.0, 700.0);
        if let Some(w) = self.window("finances")? {
            self.close(&w)?;
        }
        Ok(())
    }

    /// The earnings of the buses that serve a pair of stops (sum of this year's profits).
    fn service_profit(&self, s: &Service) -> Result<(usize, i64), String> {
        let stations = self.mcp.read("openttd://stations")?;
        let at = |t: (usize, usize)| stations.get("stations").and_then(Value::as_array)
            .and_then(|a| a.iter().find(|x| x.get("x").and_then(Value::as_u64) == Some(t.0 as u64) && x.get("y").and_then(Value::as_u64) == Some(t.1 as u64)).cloned())
            .and_then(|x| x.get("id").and_then(Value::as_u64));
        let ids = [at(s.stops[0]), at(s.stops[1])];
        let v = self.mcp.read("openttd://vehicles")?;
        let mut n = 0;
        let mut profit = 0;
        for x in v.get("vehicles").and_then(Value::as_array).cloned().unwrap_or_default() {
            let dests: Vec<Option<u64>> = x.get("orders").and_then(Value::as_array).cloned().unwrap_or_default().iter()
                .map(|o| o.get("destination").and_then(Value::as_u64)).collect();
            if ids.iter().all(|id| id.is_some() && dests.contains(id)) {
                n += 1;
                profit += x.get("profit_this_year").and_then(Value::as_i64).unwrap_or(0);
            }
        }
        Ok((n, profit))
    }

    /// Watch the game for a while: follow one of the company's buses, as a person does between jobs.
    pub fn watch(&mut self, seconds: u64) -> Result<(), String> {
        let buses = self.own_vehicles()?;
        if !buses.is_empty() {
            let k = (self.rng.unit() * buses.len() as f64) as usize % buses.len();
            self.say(&format!("watch bus {} for {seconds} s", buses[k]));
            let _ = self.mcp.call("openttd.view.follow", serde_json::json!({"vehicle": buses[k]}));
        }
        std::thread::sleep(Duration::from_secs(seconds));
        let _ = self.mcp.call("openttd.view.follow", serde_json::json!({"vehicle": null}));
        // Read what the game reported meanwhile, then clear the news off the screen.
        if let Ok(news) = self.mcp.call("openttd.view.news.list", serde_json::json!({"limit": 3})) {
            let n = news.get("news").and_then(Value::as_array).map(Vec::len).unwrap_or(0);
            if n > 0 {
                self.say(&format!("read {n} news item(s)"));
            }
        }
        let _ = self.mcp.call("openttd.view.news.close", serde_json::json!({}));
        Ok(())
    }

    /// Play the company for `minutes` of real time: expand, reinforce, borrow, watch.
    /// When the owner closes the game window the company stays as it is: that ends the game, it
    /// is no failure, and what was built is still reported.
    pub fn play_game(&mut self, minutes: u64, policy: Policy) -> Result<Vec<Service>, String> {
        let deadline = Instant::now() + Duration::from_secs(minutes * 60);
        let mut services: Vec<Service> = Vec::new();
        match self.play_until(deadline, policy, &mut services) {
            Err(e) if game_closed(&e) => {
                self.say("the game was closed: stop playing");
                Ok(services)
            }
            Err(e) => Err(e),
            Ok(()) => Ok(services),
        }
    }

    fn play_until(&mut self, deadline: Instant, policy: Policy, services: &mut Vec<Service>) -> Result<(), String> {
        let mut tried: Vec<(u64, u64)> = Vec::new();
        let mut borrows = 0;
        let mut may_borrow: Option<bool> = None;
        self.require_tools()?;
        self.look_around()?;
        while Instant::now() < deadline {
            let money = self.money()?;
            self.say(&format!("money {money}, {} service(s), {} bus(es)", services.len(), services.iter().map(|s| s.buses).sum::<usize>()));
            if money < policy.borrow_below && borrows < policy.max_borrows {
                if may_borrow.is_none() {
                    // Ask once; no answer is not a no, the default policy then applies.
                    may_borrow = Some(self.ask_owner("Cash is short. May I take a bank loan to keep building?", 30)?.unwrap_or(true));
                }
                if may_borrow == Some(false) {
                    self.say("the owner said no to a loan");
                    self.watch(25)?;
                    continue;
                }
                self.say("short of cash: borrow");
                // A failed attempt counts too: the player never retries without end.
                borrows += 1;
                if let Err(e) = self.borrow() {
                    self.say(&format!("could not borrow: {e}"));
                    let _ = self.escape();
                    let _ = self.tidy();
                }
                continue;
            }
            // Reinforce first: a service whose buses already earn gets another bus.
            let mut reinforced = false;
            for i in 0..services.len() {
                if services[i].buses >= policy.max_buses || money < policy.expand_above / 2 {
                    continue;
                }
                let (n, profit) = self.service_profit(&services[i])?;
                if n > 0 && profit > 0 {
                    self.say(&format!("{} earns {profit} with {n} bus(es): add one", services[i].name));
                    let (depot, stops) = (services[i].depot, services[i].stops);
                    match self.buy_bus(depot, stops) {
                        Ok(()) => {
                            services[i].buses += 1;
                            reinforced = true;
                        }
                        Err(e) => {
                            self.say(&format!("could not add a bus: {e}"));
                            let _ = self.escape();
                            let _ = self.tidy();
                        }
                    }
                    break;
                }
            }
            if reinforced {
                continue;
            }
            // Expand: a new town pair, while money allows.
            if money >= policy.expand_above {
                let pair = self.town_pairs()?.into_iter().find(|(a, b)| {
                    let key = (a.get("id").and_then(Value::as_u64).unwrap_or(0), b.get("id").and_then(Value::as_u64).unwrap_or(0));
                    !tried.contains(&key) && !tried.contains(&(key.1, key.0))
                });
                if let Some((a, b)) = pair {
                    let n = |t: &Value| t.get("name").and_then(Value::as_str).unwrap_or("?").to_string();
                    tried.push((a.get("id").and_then(Value::as_u64).unwrap_or(0), b.get("id").and_then(Value::as_u64).unwrap_or(0)));
                    self.say(&format!("expand: {} and {}", n(&a), n(&b)));
                    match self.serve_towns_at(&a, &b) {
                        Ok((stops, depot)) => services.push(Service { name: format!("{}-{}", n(&a), n(&b)), depot, stops, buses: 1 }),
                        Err(e) => {
                            self.say(&format!("skip {} and {}: {e}", n(&a), n(&b)));
                            let _ = self.escape();
                            let _ = self.tidy();
                        }
                    }
                    continue;
                }
            }
            // Nothing to do right now: watch the buses and let the money come in.
            self.watch(25)?;
        }
        Ok(())
    }
}

/// Whether an error says the game's MCP endpoint is gone (the window was closed).
pub fn game_closed(e: &str) -> bool {
    e.contains("Connection refused") || e.contains("Connection reset")
}

#[cfg(test)]
mod tests {
    use super::game_closed;

    #[test]
    fn a_closed_game_is_told_from_other_errors() {
        assert!(game_closed("http://127.0.0.1:1/mcp: Connection Failed: Connect error: Connection refused (os error 61)"));
        assert!(!game_closed("no window says 'depot'"));
    }
}
