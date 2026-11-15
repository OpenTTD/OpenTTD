//! What the person does: tasks built from the hand's moves, all through the game's own
//! interface. Each task reads the world from the published resources, decides, and then acts by
//! pointing, clicking and dragging; nothing here issues a command directly.

use crate::hand::{Hand, Target};
use crate::plan::{self, Grid};
use serde_json::{json, Value};

/// What the road toolbar's tools say, in the game's own words (tooltips of the open toolbar).
const ROAD_MENU: &str = "build road infrastructure";
const AUTOROAD: &str = "autoroad mode";

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
        for (i, (s, e)) in runs.iter().enumerate() {
            // Skip a run that is road already from end to end.
            if line(*s, *e).iter().all(|&(x, y)| grid.kind(x, y) == b'r') {
                continue;
            }
            // Centre the run, then find both ends without scrolling: the press must land where seen.
            self.focus((s.0 + e.0) / 2, (s.1 + e.1) / 2)?;
            let (Some(from), Some(to)) = (self.spot(s.0, s.1)?, self.spot(e.0, e.1)?) else {
                return Err(format!("run {s:?} -> {e:?} does not fit on screen"));
            };
            self.say(&format!("drag {}/{}: {:?} -> {:?}", i + 1, runs.len(), s, e));
            self.drag(from, to)?;
            self.think(200.0, 500.0);
            if self.find_widget(AUTOROAD)?.map(|(_, w)| w.get("lowered") == Some(&Value::Bool(true))) != Some(true) {
                self.pick_autoroad()?;
            }
        }
        Ok(runs.len())
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
        for (a, b) in candidates {
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
