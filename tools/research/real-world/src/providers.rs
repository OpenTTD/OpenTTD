//! The provider and layer registry (`providers.v1.json`, compiled in): who may be asked for which
//! layer, under which terms. Sources are built from it, and snapshots are validated by layer kind,
//! never by provider id.

use crate::model::Source;
use serde::Deserialize;

pub const REGISTRY: &str = include_str!("../providers.v1.json");
pub const KINDS: [&str; 3] = ["geocoder", "vector", "elevation"];

#[derive(Deserialize, Debug, Clone)]
pub struct Provider {
    pub id: String,
    pub kind: String,
    pub layer: String,
    pub provider: String,
    pub endpoint: String,
    pub license: String,
    pub attribution: String,
    pub allowed_purpose: String,
    pub retention: String,
    pub redistribution: String,
    pub terms_checked: String,
}

#[derive(Deserialize)]
struct Registry {
    providers: Vec<Provider>,
}

pub fn all() -> Vec<Provider> {
    let r: Registry = serde_json::from_str(REGISTRY).expect("providers.v1.json parses");
    r.providers
}

/// The registry's provider for a layer kind (the first row of that kind).
pub fn for_kind(kind: &str) -> Provider {
    all().into_iter().find(|p| p.kind == kind).unwrap_or_else(|| panic!("providers.v1.json has no {kind} provider"))
}

/// A snapshot source record from the registry row plus what this retrieval observed.
pub fn source(p: &Provider, retrieved_at: &str, raw: Vec<String>, requests: u32, completeness: &str, diagnostics: Vec<String>) -> Source {
    Source {
        id: p.id.clone(),
        kind: p.kind.clone(),
        provider: p.provider.clone(),
        endpoint: p.endpoint.clone(),
        layer: p.layer.clone(),
        retrieved_at: retrieved_at.into(),
        license: p.license.clone(),
        attribution: p.attribution.clone(),
        allowed_purpose: p.allowed_purpose.clone(),
        retention: p.retention.clone(),
        redistribution: p.redistribution.clone(),
        completeness: completeness.into(),
        requests,
        raw_sha256: raw,
        terms_checked: p.terms_checked.clone(),
        diagnostics,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_kind_has_a_provider_with_terms_and_attribution() {
        let all = all();
        for k in KINDS {
            let p = all.iter().find(|p| p.kind == k).unwrap_or_else(|| panic!("no {k} provider"));
            assert!(!p.license.is_empty() && !p.attribution.is_empty() && !p.terms_checked.is_empty(), "{}", p.id);
        }
        let ids: std::collections::BTreeSet<&str> = all.iter().map(|p| p.id.as_str()).collect();
        assert_eq!(ids.len(), all.len(), "provider ids are unique");
        assert!(all.iter().all(|p| KINDS.contains(&p.kind.as_str())));
        let dem = for_kind("elevation");
        assert!(dem.attribution.contains("do not incur any liability"), "Copernicus Art. 6(c)");
        assert!(dem.attribution.contains("not endorsed") && dem.attribution.contains("OpenStreetMap Foundation"), "Copernicus Art. 6(d)");
        assert!(dem.attribution.contains("bound by the same obligations"), "Copernicus Art. 6(e)");
        assert!(dem.attribution.contains("produced using Copernicus WorldDEM-30"), "Copernicus Art. 6(b)");
    }
}
