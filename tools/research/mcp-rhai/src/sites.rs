//! RESEARCH-ONLY: where a rail station for a planned pair can go, by the same rule PrismaticAI uses
//! (bridge repository rail.nut BuildRailStationAt), so the rule lives in one place and an executor
//! (the bot, or openttd-07's person-like player laying AlgoGlyph track-planning pieces) consumes it.
//!
//! Rule: the station's axis points at the other end (along x when the ends differ more in x); the
//! whole rectangle is buildable (clear land or trees) and flat (all four corner heights equal, from
//! openttd://map heights, which are each tile's north corner); the tile beyond the end that faces the
//! other end (the exit, where the line starts) is free and flat too. Among such sites, the one
//! nearest the industry or town wins, preferring one whose depot tile is free as well.
//!
//! Depot: the tile beyond the station's back end (opposite the exit), free and flat, with the depot's
//! entrance facing the station. A rail station connects at both ends of its axis, so the depot joins
//! the station track directly and a train leaves it straight onto the platform. Its `dir` is the side
//! the depot opens to, which is the same as the exit's (both point from the back toward the far end).
//! `depot` is null when no site near the industry or town has that tile free.
use serde_json::{json, Value};

struct MapView<'a> { w: usize, h: usize, types: Vec<&'a [u8]>, heights: Vec<&'a [u8]> }

impl<'a> MapView<'a> {
    fn new(map: &'a Value) -> Option<MapView<'a>> {
        let w = map.get("width")?.as_u64()? as usize;
        let h = map.get("height")?.as_u64()? as usize;
        let types: Vec<&[u8]> = map.get("types")?.as_array()?.iter().filter_map(|r| r.as_str().map(str::as_bytes)).collect();
        let heights: Vec<&[u8]> = map.get("heights")?.as_array()?.iter().filter_map(|r| r.as_str().map(str::as_bytes)).collect();
        if types.len() < h || heights.len() < h { return None; }
        Some(MapView { w, h, types, heights })
    }
    fn z(&self, x: usize, y: usize) -> Option<u32> {
        let row = self.heights.get(y)?;
        let s = std::str::from_utf8(row.get(2 * x..2 * x + 2)?).ok()?;
        u32::from_str_radix(s, 16).ok()
    }
    /// Buildable and flat: clear or trees, and the four corner heights equal.
    fn free_flat(&self, x: usize, y: usize) -> bool {
        if x + 1 >= self.w || y + 1 >= self.h { return false; }
        if !matches!(self.types[y].get(x), Some(b'C') | Some(b'T')) { return false; }
        let z = self.z(x, y);
        z.is_some() && z == self.z(x + 1, y) && z == self.z(x, y + 1) && z == self.z(x + 1, y + 1)
    }
}

/// A station site near (ox, oy) whose axis points at (tx, ty): {tile, x, y, axis, length, end, exit,
/// depot} with exit {x, y, dir}, dir being the side the line leaves by (NE x-1, SE y+1, SW x+1,
/// NW y-1), and depot {x, y, tile, dir} or null.
pub fn site(map: &Value, ox: i64, oy: i64, tx: i64, ty: i64, len: usize) -> Option<Value> {
    let m = MapView::new(map)?;
    let (dx, dy) = (tx - ox, ty - oy);
    let along_x = dx.abs() >= dy.abs();
    let forward = if along_x { dx >= 0 } else { dy >= 0 };
    let mut best: Option<(i64, Value)> = None;
    for sy in (oy - 10)..=(oy + 10) {
        for sx in (ox - 10)..=(ox + 10) {
            if sx < 2 || sy < 2 { continue; }
            let tiles: Vec<(i64, i64)> = (0..len as i64).map(|i| if along_x { (sx + i, sy) } else { (sx, sy + i) }).collect();
            if !tiles.iter().all(|&(x, y)| m.free_flat(x as usize, y as usize)) { continue; }
            let end = if forward { *tiles.last()? } else { tiles[0] };
            let (ex, ey, dir) = match (along_x, forward) {
                (true, true) => (end.0 + 1, end.1, "SW"),
                (true, false) => (end.0 - 1, end.1, "NE"),
                (false, true) => (end.0, end.1 + 1, "SE"),
                (false, false) => (end.0, end.1 - 1, "NW"),
            };
            if ex < 1 || ey < 1 || !m.free_flat(ex as usize, ey as usize) { continue; }
            // The exit must lead on (as PrismaticAI's ExitLeadsOn): straight ahead or to either side
            // is land (clear or trees) or a road the line may cross. A station between houses whose
            // exit faces only a junction is a dead end (seed 20208, Ganingbury).
            let (ox2, oy2) = (ex - end.0, ey - end.1);
            let onward = [(ex + ox2, ey + oy2), (ex + oy2, ey + ox2), (ex - oy2, ey - ox2)];
            if !onward.iter().any(|&(nx, ny)| nx >= 1 && ny >= 1 && (nx as usize) < m.w && (ny as usize) < m.h
                && matches!(m.types[ny as usize].get(nx as usize), Some(b'C') | Some(b'T') | Some(b'r'))) { continue; }
            // The back end and the tile beyond it, on the side away from the exit.
            let back = if forward { tiles[0] } else { *tiles.last()? };
            let (bx, by) = match (along_x, forward) {
                (true, true) => (back.0 - 1, back.1),
                (true, false) => (back.0 + 1, back.1),
                (false, true) => (back.0, back.1 - 1),
                (false, false) => (back.0, back.1 + 1),
            };
            let depot = (bx >= 1 && by >= 1 && m.free_flat(bx as usize, by as usize))
                .then(|| json!({"x": bx, "y": by, "tile": by * m.w as i64 + bx, "dir": dir}));
            // Nearest first; a site without a depot tile counts as farther than any site with one.
            let d = (sx - ox).abs() + (sy - oy).abs() + if depot.is_some() { 0 } else { 1000 };
            if best.as_ref().map_or(true, |(bd, _)| d < *bd) {
                best = Some((d, json!({
                    "tile": sy * m.w as i64 + sx, "x": sx, "y": sy, "length": len,
                    "axis": if along_x { "NE_SW" } else { "NW_SE" },
                    "end": {"x": end.0, "y": end.1}, "exit": {"x": ex, "y": ey, "dir": dir},
                    "depot": depot,
                })));
            }
        }
    }
    best.map(|(_, v)| v)
}

#[cfg(test)]
mod tests {
    use super::site;
    use serde_json::json;
    #[test]
    fn a_flat_field_gets_a_station_whose_exit_faces_the_other_end() {
        let row_t = "C".repeat(30);
        let row_h = "00".repeat(30);
        let map = json!({"width": 30, "height": 30, "types": vec![row_t; 30], "heights": vec![row_h; 30]});
        let s = site(&map, 10, 10, 25, 11, 3).expect("a site");
        assert_eq!(s["axis"], "NE_SW");
        assert_eq!(s["exit"]["dir"], "SW");
        assert_eq!(s["exit"]["x"].as_i64().unwrap(), s["end"]["x"].as_i64().unwrap() + 1);
        // The depot is behind the back end (x - 1 of the first tile), opening toward the station.
        assert_eq!(s["depot"]["x"].as_i64().unwrap(), s["x"].as_i64().unwrap() - 1);
        assert_eq!(s["depot"]["y"], s["y"]);
        assert_eq!(s["depot"]["dir"], "SW");
    }

    #[test]
    fn a_site_whose_back_tile_is_free_wins_over_a_nearer_one_without() {
        // A water column at x = 9 blocks the depot tile of every site starting at x = 10.
        let rows_t: Vec<String> = (0..30).map(|_| (0..30).map(|x| if x == 9 { 'W' } else { 'C' }).collect()).collect();
        let row_h = "00".repeat(30);
        let map = json!({"width": 30, "height": 30, "types": rows_t, "heights": vec![row_h; 30]});
        let s = site(&map, 10, 10, 25, 10, 3).expect("a site");
        let (sx, dx) = (s["x"].as_i64().unwrap(), s["depot"]["x"].as_i64().unwrap());
        assert_eq!(dx, sx - 1);
        assert_ne!(dx, 9);
    }

    #[test]
    fn a_site_whose_exit_leads_nowhere_is_not_taken() {
        // Houses fill everything except a pocket at x 10..14, y 10: a station there would exit into
        // a house at x 14; the site must be elsewhere (the open field from x 20 on).
        let rows_t: Vec<String> = (0..30).map(|y| (0..30).map(|x| if x >= 20 || (y == 10 && (10..14).contains(&x)) { 'C' } else { 'H' }).collect()).collect();
        let row_h = "00".repeat(30);
        let map = json!({"width": 30, "height": 30, "types": rows_t, "heights": vec![row_h; 30]});
        let s = site(&map, 10, 10, 25, 10, 3).expect("a site in the field");
        assert!(s["x"].as_i64().unwrap() >= 20);
    }

    #[test]
    fn a_reversed_station_has_its_depot_beyond_the_far_side() {
        let row_t = "C".repeat(30);
        let row_h = "00".repeat(30);
        let map = json!({"width": 30, "height": 30, "types": vec![row_t; 30], "heights": vec![row_h; 30]});
        let s = site(&map, 20, 10, 5, 10, 3).expect("a site");
        assert_eq!(s["exit"]["dir"], "NE");
        assert_eq!(s["depot"]["x"].as_i64().unwrap(), s["x"].as_i64().unwrap() + 3);
        assert_eq!(s["depot"]["dir"], "NE");
    }
}
