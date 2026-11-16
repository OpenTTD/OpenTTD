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
        })
    }

    /// Every tile of the station.
    pub fn tiles(&self) -> Vec<(usize, usize)> {
        (0..self.length).map(|i| if self.along_x { (self.x + i, self.y) } else { (self.x, self.y + i) }).collect()
    }
}

/// One planned piece of track: a tile and its track bit (X=1, Y=2, upper=4, lower=8, left=16, right=32).
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Piece { pub x: usize, pub y: usize, pub track: u32 }

/// The pieces of a found plan; a plan with bridges or tunnels is refused until the player builds them.
pub fn pieces(plan: &Value) -> Result<Vec<Piece>, String> {
    let r = plan.get("result").unwrap_or(plan);
    if r.get("status").and_then(Value::as_str) != Some("found") {
        return Err(format!("the track planner found no route ({})", r.get("status").and_then(Value::as_str).unwrap_or("no status")));
    }
    let list = r.get("pieces").and_then(Value::as_array).ok_or("the plan has no pieces")?;
    list.iter().map(|p| {
        let kind = p.get("kind").and_then(Value::as_str).unwrap_or("");
        if kind != "track" {
            return Err(format!("the plan has a {kind}; this player lays plain track only so far"));
        }
        Ok(Piece {
            x: p.get("x").and_then(Value::as_u64).ok_or("piece without x")? as usize,
            y: p.get("y").and_then(Value::as_u64).ok_or("piece without y")? as usize,
            track: p.get("track").and_then(Value::as_u64).ok_or("piece without track")? as u32,
        })
    }).collect()
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
    let mut seen: std::collections::HashSet<(usize, usize)> = from.iter().copied().collect();
    let mut queue: Vec<(usize, usize)> = from.to_vec();
    while let Some((x, y)) = queue.pop() {
        if to.contains(&(x, y)) {
            return true;
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
        self.click_tile(site.x, site.y)?;
        self.think(500.0, 900.0);
        let ways = self.mcp.read("openttd://map/ways")?;
        if site.tiles().iter().any(|&(x, y)| rail_bits(&ways, x, y) & axis_bit == 0) {
            return Err(format!("the station at {},{} was not built", site.x, site.y));
        }
        Ok(())
    }

    /// Ask AlgoGlyph's track planner for the route between two station sites.
    pub fn plan_track(&mut self, a: &Site, b: &Site) -> Result<(Value, Vec<Piece>), String> {
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
        let p = pieces(&plan)?;
        Ok((plan, p))
    }

    /// Lay planned pieces with the game's tools, then lay again once what the game does not have.
    pub fn lay_track(&mut self, pieces: &[Piece]) -> Result<(), String> {
        let strokes = strokes(pieces, 10);
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
        let (_, track) = self.plan_track(&a, &b)?;
        self.tell(&format!("rail line {},{} -> {},{}: {} pieces of track", a.x, a.y, b.x, b.y, track.len()));
        self.build_rail_station(&a)?;
        self.build_rail_station(&b)?;
        self.lay_track(&track)?;
        let _ = self.escape();
        // The line is only a line when the game's own rails join the two stations.
        if !rail_connected(&self.mcp.read("openttd://map/ways")?, &a.tiles(), &b.tiles()) {
            return Err("every piece is down, but the rails do not join the two stations".into());
        }
        Ok(Line { from: a, to: b, pieces: track })
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
    fn a_half_tile_is_aimed_on_its_own_half() {
        let t = Target { x: 100, y: 100, w: 64, h: 32 };
        let up = half_target(t, 4);
        assert!(up.y < 100 && up.y + up.h / 2 < 100, "the whole upper target is above the centre");
        let left = half_target(t, 16);
        assert!(left.x < 100 && left.x + left.w / 2 < 100);
    }

    #[test]
    fn only_plain_track_is_laid_so_far() {
        let found = json!({"result": {"status": "found", "pieces": [{"kind": "track", "x": 1, "y": 2, "track": 1}]}});
        assert_eq!(pieces(&found).unwrap(), vec![p(1, 2, 1)]);
        let bridge = json!({"result": {"status": "found", "pieces": [{"kind": "bridge", "x": 1, "y": 2}]}});
        assert!(pieces(&bridge).unwrap_err().contains("bridge"));
        assert!(pieces(&json!({"result": {"status": "unreachable"}})).is_err());
    }
}
