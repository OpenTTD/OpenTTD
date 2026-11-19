//! The versioned, data-driven rules (`rules/*.json`): how source features are classified, which
//! game role each class becomes, the game profile the roles must match, and the balance
//! assumptions. No place name appears in them or in the code that reads them.

use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Rules {
    pub id: String,
    pub version: u32,
    pub description: String,
    pub profile: Profile,
    pub classify: Vec<ClassifyRule>,
    pub keep_tags: Vec<String>,
    /// Per feature class: at most this many are kept at acquisition, largest first.
    pub acquire_limit: BTreeMap<String, usize>,
    pub map: BTreeMap<String, MapRule>,
    pub settlement: SettlementRules,
    pub economy: EconomyRules,
    pub heightmap: HeightmapRules,
    pub border_tiles: u32,
    pub industry_spacing_tiles: u32,
    /// Landmark signs keep this distance from each other (so a dense cluster does not crowd out the rest).
    pub sign_spacing_tiles: u32,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Profile {
    pub climate: String,
    /// `game_creation.landscape` value of the climate.
    pub landscape: u32,
    pub source: String,
    pub industries: BTreeMap<String, Role>,
    pub town: Role,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Role {
    pub produces: Vec<String>,
    pub accepts: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ClassifyRule {
    pub class: String,
    pub elements: Vec<String>,
    /// Every key must carry one of its values (an empty list: any value).
    pub tags: BTreeMap<String, Vec<String>>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct MapRule {
    /// `town`, `industry`, `sign` or `none` (not represented; reported as lost detail).
    pub kind: String,
    #[serde(default)]
    pub role: String,
    pub evidence: crate::model::Evidence,
    pub confidence: f64,
    pub rationale: String,
    #[serde(default)]
    pub max: usize,
    /// The role is a gameplay stand-in the source does not support (no game industry matches what
    /// was mapped): the entity is HYPOTHETICAL, anchored at the feature but not evidenced by it.
    #[serde(default)]
    pub substitute: bool,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct SettlementRules {
    /// Game population = source population x scale, clamped (a balance assumption, not a fact).
    pub population_scale: f64,
    /// Game population = scale x source population ^ exponent (compresses a city against its villages).
    pub population_exponent: f64,
    pub min_population: u32,
    pub max_population: u32,
    /// Population assumed when the source has none, by `place` value (INFERRED).
    pub default_population: BTreeMap<String, u32>,
    pub city_from_population: u32,
    pub max_towns: usize,
    /// `place` values that are parts of a larger settlement (a suburb of a city): never a town of
    /// their own; counted as part of the nearest city or town.
    #[serde(default)]
    pub part_of_city_places: Vec<String>,
    /// `place` values a part is assigned to (the nearest one), e.g. city and town.
    #[serde(default)]
    pub whole_places: Vec<String>,
    pub min_spacing_tiles: u32,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct EconomyRules {
    pub max_consumers_per_cargo: usize,
    pub max_edge_tiles: u32,
    pub passenger_min_tiles: u32,
    pub passenger_max_tiles: u32,
    pub passenger_pairs: usize,
    /// Towns from this game population accept goods (the game needs enough houses that accept them).
    pub goods_town_min_population: u32,
    pub station_radius_tiles: u32,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HeightmapRules {
    pub metres_per_level: f64,
    pub min_levels: u32,
    pub max_levels: u32,
}

impl Rules {
    pub fn load(path: &std::path::Path) -> Result<(Rules, Vec<u8>), String> {
        let bytes = std::fs::read(path).map_err(|e| format!("{}: {e}", path.display()))?;
        let r: Rules = serde_json::from_slice(&bytes).map_err(|e| format!("{}: {e}", path.display()))?;
        r.check()?;
        Ok((r, bytes))
    }

    /// Consistency: every mapped role exists in the profile, every class is classified.
    pub fn check(&self) -> Result<(), String> {
        for (class, m) in &self.map {
            if m.kind == "industry" && !self.profile.industries.contains_key(&m.role) {
                return Err(format!("rules: class {class} maps to role {} which the profile does not define", m.role));
            }
            if !self.classify.iter().any(|c| &c.class == class) {
                return Err(format!("rules: class {class} is mapped but never classified"));
            }
            if m.substitute && m.evidence != crate::model::Evidence::Hypothetical {
                return Err(format!("rules: class {class} is a gameplay stand-in and must be HYPOTHETICAL"));
            }
            if !(0.0..=1.0).contains(&m.confidence) {
                return Err(format!("rules: class {class} has confidence outside 0..1"));
            }
        }
        for c in &self.classify {
            if !self.map.contains_key(&c.class) {
                return Err(format!("rules: class {} is classified but has no map rule", c.class));
            }
        }
        Ok(())
    }

    pub fn rule_ref(&self, name: &str) -> String {
        format!("{name}@{}/v{}", self.id, self.version)
    }

    /// The first classify rule a tag set satisfies.
    pub fn classify_tags(&self, element: &str, tags: &BTreeMap<String, String>) -> Option<&ClassifyRule> {
        self.classify.iter().find(|r| {
            r.elements.iter().any(|e| e == element)
                && r.tags.iter().all(|(k, vs)| tags.get(k).map(|v| vs.is_empty() || vs.contains(v)).unwrap_or(false))
        })
    }

    /// One Overpass QL query for every classify rule, bounded by the extent's box.
    pub fn overpass_query(&self, s: f64, w: f64, n: f64, e: f64) -> String {
        let mut q = format!("[out:json][timeout:90][bbox:{s},{w},{n},{e}];\n(\n");
        for r in &self.classify {
            for el in &r.elements {
                q.push_str("  ");
                q.push_str(el);
                for (k, vs) in &r.tags {
                    if vs.is_empty() {
                        q.push_str(&format!("[\"{k}\"]"));
                    } else if vs.len() == 1 {
                        q.push_str(&format!("[\"{k}\"=\"{}\"]", vs[0]));
                    } else {
                        q.push_str(&format!("[\"{k}\"~\"^({})$\"]", vs.join("|")));
                    }
                }
                q.push_str(";\n");
            }
        }
        q.push_str(")->.a;\nnode.a;\nout body;\n(way.a; relation.a;);\nout tags bb;\n");
        q
    }
}
