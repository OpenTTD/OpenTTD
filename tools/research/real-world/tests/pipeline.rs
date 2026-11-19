//! Offline proof of the pipeline: determinism, replay, licensing and provenance behaviour,
//! evidence rules, the landmark test, place-independence and no network or game mutation.

use rw_scenario::model::*;
use rw_scenario::{compile, resolve, rules::Rules, snapshot};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

fn dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
}

fn rules() -> (Rules, Vec<u8>) {
    Rules::load(&dir().join("rules/temperate-default.v1.json")).unwrap()
}

fn fixture(name: &str) -> Snapshot {
    snapshot::read(&dir().join("fixtures").join(name).join("snapshot.json")).unwrap()
}

fn compile_into(s: &Snapshot, out: &Path) -> Scenario {
    let (r, b) = rules();
    let c = compile::compile(s, &r, &b).unwrap();
    std::fs::create_dir_all(out).unwrap();
    rw_scenario::write_outputs(out, c, s, &r).unwrap()
}

fn tmp(tag: &str) -> PathBuf {
    let p = std::env::temp_dir().join(format!("rw-test-{}-{tag}", std::process::id()));
    let _ = std::fs::remove_dir_all(&p);
    p
}

/// A synthetic place, built in code: no real names, so it proves the compiler needs none.
fn synthetic() -> Snapshot {
    let center = LatLon { lat: 45.0, lon: 7.0 };
    let bbox = BBox { south: 44.95, west: 6.93, north: 45.05, east: 7.07 };
    let (extent, _) = rw_scenario::geo::square_extent(&center, &bbox, 0.1, 128);
    let cand = Candidate {
        rank: 1, id: "osm:relation/1".into(), name: "Synthville".into(), display_name: "Synthville, Testland".into(),
        category: "boundary".into(), kind: "administrative".into(), place_rank: 16, importance: 0.5,
        country_code: "zz".into(), bbox, center: center.clone(), settlement: true,
    };
    let feat = |id: &str, class: &str, name: &str, dlat: f64, dlon: f64, tags: &[(&str, &str)], area: f64| Feature {
        id: id.into(), class: class.into(), name: Some(name.into()),
        at: LatLon { lat: center.lat + dlat, lon: center.lon + dlon }, bbox: None, bbox_area_m2: area,
        tags: tags.iter().map(|(k, v)| (k.to_string(), v.to_string())).chain([("name".to_string(), name.to_string())]).collect::<BTreeMap<_, _>>(),
        source: "overpass-features".into(), evidence: Evidence::Observed,
    };
    let mut features = vec![
        feat("osm:node/1", "settlement", "Synthville", 0.0, 0.0, &[("place", "city"), ("population", "120000")], 0.0),
        feat("osm:node/2", "settlement", "Northdorf", 0.035, 0.01, &[("place", "village")], 0.0),
        feat("osm:node/3", "landmark", "Testberg", -0.02, 0.03, &[("natural", "peak"), ("ele", "512")], 0.0),
        feat("osm:way/4", "extraction", "Test Quarry", -0.021, 0.031, &[("landuse", "quarry")], 90_000.0),
        feat("osm:way/5", "forest", "Greenwood", 0.02, -0.04, &[("landuse", "forest")], 400_000.0),
        feat("osm:node/6", "settlement", "Oldtown Suburb", -0.03, -0.02, &[("place", "suburb")], 0.0),
    ];
    features.sort_by(|a, b| a.id.cmp(&b.id));
    let src = |id: &str, kind: &str| Source {
        id: id.into(), kind: kind.into(), provider: "synthetic".into(), endpoint: "none".into(), layer: id.into(),
        retrieved_at: "2026-01-01T00:00:00Z".into(), license: "CC0-1.0 (synthetic test data)".into(),
        attribution: "synthetic test data".into(), allowed_purpose: "tests".into(), retention: "n/a".into(),
        redistribution: "allowed".into(), completeness: "complete".into(), requests: 0, raw_sha256: vec![],
        terms_checked: "n/a".into(), diagnostics: vec![],
    };
    let n = 8u32;
    let decimetres = (0..n * n).map(|i| Some(2000 + (i % n) as i32 * 50 + (i / n) as i32 * 20)).collect();
    let mut s = Snapshot {
        schema: SNAPSHOT_SCHEMA.into(), tool: Tool::this(),
        request: Request { query: "Synthville".into(), map_size: 128, elevation_grid: n, margin: 0.1, pick: None },
        resolution: resolve::choose(vec![cand], None, "synthetic", "nominatim-search"),
        extent, sources: vec![src("nominatim-search", "geocoder"), src("overpass-features", "vector"), src("copernicus-glo30-aws", "elevation")],
        elevation: ElevationLayer { source: "copernicus-glo30-aws".into(), grid: n, decimetres },
        features, transformations: vec![], content_sha256: String::new(),
    };
    s.content_sha256 = snapshot::content_hash(&s);
    s
}

#[test]
fn the_brno_fixture_is_valid_and_carries_its_licences() {
    let s = fixture("brno");
    assert_eq!(snapshot::validate(&s), Vec::<String>::new());
    let osm = s.sources.iter().find(|x| x.id == "overpass-features").unwrap();
    assert!(osm.license.contains("ODbL") && osm.attribution.contains("OpenStreetMap contributors"));
    let dem = s.sources.iter().find(|x| x.kind == "elevation").unwrap();
    assert_eq!(dem.id, "copernicus-glo30-aws");
    assert!(dem.license.contains("GLO-30") && dem.attribution.contains("WorldDEM™-30"));
    assert!(s.features.iter().all(|f| f.evidence == Evidence::Observed));
}

#[test]
fn compiling_twice_gives_the_same_bytes_and_matches_the_recorded_hashes() {
    let s = fixture("brno");
    let (a, b) = (tmp("a"), tmp("b"));
    compile_into(&s, &a);
    compile_into(&s, &b);
    let sa = std::fs::read_to_string(a.join("SHA256SUMS")).unwrap();
    assert_eq!(sa, std::fs::read_to_string(b.join("SHA256SUMS")).unwrap());
    let want = std::fs::read_to_string(dir().join("fixtures/brno/SHA256SUMS")).unwrap();
    assert_eq!(sa, want, "the compiler's output changed; if intended, regenerate fixtures/brno/SHA256SUMS");
    let _ = std::fs::remove_dir_all(&a);
    let _ = std::fs::remove_dir_all(&b);
}

#[test]
fn an_edited_snapshot_is_refused() {
    let mut s = fixture("brno");
    s.features[0].tags.insert("name".into(), "tampered".into());
    let (r, b) = rules();
    let err = compile::compile(&s, &r, &b).err().unwrap();
    assert!(err.contains("content_sha256"), "{err}");
}

#[test]
fn a_snapshot_without_licence_or_with_a_failed_layer_is_refused() {
    let mut s = fixture("brno");
    s.sources[1].license = String::new();
    s.content_sha256 = snapshot::content_hash(&s);
    assert!(snapshot::validate(&s).iter().any(|e| e.contains("no license")));
    let mut s = fixture("brno");
    s.sources.iter_mut().find(|x| x.kind == "elevation").unwrap().completeness = "failed".into();
    s.content_sha256 = snapshot::content_hash(&s);
    assert!(snapshot::validate(&s).iter().any(|e| e.contains("failed")));
}

#[test]
fn every_entity_and_edge_carries_one_evidence_class_and_hypotheticals_are_marked() {
    let s = fixture("brno");
    let out = tmp("ev");
    let sc = compile_into(&s, &out);
    assert_eq!(compile::validate(&sc), Vec::<String>::new());
    for e in &sc.entities {
        assert_ne!(e.evidence, Evidence::Observed);
        assert_eq!(e.evidence_feature.is_none(), e.evidence == Evidence::Hypothetical, "{}", e.id);
    }
    for e in sc.entities.iter().filter(|e| e.evidence == Evidence::Hypothetical) {
        assert!((e.name.contains("hypothetical") || e.name.contains("gameplay stand-in")) && e.rationale.starts_with("HYPOTHETICAL"), "{}", e.id);
    }
    assert!(sc.economy.isolated.is_empty(), "{:?}", sc.economy.isolated);
    assert!(sc.economy.edges.iter().any(|e| e.cargo != "PASS"), "no cargo chain");
    let _ = std::fs::remove_dir_all(&out);
}

#[test]
fn a_landmark_never_becomes_an_industry() {
    for s in [fixture("brno"), synthetic()] {
        let out = tmp("lm");
        let sc = compile_into(&s, &out);
        let landmarks: Vec<&str> = s.features.iter().filter(|f| f.class == "landmark").map(|f| f.id.as_str()).collect();
        for e in &sc.entities {
            if let Some(f) = &e.evidence_feature {
                if landmarks.contains(&f.as_str()) {
                    assert_eq!(e.game.kind, "sign", "{} became {}", f, e.game.kind);
                    assert!(e.game.produces.is_empty() && e.game.accepts.is_empty());
                }
            }
        }
        let _ = std::fs::remove_dir_all(&out);
    }
}

#[test]
fn the_klajdovka_test_holds_on_the_brno_fixture() {
    let s = fixture("brno");
    let k: Vec<&Feature> = s.features.iter().filter(|f| f.name.as_deref().map(|n| n.contains("Klajdovka")).unwrap_or(false)).collect();
    assert!(!k.is_empty(), "the fixture has no Klajdovka feature");
    for f in &k {
        assert_ne!(f.class, "extraction", "{} is classified as extraction", f.id);
    }
    // In OpenStreetMap it is the nature monument Velka Klajdovka (leisure=nature_reserve), not a peak or a mine.
    let monument = s.features.iter().find(|f| f.id == "osm:relation/2331971").expect("the fixture keeps osm:relation/2331971");
    assert_eq!(monument.class, "protected_area", "Velka Klajdovka is the protected area OSM maps");
    assert_eq!(monument.tags.get("leisure").map(String::as_str), Some("nature_reserve"));
    let out = tmp("kl");
    let sc = compile_into(&s, &out);
    for e in sc.entities.iter().filter(|e| e.game.kind == "industry") {
        assert!(!e.name.contains("Klajdovka"), "{} is an industry named after a viewpoint", e.id);
    }
    let _ = std::fs::remove_dir_all(&out);
}

#[test]
fn a_different_place_compiles_with_the_same_code_and_rules() {
    let s = synthetic();
    assert_eq!(snapshot::validate(&s), Vec::<String>::new());
    let out = tmp("syn");
    let sc = compile_into(&s, &out);
    assert_eq!(compile::validate(&sc), Vec::<String>::new());
    assert_eq!(sc.chosen, "osm:relation/1");
    assert!(sc.entities.iter().any(|e| e.name == "Synthville" && e.city));
    // The quarry gets a HYPOTHETICAL coal-mine stand-in anchored at it, evidenced by nothing; the
    // forest needs a HYPOTHETICAL sawmill; the stand-in's coal a HYPOTHETICAL power station.
    let stand_in = sc.entities.iter().find(|e| e.game.role == "coal_mine").unwrap();
    assert_eq!(stand_in.evidence, Evidence::Hypothetical);
    assert_eq!(stand_in.evidence_feature, None);
    assert_eq!(stand_in.anchor_feature.as_deref(), Some("osm:way/4"));
    assert!(!stand_in.name.contains("Test Quarry"), "a stand-in is never named after the site or its operator");
    assert!(sc.entities.iter().filter(|e| e.game.kind == "town").all(|e| e.name != "Oldtown Suburb"), "a suburb is not a town");
    assert!(sc.entities.iter().any(|e| e.game.role == "sawmill" && e.evidence == Evidence::Hypothetical));
    assert!(sc.entities.iter().any(|e| e.game.role == "power_station" && e.evidence == Evidence::Hypothetical));
    let _ = std::fs::remove_dir_all(&out);
}

#[test]
fn an_ambiguous_place_is_surfaced_and_never_chosen_silently() {
    let body = std::fs::read(dir().join("fixtures/springfield/nominatim.json")).unwrap();
    let c = resolve::candidates_from_nominatim(&body).unwrap();
    assert!(c.iter().filter(|x| x.settlement).count() >= 2);
    let r = resolve::choose(c.clone(), None, "nominatim", "file");
    assert!(r.ambiguous && r.chosen.is_none(), "{}", r.reason);
    let pick = c.iter().find(|x| x.settlement).unwrap().id.clone();
    let r = resolve::choose(c, Some(&pick), "nominatim", "file");
    assert_eq!(r.chosen.as_deref(), Some(pick.as_str()));
}

#[test]
fn the_brno_query_resolves_to_the_city_not_the_administrative_district() {
    let s = fixture("brno");
    let city = s.resolution.candidates.iter().find(|c| Some(&c.id) == s.resolution.chosen.as_ref()).unwrap();
    assert!(city.settlement);
    assert!(s.resolution.candidates.len() >= 1);
}

#[test]
fn the_compiler_does_not_touch_the_network_or_the_game() {
    // Source-level containment: only net.rs names the HTTP client, and nothing writes game state
    // other than the GameScript, which uses the Script API (the game's own commands).
    for f in std::fs::read_dir(dir().join("src")).unwrap() {
        let p = f.unwrap().path();
        let text = std::fs::read_to_string(&p).unwrap();
        let name = p.file_name().unwrap().to_str().unwrap().to_string();
        if name != "net.rs" {
            assert!(!text.contains("ureq::"), "{name} uses the HTTP client");
        }
        if ["compile.rs", "economy.rs", "geo.rs", "png.rs", "preview.rs", "snapshot.rs"].contains(&name.as_str()) {
            for banned in ["SystemTime", "Instant::now", "HashMap", "rand::", "Client::new"] {
                assert!(!text.contains(banned), "{name} contains {banned}");
            }
        }
    }
    let gs = rw_scenario::gamescript::main_nut(&compile::compile(&synthetic(), &rules().0, &rules().1).unwrap().scenario, &rules().0);
    for api in ["GSTown.FoundTown", "GSIndustryType.BuildIndustry", "GSSign.BuildSign", "GSTown.SetName", "GSTown.SetText", "GSIndustry.SetText", "placed = this.placed"] {
        assert!(gs.contains(api));
    }
    // Without the byte-order mark OpenTTD reads the script byte by byte and garbles every non-ASCII name.
    assert!(gs.starts_with('\u{FEFF}'), "main.nut must start with a UTF-8 byte-order mark");
}

#[test]
fn stand_ins_suburbs_settings_and_the_in_game_credit_follow_the_rules() {
    let s = fixture("brno");
    let out = tmp("si");
    let sc = compile_into(&s, &out);
    for e in sc.entities.iter().filter(|e| e.game.kind == "industry" && e.evidence == Evidence::Hypothetical) {
        assert!(e.evidence_feature.is_none(), "{}", e.id);
        if let Some(a) = &e.anchor_feature {
            let site = s.features.iter().find(|f| &f.id == a).unwrap();
            if let Some(n) = &site.name {
                assert!(!e.name.contains(n.as_str()), "{} carries the site's name {n}", e.id);
            }
        }
    }
    // Quarries are never evidence for a coal mine.
    for e in sc.entities.iter().filter(|e| e.game.role == "coal_mine") {
        assert_eq!(e.evidence, Evidence::Hypothetical, "{}", e.id);
    }
    // No suburb is a town.
    for e in sc.entities.iter().filter(|e| e.game.kind == "town") {
        let f = s.features.iter().find(|f| Some(&f.id) == e.evidence_feature.as_ref()).unwrap();
        assert_ne!(f.tags.get("place").map(String::as_str), Some("suburb"), "{}", e.id);
    }
    assert_eq!(sc.settings.get("economy.multiple_industry_per_town").map(String::as_str), Some("true"));
    let credit = rw_scenario::gamescript::in_game_attribution(&sc);
    for src in &s.sources {
        assert!(credit.contains(&src.attribution), "the in-game credit lacks {}", src.id);
    }
    let _ = std::fs::remove_dir_all(&out);
}

#[test]
fn every_anchored_industry_carries_its_site_and_a_site_the_map_cuts_is_clamped_not_dropped() {
    let (r, b) = rules();
    let c = compile::compile(&fixture("brno"), &r, &b).unwrap();
    for e in c.scenario.entities.iter().filter(|e| e.game.kind == "industry" && e.anchor_feature.is_some()) {
        let site = e.site.as_ref().unwrap_or_else(|| panic!("{} has an anchor but no site", e.id));
        assert!(site.x0 <= site.x1 && site.y0 <= site.y1, "{}: {site:?}", e.id);
    }
    // A forest larger than the map's north edge keeps the part inside the map (x0 clamped to 0),
    // and its compiled tile lies inside that box.
    let forest = c.scenario.entities.iter().find(|e| e.id == "industry:forest:osm:relation/116315").unwrap();
    let site = forest.site.as_ref().unwrap();
    assert_eq!(site.x0, 0);
    assert!(site.x1 > site.x0 + 10, "a clamped box, not a line: {site:?}");
    assert!((site.x0..=site.x1).contains(&forest.tile.x) && (site.y0..=site.y1).contains(&forest.tile.y));
}
