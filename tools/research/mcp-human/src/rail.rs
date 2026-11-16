//! A rail line the way a person builds one: two stations at the sites the shared planner chose
//! (mcp-plan's rail candidates carry `stations {from, to}`, each {x, y, axis, length, end, exit}),
//! the track between them from AlgoGlyph's track-planning kernel, laid piece by piece with the
//! game's own rail tools, then checked against the game's real rail bits (openttd://map/ways).
//!
//! The kernel plans "from the edge of `from` (a station tile, leaving heading from.dir) into `to`
//! (a station tile, entered heading to.dir)", so it gets each station's end tile: the first and
//! last pieces land exactly on the two exit tiles. Its input is the game's own resources verbatim,
//! as the hub's algoglyph.track_plan assembles it (map, map/ways, map/terrain, economy/prices,
//! bridges), so the plan sees what the game sees.
//!
//! The four track buttons share one tooltip; they are told apart by their order in the window
//! (NS, X, EW, Y, as the game builds it), and so are the station picker's orientation, platform
//! and length buttons. A half-tile piece is placed by where on the tile the cursor is, exactly as
//! the game decides it (viewport.cpp): above the centre is the upper track, left of it the left.

use crate::algoglyph::Flow;
use crate::hand::{Hand, Target};
use serde_json::{json, Value};

const RAIL_MENU: &str = "build railway infrastructure";
const TRACK_TOOL: &str = "build railway track.";
const STATION_TOOL: &str = "build railway station";
const ORIENTATION: &str = "select railway station orientation";
const PLATFORMS: &str = "select number of platforms";
const LENGTH: &str = "select length of railway station";
const DEPOT_TOOL: &str = "build train depot";
const DEPOT_ORIENTATION: &str = "select railway depot orientation";
const NEW_TRAIN: &str = "buy new train vehicle";
const TRAIN_SHOP: &str = "rail vehicles";
const BRIDGE_TOOL: &str = "build railway bridge";
const TUNNEL_TOOL: &str = "build railway tunnel";
const BRIDGE_LIST: &str = "bridge selection";

/// The resources the track planner reads, as the keys of its input (hub_algoglyph.rs TRACK_INPUTS).
pub const TRACK_INPUTS: [(&str, &str); 5] = [("map", "openttd://map"), ("ways", "openttd://map/ways"),
    ("terrain", "openttd://map/terrain"), ("prices", "openttd://economy/prices"), ("bridges", "openttd://bridges")];

/// A side of a tile: the four ways a line can leave it.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Heading { NE, SE, SW, NW }

impl Heading {
    pub fn parse(s: &str) -> Option<Heading> {
        Some(match s { "NE" => Heading::NE, "SE" => Heading::SE, "SW" => Heading::SW, "NW" => Heading::NW, _ => return None })
    }
    pub fn name(self) -> &'static str {
        match self { Heading::NE => "NE", Heading::SE => "SE", Heading::SW => "SW", Heading::NW => "NW" }
    }
    /// The tile one step that way (NE is x-1, SE y+1, SW x+1, NW y-1).
    pub fn step(self, (x, y): (usize, usize)) -> Option<(usize, usize)> {
        let (dx, dy) = match self { Heading::NE => (-1, 0), Heading::SE => (0, 1), Heading::SW => (1, 0), Heading::NW => (0, -1) };
        Some((usize::try_from(x as i64 + dx).ok()?, usize::try_from(y as i64 + dy).ok()?))
    }
    /// Its position among the game's four orientation buttons (NE, SE, SW, NW).
    pub fn index(self) -> usize {
        match self { Heading::NE => 0, Heading::SE => 1, Heading::SW => 2, Heading::NW => 3 }
    }
    pub fn reverse(self) -> Heading {
        match self { Heading::NE => Heading::SW, Heading::SW => Heading::NE, Heading::SE => Heading::NW, Heading::NW => Heading::SE }
    }
}

/// A station site from the planner.
#[derive(Debug, Clone, PartialEq)]
pub struct Site {
    /// The station's north tile, where the game anchors it.
    pub x: usize,
    pub y: usize,
    /// Along x (NE_SW) or along y (NW_SE).
    pub along_x: bool,
    pub length: usize,
    /// The tile at the end that faces the other station.
    pub end: (usize, usize),
    /// The side the line leaves the station by.
    pub exit: Heading,
    /// The planner's depot site, when it gave one ({x, y, dir}).
    pub planned_depot: Option<((usize, usize), Heading)>,
}

impl Site {
    pub fn from_value(v: &Value) -> Option<Site> {
        let u = |p: &str| v.pointer(p).and_then(Value::as_u64).map(|n| n as usize);
        Some(Site {
            x: u("/x")?, y: u("/y")?,
            along_x: v.get("axis")?.as_str()? == "NE_SW",
            length: u("/length").unwrap_or(3),
            end: (u("/end/x")?, u("/end/y")?),
            exit: Heading::parse(v.pointer("/exit/dir")?.as_str()?)?,
            planned_depot: (|| Some(((u("/depot/x")?, u("/depot/y")?), Heading::parse(v.pointer("/depot/dir")?.as_str()?)?)))(),
        })
    }

    /// Where its depot goes: the tile beyond the back end (the end away from the exit), with the
    /// entrance facing the platform, so a train leaves the depot straight onto it.
    pub fn depot(&self) -> Option<((usize, usize), Heading)> {
        // The planner's site wins (it chose the station for a free depot tile); the rule is the same.
        if self.planned_depot.is_some() {
            return self.planned_depot;
        }
        let tiles = self.tiles();
        let back = if tiles.first() == Some(&self.end) { *tiles.last()? } else { *tiles.first()? };
        Some((self.exit.reverse().step(back)?, self.exit))
    }

    /// Every tile of the station.
    pub fn tiles(&self) -> Vec<(usize, usize)> {
        (0..self.length).map(|i| if self.along_x { (self.x + i, self.y) } else { (self.x, self.y + i) }).collect()
    }
}

/// One planned piece of track: a tile and its track bit (X=1, Y=2, upper=4, lower=8, left=16, right=32).
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Piece { pub x: usize, pub y: usize, pub track: u32 }

/// A bridge or tunnel of a plan: its two head tiles.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Span { pub tunnel: bool, pub from: (usize, usize), pub to: (usize, usize) }

/// The track pieces and the bridges and tunnels of a found plan (AlgoGlyph's contract: track
/// {kind, x, y, track, ...}; bridge or tunnel {kind, from, to, heading, axis, length, z}).
pub fn plan_parts(plan: &Value) -> Result<(Vec<Piece>, Vec<Span>), String> {
    let r = plan.get("result").unwrap_or(plan);
    if r.get("status").and_then(Value::as_str) != Some("found") {
        return Err(format!("the track planner found no route ({})", r.get("status").and_then(Value::as_str).unwrap_or("no status")));
    }
    let list = r.get("pieces").and_then(Value::as_array).ok_or("the plan has no pieces")?;
    // A head is [x, y] (the kernel's form) or {x, y}.
    let xy = |v: Option<&Value>| -> Result<(usize, usize), String> {
        let v = v.ok_or("span without an end")?;
        let (x, y) = match v.as_array() {
            Some(a) if a.len() == 2 => (a[0].as_u64(), a[1].as_u64()),
            _ => (v.get("x").and_then(Value::as_u64), v.get("y").and_then(Value::as_u64)),
        };
        Ok((x.ok_or("end without x")? as usize, y.ok_or("end without y")? as usize))
    };
    let (mut track, mut spans) = (Vec::new(), Vec::new());
    for p in list {
        match p.get("kind").and_then(Value::as_str).unwrap_or("") {
            "track" => track.push(Piece {
                x: p.get("x").and_then(Value::as_u64).ok_or("piece without x")? as usize,
                y: p.get("y").and_then(Value::as_u64).ok_or("piece without y")? as usize,
                track: p.get("track").and_then(Value::as_u64).ok_or("piece without track")? as u32,
            }),
            k @ ("bridge" | "tunnel") => spans.push(Span { tunnel: k == "tunnel", from: xy(p.get("from"))?, to: xy(p.get("to"))? }),
            k => return Err(format!("the plan has a piece of kind '{k}' this player does not know")),
        }
    }
    Ok((track, spans))
}

/// The track pieces of a found plan (its bridges and tunnels left out).
#[cfg(test)]
pub fn pieces(plan: &Value) -> Result<Vec<Piece>, String> {
    plan_parts(plan).map(|(t, _)| t)
}

/// Whether the game has a rail bridge or tunnel between two head tiles (openttd://map/ways crossings).
pub fn has_span(ways: &Value, s: &Span) -> bool {
    ways.get("crossings").and_then(Value::as_array).is_some_and(|c| c.iter().any(|x| {
        let end = |k: &str| (x.pointer(&format!("/{k}/x")).and_then(Value::as_u64).unwrap_or(u64::MAX) as usize, x.pointer(&format!("/{k}/y")).and_then(Value::as_u64).unwrap_or(u64::MAX) as usize);
        x.get("transport").and_then(Value::as_str) == Some("rail")
            && ((end("from") == s.from && end("to") == s.to) || (end("from") == s.to && end("to") == s.from))
    }))
}

/// What a person does for a group of pieces: drag a straight X or Y run, or click one piece.
#[derive(Debug, Clone, PartialEq)]
pub enum Stroke {
    Drag { tool: usize, from: (usize, usize), to: (usize, usize) },
    Click { tool: usize, at: (usize, usize), track: u32 },
}

/// The track button for a track bit, by its order among the window's track buttons (NS, X, EW, Y).
pub fn tool_of(track: u32) -> usize {
    match track { 1 => 1, 2 => 3, 4 | 8 => 2, _ => 0 }
}

/// Group pieces into strokes: consecutive X pieces along x (or Y pieces along y) become one drag
/// of at most `max` tiles; every other piece is one click.
pub fn strokes(pieces: &[Piece], max: usize) -> Vec<Stroke> {
    let mut out = Vec::new();
    let mut i = 0;
    while i < pieces.len() {
        let p = pieces[i];
        if p.track == 1 || p.track == 2 {
            let mut j = i;
            while j + 1 < pieces.len() && j + 1 - i < max {
                let (a, b) = (pieces[j], pieces[j + 1]);
                let straight = b.track == p.track && if p.track == 1 { b.y == a.y && b.x.abs_diff(a.x) == 1 } else { b.x == a.x && b.y.abs_diff(a.y) == 1 };
                if !straight {
                    break;
                }
                j += 1;
            }
            if j > i {
                out.push(Stroke::Drag { tool: tool_of(p.track), from: (p.x, p.y), to: (pieces[j].x, pieces[j].y) });
                i = j + 1;
                continue;
            }
        }
        out.push(Stroke::Click { tool: tool_of(p.track), at: (p.x, p.y), track: p.track });
        i += 1;
    }
    out
}

/// The track bits of a tile in openttd://map/ways (two hex digits per tile).
pub fn rail_bits(ways: &Value, x: usize, y: usize) -> u32 {
    ways.get("rails").and_then(Value::as_array).and_then(|rows| rows.get(y)).and_then(Value::as_str)
        .and_then(|row| row.get(2 * x..2 * x + 2)).and_then(|h| u32::from_str_radix(h, 16).ok()).unwrap_or(0)
}

/// The sides of a tile its tracks reach (NE, SE, SW, NW as 0..3), from its track bits.
fn sides(bits: u32) -> [bool; 4] {
    // X: NE-SW, Y: NW-SE, upper: NW-NE, lower: SW-SE, left: NW-SW, right: NE-SE.
    const REACH: [(u32, [usize; 2]); 6] = [(1, [0, 2]), (2, [3, 1]), (4, [3, 0]), (8, [2, 1]), (16, [3, 2]), (32, [0, 1])];
    let mut s = [false; 4];
    for (bit, ends) in REACH {
        if bits & bit != 0 {
            s[ends[0]] = true;
            s[ends[1]] = true;
        }
    }
    s
}

/// Whether the game's rails join one set of tiles to another: a walk over the real track bits,
/// stepping to a neighbour only when both tiles' tracks reach their shared side.
pub fn rail_connected(ways: &Value, from: &[(usize, usize)], to: &[(usize, usize)]) -> bool {
    const STEP: [(i64, i64); 4] = [(-1, 0), (0, 1), (1, 0), (0, -1)]; // NE, SE, SW, NW
    // A rail bridge or tunnel joins its two heads; the tiles between carry no rail bits.
    let mut far: std::collections::HashMap<(usize, usize), (usize, usize)> = std::collections::HashMap::new();
    for c in ways.get("crossings").and_then(Value::as_array).cloned().unwrap_or_default() {
        if c.get("transport").and_then(Value::as_str) != Some("rail") {
            continue;
        }
        let end = |k: &str| Some((c.pointer(&format!("/{k}/x"))?.as_u64()? as usize, c.pointer(&format!("/{k}/y"))?.as_u64()? as usize));
        if let (Some(a), Some(b)) = (end("from"), end("to")) {
            far.insert(a, b);
            far.insert(b, a);
        }
    }
    let mut seen: std::collections::HashSet<(usize, usize)> = from.iter().copied().collect();
    let mut queue: Vec<(usize, usize)> = from.to_vec();
    while let Some((x, y)) = queue.pop() {
        if to.contains(&(x, y)) {
            return true;
        }
        if let Some(&other) = far.get(&(x, y)) {
            if seen.insert(other) {
                queue.push(other);
            }
        }
        let here = sides(rail_bits(ways, x, y));
        for d in 0..4 {
            if !here[d] {
                continue;
            }
            let (nx, ny) = (x as i64 + STEP[d].0, y as i64 + STEP[d].1);
            if nx < 0 || ny < 0 {
                continue;
            }
            let n = (nx as usize, ny as usize);
            if !seen.contains(&n) && sides(rail_bits(ways, n.0, n.1))[(d + 2) % 4] {
                seen.insert(n);
                queue.push(n);
            }
        }
    }
    false
}

/// Whether a tile is clear (or trees) and flat on openttd://map: its four corner heights equal.
pub fn free_flat(map: &Value, x: usize, y: usize) -> bool {
    let row = |k: &str, y: usize| map.get(k).and_then(Value::as_array).and_then(|r| r.get(y)).and_then(Value::as_str).map(str::to_string);
    let z = |x: usize, y: usize| row("heights", y).and_then(|r| r.get(2 * x..2 * x + 2).and_then(|h| u32::from_str_radix(h, 16).ok()));
    let kind = row("types", y).and_then(|r| r.as_bytes().get(x).copied());
    matches!(kind, Some(b'C') | Some(b'T')) && z(x, y).is_some() && z(x, y) == z(x + 1, y) && z(x, y) == z(x, y + 1) && z(x, y) == z(x + 1, y + 1)
}

/// The engine and wagon of a planner option's vehicle name, "Engine + 4 x Wagon".
pub fn consist(name: &str) -> Option<(String, String, usize)> {
    let (engine, rest) = name.split_once(" + ")?;
    let (n, wagon) = rest.split_once(" x ")?;
    Some((engine.trim().to_string(), wagon.trim().to_string(), n.trim().parse().ok()?))
}

/// The pieces the game does not have yet.
pub fn missing(pieces: &[Piece], ways: &Value) -> Vec<Piece> {
    pieces.iter().copied().filter(|p| rail_bits(ways, p.x, p.y) & p.track == 0).collect()
}

/// Where to point for a half-tile piece: off the tile's centre towards that half, with a target
/// small enough that the hand's aim cannot cross into the other half.
pub fn half_target(t: Target, track: u32) -> Target {
    let (qx, qy) = (t.w / 4, t.h / 4);
    let (dx, dy) = match track { 4 => (0, -qy), 8 => (0, qy), 16 => (-qx, 0), 32 => (qx, 0), _ => (0, 0) };
    Target { x: t.x + dx, y: t.y + dy, w: (t.w / 4).max(4), h: (t.h / 4).max(4) }
}

/// A rail service as built: its line, depot and train.
#[derive(Debug, Clone)]
pub struct Service { pub label: String, pub line: Line, pub depot: (usize, usize), pub train: u64, pub consist: String }

/// A line as built: its stations and every piece of track between them.
#[derive(Debug, Clone)]
pub struct Line { pub from: Site, pub to: Site, pub pieces: Vec<Piece> }

impl Hand {
    /// The rail construction window, opened from the toolbar's rail menu (the first rail type).
    pub fn open_rail_toolbar(&mut self) -> Result<Value, String> {
        if let Some(w) = self.rail_window()? {
            return Ok(w);
        }
        self.menu_pick(RAIL_MENU, |items| if items.is_empty() { None } else { Some(0) })?;
        self.think(300.0, 600.0);
        self.rail_window()?.ok_or_else(|| "the rail construction window did not open".to_string())
    }

    /// The window that holds the track buttons.
    fn rail_window(&self) -> Result<Option<Value>, String> {
        Ok(self.windows()?.into_iter().find(|w| !Self::buttons(w, TRACK_TOOL).is_empty()))
    }

    /// The front-most window holding buttons that say `needle`, waited for a few seconds. Windows
    /// are found by what they hold: the station picker, for one, reports no caption.
    fn wait_holding(&self, needle: &str) -> Result<Value, String> {
        let started = std::time::Instant::now();
        loop {
            if let Some(w) = self.windows()?.into_iter().find(|w| !Self::buttons(w, needle).is_empty()) {
                return Ok(w);
            }
            if started.elapsed() > std::time::Duration::from_secs(6) {
                return Err(format!("no window holds '{needle}'"));
            }
            std::thread::sleep(std::time::Duration::from_millis(150));
        }
    }

    /// A window's widgets whose tooltip contains `needle`, in the window's own order.
    fn buttons(w: &Value, needle: &str) -> Vec<Value> {
        let mut v: Vec<Value> = w.get("widgets").and_then(Value::as_array).cloned().unwrap_or_default().into_iter()
            .filter(|x| x.get("tooltip").and_then(Value::as_str).is_some_and(|t| t.to_lowercase().contains(needle)))
            .collect();
        v.sort_by_key(|x| x.get("index").and_then(Value::as_i64).unwrap_or(0));
        v
    }

    /// Press the n-th button of a group unless it is already down.
    fn choose_button(&mut self, w: &Value, needle: &str, n: usize) -> Result<(), String> {
        let b = Self::buttons(w, needle).get(n).cloned().ok_or_else(|| format!("no button {n} saying '{needle}'"))?;
        if b.get("lowered") == Some(&Value::Bool(true)) {
            return Ok(());
        }
        let t = b.get("rect").and_then(Target::from_rect).ok_or("button has no rectangle")?;
        self.click(t)
    }

    /// Pick one of the four track tools.
    fn track_tool(&mut self, tool: usize) -> Result<(), String> {
        let w = self.open_rail_toolbar()?;
        self.choose_button(&w, TRACK_TOOL, tool)
    }

    /// Build a station at a site: the picker set to its orientation, one platform, its length.
    pub fn build_rail_station(&mut self, site: &Site) -> Result<(), String> {
        let ways = self.mcp.read("openttd://map/ways")?;
        let axis_bit = if site.along_x { 1 } else { 2 };
        if site.tiles().iter().all(|&(x, y)| rail_bits(&ways, x, y) & axis_bit != 0) {
            self.say(&format!("a station already stands at {},{}", site.x, site.y));
            return Ok(());
        }
        let rail = self.open_rail_toolbar()?;
        let b = Self::buttons(&rail, STATION_TOOL).first().cloned().ok_or("no station button")?;
        if b.get("lowered") != Some(&Value::Bool(true)) {
            self.click(b.get("rect").and_then(Target::from_rect).ok_or("station button has no rectangle")?)?;
        }
        let w = self.wait_holding(ORIENTATION)?;
        self.choose_button(&w, ORIENTATION, if site.along_x { 0 } else { 1 })?;
        let w = self.wait_holding(ORIENTATION)?;
        self.choose_button(&w, PLATFORMS, 0)?;
        let w = self.wait_holding(ORIENTATION)?;
        self.choose_button(&w, LENGTH, site.length.clamp(1, 7) - 1)?;
        self.say(&format!("rail station at {},{} ({}, {} tiles)", site.x, site.y, if site.along_x { "along x" } else { "along y" }, site.length));
        for attempt in 0..2 {
            self.click_tile(site.x, site.y)?;
            self.think(500.0, 900.0);
            let ways = self.mcp.read("openttd://map/ways")?;
            if site.tiles().iter().all(|&(x, y)| rail_bits(&ways, x, y) & axis_bit != 0) {
                return Ok(());
            }
            // The game says why it refused (an error message window); with no word from it the click
            // never reached the map, so clear the screen and click once more.
            if let Some(why) = self.game_error()? {
                return Err(format!("the station at {},{} was refused: {why}", site.x, site.y));
            }
            if attempt == 0 {
                self.say("the station did not appear and the game said nothing: clear the screen, click again");
                self.tidy_except(&["railway construction", "rail station"])?;
            }
        }
        Err(format!("the station at {},{} was not built", site.x, site.y))
    }

    /// The text of an error message the game shows right now ("Can't build ... here"), if any.
    pub fn game_error(&self) -> Result<Option<String>, String> {
        for w in self.windows()? {
            let texts: Vec<String> = w.get("widgets").and_then(Value::as_array).cloned().unwrap_or_default().iter()
                .filter_map(|x| x.get("text").and_then(Value::as_str).map(str::to_string)).collect();
            if texts.iter().any(|t| t.to_lowercase().contains("can't")) {
                return Ok(Some(texts.join(" ").chars().filter(|c| !('\u{e000}'..='\u{f8ff}').contains(c)).collect::<String>().trim().to_string()));
            }
        }
        Ok(None)
    }

    /// Ask AlgoGlyph's track planner for the route between two station sites.
    pub fn plan_track(&mut self, a: &Site, b: &Site) -> Result<(Value, Vec<Piece>, Vec<Span>), String> {
        let mut input = json!({
            "from": {"x": a.end.0, "y": a.end.1, "dir": a.exit.name()},
            "to": {"x": b.end.0, "y": b.end.1, "dir": b.exit.reverse().name()},
        });
        for (key, uri) in TRACK_INPUTS {
            if let Ok(data) = self.mcp.read(uri) {
                input[key] = data;
            }
        }
        let mut flow = Flow::from_env().ok_or("set MCP_ALGOGLYPH_DIR to an AlgoGlyph checkout with the track-planning kernel")?;
        let plan = flow.kernel("track-planning", input)?;
        let (p, spans) = plan_parts(&plan)?;
        Ok((plan, p, spans))
    }

    /// Lay planned pieces with the game's tools, then lay again once what the game does not have.
    pub fn lay_track(&mut self, pieces: &[Piece]) -> Result<(), String> {
        // Only what the game does not have yet: laying a piece twice looks like rebuilding it.
        let todo = missing(pieces, &self.mcp.read("openttd://map/ways")?);
        let strokes = strokes(&todo, 10);
        self.say(&format!("track: {} pieces in {} strokes", pieces.len(), strokes.len()));
        for s in &strokes {
            if let Err(e) = self.stroke(s) {
                if crate::economy::game_closed(&e) {
                    return Err(e);
                }
                self.say(&format!("stroke {s:?}: {e}"));
            }
        }
        let left = missing(pieces, &self.mcp.read("openttd://map/ways")?);
        if !left.is_empty() {
            self.say(&format!("{} piece(s) missing; laying them one by one", left.len()));
            for p in &left {
                let _ = self.stroke(&Stroke::Click { tool: tool_of(p.track), at: (p.x, p.y), track: p.track });
            }
        }
        let left = missing(pieces, &self.mcp.read("openttd://map/ways")?);
        if !left.is_empty() {
            return Err(format!("{} of {} piece(s) are not on the map, first at {},{}", left.len(), pieces.len(), left[0].x, left[0].y));
        }
        Ok(())
    }

    /// A bridge (drag from head to head, then pick a bridge in the list) or a tunnel (click its
    /// entrance; the game digs to the far side), checked in the game's crossings.
    pub fn build_span(&mut self, s: &Span) -> Result<(), String> {
        if has_span(&self.mcp.read("openttd://map/ways")?, s) {
            return Ok(());
        }
        let rail = self.open_rail_toolbar()?;
        let tool = Self::buttons(&rail, if s.tunnel { TUNNEL_TOOL } else { BRIDGE_TOOL }).first().cloned().ok_or("no bridge or tunnel button")?;
        if tool.get("lowered") != Some(&Value::Bool(true)) {
            self.click(tool.get("rect").and_then(Target::from_rect).ok_or("tool has no rectangle")?)?;
        }
        if s.tunnel {
            self.say(&format!("tunnel from {},{} to {},{}", s.from.0, s.from.1, s.to.0, s.to.1));
            self.click_tile(s.from.0, s.from.1)?;
        } else {
            self.say(&format!("bridge from {},{} to {},{}", s.from.0, s.from.1, s.to.0, s.to.1));
            self.focus((s.from.0 + s.to.0) / 2, (s.from.1 + s.to.1) / 2)?;
            let a = self.spot(s.from.0, s.from.1)?.ok_or("the bridge's first head is covered")?;
            let b = self.spot(s.to.0, s.to.1)?.ok_or("the bridge's far head is covered")?;
            self.drag(a, b)?;
            // The game asks which bridge: the list's first row (it builds on the click).
            let started = std::time::Instant::now();
            while started.elapsed() < std::time::Duration::from_secs(3) {
                if let Some(w) = self.windows()?.into_iter().find(|w| !Self::buttons(w, BRIDGE_LIST).is_empty()) {
                    let list = Self::buttons(&w, BRIDGE_LIST).first().cloned().ok_or("no bridge list")?;
                    let r = list.get("rect").cloned().unwrap_or_default();
                    let row = list.get("row_height").and_then(Value::as_i64).unwrap_or(20) as i32;
                    let first = Target {
                        x: r.get("cx").and_then(Value::as_i64).unwrap_or(0) as i32,
                        y: r.get("y").and_then(Value::as_i64).unwrap_or(0) as i32 + row / 2,
                        w: r.get("w").and_then(Value::as_i64).unwrap_or(100) as i32 / 2,
                        h: (row - 4).max(4),
                    };
                    self.say("pick the first bridge in the list");
                    self.raise_if_covered(&w, first)?;
                    self.click(first)?;
                    break;
                }
                std::thread::sleep(std::time::Duration::from_millis(150));
            }
        }
        self.think(500.0, 900.0);
        if !has_span(&self.mcp.read("openttd://map/ways")?, s) {
            return Err(match self.game_error()? {
                Some(why) => format!("the {} from {},{} was refused: {why}", if s.tunnel { "tunnel" } else { "bridge" }, s.from.0, s.from.1),
                None => format!("the {} from {},{} to {},{} is not on the map", if s.tunnel { "tunnel" } else { "bridge" }, s.from.0, s.from.1, s.to.0, s.to.1),
            });
        }
        Ok(())
    }

    fn stroke(&mut self, s: &Stroke) -> Result<(), String> {
        match *s {
            Stroke::Drag { tool, from, to } => {
                self.track_tool(tool)?;
                self.focus((from.0 + to.0) / 2, (from.1 + to.1) / 2)?;
                let (a, b) = match (self.spot(from.0, from.1)?, self.spot(to.0, to.1)?) {
                    (Some(a), Some(b)) => (a, b),
                    _ => {
                        self.tidy_except(&["railway construction"])?;
                        (self.spot(from.0, from.1)?.ok_or("run start covered")?, self.spot(to.0, to.1)?.ok_or("run end covered")?)
                    }
                };
                self.drag(a, b)?;
            }
            Stroke::Click { tool, at, track } => {
                self.track_tool(tool)?;
                let t = self.reveal(at.0, at.1)?;
                self.click(half_target(t, track))?;
            }
        }
        self.think(150.0, 350.0);
        Ok(())
    }

    /// A whole line between a candidate's two sites: both stations, the planned track, checked.
    pub fn build_rail_line(&mut self, stations: &Value) -> Result<Line, String> {
        let a = stations.get("from").and_then(Site::from_value).ok_or("the candidate has no from site")?;
        let b = stations.get("to").and_then(Site::from_value).ok_or("the candidate has no to site")?;
        let (_, track, spans) = self.plan_track(&a, &b)?;
        self.tell(&format!("rail line {},{} -> {},{}: {} pieces of track, {} bridge(s) or tunnel(s)", a.x, a.y, b.x, b.y, track.len(), spans.len()));
        self.build_rail_station(&a)?;
        self.build_rail_station(&b)?;
        for s in &spans {
            self.build_span(s)?;
        }
        self.lay_track(&track)?;
        let _ = self.escape();
        // The line is only a line when the game's own rails join the two stations.
        if !rail_connected(&self.mcp.read("openttd://map/ways")?, &a.tiles(), &b.tiles()) {
            return Err("every piece is down, but the rails do not join the two stations".into());
        }
        Ok(Line { from: a, to: b, pieces: track })
    }

    /// A depot behind a station, built with the depot tool facing the platform.
    pub fn build_rail_depot(&mut self, site: &Site) -> Result<(usize, usize), String> {
        let (tile, facing) = site.depot().ok_or("the station has no tile behind it")?;
        // A depot of this company already there (openttd://depots) is the one to use.
        let me = self.state()?.get("local_company").and_then(Value::as_u64);
        let have = self.mcp.read("openttd://depots").ok().and_then(|d| d.get("depots").and_then(Value::as_array).cloned()).unwrap_or_default().iter().any(|d| {
            d.get("x").and_then(Value::as_u64) == Some(tile.0 as u64) && d.get("y").and_then(Value::as_u64) == Some(tile.1 as u64)
                && d.get("type").and_then(Value::as_str) == Some("rail") && d.get("owner").and_then(Value::as_u64) == me
        });
        if have {
            self.say(&format!("our train depot already stands at {},{}", tile.0, tile.1));
            return Ok(tile);
        }
        if !free_flat(&self.mcp.read("openttd://map")?, tile.0, tile.1) {
            return Err(format!("the tile behind the station, {},{}, is not free and flat", tile.0, tile.1));
        }
        let rail = self.open_rail_toolbar()?;
        let b = Self::buttons(&rail, DEPOT_TOOL).first().cloned().ok_or("no depot button")?;
        if b.get("lowered") != Some(&Value::Bool(true)) {
            self.click(b.get("rect").and_then(Target::from_rect).ok_or("depot button has no rectangle")?)?;
        }
        let w = self.wait_holding(DEPOT_ORIENTATION)?;
        self.choose_button(&w, DEPOT_ORIENTATION, facing.index())?;
        self.say(&format!("train depot at {},{} facing {}", tile.0, tile.1, facing.name()));
        self.click_tile(tile.0, tile.1)?;
        self.think(500.0, 900.0);
        let ways = self.mcp.read("openttd://map/ways")?;
        if rail_bits(&ways, tile.0, tile.1) == 0 || !rail_connected(&ways, &[tile], &site.tiles()) {
            return Err(format!("the depot at {},{} was not built onto the platform", tile.0, tile.1));
        }
        let _ = self.escape();
        Ok(tile)
    }

    /// In the open train shop: filter by a name's first word, pick the first row, buy `times`.
    fn buy_in_shop(&mut self, name: &str, times: usize) -> Result<(), String> {
        let word = name.split_whitespace().next().unwrap_or(name).to_string();
        self.say(&format!("filter the list: {word}"));
        self.fill_edit(TRAIN_SHOP, &word)?;
        let shop = self.wait_window(TRAIN_SHOP)?;
        let list = Self::widget_of_type(&shop, "matrix").ok_or("the shop has no list")?;
        let r = list.get("rect").cloned().unwrap_or_default();
        let row = list.get("row_height").and_then(Value::as_i64).unwrap_or(20) as i32;
        let first = Target {
            x: r.get("cx").and_then(Value::as_i64).unwrap_or(0) as i32,
            y: r.get("y").and_then(Value::as_i64).unwrap_or(0) as i32 + row / 2,
            w: r.get("w").and_then(Value::as_i64).unwrap_or(100) as i32 / 2,
            h: (row - 4).max(4),
        };
        self.say(&format!("pick {name}"));
        self.raise_if_covered(&shop, first)?;
        self.click(first)?;
        self.think(300.0, 600.0);
        let shop = self.wait_window(TRAIN_SHOP)?;
        if Self::widget_saying(&shop, crate::play::BUY).and_then(|b| b.get("disabled").cloned()) == Some(Value::Bool(true)) {
            return Err(format!("nothing is selected in the shop after picking {name}"));
        }
        for _ in 0..times {
            self.click_in(&shop, crate::play::BUY)?;
            self.think(500.0, 900.0);
        }
        Ok(())
    }

    /// Buy a train in a depot (the engine, then its wagons, which the game couples to it), give it
    /// orders to the two stations, start it, and check each step on the game's own vehicle list.
    pub fn buy_train(&mut self, depot: (usize, usize), stations: [(usize, usize); 2], engine: &str, wagon: &str, wagons: usize) -> Result<u64, String> {
        self.escape()?;
        let depot_window = self.open_depot(depot)?;
        self.think(300.0, 600.0);
        self.click_in(&depot_window, NEW_TRAIN)?;
        self.think(400.0, 800.0);
        // An engine already standing alone in this depot (an earlier attempt that stopped half way)
        // gets the wagons: a second one would also keep the game from coupling them to either.
        let waiting = self.mcp.read("openttd://vehicles")?.get("vehicles").and_then(Value::as_array).cloned().unwrap_or_default().into_iter()
            .find(|v| v.get("type").and_then(Value::as_str) == Some("train") && v.get("in_depot") == Some(&Value::Bool(true))
                && v.get("x").and_then(Value::as_u64) == Some(depot.0 as u64) && v.get("y").and_then(Value::as_u64) == Some(depot.1 as u64)
                && self.own_vehicles().is_ok_and(|own| v.get("id").and_then(Value::as_u64).is_some_and(|id| own.contains(&id))))
            .and_then(|v| v.get("id").and_then(Value::as_u64));
        let train = match waiting {
            Some(id) => {
                self.say(&format!("train {id} already waits in the depot: give it the wagons"));
                id
            }
            None => {
                let before = self.own_vehicles()?;
                self.buy_in_shop(engine, 1)?;
                self.own_vehicles()?.into_iter().find(|v| !before.contains(v)).ok_or("the engine was not bought (money? the list?)")?
            }
        };
        self.buy_in_shop(wagon, wagons)?;
        let parts = self.vehicle(train)?.get("parts").and_then(Value::as_u64).unwrap_or(0);
        if parts < 1 + wagons as u64 {
            return Err(format!("the train has {parts} part(s), not an engine and {wagons} wagon(s)"));
        }
        for needle in [TRAIN_SHOP, "depot"] {
            if let Some(w) = self.window(needle)? {
                self.close(&w)?;
                self.think(150.0, 300.0);
            }
        }
        let started = std::time::Instant::now();
        let view = loop {
            if let Some(w) = self.window_except("train #", &["orders"])? {
                break w;
            }
            if started.elapsed() > std::time::Duration::from_secs(5) {
                return Err("the new train's window did not open".into());
            }
            std::thread::sleep(std::time::Duration::from_millis(150));
        };
        self.click_in(&view, crate::play::ORDERS)?;
        self.think(400.0, 700.0);
        for (i, s) in stations.iter().enumerate() {
            // Go To must be down before the station is clicked (a click elsewhere releases it), and
            // each order must show up on the train before the next.
            for attempt in 0..3 {
                let orders = self.wait_window("(orders)")?;
                let down = Self::widget_saying(&orders, crate::play::GO_TO).and_then(|w| w.get("lowered").cloned()) == Some(Value::Bool(true));
                if !down {
                    self.click_in(&orders, crate::play::GO_TO)?;
                    self.think(300.0, 500.0);
                }
                self.say(&format!("order {}: the station at {},{}", i + 1, s.0, s.1));
                self.click_tile(s.0, s.1)?;
                self.think(400.0, 700.0);
                let n = self.vehicle(train)?.get("orders").and_then(Value::as_array).map(Vec::len).unwrap_or(0);
                if n > i {
                    break;
                }
                // The click opened the station's window instead: close it and arm Go To again.
                let _ = self.tidy_except(&["(orders)", "train #", "railway construction"]);
                if attempt == 2 {
                    return Err(format!("order {} to the station at {},{} was not added", i + 1, s.0, s.1));
                }
            }
        }
        self.escape()?;
        let n = self.vehicle(train)?.get("orders").and_then(Value::as_array).map(Vec::len).unwrap_or(0);
        if n < 2 {
            return Err(format!("the train has {n} order(s), not 2"));
        }
        for _ in 0..2 {
            let view = self.window_except("train #", &["orders"])?.ok_or("the train's window closed")?;
            self.say("start the train");
            self.click_in(&view, crate::play::START_STOP)?;
            self.think(600.0, 1000.0);
            if self.vehicle(train)?.get("stopped") == Some(&Value::Bool(false)) {
                break;
            }
        }
        if self.vehicle(train)?.get("stopped") != Some(&Value::Bool(false)) {
            return Err("the train did not start".into());
        }
        self.tidy()?;
        Ok(train)
    }

    /// A whole rail service from one planner candidate: the line, then a depot behind its first
    /// station and the train the planner priced it with, running between both stations.
    pub fn rail_service(&mut self, c: &Value) -> Result<Service, String> {
        let name = |k: &str| c.pointer(k).and_then(Value::as_str).map(str::to_string);
        let label = format!("{} -> {}", name("/producer/name").or(name("/a/name")).unwrap_or_else(|| "?".into()),
            name("/acceptor/name").or(name("/b/name")).unwrap_or_else(|| "?".into()));
        self.say(&format!("rail: {label} ({} tiles)", c["distance"]));
        let line = self.build_rail_line(&c["stations"])?;
        let vehicle = c.pointer("/options/0/vehicles/name").and_then(Value::as_str).unwrap_or("");
        let (engine, wagon, wagons) = consist(vehicle).ok_or_else(|| format!("the planner names no train for this line ('{vehicle}')"))?;
        let depot = self.build_rail_depot(&line.from)?;
        let train = self.buy_train(depot, [(line.from.x, line.from.y), (line.to.x, line.to.y)], &engine, &wagon, wagons)?;
        self.tell(&format!("train {train} runs {label}: {engine} with {wagons} x {wagon}"));
        Ok(Service { label, line, depot, train, consist: format!("{engine} + {wagons} x {wagon}") })
    }

    /// The first of these candidates that builds into a running service; the others are skipped
    /// with the reason, and a closed game ends the attempt at once.
    pub fn first_rail_service(&mut self, candidates: &[Value]) -> Result<Service, String> {
        for c in candidates {
            match self.rail_service(c) {
                Ok(s) => return Ok(s),
                Err(e) if crate::economy::game_closed(&e) => return Err(e),
                Err(e) => {
                    self.say(&format!("rail candidate skipped: {e}"));
                    let _ = self.escape();
                    let _ = self.tidy();
                }
            }
        }
        Err("no rail candidate could be built".into())
    }

    /// Rail candidates from the shared planner (MCP_PLAN), shortest first, each with both sites.
    pub fn rail_candidates(&self, max_distance: f64) -> Result<Vec<Value>, String> {
        let bin = std::env::var("MCP_PLAN").ok().filter(|b| std::path::Path::new(b).exists()).ok_or("set MCP_PLAN to the mcp-plan binary")?;
        let out = std::process::Command::new(&bin)
            .args(["--token", self.mcp.token_file(), "--params", r#"{"modes":["rail","freight_rail"]}"#, "--limit", "40"])
            .output().map_err(|e| format!("mcp-plan: {e}"))?;
        if !out.status.success() {
            return Err(format!("mcp-plan: {}", String::from_utf8_lossy(&out.stderr).trim()));
        }
        let v: Value = serde_json::from_slice(&out.stdout).map_err(|e| format!("mcp-plan: {e}"))?;
        let mut list: Vec<Value> = ["freight", "pairs"].iter()
            .flat_map(|k| v.get(*k).and_then(Value::as_array).cloned().unwrap_or_default())
            .filter(|c| c.pointer("/stations/from/x").is_some() && c.pointer("/stations/to/x").is_some())
            .filter(|c| c.get("distance").and_then(Value::as_f64).unwrap_or(f64::MAX) <= max_distance)
            .collect();
        list.sort_by(|p, q| p["distance"].as_f64().unwrap_or(0.0).total_cmp(&q["distance"].as_f64().unwrap_or(0.0)));
        Ok(list)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: usize, y: usize, track: u32) -> Piece { Piece { x, y, track } }

    #[test]
    fn straight_runs_are_dragged_and_half_tiles_clicked() {
        let pieces = [p(10, 5, 1), p(9, 5, 1), p(8, 5, 1), p(7, 5, 4), p(7, 6, 8), p(6, 6, 2)];
        assert_eq!(strokes(&pieces, 10), vec![
            Stroke::Drag { tool: 1, from: (10, 5), to: (8, 5) },
            Stroke::Click { tool: 2, at: (7, 5), track: 4 },
            Stroke::Click { tool: 2, at: (7, 6), track: 8 },
            Stroke::Click { tool: 3, at: (6, 6), track: 2 },
        ]);
        // A long run is split so both ends fit on screen.
        let long: Vec<Piece> = (0..25).map(|i| p(30 - i, 5, 1)).collect();
        assert_eq!(strokes(&long, 10).len(), 3);
    }

    #[test]
    fn a_site_and_its_tiles_come_from_the_planner() {
        let v = json!({"axis": "NW_SE", "end": {"x": 126, "y": 52}, "exit": {"dir": "SE", "x": 126, "y": 53}, "length": 3, "tile": 1, "x": 126, "y": 50});
        let s = Site::from_value(&v).unwrap();
        assert_eq!(s.tiles(), vec![(126, 50), (126, 51), (126, 52)]);
        assert_eq!(s.exit, Heading::SE);
        assert_eq!(s.exit.reverse(), Heading::NW);
        // The planner's depot, when given, is used as it is.
        let mut v = v.clone();
        v["depot"] = json!({"x": 126, "y": 49, "tile": 2, "dir": "SE"});
        assert_eq!(Site::from_value(&v).unwrap().depot(), Some(((126, 49), Heading::SE)));
    }

    #[test]
    fn missing_pieces_are_read_from_the_games_rail_bits() {
        // Row 0: tile 0 has X (01), tile 1 has upper+lower (0c); row 1: nothing.
        let ways = json!({"rails": ["010c", "0000"]});
        let want = [p(0, 0, 1), p(1, 0, 4), p(1, 0, 16), p(0, 1, 2)];
        assert_eq!(missing(&want, &ways), vec![p(1, 0, 16), p(0, 1, 2)]);
    }

    #[test]
    fn a_line_is_joined_only_through_matching_sides() {
        // Row 0: X, X, upper. Row 1: nothing, nothing, Y. Tile (2,0) upper reaches NW and NE only,
        // so (1,0)'s X joins it at its NE side, but (2,1)'s Y does not reach it (needs SE).
        let ways = json!({"rails": ["010104", "000002"]});
        assert!(rail_connected(&ways, &[(0, 0)], &[(2, 0)]));
        assert!(!rail_connected(&ways, &[(0, 0)], &[(2, 1)]));
        let joined = json!({"rails": ["010120", "000002"]}); // (2,0) right: NE and SE
        assert!(rail_connected(&joined, &[(0, 0)], &[(2, 1)]));
    }

    #[test]
    fn a_depot_goes_behind_the_station_facing_it() {
        // Along y, north tile (126, 50), end (126, 52) facing SE: the back end is (126, 50), so the
        // depot is at (126, 49) with its entrance facing SE, onto the platform.
        let s = Site { x: 126, y: 50, along_x: false, length: 3, end: (126, 52), exit: Heading::SE, planned_depot: None };
        assert_eq!(s.depot(), Some(((126, 49), Heading::SE)));
        let t = Site { x: 95, y: 167, along_x: true, length: 3, end: (97, 167), exit: Heading::SW, planned_depot: None };
        assert_eq!(t.depot(), Some(((94, 167), Heading::SW)));
        assert_eq!(consist("Ginzu 'A4' (Steam) + 4 x Coal Truck"), Some(("Ginzu 'A4' (Steam)".into(), "Coal Truck".into(), 4)));
    }

    #[test]
    fn a_half_tile_is_aimed_on_its_own_half() {
        let t = Target { x: 100, y: 100, w: 64, h: 32 };
        let up = half_target(t, 4);
        assert!(up.y < 100 && up.y + up.h / 2 < 100, "the whole upper target is above the centre");
        let left = half_target(t, 16);
        assert!(left.x < 100 && left.x + left.w / 2 < 100);
    }

    #[test]
    fn a_plan_splits_into_track_and_spans() {
        let found = json!({"result": {"status": "found", "pieces": [
            {"kind": "track", "x": 1, "y": 2, "track": 1},
            {"kind": "bridge", "from": [2, 2], "to": {"x": 6, "y": 2}, "heading": "SW", "axis": "x", "length": 3, "z": 1},
            {"kind": "track", "x": 7, "y": 2, "track": 1}]}});
        let (t, s) = plan_parts(&found).unwrap();
        assert_eq!(t, vec![p(1, 2, 1), p(7, 2, 1)]);
        assert_eq!(s, vec![Span { tunnel: false, from: (2, 2), to: (6, 2) }]);
        assert!(plan_parts(&json!({"result": {"status": "found", "pieces": [{"kind": "warp"}]}})).is_err());
        assert!(pieces(&json!({"result": {"status": "unreachable"}})).is_err());
    }

    #[test]
    fn a_bridge_joins_its_heads_in_the_walk() {
        // X track at 0 and 1, a bridge from 1 to 5 (no rail bits between), X track at 5 and 6.
        let ways = json!({"rails": ["01010000000101"], "crossings": [{"kind": "bridge", "transport": "rail", "from": {"x": 1, "y": 0}, "to": {"x": 5, "y": 0}}]});
        assert!(rail_connected(&ways, &[(0, 0)], &[(6, 0)]));
        assert!(has_span(&ways, &Span { tunnel: false, from: (5, 0), to: (1, 0) }));
        let none = json!({"rails": ["01010000000101"], "crossings": []});
        assert!(!rail_connected(&none, &[(0, 0)], &[(6, 0)]));
    }
}
