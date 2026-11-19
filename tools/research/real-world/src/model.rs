//! The canonical data contract: `real-world-snapshot/v1` (what was acquired) and
//! `real-world-scenario/v1` (what was compiled from it). The JSON Schema of both is
//! `tools/research/schema/real-world-scenario.v1.schema.json`; this file is its implementation.
//!
//! Determinism: every collection is a `Vec` sorted by a stable key before it is written, every
//! tag map is a `BTreeMap`, and floats are rounded to a fixed precision (`round_to`) where they
//! are produced, so the same inputs serialise to the same bytes.

use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

pub const SNAPSHOT_SCHEMA: &str = "real-world-snapshot/v1";
pub const SCENARIO_SCHEMA: &str = "real-world-scenario/v1";
pub const TOOL_NAME: &str = "rw-scenario";
pub const TOOL_VERSION: &str = env!("CARGO_PKG_VERSION");

/// The one evidence class every feature, entity and edge carries.
#[derive(Serialize, Deserialize, Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum Evidence {
    /// The source records it (a mapped feature, a tag value).
    Observed,
    /// A deterministic rule turned an observed fact into a game construct of the same kind.
    Derived,
    /// A judgement under uncertainty: a role the source does not state, a default for a missing value.
    Inferred,
    /// Invented for playability; no source supports it.
    Hypothetical,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Tool {
    pub name: String,
    pub version: String,
}

impl Tool {
    pub fn this() -> Self {
        Tool { name: TOOL_NAME.into(), version: TOOL_VERSION.into() }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Request {
    /// The place exactly as the user typed it.
    pub query: String,
    /// Tiles per map side (a power of two, 64..=4096); the map is square.
    pub map_size: u32,
    /// Elevation samples per side of the extent.
    pub elevation_grid: u32,
    /// Extra fraction added around the candidate's bounding box before squaring it.
    pub margin: f64,
    /// The candidate the user chose explicitly (`--pick <id>`), if any.
    pub pick: Option<String>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct BBox {
    pub south: f64,
    pub west: f64,
    pub north: f64,
    pub east: f64,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct LatLon {
    pub lat: f64,
    pub lon: f64,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Candidate {
    /// 1-based rank after the deterministic ordering of `resolve::rank`.
    pub rank: u32,
    /// Stable source id, `osm:<type>/<id>`.
    pub id: String,
    pub name: String,
    pub display_name: String,
    pub category: String,
    pub kind: String,
    pub place_rank: u32,
    pub importance: f64,
    pub country_code: String,
    pub bbox: BBox,
    pub center: LatLon,
    /// Whether the candidate is a settlement (a city, town or village, or the boundary of one).
    pub settlement: bool,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Resolution {
    pub provider: String,
    pub source: String,
    pub candidates: Vec<Candidate>,
    /// The chosen candidate's id; `None` when the query is ambiguous and nobody picked.
    pub chosen: Option<String>,
    pub ambiguous: bool,
    /// The rule that chose (or refused to choose) and why, in words.
    pub rule: String,
    pub reason: String,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Crs {
    pub id: String,
    pub description: String,
    pub origin: LatLon,
    pub radius_m: f64,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Extent {
    pub crs: Crs,
    /// Side of the square extent in metres (local CRS).
    pub side_m: f64,
    /// WGS84 box enclosing the square; what every bounded request asks for.
    pub bbox: BBox,
    pub map_size: u32,
    pub metres_per_tile: f64,
}

/// One source the snapshot used: who, when, under which terms, and what came back.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Source {
    pub id: String,
    /// The layer kind from the provider registry (`providers.v1.json`): geocoder, vector, elevation.
    pub kind: String,
    pub provider: String,
    pub endpoint: String,
    pub layer: String,
    /// ISO 8601 UTC; recorded at acquisition, never read by the compiler.
    pub retrieved_at: String,
    pub license: String,
    pub attribution: String,
    pub allowed_purpose: String,
    pub retention: String,
    pub redistribution: String,
    /// `complete`, `partial` or `failed`.
    pub completeness: String,
    pub requests: u32,
    /// SHA-256 of every raw payload, in request order; the payloads themselves stay in the cache.
    pub raw_sha256: Vec<String>,
    pub terms_checked: String,
    pub diagnostics: Vec<String>,
}

/// A normalised source feature. `evidence` is always OBSERVED: it is what the source records.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Feature {
    /// `osm:<node|way|relation>/<id>`.
    pub id: String,
    /// The normalised class: settlement, landmark, extraction, power_plant, works,
    /// industrial_area, farmland, forest, rail_station, water.
    pub class: String,
    pub name: Option<String>,
    pub at: LatLon,
    /// The source's bounding box for an area feature.
    pub bbox: Option<BBox>,
    /// Area of the bounding box in square metres (local CRS), 0 for a point.
    pub bbox_area_m2: f64,
    /// The source tags kept (`rules.keep_tags`), verbatim.
    pub tags: BTreeMap<String, String>,
    pub source: String,
    pub evidence: Evidence,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct ElevationLayer {
    pub source: String,
    /// Samples per side; row-major, north to south, west to east, at `i / (n - 1)` of the side.
    pub grid: u32,
    /// Elevation in decimetres above sea level, `None` where the source returned none.
    pub decimetres: Vec<Option<i32>>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Transformation {
    pub seq: u32,
    pub step: String,
    pub tool: Tool,
    pub params: serde_json::Value,
    pub output_sha256: String,
    pub diagnostics: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Snapshot {
    pub schema: String,
    pub tool: Tool,
    pub request: Request,
    pub resolution: Resolution,
    pub extent: Extent,
    pub sources: Vec<Source>,
    pub elevation: ElevationLayer,
    pub features: Vec<Feature>,
    pub transformations: Vec<Transformation>,
    /// SHA-256 of this document serialised with this field empty.
    pub content_sha256: String,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Tile {
    pub x: u32,
    pub y: u32,
}

/// What a game entity is in the game.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct GameType {
    /// `town`, `industry` or `sign`.
    pub kind: String,
    /// The profile's role (`coal_mine`, `power_station`, ...) for an industry; empty otherwise.
    pub role: String,
    pub produces: Vec<String>,
    pub accepts: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Entity {
    pub id: String,
    pub name: String,
    pub game: GameType,
    pub tile: Tile,
    /// The feature that is evidence for this entity as it is in the game; `None` exactly when the
    /// entity is HYPOTHETICAL (invented, or a gameplay stand-in the source does not support).
    pub evidence_feature: Option<String>,
    /// The feature whose position the entity takes, also for a HYPOTHETICAL stand-in (a quarry site
    /// standing in for a coal mine is anchored at the quarry but evidenced by nothing).
    pub anchor_feature: Option<String>,
    /// The source's own label for it (class plus the tag that classified it), kept apart from its role.
    pub source_label: Option<String>,
    pub evidence: Evidence,
    pub confidence: f64,
    pub rationale: String,
    /// `<rule id>@<rules version>`.
    pub rule: String,
    /// Town only: target population in the game and how it was obtained.
    pub population: Option<u32>,
    pub population_basis: Option<String>,
    pub city: bool,
    /// The anchor feature's mapped extent in tiles (its bounding box), so a placement can be judged
    /// "at its mapped site" or "relocated outside it"; absent for a point anchor or none.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub site: Option<TileBox>,
}

/// An inclusive box of tiles.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct TileBox {
    pub x0: u32,
    pub y0: u32,
    pub x1: u32,
    pub y1: u32,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Edge {
    pub id: String,
    pub from: String,
    pub to: String,
    pub cargo: String,
    pub distance_tiles: u32,
    pub basis: String,
    pub evidence: Evidence,
    pub confidence: f64,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Economy {
    pub edges: Vec<Edge>,
    /// Economy nodes (towns and industries) with no edge; must be empty for a playable result.
    pub isolated: Vec<String>,
    /// Cargo chains that end in a consumer, by cargo.
    pub chains: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Heightmap {
    pub file: String,
    pub sha256: String,
    pub width: u32,
    pub height: u32,
    pub min_elevation_m: f64,
    pub max_elevation_m: f64,
    /// `game_creation.heightmap_height`: the height level grey 255 becomes.
    pub levels: u32,
    pub metres_per_level: f64,
    pub grey_rule: String,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Artifact {
    pub file: String,
    pub sha256: String,
    pub purpose: String,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct RulesRef {
    pub id: String,
    pub version: u32,
    pub sha256: String,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Scenario {
    pub schema: String,
    pub tool: Tool,
    pub rules: RulesRef,
    pub snapshot_sha256: String,
    pub query: String,
    pub chosen: String,
    pub extent: Extent,
    pub heightmap: Heightmap,
    /// The OpenTTD settings the heightmap and GameScript assume, as `section.key = value`.
    pub settings: BTreeMap<String, String>,
    pub entities: Vec<Entity>,
    pub economy: Economy,
    /// Features not represented in the game, and detail lost on the way.
    pub lost_detail: Vec<String>,
    pub attribution: Vec<String>,
    pub transformations: Vec<Transformation>,
    pub artifacts: Vec<Artifact>,
    pub diagnostics: Vec<String>,
}

/// Round to `decimals` places so that the same value always serialises to the same digits.
pub fn round_to(v: f64, decimals: i32) -> f64 {
    let m = 10f64.powi(decimals);
    (v * m).round() / m
}
