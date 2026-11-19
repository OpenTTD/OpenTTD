//! The pseudo-economy: producer-consumer edges over the placed towns and industries, and the
//! chain completion that keeps it playable.
//!
//! Rule `nearest-consumer/v1`: each cargo an industry produces goes to its nearest
//! `max_consumers_per_cargo` consumers that accept it within `max_edge_tiles` (Manhattan), towns
//! accepting goods only from `goods_town_min_population`. Passengers link town pairs by a
//! gravity score (population product over squared distance). An edge is INFERRED (proximity is
//! a judgement, not a recorded flow) unless an end is HYPOTHETICAL, then HYPOTHETICAL.
//!
//! Rule `complete-chain/v1`: a produced cargo with no consumer gets one HYPOTHETICAL consumer
//! (the first profile role, by name, accepting it) at the centroid of its producers; a consumer
//! with no supplier of any input gets one HYPOTHETICAL producer of its first input, offset from it.
//! Both are marked as invented for playability.

use crate::geo::manhattan;
use crate::model::*;
use crate::rules::Rules;

/// The connectivity basis of an edge: mapped railway stations near each end.
fn station_note(r: &Rules, stations: &[Tile], a: &Entity, b: &Entity) -> String {
    let near = |t: &Tile| stations.iter().filter(|s| manhattan(s, t) <= r.economy.station_radius_tiles).count();
    format!("; mapped railway stations within {} tiles: {} at the source, {} at the consumer", r.economy.station_radius_tiles, near(&a.tile), near(&b.tile))
}

fn accepts(e: &Entity, cargo: &str, r: &Rules) -> bool {
    if e.game.kind == "town" {
        if cargo == "GOOD" {
            return e.population.unwrap_or(0) >= r.economy.goods_town_min_population;
        }
        return false; // passengers and mail are linked town to town below
    }
    e.game.accepts.iter().any(|c| c == cargo)
}

fn free_tile(near: &Tile, taken: &[Tile], spacing: u32, size: u32, border: u32) -> Tile {
    // Deterministic outward ring search for a tile at least `spacing` from every taken tile.
    let lo = border as i64;
    let hi = (size - 1 - border) as i64;
    for rad in 0i64..64 {
        for dx in -rad..=rad {
            for dy in -rad..=rad {
                if dx.abs().max(dy.abs()) != rad {
                    continue;
                }
                let (x, y) = (near.x as i64 + dx * 2, near.y as i64 + dy * 2);
                if x < lo || y < lo || x > hi || y > hi {
                    continue;
                }
                let t = Tile { x: x as u32, y: y as u32 };
                if taken.iter().all(|o| manhattan(o, &t) >= spacing) {
                    return t;
                }
            }
        }
    }
    near.clone()
}

fn hypothetical(id: String, role: &str, r: &Rules, tile: Tile, why: String) -> Entity {
    let p = &r.profile.industries[role];
    Entity {
        id,
        name: format!("{role} (hypothetical)"),
        game: GameType { kind: "industry".into(), role: role.into(), produces: p.produces.clone(), accepts: p.accepts.clone() },
        tile,
        evidence_feature: None,
        anchor_feature: None,
        source_label: None,
        evidence: Evidence::Hypothetical,
        confidence: 0.0,
        rationale: why,
        rule: r.rule_ref("economy.complete-chain"),
        population: None,
        population_basis: None,
        city: false,
        site: None,
    }
}

/// Edges of the current node set, by the nearest-consumer rule.
fn cargo_edges(r: &Rules, nodes: &[&Entity], stations: &[Tile]) -> Vec<Edge> {
    let mut out = Vec::new();
    for p in nodes.iter().filter(|e| e.game.kind == "industry") {
        for cargo in &p.game.produces {
            let mut cons: Vec<(&&Entity, u32)> = nodes
                .iter()
                .filter(|c| c.id != p.id && accepts(c, cargo, r))
                .map(|c| (c, manhattan(&p.tile, &c.tile)))
                .filter(|(_, d)| *d <= r.economy.max_edge_tiles)
                .collect();
            cons.sort_by(|a, b| a.1.cmp(&b.1).then(a.0.id.cmp(&b.0.id)));
            for (c, d) in cons.into_iter().take(r.economy.max_consumers_per_cargo) {
                let hyp = p.evidence == Evidence::Hypothetical || c.evidence == Evidence::Hypothetical;
                out.push(Edge {
                    id: format!("{}>{}>{}", p.id, cargo, c.id),
                    from: p.id.clone(),
                    to: c.id.clone(),
                    cargo: cargo.clone(),
                    distance_tiles: d,
                    basis: format!("nearest consumer accepting {cargo} ({} rule nearest-consumer/v1), {d} tiles{}", r.id, station_note(r, stations, p, c)),
                    evidence: if hyp { Evidence::Hypothetical } else { Evidence::Inferred },
                    confidence: if hyp { 0.0 } else { round_to((p.confidence.min(c.confidence) * (1.0 - d as f64 / (2.0 * r.economy.max_edge_tiles as f64))).max(0.05), 2) },
                });
            }
        }
    }
    out
}

pub fn build(r: &Rules, size: u32, stations: &[Tile], towns: &[Entity], inds: &mut Vec<Entity>, diag: &mut Vec<String>) -> Economy {
    let size_border = |e: &[Entity]| e.iter().map(|x| x.tile.clone()).collect::<Vec<_>>();
    for _round in 0..3 {
        let nodes: Vec<&Entity> = towns.iter().chain(inds.iter()).collect();
        let edges = cargo_edges(r, &nodes, stations);
        let mut added = Vec::new();
        // Produced cargo with no consumer.
        let mut cargos: Vec<String> = inds.iter().flat_map(|e| e.game.produces.clone()).collect();
        cargos.sort();
        cargos.dedup();
        for cargo in cargos {
            let producers: Vec<&Entity> = inds.iter().filter(|e| e.game.produces.contains(&cargo)).collect();
            let consumed = producers.iter().any(|p| edges.iter().any(|e| e.from == p.id && e.cargo == cargo));
            if consumed {
                continue;
            }
            let Some((role, _)) = r.profile.industries.iter().find(|(_, x)| x.accepts.contains(&cargo)) else {
                diag.push(format!("{cargo}: produced but no profile role accepts it"));
                continue;
            };
            let cx = producers.iter().map(|p| p.tile.x as u64).sum::<u64>() / producers.len() as u64;
            let cy = producers.iter().map(|p| p.tile.y as u64).sum::<u64>() / producers.len() as u64;
            let mut taken = size_border(towns);
            taken.extend(size_border(inds));
            let tile = free_tile(&Tile { x: cx as u32, y: cy as u32 }, &taken, r.industry_spacing_tiles, size, r.border_tiles);
            let why = format!(
                "HYPOTHETICAL: added for playability because {} produced here has no consumer among the mapped features; \
                 no source records a {role} here",
                cargo
            );
            added.push(hypothetical(format!("industry:{role}:hypothetical-for-{}", cargo.to_lowercase()), role, r, tile, why));
        }
        // Consumers with no supplier of any input.
        for c in inds.iter().filter(|e| !e.game.accepts.is_empty()) {
            if edges.iter().any(|e| e.to == c.id) || added.iter().any(|a: &Entity| a.game.produces.iter().any(|p| c.game.accepts.contains(p))) {
                continue;
            }
            let input = &c.game.accepts[0];
            let Some((role, _)) = r.profile.industries.iter().find(|(_, x)| x.produces.contains(input)) else { continue };
            let id = format!("industry:{role}:hypothetical-for-{}", c.id.replace(':', "-"));
            if inds.iter().chain(added.iter()).any(|e| e.id == id) {
                continue;
            }
            let mut taken = size_border(towns);
            taken.extend(size_border(inds));
            taken.extend(added.iter().map(|a| a.tile.clone()));
            let near = Tile { x: c.tile.x.saturating_add(24).min(size - 1 - r.border_tiles), y: c.tile.y };
            let tile = free_tile(&near, &taken, r.industry_spacing_tiles, size, r.border_tiles);
            let why = format!("HYPOTHETICAL: added for playability because {} accepts {input} and no mapped feature supplies it; no source records a {role} here", c.name);
            added.push(hypothetical(id, role, r, tile, why));
        }
        if added.is_empty() {
            break;
        }
        for a in &added {
            diag.push(format!("{}: {}", a.id, a.rationale));
        }
        inds.extend(added);
        inds.sort_by(|a, b| a.id.cmp(&b.id));
    }

    let nodes: Vec<&Entity> = towns.iter().chain(inds.iter()).collect();
    let mut edges = cargo_edges(r, &nodes, stations);

    // Passengers: gravity between town pairs.
    let mut pairs = Vec::new();
    for (i, a) in towns.iter().enumerate() {
        for b in towns.iter().skip(i + 1) {
            let d = manhattan(&a.tile, &b.tile);
            if d < r.economy.passenger_min_tiles || d > r.economy.passenger_max_tiles {
                continue;
            }
            let score = a.population.unwrap_or(0) as f64 * b.population.unwrap_or(0) as f64 / (d as f64 * d as f64);
            pairs.push((score, a, b, d));
        }
    }
    pairs.sort_by(|x, y| y.0.partial_cmp(&x.0).unwrap_or(std::cmp::Ordering::Equal).then(x.1.id.cmp(&y.1.id)).then(x.2.id.cmp(&y.2.id)));
    // The best pairs overall, then the best pair of every town still without one (no town is left
    // without passengers to carry).
    let mut chosen: Vec<(f64, &Entity, &Entity, u32)> = pairs.iter().take(r.economy.passenger_pairs).cloned().collect();
    for t in towns {
        if chosen.iter().any(|(_, a, b, _)| a.id == t.id || b.id == t.id) {
            continue;
        }
        if let Some(p) = pairs.iter().find(|(_, a, b, _)| a.id == t.id || b.id == t.id) {
            chosen.push(*p);
        }
    }
    for (score, a, b, d) in chosen {
        edges.push(Edge {
            id: format!("{}>PASS>{}", a.id, b.id),
            from: a.id.clone(),
            to: b.id.clone(),
            cargo: "PASS".into(),
            distance_tiles: d,
            basis: format!("gravity score {:.1} (game populations over squared distance), {d} tiles, rule gravity/v1{}", score, station_note(r, stations, a, b)),
            evidence: Evidence::Inferred,
            confidence: round_to((a.confidence.min(b.confidence) * 0.8).max(0.05), 2),
        });
    }
    edges.sort_by(|a, b| a.id.cmp(&b.id));
    edges.dedup_by(|a, b| a.id == b.id);

    let isolated: Vec<String> = nodes
        .iter()
        .filter(|n| !edges.iter().any(|e| e.from == n.id || e.to == n.id))
        .map(|n| n.id.clone())
        .collect();
    let mut chains: Vec<String> = Vec::new();
    for e in &edges {
        let line = format!("{} -> {}", e.cargo, nodes.iter().find(|n| n.id == e.to).map(|n| if n.game.kind == "town" { "town".to_string() } else { n.game.role.clone() }).unwrap_or_default());
        if !chains.contains(&line) {
            chains.push(line);
        }
    }
    chains.sort();
    Economy { edges, isolated, chains }
}
