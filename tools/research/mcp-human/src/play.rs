//! What the person does: tasks built from the hand's moves, all through the game's own
//! interface. Each task reads the world from the published resources, decides, and then acts by
//! pointing, clicking and dragging; nothing here issues a command directly.

use crate::hand::{Hand, Target};
use crate::plan::{self, Axis, Grid};
use serde_json::{json, Value};

/// What the road toolbar's tools say, in the game's own words (tooltips of the open toolbar).
const ROAD_MENU: &str = "build road infrastructure";
const AUTOROAD: &str = "autoroad mode";
const BUS_STATION: &str = "build bus station";
const DEPOT: &str = "build road vehicle depot";
const NEW_VEHICLES: &str = "buy new road vehicle";
const BUY: &str = "buy vehicle";
const ORDERS: &str = "show vehicle's orders";
const GO_TO: &str = "go to";
const START_STOP: &str = "click to stop/start vehicle";
/// Widget indices of the pickers, in the order the game declares them (src/widgets/road_widget.h):
/// the bus stop picker's drive-through orientations, and the depot picker's first orientation.
const STOP_X: i64 = 5;
const STOP_Y: i64 = 6;
const DEPOT_NE: i64 = 1;

impl Hand {
    /// Open the road toolbar the way a person does: the toolbar button, then "Road" in its menu.
    pub fn open_road_toolbar(&mut self) -> Result<(), String> {
        if self.find_widget(AUTOROAD)?.is_some() {
            return Ok(());
        }
        self.click_widget(ROAD_MENU)?;
        self.think(250.0, 500.0);
        let item = self.dropdown_item(0)?.ok_or("the road menu did not open")?;
        self.say("choose the road type");
        self.click(item)?;
        self.think(300.0, 600.0);
        if self.find_widget(AUTOROAD)?.is_none() {
            return Err("the road toolbar did not open".into());
        }
        Ok(())
    }

    /// Pick the autoroad tool if it is not the active one.
    pub fn pick_autoroad(&mut self) -> Result<(), String> {
        self.open_road_toolbar()?;
        let (_, wd) = self.find_widget(AUTOROAD)?.ok_or("no autoroad tool")?;
        if wd.get("lowered") != Some(&Value::Bool(true)) {
            self.click_widget(AUTOROAD)?;
            self.think(150.0, 350.0);
        }
        Ok(())
    }

    /// Lay a road from one tile to another: plan the route over the published map, then drag each
    /// straight run with the autoroad tool, scrolling the view to each run as a person would.
    pub fn build_road(&mut self, a: (usize, usize), b: (usize, usize)) -> Result<usize, String> {
        let grid = Grid::from_resource(&self.mcp.read("openttd://map")?)?;
        let path = plan::route(&grid, a, b).ok_or_else(|| format!("no road route from {a:?} to {b:?}"))?;
        let mut runs = Vec::new();
        for (s, e) in plan::runs(&path) {
            // Long runs are dragged in pieces whose two ends fit on screen together.
            runs.extend(split(s, e, 14));
        }
        self.say(&format!("road {a:?} -> {b:?}: {} tiles, {} drags", path.len(), runs.len()));
        self.pick_autoroad()?;
        // A run whose two ends cannot both be clicked from one camera position is halved.
        let mut queue: std::collections::VecDeque<((usize, usize), (usize, usize))> = runs.into_iter().collect();
        let mut i = 0;
        while let Some((s, e)) = queue.pop_front() {
            i += 1;
            let (s, e) = (&s, &e);
            // Skip a run that is road already from end to end.
            if line(*s, *e).iter().all(|&(x, y)| grid.kind(x, y) == b'r') {
                continue;
            }
            // Centre the run, then find both ends without scrolling: the press must land where seen.
            self.focus((s.0 + e.0) / 2, (s.1 + e.1) / 2)?;
            let (mut from, mut to) = (self.spot(s.0, s.1)?, self.spot(e.0, e.1)?);
            if from.is_none() || to.is_none() {
                // Something covers an end: clear the screen of what this job does not need.
                self.tidy_except(&["road construction"])?;
                from = self.spot(s.0, s.1)?;
                to = self.spot(e.0, e.1)?;
            }
            let (Some(from), Some(to)) = (from, to) else {
                let len = s.0.abs_diff(e.0).max(s.1.abs_diff(e.1));
                if len < 2 {
                    return Err(format!("tile {s:?} or {e:?} cannot be clicked"));
                }
                let mid = ((s.0 + e.0) / 2, (s.1 + e.1) / 2); // runs are straight, so this is on the run
                self.say(&format!("run {s:?} -> {e:?} does not fit on screen; halving at {mid:?}"));
                queue.push_front((mid, *e));
                queue.push_front((*s, mid));
                continue;
            };
            self.say(&format!("drag {i}: {:?} -> {:?}", s, e));
            self.drag(from, to)?;
            self.think(200.0, 500.0);
            if self.find_widget(AUTOROAD)?.map(|(_, w)| w.get("lowered") == Some(&Value::Bool(true))) != Some(true) {
                self.pick_autoroad()?;
            }
        }
        Ok(i)
    }

    /// Connect two towns by road: the largest town and its nearest neighbour unless given.
    pub fn connect_towns(&mut self, pair: Option<(u64, u64)>) -> Result<(String, String), String> {
        let towns = self.mcp.read("openttd://towns")?;
        let list: Vec<Value> = towns.get("towns").and_then(Value::as_array).cloned().unwrap_or_default();
        if list.len() < 2 {
            return Err("fewer than two towns".into());
        }
        let pos = |t: &Value| (t.get("x").and_then(Value::as_u64).unwrap_or(0) as usize, t.get("y").and_then(Value::as_u64).unwrap_or(0) as usize);
        let name = |t: &Value| t.get("name").and_then(Value::as_str).unwrap_or("?").to_string();
        let by_id = |id: u64| list.iter().find(|t| t.get("id").and_then(Value::as_u64) == Some(id)).cloned();
        let candidates: Vec<(Value, Value)> = match pair {
            Some((a, b)) => vec![(by_id(a).ok_or("no such town")?, by_id(b).ok_or("no such town")?)],
            None => {
                let mut sorted = list.clone();
                sorted.sort_by_key(|t| std::cmp::Reverse(t.get("population").and_then(Value::as_u64).unwrap_or(0)));
                let mut pairs = Vec::new();
                for a in sorted.iter().take(4) {
                    let mut near: Vec<&Value> = list.iter().filter(|t| t.get("id") != a.get("id")).collect();
                    let (ax, ay) = pos(a);
                    near.sort_by_key(|t| {
                        let (x, y) = pos(t);
                        (x as i64 - ax as i64).abs() + (y as i64 - ay as i64).abs()
                    });
                    for b in near.into_iter().take(3) {
                        pairs.push((a.clone(), b.clone()));
                    }
                }
                pairs
            }
        };
        let mut last_err = String::new();
        let grid = Grid::from_resource(&self.mcp.read("openttd://map")?)?;
        for (a, b) in candidates {
            if pair.is_none() && plan::connected(&grid, pos(&a), pos(&b)) {
                continue;
            }
            self.say(&format!("connect {} and {}", name(&a), name(&b)));
            match self.build_road(pos(&a), pos(&b)) {
                Ok(_) => return Ok((name(&a), name(&b))),
                Err(e) if e.starts_with("no road route") => {
                    self.say(&e);
                    last_err = e;
                }
                Err(e) => return Err(e),
            }
        }
        Err(format!("no pair of towns could be connected: {last_err}"))
    }

    /// A drive-through bus stop on a straight road: the tool, the orientation, the tile.
    pub fn build_stop(&mut self, at: (usize, usize), axis: Axis) -> Result<(), String> {
        self.escape()?;
        self.open_road_toolbar()?;
        self.click_widget(BUS_STATION)?;
        self.think(300.0, 600.0);
        let picker = self.wait_window("orientation")?;
        self.say(&format!("drive-through stop along {axis:?}"));
        self.click_index(&picker, if axis == Axis::X { STOP_X } else { STOP_Y })?;
        self.think(200.0, 400.0);
        self.click_tile(at.0, at.1)?;
        self.think(400.0, 700.0);
        self.escape()
    }

    /// A road depot on a free tile facing the road: the tool, the orientation, the tile.
    pub fn build_depot(&mut self, at: (usize, usize), side: usize) -> Result<(), String> {
        self.escape()?;
        self.open_road_toolbar()?;
        self.click_widget(DEPOT)?;
        self.think(300.0, 600.0);
        let picker = self.wait_window("depot orientation")?;
        self.click_index(&picker, DEPOT_NE + side as i64)?;
        self.think(200.0, 400.0);
        self.click_tile(at.0, at.1)?;
        self.think(400.0, 700.0);
        self.escape()
    }

    /// Buy a bus in a depot and send it between two stops: open the depot, filter the list for
    /// buses, pick the first, buy it, give it its orders with Go To, start it, tidy up.
    pub fn buy_bus(&mut self, depot: (usize, usize), stops: [(usize, usize); 2]) -> Result<(), String> {
        self.escape()?;
        self.click_tile(depot.0, depot.1)?;
        let depot_window = self.wait_window("depot")?;
        self.think(300.0, 600.0);
        self.click_in(&depot_window, NEW_VEHICLES)?;
        self.think(400.0, 800.0);
        let buy = self.wait_window("new road vehicles")?;
        if let Some(edit) = Self::widget_of_type(&buy, "edit_box") {
            let t = edit.get("rect").and_then(Target::from_rect).ok_or("edit box has no rectangle")?;
            self.say("filter the list: bus");
            self.click(t)?;
            self.type_text("bus")?;
            self.think(300.0, 600.0);
        }
        let buy = self.wait_window("new road vehicles")?;
        let list = Self::widget_of_type(&buy, "matrix").ok_or("the buy window has no list")?;
        let r = list.get("rect").cloned().unwrap_or_default();
        let row = list.get("row_height").and_then(Value::as_i64).unwrap_or(20) as i32;
        let first = Target {
            x: r.get("cx").and_then(Value::as_i64).unwrap_or(0) as i32,
            y: r.get("y").and_then(Value::as_i64).unwrap_or(0) as i32 + row / 2,
            w: r.get("w").and_then(Value::as_i64).unwrap_or(100) as i32 / 2,
            h: (row - 4).max(4),
        };
        self.say("pick the first bus");
        self.click(first)?;
        self.think(400.0, 800.0);
        let before = self.own_vehicles()?;
        self.click_in(&buy, BUY)?;
        self.think(600.0, 1000.0);
        let after = self.own_vehicles()?;
        let bus = after.iter().find(|v| !before.contains(v)).copied().ok_or("the bus was not bought (money? the list?)")?;
        // Done with the shop: close the list and the depot so they cover nothing.
        for needle in ["new road vehicles", "depot"] {
            if let Some(w) = self.window(needle)? {
                self.close(&w)?;
                self.think(150.0, 300.0);
            }
        }
        // The new vehicle's own window, not its orders window.
        let started = std::time::Instant::now();
        let vehicle = loop {
            if let Some(w) = self.window_except("road vehicle #", &["orders"])? {
                break w;
            }
            if started.elapsed() > std::time::Duration::from_secs(5) {
                return Err("the new vehicle's window did not open".into());
            }
            std::thread::sleep(std::time::Duration::from_millis(150));
        };
        self.click_in(&vehicle, ORDERS)?;
        self.think(400.0, 700.0);
        for attempt in 0..2 {
            let orders = self.wait_window("(orders)")?;
            self.click_in(&orders, GO_TO)?;
            self.think(300.0, 500.0);
            let armed = self.wait_window("(orders)").ok().and_then(|w| Self::widget_saying(&w, GO_TO)).and_then(|w| w.get("lowered").cloned()) == Some(Value::Bool(true));
            if armed {
                break;
            }
            if attempt == 1 {
                return Err("Go To did not engage".into());
            }
        }
        for (i, s) in stops.iter().enumerate() {
            self.say(&format!("order {}: the stop at {},{}", i + 1, s.0, s.1));
            self.click_tile(s.0, s.1)?;
            self.think(400.0, 700.0);
        }
        self.escape()?;
        let orders = self.vehicle(bus)?.get("orders").and_then(Value::as_array).map(Vec::len).unwrap_or(0);
        if orders < 2 {
            return Err(format!("the bus has {orders} order(s), not 2"));
        }
        for _ in 0..2 {
            let vehicle = self.window_except("road vehicle #", &["orders"])?.ok_or("the vehicle window closed")?;
            self.say("start the bus");
            self.click_in(&vehicle, START_STOP)?;
            self.think(600.0, 1000.0);
            if self.vehicle(bus)?.get("stopped") == Some(&Value::Bool(false)) {
                break;
            }
        }
        if self.vehicle(bus)?.get("stopped") != Some(&Value::Bool(false)) {
            return Err("the bus did not start".into());
        }
        self.tidy()?;
        let _ = depot_window;
        Ok(())
    }

    /// Ids of this client's company's vehicles (openttd://vehicles, as the client sees the game).
    pub fn own_vehicles(&self) -> Result<Vec<u64>, String> {
        let me = self.state()?.get("local_company").and_then(Value::as_u64).unwrap_or(255);
        let v = self.mcp.read("openttd://vehicles")?;
        Ok(v.get("vehicles").and_then(Value::as_array).cloned().unwrap_or_default().iter()
            .filter(|x| x.get("owner").and_then(Value::as_u64) == Some(me))
            .filter_map(|x| x.get("id").and_then(Value::as_u64)).collect())
    }

    pub fn vehicle(&self, id: u64) -> Result<Value, String> {
        let v = self.mcp.read("openttd://vehicles")?;
        v.get("vehicles").and_then(Value::as_array).and_then(|a| a.iter().find(|x| x.get("id").and_then(Value::as_u64) == Some(id)).cloned())
            .ok_or_else(|| format!("no vehicle {id}"))
    }

    /// A bus service between two towns: the road if they are not joined, a stop in each, a depot,
    /// and a bus running between the stops. Answers the stop tiles.
    pub fn serve_towns(&mut self, a: &Value, b: &Value) -> Result<[(usize, usize); 2], String> {
        let pos = |t: &Value| (t.get("x").and_then(Value::as_u64).unwrap_or(0) as usize, t.get("y").and_then(Value::as_u64).unwrap_or(0) as usize);
        let name = |t: &Value| t.get("name").and_then(Value::as_str).unwrap_or("?").to_string();
        let (pa, pb) = (pos(a), pos(b));
        self.escape()?;
        self.tidy()?;
        let grid = Grid::from_resource(&self.mcp.read("openttd://map")?)?;
        if !plan::connected(&grid, pa, pb) {
            self.build_road(pa, pb)?;
        }
        let grid = Grid::from_resource(&self.mcp.read("openttd://map")?)?;
        let (sa, ax) = plan::stop_site(&grid, pa, &[]).ok_or_else(|| format!("no straight road for a stop in {}", name(a)))?;
        let (sb, bx) = plan::stop_site(&grid, pb, &[sa]).ok_or_else(|| format!("no straight road for a stop in {}", name(b)))?;
        self.say(&format!("stops: {} at {sa:?}, {} at {sb:?}", name(a), name(b)));
        self.build_stop(sa, ax)?;
        self.build_stop(sb, bx)?;
        let grid = Grid::from_resource(&self.mcp.read("openttd://map")?)?;
        let (depot, side) = plan::depot_site(&grid, pa).ok_or_else(|| format!("no depot site near {}", name(a)))?;
        self.say(&format!("depot near {} at {depot:?}", name(a)));
        self.build_depot(depot, side)?;
        self.buy_bus(depot, [sa, sb])?;
        Ok([sa, sb])
    }

    /// Town pairs worth serving, nearest neighbours of the largest towns first.
    pub fn town_pairs(&self) -> Result<Vec<(Value, Value)>, String> {
        let towns = self.mcp.read("openttd://towns")?;
        let list: Vec<Value> = towns.get("towns").and_then(Value::as_array).cloned().unwrap_or_default();
        let pos = |t: &Value| (t.get("x").and_then(Value::as_u64).unwrap_or(0) as i64, t.get("y").and_then(Value::as_u64).unwrap_or(0) as i64);
        let mut sorted = list.clone();
        sorted.sort_by_key(|t| std::cmp::Reverse(t.get("population").and_then(Value::as_u64).unwrap_or(0)));
        let mut pairs = Vec::new();
        for a in sorted.iter().take(6) {
            let (ax, ay) = pos(a);
            let mut near: Vec<&Value> = list.iter().filter(|t| t.get("id") != a.get("id")).collect();
            near.sort_by_key(|t| {
                let (x, y) = pos(t);
                (x - ax).abs() + (y - ay).abs()
            });
            for b in near.into_iter().take(2) {
                let (bx, by) = pos(b);
                if (bx - ax).abs() + (by - ay).abs() <= 70 {
                    pairs.push((a.clone(), b.clone()));
                }
            }
        }
        Ok(pairs)
    }

    /// Look around: glide over the toolbar and hover a few buttons, then over the map, the way a
    /// person orients themselves after sitting down.
    pub fn look_around(&mut self) -> Result<(), String> {
        let windows = self.windows()?;
        let toolbar: Vec<Value> = windows
            .iter()
            .filter(|w| w.get("widgets").and_then(Value::as_array).is_some_and(|ws| ws.iter().any(|x| x.get("tooltip").and_then(Value::as_str).is_some_and(|t| t.to_lowercase().contains(ROAD_MENU)))))
            .flat_map(|w| w.get("widgets").and_then(Value::as_array).cloned().unwrap_or_default())
            .filter(|w| w.get("tooltip").is_some())
            .collect();
        for i in 0..4.min(toolbar.len()) {
            let k = (self.rng.unit() * toolbar.len() as f64) as usize % toolbar.len();
            if let Some(t) = toolbar[k].get("rect").and_then(Target::from_rect) {
                self.say(&format!("hover '{}'", toolbar[k].get("tooltip").and_then(Value::as_str).unwrap_or("").chars().take(40).collect::<String>()));
                self.move_to(t)?;
                self.think(400.0, 900.0 + i as f64 * 100.0);
            }
        }
        let st = self.state()?;
        let (w, h) = (st.pointer("/screen/w").and_then(Value::as_i64).unwrap_or(800) as i32, st.pointer("/screen/h").and_then(Value::as_i64).unwrap_or(600) as i32);
        self.move_to(Target { x: w / 2, y: h / 2, w: 200, h: 200 })?;
        Ok(())
    }

    /// The tools the client offers, to prove the surface the hand relies on is there.
    pub fn require_tools(&self) -> Result<(), String> {
        let tools = self.mcp.tools()?;
        for t in ["openttd.ui.windows", "openttd.ui.locate", "openttd.input.pointer", "openttd.input.button", "openttd.view.scroll_to"] {
            if !tools.iter().any(|x| x == t) {
                return Err(format!("the client does not offer {t}"));
            }
        }
        let st = self.state()?;
        if st.get("armed") != Some(&json!(true)) {
            return Err("input is not armed in the client (PRISMATIC_MCP_INPUT=1)".into());
        }
        Ok(())
    }
}

fn line(s: (usize, usize), e: (usize, usize)) -> Vec<(usize, usize)> {
    let (dx, dy) = ((e.0 as i64 - s.0 as i64).signum(), (e.1 as i64 - s.1 as i64).signum());
    let n = (e.0 as i64 - s.0 as i64).abs().max((e.1 as i64 - s.1 as i64).abs());
    (0..=n).map(|i| ((s.0 as i64 + dx * i) as usize, (s.1 as i64 + dy * i) as usize)).collect()
}

fn split(s: (usize, usize), e: (usize, usize), max: usize) -> Vec<((usize, usize), (usize, usize))> {
    let tiles = line(s, e);
    let mut out = Vec::new();
    let mut i = 0;
    while i + 1 < tiles.len() {
        let j = (i + max).min(tiles.len() - 1);
        out.push((tiles[i], tiles[j]));
        i = j;
    }
    out
}
