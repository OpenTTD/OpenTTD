//! Bounded acquisition (only with `--live`) and the normalisation every source goes through.
//!
//! Layers: place candidates (Nominatim search), vector features in the extent's box (one
//! Overpass query generated from the rules' classify table), and an elevation grid
//! (Copernicus DEM GLO-30, a Cloud-Optimized GeoTIFF read by HTTP range requests from the anonymous AWS bucket). A layer that fails is recorded with
//! `completeness: failed` and its error; the snapshot is still written so the failure is
//! visible, and `compile` refuses a snapshot whose elevation or features failed.

use crate::geo;
use crate::model::*;
#[cfg(feature = "live")]
use crate::net::{self, Client};
#[cfg(feature = "live")]
use crate::providers;
#[cfg(feature = "live")]
use crate::resolve;
use crate::rules::Rules;
#[cfg(feature = "live")]
use serde_json::json;
use serde_json::Value;
use std::collections::BTreeMap;

/// Resolve the query against the registry's geocoder (live) and choose a candidate.
#[cfg(feature = "live")]
pub fn resolve_live(client: &mut Client, query: &str, pick: Option<&str>) -> Result<(Resolution, Source), String> {
    let gp = providers::for_kind("geocoder");
    let url = format!("{}?q={}&format=jsonv2&limit=10&addressdetails=1", gp.endpoint, net::encode(query));
    let f = client.fetch(&url, None)?;
    let at = if f.fetched_at.is_empty() { net::now_utc() } else { f.fetched_at.clone() };
    let cands = resolve::candidates_from_nominatim(&f.body)?;
    let mut diag = vec![format!("{} candidate(s)", cands.len())];
    if f.cached {
        diag.push(format!("answered from the local cache (fetched {at})"));
    }
    let res = resolve::choose(cands, pick, &gp.provider, &gp.id);
    Ok((res, providers::source(&gp, &at, vec![f.sha256], 1, "complete", diag)))
}

fn tags_of(el: &Value, keep: &[String]) -> (BTreeMap<String, String>, BTreeMap<String, String>) {
    let mut all = BTreeMap::new();
    if let Some(t) = el["tags"].as_object() {
        for (k, v) in t {
            if let Some(s) = v.as_str() {
                all.insert(k.clone(), s.to_string());
            }
        }
    }
    let kept = all.iter().filter(|(k, _)| keep.contains(k)).map(|(k, v)| (k.clone(), v.clone())).collect();
    (all, kept)
}

/// Overpass JSON to normalised features: classified by the rules, ordered by id, duplicates
/// (the same element twice) dropped, each class limited to its largest `acquire_limit` items.
pub fn features_from_overpass(body: &[u8], rules: &Rules, extent: &Extent, source: &str) -> Result<(Vec<Feature>, Vec<String>), String> {
    let v: Value = serde_json::from_slice(body).map_err(|e| format!("overpass: not JSON: {e}"))?;
    let els = v["elements"].as_array().ok_or("overpass: no elements")?;
    let mut by_id: BTreeMap<String, Feature> = BTreeMap::new();
    let mut diag = Vec::new();
    let mut unclassified = 0;
    for el in els {
        let ty = el["type"].as_str().unwrap_or("");
        let id = format!("osm:{ty}/{}", el["id"].as_u64().unwrap_or(0));
        let (all, kept) = tags_of(el, &rules.keep_tags);
        let Some(rule) = rules.classify_tags(ty, &all) else {
            unclassified += 1;
            continue;
        };
        let (at, bbox) = if ty == "node" {
            (LatLon { lat: round_to(el["lat"].as_f64().unwrap_or(f64::NAN), 7), lon: round_to(el["lon"].as_f64().unwrap_or(f64::NAN), 7) }, None)
        } else {
            let b = &el["bounds"];
            let bb = BBox {
                south: round_to(b["minlat"].as_f64().unwrap_or(f64::NAN), 7),
                west: round_to(b["minlon"].as_f64().unwrap_or(f64::NAN), 7),
                north: round_to(b["maxlat"].as_f64().unwrap_or(f64::NAN), 7),
                east: round_to(b["maxlon"].as_f64().unwrap_or(f64::NAN), 7),
            };
            (LatLon { lat: round_to((bb.south + bb.north) / 2.0, 7), lon: round_to((bb.west + bb.east) / 2.0, 7) }, Some(bb))
        };
        if !at.lat.is_finite() || !at.lon.is_finite() {
            diag.push(format!("{id}: no coordinates; rejected"));
            continue;
        }
        let area = bbox.as_ref().map(|b| round_to(geo::bbox_area_m2(&extent.crs.origin, b), 0)).unwrap_or(0.0);
        if by_id.contains_key(&id) {
            diag.push(format!("{id}: duplicate element; the first kept"));
            continue;
        }
        by_id.insert(id.clone(), Feature {
            id,
            class: rule.class.clone(),
            name: kept.get("name").cloned(),
            at,
            bbox,
            bbox_area_m2: area,
            tags: kept,
            source: source.to_string(),
            evidence: Evidence::Observed,
        });
    }
    if unclassified > 0 {
        diag.push(format!("{unclassified} element(s) matched no classify rule (relation members, mixed tags); dropped"));
    }
    let mut out: Vec<Feature> = Vec::new();
    let mut classes: BTreeMap<String, Vec<Feature>> = BTreeMap::new();
    for f in by_id.into_values() {
        classes.entry(f.class.clone()).or_default().push(f);
    }
    for (class, mut fs) in classes {
        let limit = rules.acquire_limit.get(&class).copied().unwrap_or(50);
        if fs.len() > limit {
            // Largest first: area for areas, population for settlements; ties by id.
            fs.sort_by(|a, b| size_key(b).partial_cmp(&size_key(a)).unwrap_or(std::cmp::Ordering::Equal).then(a.id.cmp(&b.id)));
            diag.push(format!("{class}: {} found, the largest {limit} kept (rank-limit)", fs.len()));
            fs.truncate(limit);
        }
        out.extend(fs);
    }
    out.sort_by(|a, b| a.id.cmp(&b.id));
    Ok((out, diag))
}

/// What "largest" means per class: population for settlements, a name then elevation for
/// point landmarks (an unnamed peak cannot be signed), the bounding-box area otherwise.
fn size_key(f: &Feature) -> f64 {
    if f.class == "settlement" {
        return f.tags.get("population").and_then(|p| p.replace([' ', ','], "").parse::<f64>().ok()).unwrap_or(0.0);
    }
    if f.class == "landmark" {
        let named = if f.name.is_some() { 1.0e6 } else { 0.0 };
        return named + f.tags.get("ele").and_then(|e| e.parse::<f64>().ok()).unwrap_or(0.0) + f.bbox_area_m2.min(1.0e5) / 1.0e5;
    }
    f.bbox_area_m2
}

/// The elevation sample points of the extent, row-major north to south, west to east.
pub fn grid_points(extent: &Extent, n: u32) -> Vec<LatLon> {
    let h = extent.side_m / 2.0;
    let mut pts = Vec::new();
    for j in 0..n {
        for i in 0..n {
            let x = -h + extent.side_m * i as f64 / (n - 1) as f64;
            let y = h - extent.side_m * j as f64 / (n - 1) as f64;
            let p = geo::unproject(&extent.crs.origin, x, y);
            pts.push(LatLon { lat: round_to(p.lat, 5), lon: round_to(p.lon, 5) });
        }
    }
    pts
}

#[cfg(feature = "live")]
pub fn acquire_live(client: &mut Client, req: &Request, rules: &Rules) -> Result<Snapshot, String> {
    let (resolution, nsrc) = resolve_live(client, &req.query, req.pick.as_deref())?;
    let Some(chosen) = resolution.chosen.clone() else {
        return Err(format!("{}\ncandidates:\n{}", resolution.reason, list(&resolution.candidates)));
    };
    let cand = resolution.candidates.iter().find(|c| c.id == chosen).unwrap().clone();
    let (extent, clamp) = geo::square_extent(&cand.center, &cand.bbox, req.margin, req.map_size);
    let mut transformations = vec![];
    let mut seq = 0;
    let mut tr = |step: &str, params: Value, out: &str, diagnostics: Vec<String>| {
        seq += 1;
        transformations.push(Transformation { seq, step: step.into(), tool: Tool::this(), params, output_sha256: out.into(), diagnostics });
    };
    tr("resolve", json!({"rule": resolve::RULE, "dominance": resolve::DOMINANCE, "query": req.query, "pick": req.pick}),
       &net::sha256_hex(serde_json::to_string(&resolution).unwrap().as_bytes()), vec![resolution.reason.clone()]);
    tr("extent", json!({"crs": extent.crs.id, "margin": req.margin, "map_size": req.map_size,
                        "min_side_m": geo::MIN_SIDE_M, "max_side_m": geo::MAX_SIDE_M}),
       &net::sha256_hex(serde_json::to_string(&extent).unwrap().as_bytes()), clamp.into_iter().collect());

    // Vector features.
    let b = &extent.bbox;
    let q = rules.overpass_query(b.south, b.west, b.north, b.east);
    let now = net::now_utc();
    let vec_p = providers::for_kind("vector");
    let (features, osrc) = match client.fetch(&vec_p.endpoint, Some(&[("data", q.as_str())])) {
        Ok(f) => {
            let at = if f.fetched_at.is_empty() { now.clone() } else { f.fetched_at.clone() };
            match features_from_overpass(&f.body, rules, &extent, &vec_p.id) {
                Ok((fs, mut d)) => {
                    d.extend(client.notes.drain(..));
                    if f.cached {
                        d.push(format!("answered from the local cache (fetched {at})"));
                    }
                    (fs, providers::source(&vec_p, &at, vec![f.sha256], 1, "complete", d))
                }
                Err(e) => (vec![], providers::source(&vec_p, &at, vec![f.sha256], 1, "failed", vec![e])),
            }
        }
        Err(e) => (vec![], providers::source(&vec_p, &now, vec![], 1, "failed", vec![e])),
    };
    let fsha = net::sha256_hex(serde_json::to_string(&features).unwrap().as_bytes());
    tr("normalise-features", json!({"query_sha256": net::sha256_hex(q.as_bytes()), "rules": rules.id, "rules_version": rules.version,
                                    "keep_tags": rules.keep_tags, "acquire_limit": rules.acquire_limit, "order": "id ascending",
                                    "duplicates": "first kept", "coordinates": "WGS84 degrees, 7 decimals; area features at the centre of their bounding box"}),
       &fsha, osrc.diagnostics.clone());

    // Elevation.
    let dem = providers::for_kind("elevation");
    let pts = grid_points(&extent, req.elevation_grid);
    let mut at = String::new();
    let (mut cached, mut requests) = (0u32, 0u32);
    let mut dm: Vec<Option<i32>> = vec![None; pts.len()];
    let mut raws = vec![];
    let mut diag = vec![];
    let mut complete = "complete";
    // Copernicus GLO-30: one Cloud Optimized GeoTIFF per 1x1 degree cell. Per cell: its directories
    // (a range read of the header), then only the internal tiles the grid's points touch.
    let mut cells: BTreeMap<(i32, i32), Vec<usize>> = BTreeMap::new();
    for (k, p) in pts.iter().enumerate() {
        cells.entry((p.lat.floor() as i32, p.lon.floor() as i32)).or_default().push(k);
    }
    let mut read = |client: &mut Client, url: &str, a: u64, b: u64, raws: &mut Vec<String>, at: &mut String| -> Result<Vec<u8>, String> {
        requests += 1;
        let f = client.fetch_range(url, a, b)?;
        cached += f.cached as u32;
        raws.push(f.sha256.clone());
        if !f.fetched_at.is_empty() && (at.is_empty() || f.fetched_at < *at) {
            *at = f.fetched_at.clone();
        }
        Ok(f.body)
    };
    for ((lat, lon), idx) in &cells {
        let name = format!(
            "Copernicus_DSM_COG_10_{}{:02}_00_{}{:03}_00_DEM",
            if *lat >= 0 { 'N' } else { 'S' }, lat.abs(), if *lon >= 0 { 'E' } else { 'W' }, lon.abs()
        );
        let url = format!("{}{name}/{name}.tif", dem.endpoint);
        let mut want = 16 * 1024u64;
        let mut ifd = None;
        for _ in 0..3 {
            match read(client, &url, 0, want - 1, &mut raws, &mut at).map(|b| crate::cog::first_ifd(&b)) {
                Ok(Ok(i)) => { ifd = Some(i); break; }
                Ok(Err(Ok(need))) => want = (need as u64).div_ceil(16 * 1024) * 16 * 1024,
                Ok(Err(Err(e))) => { diag.push(format!("{name}: {e}")); break; }
                Err(e) => { diag.push(format!("{name}: {e} (a cell with no land has no tile)")); break; }
            }
        }
        let Some(ifd) = ifd else { complete = "partial"; continue };
        // The internal tiles every point's bilinear neighbourhood touches.
        let mut need: std::collections::BTreeSet<usize> = std::collections::BTreeSet::new();
        for &k in idx {
            let (c, r) = ifd.pixel_of(pts[k].lat, pts[k].lon);
            for (dc, dr) in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)] {
                let cc = (c.floor() + dc).clamp(0.0, (ifd.width - 1) as f64) as u32;
                let rr = (r.floor() + dr).clamp(0.0, (ifd.height - 1) as f64) as u32;
                need.insert(ifd.tile_index(cc, rr));
            }
        }
        let mut tiles: BTreeMap<usize, Vec<f32>> = BTreeMap::new();
        for t in &need {
            let (off, len) = (ifd.tile_offsets[*t], ifd.tile_bytes[*t]);
            match read(client, &url, off, off + len - 1, &mut raws, &mut at).and_then(|b| crate::cog::decode_tile(&b, &ifd)) {
                Ok(v) => { tiles.insert(*t, v); }
                Err(e) => { diag.push(format!("{name} tile {t}: {e}")); complete = "partial"; }
            }
        }
        diag.push(format!("{name}: {} of {} internal tiles read for {} points", tiles.len(), ifd.tile_offsets.len(), idx.len()));
        for &k in idx {
            let (c, r) = ifd.pixel_of(pts[k].lat, pts[k].lon);
            let v = crate::cog::bilinear(c, r, ifd.width, ifd.height, |cc, rr| {
                let t = tiles.get(&ifd.tile_index(cc, rr))?;
                let i = (rr % ifd.tile_height) as usize * ifd.tile_width as usize + (cc % ifd.tile_width) as usize;
                t.get(i).copied().filter(|x| x.is_finite() && *x > -1000.0)
            });
            dm[k] = v.map(|m| (m * 10.0).round() as i32);
        }
    }
    if dm.iter().all(|x| x.is_none()) {
        complete = "failed";
    }
    let missing = dm.iter().filter(|x| x.is_none()).count();
    if missing > 0 {
        diag.push(format!("{missing} of {} samples have no value", dm.len()));
        if complete == "complete" {
            complete = "partial";
        }
    }
    diag.extend(client.notes.drain(..));
    diag.push(format!("{requests} range request(s), {cached} answered from the local cache"));
    if at.is_empty() {
        at = net::now_utc();
    }
    let esrc = providers::source(&dem, &at, raws, requests, complete, diag);
    let elevation = ElevationLayer { source: esrc.id.clone(), grid: req.elevation_grid, decimetres: dm };
    tr("sample-elevation", json!({"grid": req.elevation_grid, "points": "row-major, north to south, west to east, at i/(n-1) of the side",
                                  "raster": "Copernicus DEM GLO-30 COG, full resolution (1 arc-second), pixel-is-point (GeoKey 1025 = 2), bilinear at each grid point",
                                  "coordinates": "5 decimals", "unit": "decimetre"}),
       &net::sha256_hex(serde_json::to_string(&elevation).unwrap().as_bytes()), esrc.diagnostics.clone());

    let mut snap = Snapshot {
        schema: SNAPSHOT_SCHEMA.into(),
        tool: Tool::this(),
        request: req.clone(),
        resolution,
        extent,
        sources: vec![nsrc, osrc, esrc],
        elevation,
        features,
        transformations,
        content_sha256: String::new(),
    };
    snap.content_sha256 = crate::snapshot::content_hash(&snap);
    Ok(snap)
}

pub fn list(c: &[Candidate]) -> String {
    c.iter()
        .map(|c| format!("  {:>2}. {:<22} {:<10} importance {:<8} settlement {:<5} {}", c.rank, c.id, c.kind, c.importance, c.settlement, c.display_name))
        .collect::<Vec<_>>()
        .join("\n")
}
