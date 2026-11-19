//! Reading, hashing and validating a snapshot. Validation is the contract the compiler relies
//! on: a snapshot that fails it is never compiled.

use crate::model::*;
use crate::net::sha256_hex;
use std::path::Path;

pub fn to_bytes<T: serde::Serialize>(v: &T) -> Vec<u8> {
    let mut b = serde_json::to_vec_pretty(v).expect("serialisable");
    b.push(b'\n');
    b
}

pub fn content_hash(s: &Snapshot) -> String {
    let mut c = s.clone();
    c.content_sha256 = String::new();
    sha256_hex(&to_bytes(&c))
}

pub fn read(path: &Path) -> Result<Snapshot, String> {
    let b = std::fs::read(path).map_err(|e| format!("{}: {e}", path.display()))?;
    serde_json::from_slice(&b).map_err(|e| format!("{}: not a {SNAPSHOT_SCHEMA}: {e}", path.display()))
}

pub fn write(path: &Path, s: &Snapshot) -> Result<(), String> {
    if let Some(d) = path.parent() {
        std::fs::create_dir_all(d).map_err(|e| format!("{}: {e}", d.display()))?;
    }
    std::fs::write(path, to_bytes(s)).map_err(|e| format!("{}: {e}", path.display()))
}

/// Every finding is an error; an empty list is a valid snapshot.
pub fn validate(s: &Snapshot) -> Vec<String> {
    let mut e = Vec::new();
    if s.schema != SNAPSHOT_SCHEMA {
        e.push(format!("schema is {:?}, expected {SNAPSHOT_SCHEMA}", s.schema));
    }
    if content_hash(s) != s.content_sha256 {
        e.push("content_sha256 does not match the content (edited after acquisition?)".into());
    }
    match &s.resolution.chosen {
        None => e.push(format!("no candidate chosen: {}", s.resolution.reason)),
        Some(id) if !s.resolution.candidates.iter().any(|c| &c.id == id) => e.push(format!("chosen {id} is not among the candidates")),
        _ => {}
    }
    if !s.request.map_size.is_power_of_two() || !(64..=4096).contains(&s.request.map_size) {
        e.push(format!("map_size {} is not a power of two in 64..=4096", s.request.map_size));
    }
    if s.extent.map_size != s.request.map_size {
        e.push("extent.map_size differs from request.map_size".into());
    }
    for src in &s.sources {
        if src.license.trim().is_empty() || src.attribution.trim().is_empty() {
            e.push(format!("source {} has no license or attribution", src.id));
        }
        if !["complete", "partial", "failed"].contains(&src.completeness.as_str()) {
            e.push(format!("source {} completeness {:?}", src.id, src.completeness));
        }
    }
    // By layer kind, never by provider id (providers.v1.json): one vector and one elevation layer.
    for kind in ["vector", "elevation"] {
        let of: Vec<&Source> = s.sources.iter().filter(|x| x.kind == kind).collect();
        match of.as_slice() {
            [] => e.push(format!("no {kind} source")),
            [x] if x.completeness == "failed" => e.push(format!("{kind} source {} failed: {}", x.id, x.diagnostics.join("; "))),
            [_] => {}
            _ => e.push(format!("{} {kind} sources; exactly one is compiled", of.len())),
        }
    }
    for x in &s.sources {
        if !crate::providers::KINDS.contains(&x.kind.as_str()) {
            e.push(format!("source {} has unknown kind {:?}", x.id, x.kind));
        }
    }
    if !s.sources.iter().any(|x| x.id == s.elevation.source && x.kind == "elevation") {
        e.push(format!("elevation names source {} which is not an elevation source of this snapshot", s.elevation.source));
    }
    let n = s.elevation.grid as usize;
    if n < 2 || s.elevation.decimetres.len() != n * n {
        e.push(format!("elevation grid {n} needs {} samples, has {}", n * n, s.elevation.decimetres.len()));
    } else if s.elevation.decimetres.iter().all(|x| x.is_none()) {
        e.push("elevation grid has no values".into());
    }
    let mut last = "";
    for f in &s.features {
        if f.id.as_str() <= last {
            e.push(format!("features not strictly ordered by id at {}", f.id));
        }
        last = &f.id;
        if f.evidence != Evidence::Observed {
            e.push(format!("{}: a source feature is OBSERVED, not {:?}", f.id, f.evidence));
        }
        if !s.sources.iter().any(|x| x.id == f.source && x.kind == "vector") {
            e.push(format!("{}: source {} is not a vector source of this snapshot", f.id, f.source));
        }
    }
    for (i, t) in s.transformations.iter().enumerate() {
        if t.seq != i as u32 + 1 {
            e.push(format!("transformation {} out of sequence", t.seq));
        }
    }
    e
}
