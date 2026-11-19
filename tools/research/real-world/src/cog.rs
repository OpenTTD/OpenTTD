//! A minimal reader for the Cloud Optimized GeoTIFFs of Copernicus DEM GLO-30 (RESEARCH-ONLY):
//! classic little-endian TIFF, tiled, one band of 32-bit floats, DEFLATE (compression 8) with or
//! without the floating-point predictor (3). Anything else is refused with its tag value, never
//! guessed. It reads bytes it is given; which bytes to fetch (the header, then only the tiles a
//! window needs) is the caller's choice, so the network sees range requests only.

pub struct Ifd {
    pub width: u32,
    pub height: u32,
    pub tile_width: u32,
    pub tile_height: u32,
    pub tile_offsets: Vec<u64>,
    pub tile_bytes: Vec<u64>,
    pub predictor: u16,
    /// Degrees per pixel (x, y) and the model position of the raster's upper-left corner.
    pub scale: (f64, f64),
    pub origin: (f64, f64),
    /// 1 PixelIsArea (the tiepoint is a pixel corner), 2 PixelIsPoint (a pixel centre).
    pub raster_type: u16,
}

fn u16le(b: &[u8], o: usize) -> Option<u16> {
    Some(u16::from_le_bytes(b.get(o..o + 2)?.try_into().ok()?))
}
fn u32le(b: &[u8], o: usize) -> Option<u32> {
    Some(u32::from_le_bytes(b.get(o..o + 4)?.try_into().ok()?))
}
fn f64le(b: &[u8], o: usize) -> Option<f64> {
    Some(f64::from_le_bytes(b.get(o..o + 8)?.try_into().ok()?))
}

/// The values of one entry as u64 (SHORT, LONG) or f64 (DOUBLE); `Err(needed)` when the entry's
/// values lie beyond the bytes given (the caller fetches more and tries again).
enum Vals {
    Ints(Vec<u64>),
    Floats(Vec<f64>),
}

fn entry(b: &[u8], e: usize) -> Result<(u16, Vals), usize> {
    let bad = b.len() + 1;
    let tag = u16le(b, e).ok_or(bad)?;
    let typ = u16le(b, e + 2).ok_or(bad)?;
    let count = u32le(b, e + 4).ok_or(bad)? as usize;
    let size = match typ { 3 => 2, 4 => 4, 12 => 8, 1 | 2 | 6 | 7 => 1, 8 => 2, 9 | 11 => 4, 16 => 8, _ => 1 } * count;
    let at = if size <= 4 { e + 8 } else { u32le(b, e + 8).ok_or(bad)? as usize };
    if at + size > b.len() {
        return Err(at + size);
    }
    let v = match typ {
        3 => Vals::Ints((0..count).map(|i| u16le(b, at + 2 * i).unwrap() as u64).collect()),
        4 => Vals::Ints((0..count).map(|i| u32le(b, at + 4 * i).unwrap() as u64).collect()),
        12 => Vals::Floats((0..count).map(|i| f64le(b, at + 8 * i).unwrap()).collect()),
        _ => Vals::Ints(vec![]),
    };
    Ok((tag, v))
}

/// The first image directory (full resolution). `Err(Ok(n))`: give me the first `n` bytes;
/// `Err(Err(msg))`: not a file this reader supports.
pub fn first_ifd(b: &[u8]) -> Result<Ifd, Result<usize, String>> {
    if b.len() < 8 {
        return Err(Ok(16 * 1024));
    }
    if &b[0..4] != b"II*\0" {
        return Err(Err("not a classic little-endian TIFF (II*\\0); BigTIFF and big-endian are not supported".into()));
    }
    let off = u32le(b, 4).unwrap() as usize;
    let n = match u16le(b, off) { Some(n) => n as usize, None => return Err(Ok(off + 2 + 4096)) };
    if off + 2 + 12 * n + 4 > b.len() {
        return Err(Ok(off + 2 + 12 * n + 4));
    }
    let (mut w, mut h, mut tw, mut th, mut comp, mut pred, mut bits, mut fmt, mut spp) = (0, 0, 0, 0, 1, 1, 0, 1, 1);
    let (mut offs, mut cnts, mut scale, mut tie, mut keys) = (vec![], vec![], vec![], vec![], vec![]);
    for i in 0..n {
        let (tag, v) = match entry(b, off + 2 + 12 * i) {
            Ok(x) => x,
            Err(need) => return Err(Ok(need)),
        };
        let first = || match &v { Vals::Ints(x) => x.first().copied().unwrap_or(0), Vals::Floats(_) => 0 };
        match tag {
            256 => w = first(),
            257 => h = first(),
            258 => bits = first(),
            259 => comp = first(),
            277 => spp = first(),
            317 => pred = first(),
            322 => tw = first(),
            323 => th = first(),
            339 => fmt = first(),
            324 => if let Vals::Ints(x) = &v { offs = x.clone() },
            325 => if let Vals::Ints(x) = &v { cnts = x.clone() },
            33550 => if let Vals::Floats(x) = &v { scale = x.clone() },
            33922 => if let Vals::Floats(x) = &v { tie = x.clone() },
            34735 => if let Vals::Ints(x) = &v { keys = x.clone() },
            _ => {}
        }
    }
    let refuse = |m: String| Err(Err(m));
    if comp != 8 { return refuse(format!("compression {comp}: only DEFLATE (8) is supported")); }
    if !(pred == 1 || pred == 3) { return refuse(format!("predictor {pred}: only none (1) and floating point (3)")); }
    if bits != 32 || fmt != 3 || spp != 1 { return refuse(format!("{spp} sample(s) of {bits}-bit format {fmt}: only one 32-bit float band")); }
    if tw == 0 || th == 0 || offs.is_empty() || offs.len() != cnts.len() { return refuse("not a tiled TIFF".into()); }
    if scale.len() < 2 || tie.len() < 6 { return refuse("no ModelPixelScale or ModelTiepoint: not georeferenced".into()); }
    // GeoKey 1025 GTRasterTypeGeoKey: header (4 shorts), then entries of 4 shorts.
    let mut raster_type = 1;
    for k in keys.chunks(4).skip(1) {
        if k.len() == 4 && k[0] == 1025 && k[1] == 0 {
            raster_type = k[3] as u16;
        }
    }
    Ok(Ifd {
        width: w as u32,
        height: h as u32,
        tile_width: tw as u32,
        tile_height: th as u32,
        tile_offsets: offs,
        tile_bytes: cnts,
        predictor: pred as u16,
        scale: (scale[0], scale[1]),
        origin: (tie[3] - tie[0] * scale[0], tie[4] + tie[1] * scale[1]),
        raster_type,
    })
}

impl Ifd {
    pub fn tiles_across(&self) -> u32 {
        self.width.div_ceil(self.tile_width)
    }

    /// Fractional pixel position (column, row) of a WGS84 point, pixel centres at whole numbers.
    pub fn pixel_of(&self, lat: f64, lon: f64) -> (f64, f64) {
        let half = if self.raster_type == 2 { 0.0 } else { 0.5 };
        ((lon - self.origin.0) / self.scale.0 - half, (self.origin.1 - lat) / self.scale.1 - half)
    }

    /// The index of the tile holding pixel (col, row).
    pub fn tile_index(&self, col: u32, row: u32) -> usize {
        ((row / self.tile_height) * self.tiles_across() + col / self.tile_width) as usize
    }
}

/// Inflate one tile and undo its predictor: `tile_width * tile_height` floats, row-major.
pub fn decode_tile(compressed: &[u8], ifd: &Ifd) -> Result<Vec<f32>, String> {
    let raw = miniz_oxide::inflate::decompress_to_vec_zlib(compressed).map_err(|e| format!("inflate: {e:?}"))?;
    let (w, h) = (ifd.tile_width as usize, ifd.tile_height as usize);
    if raw.len() != w * h * 4 {
        return Err(format!("tile inflates to {} bytes, {} expected", raw.len(), w * h * 4));
    }
    let mut out = Vec::with_capacity(w * h);
    if ifd.predictor == 1 {
        out.extend(raw.chunks(4).map(|c| f32::from_le_bytes([c[0], c[1], c[2], c[3]])));
        return Ok(out);
    }
    // Floating-point predictor (TIFF Technical Note 3): each row is byte-wise horizontally
    // differenced over w*4 bytes, the bytes laid out as planes, most significant first.
    for r in 0..h {
        let mut row = raw[r * w * 4..(r + 1) * w * 4].to_vec();
        for i in 1..row.len() {
            row[i] = row[i].wrapping_add(row[i - 1]);
        }
        for i in 0..w {
            out.push(f32::from_be_bytes([row[i], row[w + i], row[2 * w + i], row[3 * w + i]]));
        }
    }
    Ok(out)
}

/// The inverse of `decode_tile`'s predictor and compression, for tests.
pub fn encode_tile(values: &[f32], w: usize, predictor: u16) -> Vec<u8> {
    let mut raw = Vec::with_capacity(values.len() * 4);
    for row in values.chunks(w) {
        if predictor == 1 {
            for v in row {
                raw.extend_from_slice(&v.to_le_bytes());
            }
            continue;
        }
        let mut planes = vec![0u8; w * 4];
        for (i, v) in row.iter().enumerate() {
            let b = v.to_be_bytes();
            for k in 0..4 {
                planes[k * w + i] = b[k];
            }
        }
        for i in (1..planes.len()).rev() {
            planes[i] = planes[i].wrapping_sub(planes[i - 1]);
        }
        raw.extend(planes);
    }
    miniz_oxide::deflate::compress_to_vec_zlib(&raw, 6)
}

/// Bilinear value at fractional pixel (col, row) from a pixel getter; `None` outside the raster.
pub fn bilinear(col: f64, row: f64, width: u32, height: u32, mut px: impl FnMut(u32, u32) -> Option<f32>) -> Option<f64> {
    let c = col.clamp(0.0, (width - 1) as f64);
    let r = row.clamp(0.0, (height - 1) as f64);
    let (c0, r0) = (c.floor() as u32, r.floor() as u32);
    let (c1, r1) = ((c0 + 1).min(width - 1), (r0 + 1).min(height - 1));
    let (tc, tr) = (c - c0 as f64, r - r0 as f64);
    let a = px(c0, r0)? as f64 * (1.0 - tc) + px(c1, r0)? as f64 * tc;
    let b = px(c0, r1)? as f64 * (1.0 - tc) + px(c1, r1)? as f64 * tc;
    Some(a * (1.0 - tr) + b * tr)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ifd(pred: u16) -> Ifd {
        Ifd { width: 8, height: 4, tile_width: 4, tile_height: 4, tile_offsets: vec![0, 0], tile_bytes: vec![0, 0], predictor: pred,
              scale: (0.25, 0.25), origin: (16.0, 50.0), raster_type: 1 }
    }

    #[test]
    fn tiles_round_trip_with_and_without_the_floating_point_predictor() {
        let v: Vec<f32> = (0..16).map(|i| 200.0 + i as f32 * 1.25 - (i % 3) as f32 * 7.5).collect();
        for pred in [1, 3] {
            let enc = encode_tile(&v, 4, pred);
            assert_eq!(decode_tile(&enc, &ifd(pred)).unwrap(), v, "predictor {pred}");
        }
    }

    #[test]
    fn pixel_positions_follow_pixel_is_area() {
        let i = ifd(3);
        let (c, r) = i.pixel_of(50.0 - 0.125, 16.125);
        assert!((c - 0.0).abs() < 1e-9 && (r - 0.0).abs() < 1e-9, "the first pixel's centre is pixel (0, 0)");
        assert_eq!(i.tile_index(5, 1), 1);
        let v = bilinear(0.5, 0.0, 8, 4, |c, _| Some(c as f32 * 10.0)).unwrap();
        assert!((v - 5.0).abs() < 1e-9);
    }

    #[test]
    fn the_glo30_header_parses_to_its_documented_layout() {
        // The first 1200 bytes of Copernicus_DSM_COG_10_N49_00_E016_00_DEM.tif (its directories only).
        let b = std::fs::read(concat!(env!("CARGO_MANIFEST_DIR"), "/fixtures/glo30/N49_00_E016_00.header.bin")).unwrap();
        let i = first_ifd(&b).unwrap_or_else(|e| panic!("{:?}", e.err()));
        assert_eq!((i.width, i.height, i.tile_width, i.tile_height, i.predictor), (3600, 3600, 1024, 1024, 3));
        assert_eq!(i.tile_offsets.len(), 16);
        assert!((i.origin.0 - 16.0).abs() < 1e-9 && (i.origin.1 - 50.0).abs() < 1e-9);
        assert!((i.scale.0 - 1.0 / 3600.0).abs() < 1e-12);
        assert!(first_ifd(&b[..100]).is_err(), "a short header asks for more bytes");
    }
}
