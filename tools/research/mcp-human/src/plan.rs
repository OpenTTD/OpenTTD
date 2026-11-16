//! Road routes over the map the game publishes (openttd://map): A* over tiles, preferring
//! existing roads and straight runs, then cut into the straight segments a person drags with the
//! road tool. Generic graph search on the derived map; it reads nothing but the resource.

use serde_json::Value;
use std::cmp::Ordering;
use std::collections::{BinaryHeap, HashMap};

/// The map as the planner needs it: one type character and one height per tile.
pub struct Grid {
    pub width: usize,
    pub height: usize,
    types: Vec<u8>,
    heights: Vec<u8>,
}

impl Grid {
    pub fn from_resource(map: &Value) -> Result<Self, String> {
        let width = map.get("width").and_then(Value::as_u64).ok_or("map has no width")? as usize;
        let height = map.get("height").and_then(Value::as_u64).ok_or("map has no height")? as usize;
        let rows = |key: &str| -> Result<Vec<String>, String> {
            map.get(key).and_then(Value::as_array)
                .map(|a| a.iter().filter_map(|r| r.as_str().map(str::to_string)).collect())
                .ok_or(format!("map has no {key}"))
        };
        let mut types = Vec::with_capacity(width * height);
        for r in rows("types")? {
            types.extend(r.bytes());
        }
        let mut heights = Vec::with_capacity(width * height);
        for r in rows("heights")? {
            let b = r.as_bytes();
            for i in (0..b.len()).step_by(2) {
                heights.push(u8::from_str_radix(std::str::from_utf8(&b[i..i + 2]).unwrap_or("0"), 16).unwrap_or(0));
            }
        }
        if types.len() != width * height || heights.len() != width * height {
            return Err("map rows do not match its size".into());
        }
        Ok(Grid { width, height, types, heights })
    }

    pub fn kind(&self, x: usize, y: usize) -> u8 {
        self.types[y * self.width + x]
    }

    fn h(&self, x: usize, y: usize) -> i32 {
        self.heights[y * self.width + x] as i32
    }

    /// The cost of putting a road on a tile; None where a road cannot go.
    pub fn cost(&self, x: usize, y: usize) -> Option<u32> {
        if x == 0 || y == 0 || x + 1 >= self.width || y + 1 >= self.height {
            return None;
        }
        // A tile is steep when its corners differ by two levels; the published height is the
        // north corner, so the four corner heights are this tile's and its +x, +y, +x+y neighbours'.
        let c = [self.h(x, y), self.h(x + 1, y), self.h(x, y + 1), self.h(x + 1, y + 1)];
        let steep = c.iter().max().unwrap() - c.iter().min().unwrap() >= 2;
        match self.kind(x, y) {
            b'r' => Some(2),
            b'C' if !steep => Some(10),
            b'T' if !steep => Some(13),
            _ => None,
        }
    }
}

#[derive(Copy, Clone, Eq, PartialEq)]
struct Node {
    f: u32,
    g: u32,
    at: usize,
    dir: u8,
}

impl Ord for Node {
    fn cmp(&self, o: &Self) -> Ordering {
        o.f.cmp(&self.f).then_with(|| o.g.cmp(&self.g))
    }
}

impl PartialOrd for Node {
    fn partial_cmp(&self, o: &Self) -> Option<Ordering> {
        Some(self.cmp(o))
    }
}

/// The cheapest road route from `a` to `b` (tile x/y), or None. Turns cost extra, so the route
/// is made of long straight runs, as a person would lay it.
pub fn route(g: &Grid, a: (usize, usize), b: (usize, usize)) -> Option<Vec<(usize, usize)>> {
    const TURN: u32 = 25;
    let dirs: [(i64, i64); 4] = [(1, 0), (-1, 0), (0, 1), (0, -1)];
    let idx = |x: usize, y: usize| y * g.width + x;
    let hcost = |x: usize, y: usize| ((x as i64 - b.0 as i64).abs() + (y as i64 - b.1 as i64).abs()) as u32 * 2;
    let passable = |x: usize, y: usize| (x, y) == a || (x, y) == b || g.cost(x, y).is_some();
    if !passable(a.0, a.1) || !passable(b.0, b.1) {
        return None;
    }
    let mut open = BinaryHeap::new();
    // State is (tile, direction): a turn is a property of how a tile was entered.
    let mut best: HashMap<(usize, u8), u32> = HashMap::new();
    let mut came: HashMap<(usize, u8), (usize, u8)> = HashMap::new();
    let start = idx(a.0, a.1);
    for d in 0..4u8 {
        best.insert((start, d), 0);
        open.push(Node { f: hcost(a.0, a.1), g: 0, at: start, dir: d });
    }
    let goal = idx(b.0, b.1);
    let mut expanded = 0usize;
    while let Some(Node { g: cost, at, dir, .. }) = open.pop() {
        if at == goal {
            let mut path = vec![(at % g.width, at / g.width)];
            let mut key = (at, dir);
            while let Some(&prev) = came.get(&key) {
                path.push((prev.0 % g.width, prev.0 / g.width));
                key = prev;
            }
            path.reverse();
            return Some(path);
        }
        if best.get(&(at, dir)).is_some_and(|&c| c < cost) {
            continue;
        }
        expanded += 1;
        if expanded > 400_000 {
            return None;
        }
        let (x, y) = (at % g.width, at / g.width);
        for (nd, (dx, dy)) in dirs.iter().enumerate() {
            let (nx, ny) = (x as i64 + dx, y as i64 + dy);
            if nx < 0 || ny < 0 || nx as usize >= g.width || ny as usize >= g.height {
                continue;
            }
            let (nx, ny) = (nx as usize, ny as usize);
            let step = if (nx, ny) == b { 1 } else {
                match g.cost(nx, ny) {
                    Some(c) => c,
                    None => continue,
                }
            };
            let nc = cost + step + if nd as u8 != dir && at != start { TURN } else { 0 };
            let key = (idx(nx, ny), nd as u8);
            if best.get(&key).is_none_or(|&c| nc < c) {
                best.insert(key, nc);
                came.insert(key, (at, dir));
                open.push(Node { f: nc + hcost(nx, ny), g: nc, at: key.0, dir: nd as u8 });
            }
        }
    }
    None
}

/// Cut a route into straight runs (start, end), each one road-tool drag.
pub fn runs(path: &[(usize, usize)]) -> Vec<((usize, usize), (usize, usize))> {
    let mut out = Vec::new();
    if path.len() < 2 {
        return out;
    }
    let mut start = path[0];
    for i in 1..path.len() - 1 {
        let (p, c, n) = (path[i - 1], path[i], path[i + 1]);
        let along_x = p.1 == c.1 && c.1 == n.1;
        let along_y = p.0 == c.0 && c.0 == n.0;
        if !along_x && !along_y {
            out.push((start, c));
            start = c;
        }
    }
    out.push((start, *path.last().unwrap()));
    out
}

/// Are two tiles already joined by road (roads, stations and bridges, four-connected)?
pub fn connected(g: &Grid, a: (usize, usize), b: (usize, usize)) -> bool {
    let ok = |x: usize, y: usize| matches!(g.kind(x, y), b'r' | b'S' | b'B');
    if !ok(a.0, a.1) || !ok(b.0, b.1) {
        return false;
    }
    let mut seen = vec![false; g.width * g.height];
    let mut stack = vec![a];
    seen[a.1 * g.width + a.0] = true;
    while let Some((x, y)) = stack.pop() {
        if (x, y) == b {
            return true;
        }
        let n = [(x.wrapping_sub(1), y), (x + 1, y), (x, y.wrapping_sub(1)), (x, y + 1)];
        for (nx, ny) in n {
            if nx >= g.width || ny >= g.height || seen[ny * g.width + nx] || !ok(nx, ny) {
                continue;
            }
            seen[ny * g.width + nx] = true;
            stack.push((nx, ny));
        }
    }
    false
}

/// Which way a straight road runs on a tile: along x, along y, or neither (a bend, a junction).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Axis {
    X,
    Y,
}

fn road(g: &Grid, x: usize, y: usize) -> bool {
    x < g.width && y < g.height && g.kind(x, y) == b'r'
}

/// Tiles around a centre, nearest first (rings of growing Manhattan radius).
fn around(c: (usize, usize), r0: usize, r1: usize) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    for r in r0..=r1 {
        for dx in -(r as i64)..=r as i64 {
            let dy = r as i64 - dx.abs();
            for sy in if dy == 0 { vec![0] } else { vec![dy, -dy] } {
                let (x, y) = (c.0 as i64 + dx, c.1 as i64 + sy);
                if x > 0 && y > 0 {
                    out.push((x as usize, y as usize));
                }
            }
        }
    }
    out
}

/// A straight piece of road near a town centre for a drive-through stop, and its axis.
pub fn stop_site(g: &Grid, centre: (usize, usize), avoid: &[(usize, usize)]) -> Option<((usize, usize), Axis)> {
    for (x, y) in around(centre, 1, 7) {
        if x + 1 >= g.width || y + 1 >= g.height || !road(g, x, y) || avoid.contains(&(x, y)) {
            continue;
        }
        let along_x = road(g, x - 1, y) && road(g, x + 1, y) && !road(g, x, y - 1) && !road(g, x, y + 1);
        let along_y = road(g, x, y - 1) && road(g, x, y + 1) && !road(g, x - 1, y) && !road(g, x + 1, y);
        if along_x {
            return Some(((x, y), Axis::X));
        }
        if along_y {
            return Some(((x, y), Axis::Y));
        }
    }
    None
}

/// A buildable tile next to a road near a centre for a depot, and which side faces the road:
/// 0 north-east (-x), 1 south-east (+y), 2 south-west (+x), 3 north-west (-y), the order of the
/// depot picker's orientation buttons.
/// Tiles in `avoid` (sites already tried) are skipped.
pub fn depot_site_avoiding(g: &Grid, centre: (usize, usize), avoid: &[(usize, usize)]) -> Option<((usize, usize), usize)> {
    for (x, y) in around(centre, 2, 9) {
        if avoid.contains(&(x, y)) || x + 1 >= g.width || y + 1 >= g.height || g.kind(x, y) != b'C' || g.cost(x, y).is_none() {
            continue;
        }
        for (side, (nx, ny)) in [(x - 1, y), (x, y + 1), (x + 1, y), (x, y - 1)].into_iter().enumerate() {
            if road(g, nx, ny) {
                return Some(((x, y), side));
            }
        }
    }
    None
}

/// The road ways of the map as the game publishes them (openttd://map/ways): per tile, the
/// directions a road vehicle can leave it, NW=1 (y-1), SW=2 (x+1), SE=4 (y+1), NE=8 (x-1).
pub struct Ways {
    pub width: usize,
    pub height: usize,
    bits: Vec<u8>,
}

impl Ways {
    pub fn from_resource(v: &Value) -> Result<Self, String> {
        let width = v.get("width").and_then(Value::as_u64).ok_or("ways have no width")? as usize;
        let height = v.get("height").and_then(Value::as_u64).ok_or("ways have no height")? as usize;
        let mut bits = Vec::with_capacity(width * height);
        for row in v.get("roads").and_then(Value::as_array).ok_or("ways have no roads")? {
            for c in row.as_str().unwrap_or("").chars() {
                bits.push(c.to_digit(16).unwrap_or(0) as u8);
            }
        }
        if bits.len() != width * height {
            return Err("road rows do not match the map size".into());
        }
        Ok(Ways { width, height, bits })
    }

    /// The directions a road vehicle can leave a tile (0 when it has no road way at all).
    pub fn at(&self, x: usize, y: usize) -> u8 {
        if x < self.width && y < self.height { self.bits[y * self.width + x] } else { 0 }
    }

    /// Can a road vehicle drive from one tile to the other? Two tiles are joined only when each
    /// points at the other, which is what the game's own pathfinder requires.
    pub fn drivable(&self, a: (usize, usize), b: (usize, usize)) -> bool {
        let mut seen = vec![false; self.width * self.height];
        let mut stack = vec![a];
        while let Some((x, y)) = stack.pop() {
            if (x, y) == b {
                return true;
            }
            if x >= self.width || y >= self.height || seen[y * self.width + x] {
                continue;
            }
            seen[y * self.width + x] = true;
            let here = self.at(x, y);
            // (bit here, neighbour, bit the neighbour needs back)
            let steps = [(1u8, (x, y.wrapping_sub(1)), 4u8), (2, (x + 1, y), 8), (4, (x, y + 1), 1), (8, (x.wrapping_sub(1), y), 2)];
            for (bit, (nx, ny), back) in steps {
                if here & bit != 0 && self.at(nx, ny) & back != 0 {
                    stack.push((nx, ny));
                }
            }
        }
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    fn grid(rows: &[&str]) -> Grid {
        let w = rows[0].len();
        let heights: Vec<String> = rows.iter().map(|_| "00".repeat(w)).collect();
        Grid::from_resource(&json!({"width": w, "height": rows.len(), "types": rows, "heights": heights})).unwrap()
    }

    #[test]
    fn routes_around_houses_and_water() {
        let g = grid(&["VVVVVVVV", "VCCCCCCV", "VCHHHWCV", "VCCCCCCV", "VVVVVVVV"]);
        let p = route(&g, (1, 2), (6, 2)).expect("a route exists");
        assert!(p.iter().all(|&(x, y)| !matches!(g.kind(x, y), b'H' | b'W')));
        assert_eq!(p.first(), Some(&(1, 2)));
        assert_eq!(p.last(), Some(&(6, 2)));
    }

    #[test]
    fn runs_are_straight_and_join_end_to_start() {
        let path = vec![(1, 1), (2, 1), (3, 1), (3, 2), (3, 3), (4, 3)];
        let r = runs(&path);
        assert_eq!(r, vec![((1, 1), (3, 1)), ((3, 1), (3, 3)), ((3, 3), (4, 3))]);
    }

    #[test]
    fn connectivity_follows_roads_only() {
        let g = grid(&["VVVVVVV", "VrrrCrV", "VCCrCrV", "VCCrrrV", "VVVVVVV"]);
        assert!(connected(&g, (1, 1), (5, 1)));
        assert!(!connected(&g, (1, 1), (4, 1)));
    }

    #[test]
    fn a_stop_goes_on_a_straight_road_and_a_depot_faces_one() {
        let g = grid(&["VVVVVVVVV", "VCCCCCCCV", "VrrrrrrrV", "VCCCCCCCV", "VCCCCCCCV", "VVVVVVVVV"]);
        let (at, axis) = stop_site(&g, (4, 2), &[]).unwrap();
        assert_eq!(axis, Axis::X);
        assert_eq!(at.1, 2);
        let ((dx, dy), side) = depot_site_avoiding(&g, (4, 2), &[]).unwrap();
        assert_eq!(g.kind(dx, dy), b'C');
        let road_at = [(dx - 1, dy), (dx, dy + 1), (dx + 1, dy), (dx, dy - 1)][side];
        assert_eq!(g.kind(road_at.0, road_at.1), b'r');
    }

    #[test]
    fn road_bits_must_point_at_each_other() {
        // A straight x road from (0,0) to (2,0): ends SW, middle SW|NE, end NE.
        let w = Ways::from_resource(&json!({"width": 3, "height": 2, "roads": ["2a8", "000"]})).unwrap();
        assert!(w.drivable((0, 0), (2, 0)));
        // The middle piece only points one way: the road is broken.
        let broken = Ways::from_resource(&json!({"width": 3, "height": 2, "roads": ["228", "000"]})).unwrap();
        assert!(!broken.drivable((0, 0), (2, 0)));
    }

    #[test]
    fn prefers_existing_road() {
        let g = grid(&["VVVVVVV", "VCCCCCV", "VrrrrrV", "VCCCCCV", "VVVVVVV"]);
        let p = route(&g, (1, 1), (5, 3)).unwrap();
        assert!(p.iter().filter(|&&(x, y)| g.kind(x, y) == b'r').count() >= 3, "{p:?}");
    }
}
