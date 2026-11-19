//! Place text to ranked candidates, and the rule that chooses one or refuses to.
//!
//! Rule `dominant-settlement/v1`: an explicit `--pick <id>` wins (and must name a candidate);
//! otherwise only settlement candidates (Nominatim `addresstype` or place type city, town,
//! village, municipality, hamlet) may be chosen. One settlement is chosen. Several are chosen
//! only when the first has at least `DOMINANCE` times the importance of the second; anything
//! else is AMBIGUOUS and nothing is chosen. There is no fallback to a wider area or the world.

use crate::model::{round_to, BBox, Candidate, LatLon, Resolution};
use serde_json::Value;

pub const RULE: &str = "dominant-settlement/v1";
pub const DOMINANCE: f64 = 2.0;
const SETTLEMENT: [&str; 5] = ["city", "town", "village", "municipality", "hamlet"];

fn num(v: &Value) -> f64 {
    match v {
        Value::String(s) => s.parse().unwrap_or(f64::NAN),
        Value::Number(n) => n.as_f64().unwrap_or(f64::NAN),
        _ => f64::NAN,
    }
}

/// Nominatim `format=jsonv2` search results to candidates, ranked deterministically:
/// importance descending, then place rank ascending, then id.
pub fn candidates_from_nominatim(body: &[u8]) -> Result<Vec<Candidate>, String> {
    let v: Value = serde_json::from_slice(body).map_err(|e| format!("nominatim: not JSON: {e}"))?;
    let arr = v.as_array().ok_or("nominatim: expected an array")?;
    let mut out = Vec::new();
    for c in arr {
        let bb = c["boundingbox"].as_array().ok_or("nominatim: candidate without boundingbox")?;
        if bb.len() != 4 {
            return Err("nominatim: boundingbox must have 4 values".into());
        }
        let bbox = BBox {
            south: round_to(num(&bb[0]), 7),
            north: round_to(num(&bb[1]), 7),
            west: round_to(num(&bb[2]), 7),
            east: round_to(num(&bb[3]), 7),
        };
        if [bbox.south, bbox.north, bbox.west, bbox.east].iter().any(|x| !x.is_finite()) {
            return Err("nominatim: boundingbox is not numeric".into());
        }
        let category = c["category"].as_str().or(c["class"].as_str()).unwrap_or("").to_string();
        let kind = c["type"].as_str().unwrap_or("").to_string();
        let addresstype = c["addresstype"].as_str().unwrap_or("");
        let settlement = SETTLEMENT.contains(&addresstype) || (category == "place" && SETTLEMENT.contains(&kind.as_str()));
        out.push(Candidate {
            rank: 0,
            id: format!("osm:{}/{}", c["osm_type"].as_str().unwrap_or("?"), c["osm_id"].as_u64().unwrap_or(0)),
            name: c["name"].as_str().unwrap_or("").to_string(),
            display_name: c["display_name"].as_str().unwrap_or("").to_string(),
            category,
            kind,
            place_rank: c["place_rank"].as_u64().unwrap_or(30) as u32,
            importance: round_to(num(&c["importance"]).max(0.0), 6),
            country_code: c["address"]["country_code"].as_str().unwrap_or("").to_string(),
            center: LatLon {
                lat: round_to((bbox.south + bbox.north) / 2.0, 7),
                lon: round_to((bbox.west + bbox.east) / 2.0, 7),
            },
            bbox,
            settlement,
        });
    }
    rank(&mut out);
    Ok(out)
}

pub fn rank(c: &mut [Candidate]) {
    c.sort_by(|a, b| {
        b.importance
            .partial_cmp(&a.importance)
            .unwrap_or(std::cmp::Ordering::Equal)
            .then(a.place_rank.cmp(&b.place_rank))
            .then(a.id.cmp(&b.id))
    });
    for (i, x) in c.iter_mut().enumerate() {
        x.rank = i as u32 + 1;
    }
}

pub fn choose(candidates: Vec<Candidate>, pick: Option<&str>, provider: &str, source: &str) -> Resolution {
    let mk = |chosen: Option<String>, ambiguous: bool, reason: String, candidates: Vec<Candidate>| Resolution {
        provider: provider.into(),
        source: source.into(),
        candidates,
        chosen,
        ambiguous,
        rule: RULE.into(),
        reason,
    };
    if candidates.is_empty() {
        return mk(None, true, "no candidate matched; nothing is chosen and there is no fallback to a wider area".into(), candidates);
    }
    if let Some(p) = pick {
        return match candidates.iter().find(|c| c.id == p) {
            Some(c) => {
                let r = format!("picked explicitly: {} ({})", c.id, c.display_name);
                let id = c.id.clone();
                mk(Some(id), false, r, candidates)
            }
            None => mk(None, true, format!("--pick {p} names no candidate of this query"), candidates),
        };
    }
    let s: Vec<&Candidate> = candidates.iter().filter(|c| c.settlement).collect();
    match s.len() {
        0 => mk(None, true, format!("none of the {} candidates is a settlement; pick one with --pick <id>", candidates.len()), candidates),
        1 => {
            let r = format!("the only settlement among {} candidate(s): {} ({})", candidates.len(), s[0].id, s[0].display_name);
            let id = s[0].id.clone();
            mk(Some(id), false, r, candidates)
        }
        _ => {
            let (a, b) = (s[0], s[1]);
            if b.importance <= 0.0 || a.importance / b.importance >= DOMINANCE {
                let r = format!(
                    "{} settlements; the first ({}, importance {}) dominates the second ({}, importance {}) by at least {}x",
                    s.len(), a.id, a.importance, b.id, b.importance, DOMINANCE
                );
                let id = a.id.clone();
                mk(Some(id), false, r, candidates)
            } else {
                let r = format!(
                    "AMBIGUOUS: {} settlements and the first two are within {}x of each other ({} {} vs {} {}); pick one with --pick <id>",
                    s.len(), DOMINANCE, a.id, a.display_name, b.id, b.display_name
                );
                mk(None, true, r, candidates)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cand(id: &str, imp: f64, settlement: bool) -> Candidate {
        Candidate {
            rank: 0,
            id: id.into(),
            name: id.into(),
            display_name: id.into(),
            category: "boundary".into(),
            kind: "administrative".into(),
            place_rank: 16,
            importance: imp,
            country_code: "xx".into(),
            bbox: BBox { south: 0.0, west: 0.0, north: 0.1, east: 0.1 },
            center: LatLon { lat: 0.05, lon: 0.05 },
            settlement,
        }
    }

    #[test]
    fn two_comparable_settlements_are_ambiguous_and_nothing_is_chosen() {
        let r = choose(vec![cand("osm:relation/1", 0.5, true), cand("osm:relation/2", 0.4, true)], None, "t", "t");
        assert!(r.ambiguous);
        assert_eq!(r.chosen, None);
        assert!(r.reason.starts_with("AMBIGUOUS"));
    }

    #[test]
    fn a_dominant_or_only_settlement_is_chosen_and_a_pick_wins() {
        let r = choose(vec![cand("osm:relation/1", 0.7, true), cand("osm:relation/2", 0.2, false)], None, "t", "t");
        assert_eq!(r.chosen.as_deref(), Some("osm:relation/1"));
        let r = choose(vec![cand("osm:relation/1", 0.5, true), cand("osm:relation/2", 0.45, true)], Some("osm:relation/2"), "t", "t");
        assert_eq!(r.chosen.as_deref(), Some("osm:relation/2"));
        let r = choose(vec![cand("osm:relation/1", 0.5, true)], Some("osm:relation/9"), "t", "t");
        assert!(r.ambiguous && r.chosen.is_none());
    }

    #[test]
    fn no_candidate_never_falls_back() {
        let r = choose(vec![], None, "t", "t");
        assert!(r.ambiguous && r.chosen.is_none());
    }
}
