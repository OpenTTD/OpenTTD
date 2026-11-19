//! The local projected CRS and the extent-to-tile scale.
//!
//! CRS `local-eqr/v1`: an equirectangular tangent plane centred on the chosen candidate,
//! `x = R * (lon - lon0) * cos(lat0)`, `y = R * (lat - lat0)`, on the WGS84 mean sphere
//! (R = 6 371 008.8 m). Over the extents this tool allows (at most 80 km a side) its scale error
//! against a geodesic is below 0.1 % in latitude and a few tenths of a percent in longitude
//! at the extent's corners, an order of magnitude below one tile. It is not an EPSG CRS and says so.
//!
//! Map orientation (OpenTTD heightmap, rotation "clockwise", `heightmap_rotation = 1`):
//! image row 0 is the north edge, column 0 the west edge; tile x = image row, tile y = image
//! column (`src/heightmap.cpp`, `GrayscaleToMapHeights`, case HM_CLOCKWISE). The town-data
//! JSON therefore carries `x = row / height` and `y = column / width` (docs/importing_town_data.md).

use crate::model::{round_to, BBox, Crs, Extent, LatLon, Tile};

pub const EARTH_RADIUS_M: f64 = 6_371_008.8;
pub const MIN_SIDE_M: f64 = 4_000.0;
pub const MAX_SIDE_M: f64 = 80_000.0;

pub fn crs(origin: &LatLon) -> Crs {
    Crs {
        id: "local-eqr/v1".into(),
        description: "Equirectangular tangent plane at the chosen candidate's centre on the WGS84 mean sphere; \
                      x east, y north, metres. Not an EPSG CRS."
            .into(),
        origin: origin.clone(),
        radius_m: EARTH_RADIUS_M,
    }
}

/// WGS84 to local metres.
pub fn project(origin: &LatLon, p: &LatLon) -> (f64, f64) {
    let lat0 = origin.lat.to_radians();
    let x = EARTH_RADIUS_M * (p.lon - origin.lon).to_radians() * lat0.cos();
    let y = EARTH_RADIUS_M * (p.lat - origin.lat).to_radians();
    (x, y)
}

/// Local metres to WGS84.
pub fn unproject(origin: &LatLon, x: f64, y: f64) -> LatLon {
    let lat0 = origin.lat.to_radians();
    let lat = origin.lat + (y / EARTH_RADIUS_M).to_degrees();
    let lon = origin.lon + (x / (EARTH_RADIUS_M * lat0.cos())).to_degrees();
    LatLon { lat: round_to(lat, 7), lon: round_to(lon, 7) }
}

/// A square extent around the candidate box: its larger side plus `margin`, clamped to
/// `MIN_SIDE_M..=MAX_SIDE_M`. Returns the extent and a diagnostic when it was clamped.
pub fn square_extent(center: &LatLon, bbox: &BBox, margin: f64, map_size: u32) -> (Extent, Option<String>) {
    let (w, s) = project(center, &LatLon { lat: bbox.south, lon: bbox.west });
    let (e, n) = project(center, &LatLon { lat: bbox.north, lon: bbox.east });
    let raw = (e - w).abs().max((n - s).abs()) * (1.0 + margin);
    let side = raw.clamp(MIN_SIDE_M, MAX_SIDE_M);
    let note = if (side - raw).abs() > 1e-6 {
        Some(format!("extent side {:.0} m clamped to {:.0} m (allowed {:.0}..{:.0})", raw, side, MIN_SIDE_M, MAX_SIDE_M))
    } else {
        None
    };
    let side = side.round();
    let h = side / 2.0;
    let sw = unproject(center, -h, -h);
    let ne = unproject(center, h, h);
    let extent = Extent {
        crs: crs(center),
        side_m: side,
        bbox: BBox { south: sw.lat, west: sw.lon, north: ne.lat, east: ne.lon },
        map_size,
        metres_per_tile: round_to(side / map_size as f64, 3),
    };
    (extent, note)
}

/// Fractional image position (row, column) of a point, 0..1 from the north-west corner.
pub fn image_fraction(extent: &Extent, p: &LatLon) -> (f64, f64) {
    let (x, y) = project(&extent.crs.origin, p);
    let h = extent.side_m / 2.0;
    let row = (h - y) / extent.side_m;
    let col = (x + h) / extent.side_m;
    (row, col)
}

/// The tile of a point, or `None` when it lies outside the map's usable interior
/// (`border` tiles from each edge stay free: the void edge and the heightmap's border row).
pub fn tile_of(extent: &Extent, p: &LatLon, border: u32) -> Option<Tile> {
    let (row, col) = image_fraction(extent, p);
    let n = extent.map_size as f64;
    let (tx, ty) = ((row * n).floor(), (col * n).floor());
    let lo = border as f64;
    let hi = n - 1.0 - border as f64;
    if tx < lo || ty < lo || tx > hi || ty > hi {
        return None;
    }
    Some(Tile { x: tx as u32, y: ty as u32 })
}

/// Area of a WGS84 box in square metres in the local CRS.
pub fn bbox_area_m2(origin: &LatLon, b: &BBox) -> f64 {
    let (w, s) = project(origin, &LatLon { lat: b.south, lon: b.west });
    let (e, n) = project(origin, &LatLon { lat: b.north, lon: b.east });
    ((e - w) * (n - s)).abs()
}

pub fn manhattan(a: &Tile, b: &Tile) -> u32 {
    a.x.abs_diff(b.x) + a.y.abs_diff(b.y)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn projection_round_trips_within_a_centimetre() {
        let o = LatLon { lat: 49.2, lon: 16.6 };
        let p = LatLon { lat: 49.31, lon: 16.45 };
        let (x, y) = project(&o, &p);
        let q = unproject(&o, x, y);
        assert!((q.lat - p.lat).abs() < 1e-6 && (q.lon - p.lon).abs() < 1e-6);
    }

    #[test]
    fn the_extent_is_square_and_clamped() {
        let c = LatLon { lat: 50.0, lon: 10.0 };
        let tiny = BBox { south: 49.999, west: 9.999, north: 50.001, east: 10.001 };
        let (e, note) = square_extent(&c, &tiny, 0.1, 256);
        assert_eq!(e.side_m, MIN_SIDE_M);
        assert!(note.is_some());
        let (_, ns) = project(&c, &LatLon { lat: e.bbox.north, lon: c.lon });
        assert!((ns - MIN_SIDE_M / 2.0).abs() < 0.5);
    }

    #[test]
    fn north_west_is_tile_zero_and_points_outside_are_refused() {
        let c = LatLon { lat: 50.0, lon: 10.0 };
        let b = BBox { south: 49.95, west: 9.93, north: 50.05, east: 10.07 };
        let (e, _) = square_extent(&c, &b, 0.0, 256);
        let nw = unproject(&c, -e.side_m / 2.0 + 300.0, e.side_m / 2.0 - 300.0);
        let t = tile_of(&e, &nw, 1).unwrap();
        let near = (300.0 / e.metres_per_tile).ceil() as u32;
        assert!(t.x <= near && t.y <= near, "{t:?} not within {near} tiles of the north-west corner");
        let centre = tile_of(&e, &c, 1).unwrap();
        assert_eq!((centre.x, centre.y), (128, 128));
        assert!(tile_of(&e, &LatLon { lat: 51.0, lon: 10.0 }, 1).is_none());
    }
}
