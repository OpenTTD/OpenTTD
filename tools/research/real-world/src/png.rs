//! An 8-bit greyscale PNG writer with no dependencies: zlib with stored (uncompressed) deflate
//! blocks, filter 0 on every row. The bytes depend only on the pixels, so the file hash is a
//! stable function of the heightmap. OpenTTD reads it through libpng (`src/heightmap.cpp`).

fn crc_table() -> [u32; 256] {
    let mut t = [0u32; 256];
    for (n, e) in t.iter_mut().enumerate() {
        let mut c = n as u32;
        for _ in 0..8 {
            c = if c & 1 != 0 { 0xEDB8_8320 ^ (c >> 1) } else { c >> 1 };
        }
        *e = c;
    }
    t
}

fn crc32(t: &[u32; 256], parts: &[&[u8]]) -> u32 {
    let mut c = 0xFFFF_FFFFu32;
    for p in parts {
        for &b in *p {
            c = t[((c ^ b as u32) & 0xFF) as usize] ^ (c >> 8);
        }
    }
    c ^ 0xFFFF_FFFF
}

fn adler32(data: &[u8]) -> u32 {
    let (mut a, mut b) = (1u32, 0u32);
    for &x in data {
        a = (a + x as u32) % 65_521;
        b = (b + a) % 65_521;
    }
    (b << 16) | a
}

fn chunk(out: &mut Vec<u8>, t: &[u32; 256], kind: &[u8; 4], data: &[u8]) {
    out.extend_from_slice(&(data.len() as u32).to_be_bytes());
    out.extend_from_slice(kind);
    out.extend_from_slice(data);
    out.extend_from_slice(&crc32(t, &[kind, data]).to_be_bytes());
}

pub fn grey(width: u32, height: u32, pixels: &[u8]) -> Vec<u8> {
    assert_eq!(pixels.len(), (width * height) as usize);
    let t = crc_table();
    let mut raw = Vec::with_capacity(pixels.len() + height as usize);
    for row in pixels.chunks(width as usize) {
        raw.push(0);
        raw.extend_from_slice(row);
    }
    let mut z = vec![0x78, 0x01];
    let blocks: Vec<&[u8]> = raw.chunks(65_535).collect();
    for (i, b) in blocks.iter().enumerate() {
        z.push(if i + 1 == blocks.len() { 1 } else { 0 });
        let len = b.len() as u16;
        z.extend_from_slice(&len.to_le_bytes());
        z.extend_from_slice(&(!len).to_le_bytes());
        z.extend_from_slice(b);
    }
    z.extend_from_slice(&adler32(&raw).to_be_bytes());
    let mut out = vec![0x89, b'P', b'N', b'G', 0x0D, 0x0A, 0x1A, 0x0A];
    let mut ihdr = Vec::new();
    ihdr.extend_from_slice(&width.to_be_bytes());
    ihdr.extend_from_slice(&height.to_be_bytes());
    ihdr.extend_from_slice(&[8, 0, 0, 0, 0]);
    chunk(&mut out, &t, b"IHDR", &ihdr);
    chunk(&mut out, &t, b"IDAT", &z);
    chunk(&mut out, &t, b"IEND", &[]);
    out
}

#[cfg(test)]
mod tests {
    #[test]
    fn checksums_match_their_reference_values() {
        let t = super::crc_table();
        assert_eq!(super::crc32(&t, &[b"IEND"]), 0xAE42_6082);
        assert_eq!(super::adler32(b"Wikipedia"), 0x11E6_0398);
        let png = super::grey(2, 2, &[0, 64, 128, 255]);
        assert_eq!(&png[1..4], b"PNG");
        assert_eq!(&png[png.len() - 8..png.len() - 4], b"IEND");
    }
}
