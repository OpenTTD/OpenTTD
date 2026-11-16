//! The hand on the mouse: moves the visible cursor of a GUI client through its own MCP server
//! with human motion, clicks, drags, and finds what to click the way a person does, by reading
//! the windows and widgets on screen (openttd.ui.windows) rather than by knowing their ids.

use crate::mcp::Mcp;
use crate::motion::{Model, Rng};
use serde_json::{json, Value};
use std::thread::sleep;
use std::time::{Duration, Instant};

/// A rectangle on screen with its centre.
#[derive(Debug, Clone, Copy)]
pub struct Target {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

impl Target {
    pub fn from_rect(r: &Value) -> Option<Self> {
        Some(Target {
            x: r.get("cx")?.as_i64()? as i32,
            y: r.get("cy")?.as_i64()? as i32,
            w: r.get("w")?.as_i64()? as i32,
            h: r.get("h")?.as_i64()? as i32,
        })
    }

    fn width(&self) -> f64 {
        self.w.min(self.h).max(4) as f64
    }
}

pub struct Hand {
    pub mcp: Mcp,
    pub model: Model,
    pub rng: Rng,
    pub log: bool,
}

impl Hand {
    pub fn new(mcp: Mcp, model: Model, seed: u64) -> Self {
        Hand { mcp, model, rng: Rng::new(seed), log: true }
    }

    pub fn say(&self, what: &str) {
        if self.log {
            eprintln!("[human] {what}");
        }
    }

    pub fn state(&self) -> Result<Value, String> {
        self.mcp.call("openttd.input.state", json!({}))
    }

    pub fn cursor(&self) -> Result<(i32, i32), String> {
        let s = self.state()?;
        Ok((
            s.pointer("/cursor/x").and_then(Value::as_i64).unwrap_or(0) as i32,
            s.pointer("/cursor/y").and_then(Value::as_i64).unwrap_or(0) as i32,
        ))
    }

    /// Wait while the owner holds the mouse: the person always wins.
    fn yield_to_owner(&self) -> Result<(), String> {
        let started = Instant::now();
        loop {
            let s = self.state()?;
            if s.get("owner_has_mouse") != Some(&Value::Bool(true)) {
                return Ok(());
            }
            if started.elapsed() > Duration::from_secs(600) {
                return Err("the owner has held the mouse for ten minutes".into());
            }
            sleep(Duration::from_millis(500));
        }
    }

    /// Any input tool: when the owner holds the mouse, wait until they let go, then do it.
    pub fn input(&self, tool: &str, args: Value) -> Result<Value, String> {
        loop {
            match self.mcp.call(tool, args.clone()) {
                Err(e) if e.contains("owner holds the mouse") => {
                    self.say("the owner took the mouse; waiting");
                    self.yield_to_owner()?;
                }
                other => return other,
            }
        }
    }

    fn pointer(&self, x: i32, y: i32) -> Result<(), String> {
        self.input("openttd.input.pointer", json!({"x": x, "y": y})).map(|_| ())
    }

    fn button(&self, state: &str, button: &str, mods: &[&str]) -> Result<(), String> {
        self.input("openttd.input.button", json!({"state": state, "button": button, "mods": mods})).map(|_| ())
    }

    /// Move the cursor to a target with human motion.
    pub fn move_to(&mut self, t: Target) -> Result<(), String> {
        let from = self.cursor()?;
        // Aim a little off-centre, as people do, but well inside the target.
        let jx = self.rng.normal(t.w as f64 * 0.12).clamp(-(t.w as f64) * 0.3, t.w as f64 * 0.3) as i32;
        let jy = self.rng.normal(t.h as f64 * 0.12).clamp(-(t.h as f64) * 0.3, t.h as f64 * 0.3) as i32;
        let path = self.model.path(from, (t.x + jx, t.y + jy), t.width(), &mut self.rng);
        for s in path {
            self.pointer(s.x, s.y)?;
            sleep(Duration::from_millis(s.wait_ms));
        }
        Ok(())
    }

    /// Move to a target and click it with the left button.
    pub fn click(&mut self, t: Target) -> Result<(), String> {
        self.click_with(t, "left", &[])
    }

    pub fn click_with(&mut self, t: Target, button: &str, mods: &[&str]) -> Result<(), String> {
        self.move_to(t)?;
        let dwell = self.model.think_ms(&mut self.rng, 70.0, 180.0);
        sleep(Duration::from_millis(dwell));
        self.button("down", button, mods)?;
        let hold = self.model.think_ms(&mut self.rng, 60.0, 130.0);
        sleep(Duration::from_millis(hold));
        self.button("up", button, mods)
    }

    /// Press at one point, drag to another at a watched pace, release.
    pub fn drag(&mut self, from: Target, to: Target) -> Result<(), String> {
        self.move_to(from)?;
        sleep(Duration::from_millis(self.model.think_ms(&mut self.rng, 120.0, 260.0)));
        self.button("down", "left", &[])?;
        sleep(Duration::from_millis(self.model.think_ms(&mut self.rng, 80.0, 160.0)));
        let start = self.cursor()?;
        for s in self.model.drag(start, (to.x, to.y), &mut self.rng) {
            self.pointer(s.x, s.y)?;
            sleep(Duration::from_millis(s.wait_ms));
        }
        sleep(Duration::from_millis(self.model.think_ms(&mut self.rng, 120.0, 250.0)));
        self.button("up", "left", &[])
    }

    pub fn think(&mut self, lo: f64, hi: f64) {
        sleep(Duration::from_millis(self.model.think_ms(&mut self.rng, lo, hi)));
    }

    /// Every window on screen, front first, with its widgets.
    pub fn windows(&self) -> Result<Vec<Value>, String> {
        let v = self.mcp.call("openttd.ui.windows", json!({"widgets": true}))?;
        Ok(v.get("windows").and_then(Value::as_array).cloned().unwrap_or_default())
    }

    /// The front-most enabled widget whose tooltip or text contains `needle` (case-insensitive).
    pub fn find_widget(&self, needle: &str) -> Result<Option<(Value, Value)>, String> {
        let n = needle.to_lowercase();
        for w in self.windows()? {
            for wd in w.get("widgets").and_then(Value::as_array).cloned().unwrap_or_default() {
                let hay = format!(
                    "{} {}",
                    wd.get("tooltip").and_then(Value::as_str).unwrap_or(""),
                    wd.get("text").and_then(Value::as_str).unwrap_or("")
                )
                .to_lowercase();
                if hay.contains(&n) && wd.get("disabled") != Some(&Value::Bool(true)) {
                    return Ok(Some((w.clone(), wd)));
                }
            }
        }
        Ok(None)
    }

    /// Find a widget by what it says and click it; an error when nothing on screen says it.
    pub fn click_widget(&mut self, needle: &str) -> Result<Value, String> {
        let (_, wd) = self.find_widget(needle)?.ok_or_else(|| format!("nothing on screen says '{needle}'"))?;
        let t = wd.get("rect").and_then(Target::from_rect).ok_or("widget has no rectangle")?;
        self.say(&format!("click '{needle}'"));
        self.click(t)?;
        Ok(wd)
    }

    /// The first open dropdown's first selectable item, if a dropdown is open.
    pub fn dropdown_item(&self, index: usize) -> Result<Option<Target>, String> {
        for w in self.windows()? {
            if let Some(items) = w.get("items").and_then(Value::as_array) {
                let selectable: Vec<&Value> = items.iter().filter(|i| i.get("selectable") == Some(&Value::Bool(true))).collect();
                return Ok(selectable.get(index).and_then(|i| i.get("rect")).and_then(Target::from_rect));
            }
        }
        Ok(None)
    }

    /// Glide the camera so a tile is centred, and wait until the view settles.
    pub fn focus(&mut self, x: usize, y: usize) -> Result<(), String> {
        let at = self.mcp.call("openttd.ui.locate", json!({"x": x, "y": y}))?;
        let st = self.state()?;
        let sx = at.pointer("/screen/x").and_then(Value::as_i64).unwrap_or(-1);
        let sy = at.pointer("/screen/y").and_then(Value::as_i64).unwrap_or(-1);
        let w = st.pointer("/screen/w").and_then(Value::as_i64).unwrap_or(1);
        let h = st.pointer("/screen/h").and_then(Value::as_i64).unwrap_or(1);
        let central = sx > w / 5 && sx < w * 4 / 5 && sy > h / 4 && sy < h * 3 / 4;
        if central && at.get("clickable") == Some(&Value::Bool(true)) {
            return Ok(());
        }
        self.say(&format!("scroll to tile {x},{y}"));
        self.mcp.call("openttd.view.scroll_to", json!({"x": x, "y": y}))?;
        let mut last = (i64::MIN, i64::MIN);
        let started = Instant::now();
        loop {
            sleep(Duration::from_millis(120));
            let at = self.mcp.call("openttd.ui.locate", json!({"x": x, "y": y}))?;
            let p = (at.pointer("/screen/x").and_then(Value::as_i64).unwrap_or(0), at.pointer("/screen/y").and_then(Value::as_i64).unwrap_or(0));
            if p == last {
                return Ok(());
            }
            last = p;
            if started.elapsed() > Duration::from_secs(8) {
                return Err(format!("the view did not settle on tile {x},{y}"));
            }
        }
    }

    /// Where a tile is on screen right now, if a click there reaches the map; never scrolls.
    pub fn spot(&self, x: usize, y: usize) -> Result<Option<Target>, String> {
        let at = self.mcp.call("openttd.ui.locate", json!({"x": x, "y": y}))?;
        if at.get("clickable") != Some(&Value::Bool(true)) {
            return Ok(None);
        }
        let zoom = at.get("zoom").and_then(Value::as_i64).unwrap_or(2);
        // A tile is 256 pixels wide at the closest zoom level (4x in) and halves with every level out.
        let w = (256 >> zoom.clamp(0, 7)).max(4) as i32;
        Ok(Some(Target {
            x: at.pointer("/screen/x").and_then(Value::as_i64).unwrap_or(0) as i32,
            y: at.pointer("/screen/y").and_then(Value::as_i64).unwrap_or(0) as i32,
            w,
            h: (w / 2).max(4),
        }))
    }

    /// Bring a tile where a click reaches it: centre it, and when a window covers the centre,
    /// shift the view so the tile shows in a clear part of the screen instead.
    pub fn reveal(&mut self, x: usize, y: usize) -> Result<Target, String> {
        if let Some(t) = self.spot(x, y)? {
            return Ok(t);
        }
        self.focus(x, y)?;
        if let Some(t) = self.spot(x, y)? {
            return Ok(t);
        }
        for (dx, dy) in [(6i64, 6i64), (-6, -6), (6, -6), (-6, 6), (10, 0), (0, 10)] {
            let (cx, cy) = ((x as i64 + dx).max(1) as usize, (y as i64 + dy).max(1) as usize);
            self.say(&format!("tile {x},{y} is covered; shift the view"));
            self.mcp.call("openttd.view.scroll_to", json!({"x": cx, "y": cy}))?;
            sleep(Duration::from_millis(900));
            if let Some(t) = self.spot(x, y)? {
                return Ok(t);
            }
        }
        Err(format!("tile {x},{y} stays covered by windows"))
    }

    /// A tile on screen, scrolling to it first when it is not clickable.
    pub fn tile(&mut self, x: usize, y: usize) -> Result<Target, String> {
        if let Some(t) = self.spot(x, y)? {
            return Ok(t);
        }
        self.focus(x, y)?;
        self.spot(x, y)?.ok_or_else(|| format!("tile {x},{y} is not clickable even when centred"))
    }

    /// The front-most window whose caption contains `needle` (case-insensitive).
    pub fn window(&self, needle: &str) -> Result<Option<Value>, String> {
        let n = needle.to_lowercase();
        Ok(self.windows()?.into_iter().find(|w| w.get("caption").and_then(Value::as_str).is_some_and(|c| c.to_lowercase().contains(&n))))
    }

    /// Wait until a window whose caption contains `needle` is open.
    pub fn wait_window(&self, needle: &str) -> Result<Value, String> {
        let started = Instant::now();
        loop {
            if let Some(w) = self.window(needle)? {
                return Ok(w);
            }
            if started.elapsed() > Duration::from_secs(6) {
                return Err(format!("no window says '{needle}'"));
            }
            sleep(Duration::from_millis(150));
        }
    }

    /// A widget of a window by its index.
    pub fn widget(w: &Value, index: i64) -> Option<Value> {
        w.get("widgets").and_then(Value::as_array)?.iter().find(|x| x.get("index").and_then(Value::as_i64) == Some(index)).cloned()
    }

    /// Click a widget of a window by its index.
    pub fn click_index(&mut self, w: &Value, index: i64) -> Result<(), String> {
        let wd = Self::widget(w, index).ok_or_else(|| format!("window has no widget {index}"))?;
        let t = wd.get("rect").and_then(Target::from_rect).ok_or("widget has no rectangle")?;
        self.click(t)
    }

    /// The first widget of a type in a window (e.g. "edit_box", "matrix", "close_box").
    pub fn widget_of_type(w: &Value, kind: &str) -> Option<Value> {
        w.get("widgets").and_then(Value::as_array)?.iter().find(|x| x.get("type").and_then(Value::as_str) == Some(kind)).cloned()
    }

    /// Click a map tile: bring it into view if needed, then point and click.
    pub fn click_tile(&mut self, x: usize, y: usize) -> Result<(), String> {
        let t = self.reveal(x, y)?;
        self.say(&format!("click tile {x},{y}"));
        self.click(t)
    }

    /// Press a key the way the keyboard driver does (OpenTTD WKC_* keycode and the character).
    pub fn key(&self, keycode: u32, ch: &str) -> Result<(), String> {
        self.input("openttd.input.key", json!({"keycode": keycode, "char": ch})).map(|_| ())
    }

    /// Escape: drop the tool in hand, as a person does when done building.
    pub fn escape(&mut self) -> Result<(), String> {
        self.key(1, "")?;
        self.think(120.0, 260.0);
        Ok(())
    }

    /// Type text into the focused edit box, a key at a time at a typing pace.
    pub fn type_text(&mut self, text: &str) -> Result<(), String> {
        for c in text.chars() {
            let code = if c.is_ascii_alphabetic() { c.to_ascii_uppercase() as u32 } else { c as u32 };
            self.key(code, &c.to_string())?;
            self.think(70.0, 190.0);
        }
        Ok(())
    }

    /// Close a window with its close box, as a person tidies up.
    pub fn close(&mut self, w: &Value) -> Result<(), String> {
        if let Some(t) = Self::widget_of_type(w, "close_box").and_then(|c| c.get("rect").and_then(Target::from_rect)) {
            self.click(t)?;
        }
        Ok(())
    }

    /// The front-most window whose caption contains `needle` and none of `not`.
    pub fn window_except(&self, needle: &str, not: &[&str]) -> Result<Option<Value>, String> {
        let n = needle.to_lowercase();
        Ok(self.windows()?.into_iter().find(|w| {
            w.get("caption").and_then(Value::as_str).is_some_and(|c| {
                let c = c.to_lowercase();
                c.contains(&n) && !not.iter().any(|x| c.contains(&x.to_lowercase()))
            })
        }))
    }

    /// A widget of one window by what it says (tooltip or text).
    pub fn widget_saying(w: &Value, needle: &str) -> Option<Value> {
        let n = needle.to_lowercase();
        w.get("widgets").and_then(Value::as_array)?.iter().find(|wd| {
            format!("{} {}", wd.get("tooltip").and_then(Value::as_str).unwrap_or(""), wd.get("text").and_then(Value::as_str).unwrap_or(""))
                .to_lowercase()
                .contains(&n)
        }).cloned()
    }

    /// Click a widget, found by what it says, inside one window (never in another one).
    pub fn click_in(&mut self, w: &Value, needle: &str) -> Result<(), String> {
        let wd = Self::widget_saying(w, needle).ok_or_else(|| format!("'{}' has nothing saying '{needle}'", w.get("caption").and_then(Value::as_str).unwrap_or("?")))?;
        let t = wd.get("rect").and_then(Target::from_rect).ok_or("widget has no rectangle")?;
        self.raise_if_covered(w, t)?;
        self.say(&format!("click '{needle}' in '{}'", w.get("caption").and_then(Value::as_str).unwrap_or("?").trim()));
        self.click(t)
    }

    /// Close every window that has a close box: a person tidying the screen after a job.
    pub fn tidy(&mut self) -> Result<(), String> {
        for _ in 0..12 {
            let open: Vec<Value> = self.windows()?.into_iter().filter(|w| Self::widget_of_type(w, "close_box").is_some()).collect();
            let Some(w) = open.first() else { return Ok(()) };
            self.close(w)?;
            self.think(120.0, 260.0);
        }
        Ok(())
    }

    /// Is the point inside this window rather than another one on top of it?
    fn on_top(&self, w: &Value, t: Target) -> Result<bool, String> {
        let at = self.mcp.call("openttd.ui.at", json!({"x": t.x, "y": t.y}))?;
        Ok(at.pointer("/window/class") == w.get("class") && at.pointer("/window/number") == w.get("number"))
    }

    /// When another window covers the point, bring this one to the front by its title bar, as a
    /// person does before pressing a button on it.
    pub fn raise_if_covered(&mut self, w: &Value, t: Target) -> Result<(), String> {
        if self.on_top(w, t)? {
            return Ok(());
        }
        if let Some(cap) = Self::widget_of_type(w, "caption").and_then(|c| c.get("rect").and_then(Target::from_rect)) {
            self.say(&format!("bring '{}' to the front", w.get("caption").and_then(Value::as_str).unwrap_or("?").trim()));
            self.click(Target { w: cap.w / 3, ..cap })?;
            self.think(150.0, 300.0);
        }
        if !self.on_top(w, t)? {
            return Err(format!("'{}' stays covered", w.get("caption").and_then(Value::as_str).unwrap_or("?").trim()));
        }
        Ok(())
    }

    /// Close every closable window except those whose caption contains one of `keep`.
    pub fn tidy_except(&mut self, keep: &[&str]) -> Result<(), String> {
        for _ in 0..12 {
            let open: Vec<Value> = self.windows()?.into_iter().filter(|w| {
                Self::widget_of_type(w, "close_box").is_some()
                    && !w.get("caption").and_then(Value::as_str).is_some_and(|c| keep.iter().any(|k| c.to_lowercase().contains(k)))
            }).collect();
            let Some(w) = open.first() else { return Ok(()) };
            self.close(w)?;
            self.think(120.0, 260.0);
        }
        Ok(())
    }
}
