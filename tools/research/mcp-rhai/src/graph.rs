//! RESEARCH-ONLY: the road network of a running OpenTTD game as a graph, derived from the MCP
//! resources openttd://map, openttd://towns and openttd://stations.
//!
//! Network tiles are road ('r'), bridge or tunnel ends ('B') and station tiles ('S'), connected
//! through their four neighbours. Nodes are the tiles where the network is not a plain corridor
//! (junctions and dead ends), every station tile, and each town's network tile nearest its centre.
//! Each chain of corridor tiles between two nodes becomes one edge whose length is its tile count.
//! Components are counted over all network tiles.
use std::collections::{BTreeMap, BTreeSet, HashMap};

use serde_json::{json, Value};

/// The road network as a graph, from real road ways when `ways` (openttd://map/ways) is given: a tile is on the network
/// when a road vehicle can leave it, and two tiles are joined only when each points at the other
/// (NW=1 y-1, SW=2 x+1, SE=4 y+1, NE=8 x-1). Without it, adjacency is inferred from tile types.
pub fn derive_with_ways(map: &Value, towns: &Value, stations: &Value, ways: &Value) -> Result<Value, String> {
    let w = map.get("width").and_then(Value::as_u64).ok_or("map has no width")? as usize;
    let h = map.get("height").and_then(Value::as_u64).ok_or("map has no height")? as usize;
    let rows = map.get("types").and_then(Value::as_array).ok_or("map has no types")?;
    let mut net = vec![false; w * h];
    let mut kind = vec![b' '; w * h];
    for (y, row) in rows.iter().enumerate().take(h) {
        let row = row.as_str().unwrap_or("").as_bytes();
        for (x, c) in row.iter().enumerate().take(w) {
            kind[y * w + x] = *c;
            net[y * w + x] = matches!(c, b'r' | b'B' | b'S');
        }
    }
    // Real road directions per tile, when the game publishes them.
    let road_rows = ways.get("roads").and_then(Value::as_array);
    let mut dirs = vec![0u8; w * h];
    let real = road_rows.is_some();
    if let Some(rows) = road_rows {
        for (y, row) in rows.iter().enumerate().take(h) {
            for (x, c) in row.as_str().unwrap_or("").chars().enumerate().take(w) {
                let d = c.to_digit(16).unwrap_or(0) as u8;
                dirs[y * w + x] = d;
                net[y * w + x] = d != 0;
            }
        }
    }
    let neighbours = |t: usize| -> Vec<usize> {
        let (x, y) = (t % w, t / w);
        let mut n = Vec::with_capacity(4);
        if real {
            let d = dirs[t];
            if d & 1 != 0 && y > 0 && dirs[t - w] & 4 != 0 { n.push(t - w); }
            if d & 2 != 0 && x + 1 < w && dirs[t + 1] & 8 != 0 { n.push(t + 1); }
            if d & 4 != 0 && y + 1 < h && dirs[t + w] & 1 != 0 { n.push(t + w); }
            if d & 8 != 0 && x > 0 && dirs[t - 1] & 2 != 0 { n.push(t - 1); }
            return n;
        }
        if x > 0 && net[t - 1] { n.push(t - 1); }
        if x + 1 < w && net[t + 1] { n.push(t + 1); }
        if y > 0 && net[t - w] { n.push(t - w); }
        if y + 1 < h && net[t + w] { n.push(t + w); }
        n
    };

    // Nodes: junctions, dead ends, stations, and the town anchors.
    let mut nodes: BTreeMap<usize, (String, String)> = BTreeMap::new(); // tile -> (kind, label)
    let mut road_tiles = 0usize;
    for t in 0..w * h {
        if !net[t] { continue; }
        road_tiles += 1;
        let d = neighbours(t).len();
        if kind[t] == b'S' {
            nodes.insert(t, ("station".into(), String::new()));
        } else if d != 2 {
            nodes.insert(t, (if d == 0 || d == 1 { "end" } else { "junction" }.into(), String::new()));
        }
    }
    for st in stations.pointer("/stations").and_then(Value::as_array).into_iter().flatten() {
        if let Some(t) = st.get("tile").and_then(Value::as_u64) {
            let t = t as usize;
            if t < w * h && net[t] {
                nodes.insert(t, ("station".into(), st.get("name").and_then(Value::as_str).unwrap_or("").into()));
            }
        }
    }
    let mut town_anchor: Vec<(usize, Value)> = Vec::new();
    for town in towns.pointer("/towns").and_then(Value::as_array).into_iter().flatten() {
        let (tx, ty) = (town.get("x").and_then(Value::as_u64).unwrap_or(0) as i64, town.get("y").and_then(Value::as_u64).unwrap_or(0) as i64);
        // The network tile nearest the centre, within 12 tiles.
        let mut best: Option<(i64, usize)> = None;
        for dy in -12i64..=12 {
            for dx in -12i64..=12 {
                let (x, y) = (tx + dx, ty + dy);
                if x < 0 || y < 0 || x as usize >= w || y as usize >= h { continue; }
                let t = y as usize * w + x as usize;
                if net[t] && best.map_or(true, |(d, _)| dx.abs() + dy.abs() < d) { best = Some((dx.abs() + dy.abs(), t)); }
            }
        }
        if let Some((_, t)) = best {
            let label = town.get("name").and_then(Value::as_str).unwrap_or("").to_string();
            nodes.insert(t, ("town".into(), label));
            town_anchor.push((t, town.clone()));
        }
    }

    // Edges: walk each corridor from every node until the next node.
    let mut edges: BTreeSet<(usize, usize, usize)> = BTreeSet::new();
    for &start in nodes.keys() {
        for first in neighbours(start) {
            let (mut prev, mut cur, mut len) = (start, first, 1usize);
            let mut guard = 0usize;
            while !nodes.contains_key(&cur) && guard < w * h {
                let next: Vec<usize> = neighbours(cur).into_iter().filter(|&n| n != prev).collect();
                if next.is_empty() { break; }
                prev = cur;
                cur = next[0];
                len += 1;
                guard += 1;
            }
            if nodes.contains_key(&cur) && cur != start {
                let (a, b) = if start < cur { (start, cur) } else { (cur, start) };
                edges.insert((a, b, len));
            }
        }
    }

    // Components over all network tiles (union-find).
    let mut parent: HashMap<usize, usize> = HashMap::new();
    fn find(p: &mut HashMap<usize, usize>, x: usize) -> usize {
        let mut r = x;
        while p[&r] != r { r = p[&r]; }
        let mut c = x;
        while p[&c] != r { let n = p[&c]; p.insert(c, r); c = n; }
        r
    }
    for t in 0..w * h { if net[t] { parent.insert(t, t); } }
    for t in 0..w * h {
        if !net[t] { continue; }
        for n in neighbours(t) {
            let (a, b) = (find(&mut parent, t), find(&mut parent, n));
            if a != b { parent.insert(a, b); }
        }
    }
    let mut comp_of_node: BTreeMap<usize, usize> = BTreeMap::new();
    let mut comp_ids: BTreeMap<usize, usize> = BTreeMap::new();
    for &t in nodes.keys() {
        let r = find(&mut parent, t);
        let next = comp_ids.len();
        let id = *comp_ids.entry(r).or_insert(next);
        comp_of_node.insert(t, id);
    }
    let all_components: BTreeSet<usize> = (0..w * h).filter(|&t| net[t]).map(|t| find(&mut parent, t)).collect();

    let node_list: Vec<Value> = nodes.iter().map(|(&t, (k, label))| json!({
        "id": t, "tile": t, "x": t % w, "y": t / w, "kind": k, "label": label, "component": comp_of_node[&t]
    })).collect();
    let edge_list: Vec<Value> = edges.iter().map(|&(a, b, len)| json!({"source": a, "target": b, "length": len})).collect();
    // Town to town reachability over the road network: which towns share a component.
    let reach: Vec<Value> = town_anchor.iter().map(|(t, town)| json!({
        "town": town.get("id"), "name": town.get("name"), "anchor": t, "component": comp_of_node.get(t)
    })).collect();
    Ok(json!({
        "derived_from": if real { vec!["openttd://map/ways", "openttd://towns", "openttd://stations"] } else { vec!["openttd://map", "openttd://towns", "openttd://stations"] },
        "adjacency": if real { "real road ways: joined when each tile points at the other" } else { "inferred from tile types (road, bridge, station)" },
        "width": w, "height": h,
        "stats": {"road_tiles": road_tiles, "nodes": node_list.len(), "edges": edge_list.len(),
                  "components": all_components.len()},
        "nodes": node_list, "edges": edge_list, "towns": reach,
    }))
}
