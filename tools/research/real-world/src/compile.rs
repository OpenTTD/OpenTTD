//! The offline, deterministic compiler: snapshot + rules -> heightmap, towns, industries,
//! landmarks and the economy graph. No network, no clock, no randomness, no hash-map iteration.

use crate::economy;
use crate::geo;
use crate::model::*;
use crate::net::sha256_hex;
use crate::rules::Rules;
use serde_json::json;
use std::collections::{BTreeMap, BTreeSet};

pub struct Compiled {
    pub scenario: Scenario,
    pub heightmap_png: Vec<u8>,
}

/// Fill missing samples from the mean of their valid 8-neighbours, repeatedly; deterministic.
fn fill_missing(grid: &[Option<i32>], n: usize) -> (Vec<f64>, usize) {
    let mut g: Vec<Option<f64>> = grid.iter().map(|x| x.map(|v| v as f64 / 10.0)).collect();
    let filled = g.iter().filter(|x| x.is_none()).count();
    for _ in 0..(2 * n) {
        if g.iter().all(|x| x.is_some()) {
            break;
        }
        let prev = g.clone();
        for j in 0..n {
            for i in 0..n {
                if prev[j * n + i].is_some() {
                    continue;
                }
                let mut s = 0.0;
                let mut c = 0;
                for dj in -1i64..=1 {
                    for di in -1i64..=1 {
                        let (y, x) = (j as i64 + dj, i as i64 + di);
                        if y < 0 || x < 0 || y >= n as i64 || x >= n as i64 {
                            continue;
                        }
                        if let Some(v) = prev[y as usize * n + x as usize] {
                            s += v;
                            c += 1;
                        }
                    }
                }
                if c > 0 {
                    g[j * n + i] = Some(s / c as f64);
                }
            }
        }
    }
    (g.into_iter().map(|x| x.unwrap_or(0.0)).collect(), filled)
}

/// Bilinear sample of the grid at fractional image position (row, col) in 0..1.
fn sample(g: &[f64], n: usize, row: f64, col: f64) -> f64 {
    let fy = (row * (n - 1) as f64).clamp(0.0, (n - 1) as f64);
    let fx = (col * (n - 1) as f64).clamp(0.0, (n - 1) as f64);
    let (y0, x0) = (fy.floor() as usize, fx.floor() as usize);
    let (y1, x1) = ((y0 + 1).min(n - 1), (x0 + 1).min(n - 1));
    let (ty, tx) = (fy - y0 as f64, fx - x0 as f64);
    let a = g[y0 * n + x0] * (1.0 - tx) + g[y0 * n + x1] * tx;
    let b = g[y1 * n + x0] * (1.0 - tx) + g[y1 * n + x1] * tx;
    a * (1.0 - ty) + b * ty
}

pub fn heightmap(s: &Snapshot, r: &Rules) -> (Vec<u8>, Heightmap, Vec<String>) {
    let n = s.elevation.grid as usize;
    let size = s.extent.map_size as usize;
    let (g, filled) = fill_missing(&s.elevation.decimetres, n);
    let emin = g.iter().cloned().fold(f64::INFINITY, f64::min);
    let emax = g.iter().cloned().fold(f64::NEG_INFINITY, f64::max);
    let range = (emax - emin).max(0.0);
    let levels = ((range / r.heightmap.metres_per_level).round() as u32 + 1).clamp(r.heightmap.min_levels, r.heightmap.max_levels);
    let mut px = vec![0u8; size * size];
    for row in 0..size {
        for col in 0..size {
            let e = sample(&g, n, (row as f64 + 0.5) / size as f64, (col as f64 + 0.5) / size as f64);
            // Grey 0 is sea in OpenTTD; land is 1..=255, linear in elevation.
            let grey = if range > 0.0 { 1.0 + ((e - emin) / range * 254.0).round() } else { 1.0 };
            px[row * size + col] = grey.clamp(1.0, 255.0) as u8;
        }
    }
    let png = crate::png::grey(size as u32, size as u32, &px);
    let spacing = s.extent.side_m / (n - 1) as f64;
    let mpl = if levels > 1 { range / (levels as f64 - 1.0) } else { 0.0 };
    let mut lost = vec![
        format!("elevation resampled bilinearly from a {n}x{n} grid ({spacing:.0} m spacing, sampled from {}) to {size}x{size} tiles of {:.1} m", s.elevation.source, s.extent.metres_per_tile),
        format!("elevation {emin:.1}..{emax:.1} m quantised to grey 1..255 and then by OpenTTD to {levels} height levels (~{mpl:.1} m a level)"),
        "OpenTTD smooths the heightmap so neighbouring tiles differ by at most one level (slopes are the engine's, not the source's)".into(),
    ];
    if filled > 0 {
        lost.push(format!("{filled} elevation sample(s) missing at the source, filled from their neighbours' mean"));
    }
    let hm = Heightmap {
        file: "heightmap.png".into(),
        sha256: sha256_hex(&png),
        width: size as u32,
        height: size as u32,
        min_elevation_m: round_to(emin, 1),
        max_elevation_m: round_to(emax, 1),
        levels,
        metres_per_level: round_to(mpl, 2),
        grey_rule: "grey = 1 + round((e - min) / (max - min) * 254); OpenTTD height = 1 + (grey - 1) * levels / 255 (src/heightmap.cpp)".into(),
    };
    (png, hm, lost)
}

fn parse_population(s: &str) -> Option<u64> {
    let t: String = s.chars().filter(|c| c.is_ascii_digit()).collect();
    if t.is_empty() || s.chars().any(|c| c.is_ascii_alphabetic()) {
        return None;
    }
    t.parse().ok()
}

/// OpenTTD's town and sign names hold at most 31 characters (`MAX_LENGTH_TOWN_NAME_CHARS`,
/// `MAX_LENGTH_SIGN_NAME_CHARS` = 32 with the terminator, src/town_type.h, src/signs_type.h).
pub const MAX_NAME_CHARS: usize = 31;

/// A name that fits the game: longer ones are cut to `max - 1` characters and end with an ellipsis.
pub fn fit_name(name: &str, max: usize, id: &str, diag: &mut Vec<String>) -> String {
    if name.chars().count() <= max {
        return name.to_string();
    }
    let short: String = name.chars().take(max - 1).collect::<String>().trim_end().to_string() + "…";
    diag.push(format!("{id}: name \"{name}\" shortened to \"{short}\" ({max} characters is the game's limit)"));
    short
}

fn unique_name(base: &str, used: &mut BTreeSet<String>) -> String {
    let mut name = base.to_string();
    let mut k = 2;
    while used.contains(&name) {
        name = format!("{base} ({k})");
        k += 1;
    }
    used.insert(name.clone());
    name
}

pub fn towns(s: &Snapshot, r: &Rules, diag: &mut Vec<String>) -> Vec<Entity> {
    let m = &r.map["settlement"];
    let sr = &r.settlement;
    let mut cands: Vec<(u64, bool, &Feature)> = Vec::new();
    let place_of = |f: &Feature| f.tags.get("place").cloned().unwrap_or_default();
    let wholes: Vec<&Feature> = s.features.iter()
        .filter(|f| f.class == "settlement" && !sr.part_of_city_places.contains(&place_of(f)))
        .collect();
    for f in s.features.iter().filter(|f| f.class == "settlement") {
        let place = f.tags.get("place").map(String::as_str).unwrap_or("");
        if sr.part_of_city_places.iter().any(|p| p == place) {
            // A suburb is part of a settlement, not a town of its own: assigned (in the diagnostics) to
            // the nearest `whole_places` settlement by local distance and never founded separately.
            // Its population is not added: a city's own population tag already counts its suburbs.
            let (x0, y0) = geo::project(&s.extent.crs.origin, &f.at);
            let near = wholes.iter().filter(|w| sr.whole_places.contains(&place_of(w))).min_by(|a, b| {
                let d = |w: &&&Feature| { let (x, y) = geo::project(&s.extent.crs.origin, &w.at); (x - x0).hypot(y - y0) };
                d(a).partial_cmp(&d(b)).unwrap_or(std::cmp::Ordering::Equal).then(a.id.cmp(&b.id))
            });
            diag.push(format!("{}: place={place} is part of {} ({}); not a town of its own (part_of_city_places)", f.id,
                near.and_then(|w| w.name.clone()).unwrap_or_else(|| "no city or town".into()), near.map(|w| w.id.as_str()).unwrap_or("-")));
            continue;
        }
        match f.tags.get("population").and_then(|p| parse_population(p)) {
            Some(p) => cands.push((p, true, f)),
            None => cands.push((*sr.default_population.get(place).unwrap_or(&500) as u64, false, f)),
        }
    }
    cands.sort_by(|a, b| b.0.cmp(&a.0).then(b.1.cmp(&a.1)).then(a.2.id.cmp(&b.2.id)));
    let mut out: Vec<Entity> = Vec::new();
    let mut used = BTreeSet::new();
    for (pop, observed, f) in cands {
        if out.len() >= sr.max_towns {
            diag.push(format!("{}: not a town, the map already has max_towns = {}", f.id, sr.max_towns));
            continue;
        }
        let Some(tile) = geo::tile_of(&s.extent, &f.at, r.border_tiles) else {
            diag.push(format!("{}: outside the map interior; not a town", f.id));
            continue;
        };
        if let Some(near) = out.iter().find(|t| geo::manhattan(&t.tile, &tile) < sr.min_spacing_tiles) {
            diag.push(format!("{}: within {} tiles of {} ({}); merged into it (min_spacing_tiles)", f.id, sr.min_spacing_tiles, near.name, near.id));
            continue;
        }
        let place = f.tags.get("place").cloned().unwrap_or_default();
        let base = f.name.clone().unwrap_or_else(|| format!("Unnamed {place}"));
        let base = fit_name(&base, MAX_NAME_CHARS - 4, &f.id, diag);
        let name = unique_name(&base, &mut used);
        let game_pop = ((sr.population_scale * (pop as f64).powf(sr.population_exponent)).round() as u32).clamp(sr.min_population, sr.max_population);
        let formula = format!("{} x pop^{} clamped to {}..{} (balance assumption)", sr.population_scale, sr.population_exponent, sr.min_population, sr.max_population);
        let basis = if observed {
            format!("OBSERVED population tag {pop}; game population {formula}")
        } else {
            format!("INFERRED: no population tag, default {pop} for place={place}; game population {formula}")
        };
        out.push(Entity {
            id: format!("town:{}", f.id),
            name,
            game: GameType { kind: "town".into(), role: String::new(), produces: r.profile.town.produces.clone(), accepts: r.profile.town.accepts.clone() },
            tile,
            evidence_feature: Some(f.id.clone()),
            anchor_feature: Some(f.id.clone()),
            source_label: Some(format!("settlement (place={place})")),
            evidence: if observed { m.evidence } else { Evidence::Inferred },
            confidence: if observed { m.confidence } else { round_to(m.confidence * 0.7, 2) },
            rationale: m.rationale.clone(),
            rule: r.rule_ref("map.settlement"),
            population: Some(game_pop),
            population_basis: Some(basis),
            city: pop >= sr.city_from_population as u64,
            site: None,
        });
    }
    out
}

/// "coal_mine" -> "Coal Mine".
pub fn role_title(role: &str) -> String {
    role.split('_').map(|w| { let mut c = w.chars(); c.next().map(|f| f.to_uppercase().collect::<String>() + c.as_str()).unwrap_or_default() }).collect::<Vec<_>>().join(" ")
}

fn label(f: &Feature, keys: &[&str]) -> String {
    let t: Vec<String> = keys.iter().filter_map(|k| f.tags.get(*k).map(|v| format!("{k}={v}"))).collect();
    format!("{} ({})", f.class, t.join(", "))
}

/// Landmark signs: every class the rules map to `sign`, named features only, ranked per class
/// (elevation for peaks and viewpoints, area otherwise), kept `sign_spacing_tiles` apart.
pub fn signs(s: &Snapshot, r: &Rules, diag: &mut Vec<String>) -> Vec<Entity> {
    let mut out: Vec<Entity> = Vec::new();
    for (class, m) in r.map.iter().filter(|(_, m)| m.kind == "sign") {
        let mut fs: Vec<&Feature> = s.features.iter().filter(|f| &f.class == class && f.name.is_some()).collect();
        let unnamed = s.features.iter().filter(|f| &f.class == class && f.name.is_none()).count();
        if unnamed > 0 {
            diag.push(format!("{unnamed} unnamed {class} feature(s) not signed"));
        }
        let ele = |f: &Feature| f.tags.get("ele").and_then(|e| e.parse::<f64>().ok()).unwrap_or(0.0);
        fs.sort_by(|a, b| {
            let (ka, kb) = (ele(a) + a.bbox_area_m2, ele(b) + b.bbox_area_m2);
            kb.partial_cmp(&ka).unwrap_or(std::cmp::Ordering::Equal).then(a.id.cmp(&b.id))
        });
        let mut taken = 0;
        for f in fs {
            if taken >= m.max {
                diag.push(format!("{}: not signed, {class} already has max = {}", f.id, m.max));
                continue;
            }
            let Some(tile) = geo::tile_of(&s.extent, &f.at, r.border_tiles) else { continue };
            if out.iter().any(|e| geo::manhattan(&e.tile, &tile) < r.sign_spacing_tiles) {
                diag.push(format!("{}: not signed, within {} tiles of another sign", f.id, r.sign_spacing_tiles));
                continue;
            }
            taken += 1;
            out.push(Entity {
                id: format!("sign:{}", f.id),
                name: fit_name(f.name.as_deref().unwrap(), MAX_NAME_CHARS, &f.id, diag),
                game: GameType { kind: "sign".into(), role: String::new(), produces: vec![], accepts: vec![] },
                tile,
                evidence_feature: Some(f.id.clone()),
                anchor_feature: Some(f.id.clone()),
                source_label: Some(label(f, &["natural", "tourism", "ele", "leisure", "boundary", "protect_class"])),
                evidence: m.evidence,
                confidence: m.confidence,
                rationale: m.rationale.clone(),
                rule: r.rule_ref(&format!("map.{class}")),
                population: None,
                population_basis: None,
                city: false,
                site: None,
            });
        }
    }
    out
}

pub fn industries(s: &Snapshot, r: &Rules, towns: &[Entity], diag: &mut Vec<String>) -> Vec<Entity> {
    let mut out: Vec<Entity> = Vec::new();
    for (class, m) in &r.map {
        if m.kind != "industry" {
            continue;
        }
        let role = &r.profile.industries[&m.role];
        let mut fs: Vec<&Feature> = s.features.iter().filter(|f| &f.class == class).collect();
        fs.sort_by(|a, b| b.bbox_area_m2.partial_cmp(&a.bbox_area_m2).unwrap_or(std::cmp::Ordering::Equal).then(a.id.cmp(&b.id)));
        let mut taken = 0;
        for f in fs {
            if taken >= m.max {
                break;
            }
            if f.tags.contains_key("disused") || f.tags.contains_key("abandoned") {
                diag.push(format!("{}: tagged disused/abandoned; not an industry", f.id));
                continue;
            }
            let Some(tile) = geo::tile_of(&s.extent, &f.at, r.border_tiles) else { continue };
            if out.iter().chain(towns.iter()).any(|e| geo::manhattan(&e.tile, &tile) < r.industry_spacing_tiles) {
                diag.push(format!("{}: within {} tiles of a town or industry already placed; skipped", f.id, r.industry_spacing_tiles));
                continue;
            }
            taken += 1;
            let title = role_title(&m.role);
            let label = label(f, &["landuse", "power", "plant:source", "man_made", "product", "craft", "industrial", "resource"]);
            out.push(if m.substitute {
                // A gameplay stand-in: positioned at the mapped site, evidenced by nothing, named for
                // what it is in the game and never after the site's operator.
                Entity {
                    id: format!("industry:{}:stand-in-at-{}", m.role, f.id.replace(':', "-")),
                    name: format!("{title} (gameplay stand-in at {} {})", f.class, f.id),
                    game: GameType { kind: "industry".into(), role: m.role.clone(), produces: role.produces.clone(), accepts: role.accepts.clone() },
                    tile,
                    evidence_feature: None,
                    anchor_feature: Some(f.id.clone()),
                    source_label: Some(label),
                    evidence: Evidence::Hypothetical,
                    confidence: 0.0,
                    rationale: format!("HYPOTHETICAL gameplay stand-in: {}", m.rationale),
                    rule: r.rule_ref(&format!("map.{class}")),
                    population: None,
                    population_basis: None,
                    city: false,
                    site: None,
                }
            } else {
                Entity {
                    id: format!("industry:{}:{}", m.role, f.id),
                    name: f.name.clone().unwrap_or_else(|| format!("{title} at {}", f.id)),
                    game: GameType { kind: "industry".into(), role: m.role.clone(), produces: role.produces.clone(), accepts: role.accepts.clone() },
                    tile,
                    evidence_feature: Some(f.id.clone()),
                    anchor_feature: Some(f.id.clone()),
                    source_label: Some(label),
                    evidence: m.evidence,
                    confidence: m.confidence,
                    rationale: m.rationale.clone(),
                    rule: r.rule_ref(&format!("map.{class}")),
                    population: None,
                    population_basis: None,
                    city: false,
                    site: None,
                }
            });
        }
    }
    out.sort_by(|a, b| a.id.cmp(&b.id));
    out
}

pub fn compile(s: &Snapshot, r: &Rules, rules_bytes: &[u8]) -> Result<Compiled, String> {
    let errs = crate::snapshot::validate(s);
    if !errs.is_empty() {
        return Err(format!("the snapshot is not valid:\n  {}", errs.join("\n  ")));
    }
    let mut diag = Vec::new();
    let (png, hm, mut lost) = heightmap(s, r);
    let towns = towns(s, r, &mut diag);
    if towns.is_empty() {
        return Err("no settlement inside the map: a scenario needs at least one town".into());
    }
    let signs = signs(s, r, &mut diag);
    let mut inds = industries(s, r, &towns, &mut diag);
    let stations: Vec<Tile> = s.features.iter().filter(|f| f.class == "rail_station").filter_map(|f| geo::tile_of(&s.extent, &f.at, 0)).collect();
    let econ = economy::build(r, s.extent.map_size, &stations, &towns, &mut inds, &mut diag);

    let mut by_class: BTreeMap<&str, usize> = BTreeMap::new();
    for f in &s.features {
        *by_class.entry(f.class.as_str()).or_insert(0) += 1;
    }
    for (class, m) in &r.map {
        if m.kind == "none" {
            if let Some(n) = by_class.get(class.as_str()) {
                lost.push(format!("{n} {class} feature(s) not represented: {}", m.rationale));
            }
        }
    }
    let parts = s.features.iter().filter(|f| f.class == "settlement" && f.tags.get("place").map(|p| r.settlement.part_of_city_places.contains(p)).unwrap_or(false)).count();
    if parts > 0 {
        lost.push(format!("{parts} suburb(s) are parts of their city, not towns of their own: the city's one town stands for them (part_of_city_places)"));
    }
    let subs: Vec<String> = r.map.iter().filter(|(_, m)| m.substitute).map(|(c, m)| format!("{c} -> {}", m.role)).collect();
    if !subs.is_empty() {
        lost.push(format!("no temperate industry matches these mapped classes, so their industries are HYPOTHETICAL gameplay stand-ins at the mapped sites: {}", subs.join(", ")));
    }
    lost.push("OpenTTD always generates at least one town of its own at a random place inside the map, so inside the real area: the GameScript renames it \"Generated town <n>\" and labels it HYPOTHETICAL, but nearby industries may still take their in-game names from it".into());
    lost.push("roads, railways, rivers and buildings of the source are not built; the game grows its own town roads".into());

    let mut entities: Vec<Entity> = towns.into_iter().chain(inds).chain(signs).collect();
    entities.sort_by(|a, b| a.id.cmp(&b.id));
    // Each anchored entity carries its anchor's mapped extent in tiles, so the game and load-check
    // can tell "at its mapped site" from "relocated outside it" whatever the site's size.
    for e in entities.iter_mut() {
        let Some(anchor) = e.anchor_feature.as_deref() else { continue };
        let Some(b) = s.features.iter().find(|f| f.id == anchor).and_then(|f| f.bbox.clone()) else { continue };
        // Corners are clamped to the map, not dropped: a large site the map cuts keeps the part of it
        // inside the map. A site wholly outside the map has no box.
        let n = s.extent.map_size as f64;
        let corners = [(b.south, b.west), (b.south, b.east), (b.north, b.west), (b.north, b.east)];
        let fr: Vec<(f64, f64)> = corners.iter().map(|&(lat, lon)| geo::image_fraction(&s.extent, &LatLon { lat, lon })).map(|(r, c)| (r * n, c * n)).collect();
        let (rmin, rmax) = (fr.iter().map(|p| p.0).fold(f64::INFINITY, f64::min), fr.iter().map(|p| p.0).fold(f64::NEG_INFINITY, f64::max));
        let (cmin, cmax) = (fr.iter().map(|p| p.1).fold(f64::INFINITY, f64::min), fr.iter().map(|p| p.1).fold(f64::NEG_INFINITY, f64::max));
        if rmax < 0.0 || cmax < 0.0 || rmin >= n || cmin >= n { continue; }
        let clamp = |v: f64| v.floor().clamp(0.0, n - 1.0) as u32;
        e.site = Some(TileBox { x0: clamp(rmin), y0: clamp(cmin), x1: clamp(rmax), y1: clamp(cmax) });
    }

    let chosen = s.resolution.chosen.clone().unwrap_or_default();
    let mut settings = BTreeMap::new();
    let bits = s.extent.map_size.trailing_zeros();
    for (k, v) in [
        ("game_creation.map_x", bits.to_string()),
        ("game_creation.map_y", bits.to_string()),
        ("game_creation.landscape", r.profile.climate.clone()),
        ("game_creation.heightmap_rotation", "1".into()),
        ("game_creation.heightmap_height", hm.levels.to_string()),
        ("game_creation.custom_town_number", "1".into()),
        ("difficulty.number_towns", "4".into()),
        ("difficulty.industry_density", "0".into()),
        ("economy.found_town", "1".into()),
        // Several industries may share a nearest town (the default refuses the second one with
        // ERR_UNKNOWN, src/industry_cmd.cpp); a compiled map places industries by evidence, not one
        // per town.
        ("economy.multiple_industry_per_town", "true".into()),
    ] {
        settings.insert(k.to_string(), v);
    }
    let mut attribution: Vec<String> = s.sources.iter().map(|x| x.attribution.clone()).collect::<BTreeSet<_>>().into_iter().collect();
    attribution.push("Game roles, positions and links are DERIVED, INFERRED or HYPOTHETICAL as each entity says; none is a claim about a real business".into());

    let mut transformations = s.transformations.clone();
    let mut push = |step: &str, params: serde_json::Value, out: String, d: Vec<String>| {
        let seq = transformations.len() as u32 + 1;
        transformations.push(Transformation { seq, step: step.into(), tool: Tool::this(), params, output_sha256: out, diagnostics: d });
    };
    push("heightmap", json!({"resample": "bilinear", "grey": hm.grey_rule, "metres_per_level": r.heightmap.metres_per_level,
                             "levels": hm.levels, "rotation": "clockwise (tile x = image row)"}), hm.sha256.clone(), vec![]);
    push("map-entities", json!({"rules": r.id, "rules_version": r.version, "border_tiles": r.border_tiles,
                                "industry_spacing_tiles": r.industry_spacing_tiles, "settlement": r.settlement}),
         sha256_hex(serde_json::to_string(&entities).unwrap().as_bytes()), diag.clone());
    push("economy", json!({"economy": r.economy}), sha256_hex(serde_json::to_string(&econ).unwrap().as_bytes()), vec![]);

    let scenario = Scenario {
        schema: SCENARIO_SCHEMA.into(),
        tool: Tool::this(),
        rules: crate::model::RulesRef { id: r.id.clone(), version: r.version, sha256: sha256_hex(rules_bytes) },
        snapshot_sha256: s.content_sha256.clone(),
        query: s.request.query.clone(),
        chosen,
        extent: s.extent.clone(),
        heightmap: hm,
        settings,
        entities,
        economy: econ,
        lost_detail: lost,
        attribution,
        transformations,
        artifacts: vec![],
        diagnostics: diag,
    };
    Ok(Compiled { scenario, heightmap_png: png })
}

/// Contract checks of a compiled scenario; every finding is an error.
pub fn validate(sc: &Scenario) -> Vec<String> {
    let mut e = Vec::new();
    if sc.schema != SCENARIO_SCHEMA {
        e.push(format!("schema {:?}", sc.schema));
    }
    let ids: BTreeSet<&str> = sc.entities.iter().map(|x| x.id.as_str()).collect();
    if ids.len() != sc.entities.len() {
        e.push("duplicate entity ids".into());
    }
    let mut last = "";
    for x in &sc.entities {
        if x.id.as_str() <= last {
            e.push(format!("entities not ordered at {}", x.id));
        }
        last = &x.id;
        if x.tile.x >= sc.extent.map_size || x.tile.y >= sc.extent.map_size {
            e.push(format!("{} outside the map", x.id));
        }
        if !(0.0..=1.0).contains(&x.confidence) {
            e.push(format!("{} confidence {}", x.id, x.confidence));
        }
        match (x.evidence, &x.evidence_feature) {
            (Evidence::Hypothetical, Some(_)) => e.push(format!("{} is HYPOTHETICAL but names an evidence feature (a stand-in names only its anchor)", x.id)),
            (Evidence::Hypothetical, None) => {}
            (_, None) => e.push(format!("{} has no source feature but is not HYPOTHETICAL", x.id)),
            _ => {}
        }
        if x.evidence == Evidence::Observed {
            e.push(format!("{}: a game entity is never OBSERVED; the source feature is", x.id));
        }
        if x.game.kind == "sign" && (!x.game.produces.is_empty() || !x.game.accepts.is_empty()) {
            e.push(format!("{}: a landmark sign carries cargo", x.id));
        }
        if x.rationale.trim().is_empty() || x.rule.trim().is_empty() {
            e.push(format!("{} has no rationale or rule", x.id));
        }
    }
    for ed in &sc.economy.edges {
        if !ids.contains(ed.from.as_str()) || !ids.contains(ed.to.as_str()) {
            e.push(format!("edge {} names an unknown entity", ed.id));
        }
        let hyp = |id: &str| sc.entities.iter().any(|x| x.id == id && x.evidence == Evidence::Hypothetical);
        if (hyp(&ed.from) || hyp(&ed.to)) && ed.evidence != Evidence::Hypothetical {
            e.push(format!("edge {} touches a HYPOTHETICAL entity but is {:?}", ed.id, ed.evidence));
        }
    }
    if !sc.economy.isolated.is_empty() {
        e.push(format!("isolated economy nodes: {}", sc.economy.isolated.join(", ")));
    }
    if sc.attribution.is_empty() {
        e.push("no attribution".into());
    }
    e
}
