//! RESEARCH-ONLY. Real-world scenarios in the panel (research/real-world-scenario.md): every place
//! snapshot under tools/research/real-world/fixtures as a list and a page each, compiled in process
//! by the one compiler (the `rw-scenario` library, tools/research/real-world), never by a second
//! copy of it. The routes are rows of the panel's route table, so OpenAPI, Swagger UI, GraphQL on the
//! hub and Rhai derive them. Offline: nothing here touches the network or the running game.

use std::path::PathBuf;
use std::sync::Arc;

use serde_json::{json, Value};

use rw_scenario::{compile, model, rules::Rules, snapshot};

use crate::panel::{Game, Input};
use crate::panel_results::repo;

fn real_world() -> PathBuf {
    repo().join("tools/research/real-world")
}

fn rules_path() -> PathBuf {
    real_world().join("rules/temperate-default.v1.json")
}

/// A fixture directory name: lower-case letters, digits and dashes, nothing that walks out.
pub fn valid_name(name: &str) -> bool {
    !name.is_empty() && name.len() <= 40 && name.chars().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '-')
}

fn snapshot_of(name: &str) -> Option<model::Snapshot> {
    snapshot::read(&real_world().join("fixtures").join(name).join("snapshot.json")).ok()
}

/// Every place with a snapshot, by name.
pub fn names() -> Vec<String> {
    let mut v: Vec<String> = std::fs::read_dir(real_world().join("fixtures")).into_iter().flatten().flatten()
        .filter(|e| e.path().join("snapshot.json").is_file())
        .filter_map(|e| e.file_name().to_str().map(str::to_string))
        .filter(|n| valid_name(n))
        .collect();
    v.sort();
    v
}

fn compiled(s: &model::Snapshot) -> Result<compile::Compiled, String> {
    let (r, b) = Rules::load(&rules_path())?;
    compile::compile(s, &r, &b)
}

fn counts(sc: &model::Scenario) -> Value {
    let mut ent = serde_json::Map::new();
    for e in &sc.entities {
        let k = format!("{}:{}", e.game.kind, serde_json::to_value(e.evidence).unwrap().as_str().unwrap_or(""));
        let n = ent.get(&k).and_then(Value::as_u64).unwrap_or(0) + 1;
        ent.insert(k, json!(n));
    }
    let mut edges = serde_json::Map::new();
    for e in &sc.economy.edges {
        let k = serde_json::to_value(e.evidence).unwrap().as_str().unwrap_or("").to_string();
        let n = edges.get(&k).and_then(Value::as_u64).unwrap_or(0) + 1;
        edges.insert(k, json!(n));
    }
    json!({"entities": ent, "edges": edges})
}

/// One line of the list: the place, what it resolved to, the map and what the compiler made of it.
pub fn summary(name: &str, s: &model::Snapshot) -> Value {
    let chosen = s.resolution.candidates.iter().find(|c| Some(&c.id) == s.resolution.chosen.as_ref());
    let c = compiled(s);
    json!({
        "name": name, "query": s.request.query, "chosen": s.resolution.chosen,
        "chosen_name": chosen.map(|c| c.display_name.clone()), "ambiguous": s.resolution.ambiguous,
        "candidates": s.resolution.candidates.len(), "map_size": s.extent.map_size,
        "metres_per_tile": s.extent.metres_per_tile, "side_m": s.extent.side_m, "features": s.features.len(),
        "sources": s.sources.iter().map(|x| json!({"id": x.id, "completeness": x.completeness, "license": x.license, "retrieved_at": x.retrieved_at})).collect::<Vec<_>>(),
        "snapshot_sha256": s.content_sha256,
        "compiled": match &c { Ok(c) => counts(&c.scenario), Err(e) => json!({"error": e}) },
    })
}

pub fn h_list(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let items: Vec<Value> = names().iter().filter_map(|n| snapshot_of(n).map(|s| summary(n, &s))).collect();
    Ok((200, json!({"count": items.len(), "scenarios": items, "rules": rules_path().strip_prefix(repo()).unwrap_or(&rules_path()).display().to_string()})))
}

pub fn h_scenario(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = i.query.get("name").unwrap_or("").to_string();
    if !valid_name(&name) {
        return Ok((400, json!({"error": "name: a place directory under tools/research/real-world/fixtures, e.g. brno"})));
    }
    let Some(s) = snapshot_of(&name) else { return Ok((404, json!({"error": format!("no snapshot {name}")}))) };
    let c = match compiled(&s) {
        Ok(c) => c,
        Err(e) => return Ok((422, json!({"error": e, "snapshot_errors": snapshot::validate(&s)}))),
    };
    let errors = compile::validate(&c.scenario);
    Ok((200, json!({
        "summary": summary(&name, &s),
        "scenario": c.scenario,
        "resolution": s.resolution,
        "sources": s.sources,
        "features": s.features,
        "terrain": {"grid": s.elevation.grid, "source": s.elevation.source,
                    "metres": s.elevation.decimetres.iter().map(|v| v.map(|d| d as f64 / 10.0)).collect::<Vec<_>>()},
        "heightmap": format!("data:image/png;base64,{}", rw_scenario::preview::b64(&c.heightmap_png)),
        "validation": {"snapshot": snapshot::validate(&s), "scenario": errors},
        // The last headless load of this place (rw-scenario load-check), if one was run: what was
        // placed where, how far it moved, the engine's own towns and the reload check.
        "loadcheck": std::fs::read(repo().join("build-real-world").join(&name).join("loadcheck/loadcheck.json")).ok()
            .and_then(|b| serde_json::from_slice::<Value>(&b).ok())
            .map(|l| json!({"done": l["done"], "placed": l["placed"], "failures": l["failures"], "engine_generated": l["engine_generated"],
                            "displacement_threshold_tiles": l["displacement_threshold_tiles"], "displaced_beyond_threshold": l["displaced_beyond_threshold"], "relocated_outside_site": l["relocated_outside_site"],
                            "placements": l["placements"], "reload": l["reload"]})),
    })))
}

pub fn h_compile(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = i.body.get("name").and_then(Value::as_str).unwrap_or("").to_string();
    if !valid_name(&name) {
        return Ok((400, json!({"error": "name: a place directory under tools/research/real-world/fixtures, e.g. brno"})));
    }
    let Some(s) = snapshot_of(&name) else { return Ok((404, json!({"error": format!("no snapshot {name}")}))) };
    let (r, _) = Rules::load(&rules_path())?;
    let c = match compiled(&s) {
        Ok(c) => c,
        Err(e) => return Ok((422, json!({"error": e}))),
    };
    let out = repo().join("build-real-world").join(&name);
    std::fs::create_dir_all(&out).map_err(|e| format!("{}: {e}", out.display()))?;
    let sc = rw_scenario::write_outputs(&out, c, &s, &r)?;
    let sums = std::fs::read_to_string(out.join("SHA256SUMS")).unwrap_or_default();
    let recorded = std::fs::read_to_string(real_world().join("fixtures").join(&name).join("SHA256SUMS")).ok();
    Ok((200, json!({
        "out": out.strip_prefix(repo()).unwrap_or(&out).display().to_string(),
        "artifacts": sc.artifacts, "sha256sums": sums,
        "matches_recorded": recorded.as_ref().map(|r| r == &sums),
        "next": "load it headless: tools/research/real-world/rw-scenario load-check --out <out> --openttd <binary>",
    })))
}

/// The page's title and description, from the snapshot itself.
pub fn page_meta(path: &str) -> (String, String, bool) {
    if path == "/realworld" {
        return ("Real-world scenarios".into(), "Places compiled into OpenTTD maps: terrain, towns, industries and their links, each labelled OBSERVED, DERIVED, INFERRED or HYPOTHETICAL".into(), true);
    }
    let name = path.strip_prefix("/realworld/").unwrap_or("");
    match (valid_name(name), snapshot_of(name)) {
        (true, Some(s)) => (
            format!("{} as an OpenTTD map", s.request.query),
            format!("{} tiles of {} m, {} source features, chosen {}", s.extent.map_size, s.extent.metres_per_tile, s.features.len(), s.resolution.chosen.clone().unwrap_or_default()),
            true,
        ),
        _ => (format!("{name}: no such place"), String::new(), false),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_are_checked() {
        assert!(valid_name("brno"));
        assert!(!valid_name("../x"));
        assert!(!valid_name("Brno"));
        assert!(!valid_name(""));
    }

    #[test]
    fn every_fixture_compiles_and_titles_its_page() {
        assert!(!names().is_empty(), "no fixture under tools/research/real-world/fixtures");
        for n in names() {
            let s = snapshot_of(&n).unwrap();
            let c = compiled(&s).unwrap();
            assert!(compile::validate(&c.scenario).is_empty());
            let (title, _, found) = page_meta(&format!("/realworld/{n}"));
            assert!(found && title.contains(&s.request.query));
        }
        assert!(!page_meta("/realworld/nope").2);
    }
}
