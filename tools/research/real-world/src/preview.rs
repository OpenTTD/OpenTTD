//! `preview.svg`: a static, offline review image of one compiled scenario (the heightmap as
//! the ground, every source feature as a dot, every game entity and economy edge on top,
//! coloured by evidence class, with scale and attribution). It is static on purpose: it is a
//! file artifact that travels with the scenario and must open without a server. The
//! interactive view (filters in the address, a page per scenario) is the panel's `/realworld`.

use crate::model::*;

pub fn b64(data: &[u8]) -> String {
    const T: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut o = String::with_capacity(data.len() * 4 / 3 + 4);
    for c in data.chunks(3) {
        let n = (c[0] as u32) << 16 | (*c.get(1).unwrap_or(&0) as u32) << 8 | *c.get(2).unwrap_or(&0) as u32;
        o.push(T[(n >> 18) as usize & 63] as char);
        o.push(T[(n >> 12) as usize & 63] as char);
        o.push(if c.len() > 1 { T[(n >> 6) as usize & 63] as char } else { '=' });
        o.push(if c.len() > 2 { T[n as usize & 63] as char } else { '=' });
    }
    o
}

fn esc(s: &str) -> String {
    s.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;")
}

pub fn colour(e: Evidence) -> &'static str {
    match e {
        Evidence::Observed => "#6b7280",
        Evidence::Derived => "#1c64f2",
        Evidence::Inferred => "#c27803",
        Evidence::Hypothetical => "#e02424",
    }
}

pub fn svg(sc: &Scenario, snap: &Snapshot, heightmap_png: &[u8]) -> String {
    const W: f64 = 640.0;
    let n = sc.extent.map_size as f64;
    let k = W / n;
    // Screen position of a tile: column to the right, row down (north up), as the image.
    let pos = |t: &Tile| ((t.y as f64 + 0.5) * k, (t.x as f64 + 0.5) * k);
    let mut s = format!(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 {W} {h}\" width=\"{W}\" height=\"{h}\" font-family=\"Inter, system-ui, sans-serif\" font-size=\"11\">\n\
         <title>{title}</title>\n<rect width=\"100%\" height=\"100%\" fill=\"#f9fafb\"/>\n\
         <image href=\"data:image/png;base64,{img}\" x=\"0\" y=\"0\" width=\"{W}\" height=\"{W}\" opacity=\"0.85\" style=\"image-rendering:pixelated\"/>\n",
        h = W + 120.0,
        title = esc(&format!("{} -> {} ({} m/tile)", sc.query, sc.chosen, sc.extent.metres_per_tile)),
        img = b64(heightmap_png),
    );
    for f in &snap.features {
        if let Some(t) = crate::geo::tile_of(&sc.extent, &f.at, 0) {
            let (x, y) = pos(&t);
            s.push_str(&format!(
                "<circle cx=\"{x:.1}\" cy=\"{y:.1}\" r=\"1.6\" fill=\"{}\"><title>{} {} OBSERVED</title></circle>\n",
                colour(Evidence::Observed),
                esc(&f.id),
                esc(&format!("{} {}", f.class, f.name.clone().unwrap_or_default()))
            ));
        }
    }
    for e in &sc.economy.edges {
        let (Some(a), Some(b)) = (sc.entities.iter().find(|x| x.id == e.from), sc.entities.iter().find(|x| x.id == e.to)) else { continue };
        let ((x1, y1), (x2, y2)) = (pos(&a.tile), pos(&b.tile));
        let dash = if e.evidence == Evidence::Hypothetical { " stroke-dasharray=\"4 3\"" } else { "" };
        s.push_str(&format!(
            "<line x1=\"{x1:.1}\" y1=\"{y1:.1}\" x2=\"{x2:.1}\" y2=\"{y2:.1}\" stroke=\"{}\" stroke-width=\"1.4\" opacity=\"0.8\"{dash}><title>{} {} {:?}</title></line>\n",
            colour(e.evidence),
            esc(&e.cargo),
            esc(&e.id),
            e.evidence
        ));
    }
    for e in &sc.entities {
        let (x, y) = pos(&e.tile);
        let c = colour(e.evidence);
        let tip = esc(&format!("{} - {} {} - {:?} ({}) - {}", e.name, e.game.kind, e.game.role, e.evidence, e.confidence, e.rationale));
        match e.game.kind.as_str() {
            "town" => s.push_str(&format!(
                "<circle cx=\"{x:.1}\" cy=\"{y:.1}\" r=\"{}\" fill=\"{c}\" stroke=\"#fff\" stroke-width=\"1\"><title>{tip}</title></circle>\n\
                 <text x=\"{:.1}\" y=\"{:.1}\" fill=\"#111827\" stroke=\"#fff\" stroke-width=\"2.5\" paint-order=\"stroke\">{}</text>\n",
                if e.city { 6 } else { 4 },
                x + 6.0,
                y - 5.0,
                esc(&e.name)
            )),
            "industry" => s.push_str(&format!(
                "<rect x=\"{:.1}\" y=\"{:.1}\" width=\"8\" height=\"8\" fill=\"{c}\" stroke=\"#fff\"{}><title>{tip}</title></rect>\n",
                x - 4.0,
                y - 4.0,
                if e.evidence == Evidence::Hypothetical { " stroke-dasharray=\"2 1\"" } else { "" }
            )),
            _ => s.push_str(&format!(
                "<path d=\"M{x:.1},{:.1} l5,9 h-10 z\" fill=\"{c}\" stroke=\"#fff\"><title>{tip}</title></path>\n\
                 <text x=\"{:.1}\" y=\"{:.1}\" fill=\"#374151\" font-style=\"italic\" stroke=\"#fff\" stroke-width=\"2.5\" paint-order=\"stroke\">{}</text>\n",
                y - 6.0,
                x + 6.0,
                y + 3.0,
                esc(&e.name)
            )),
        }
    }
    // Scale bar: 5 km.
    let km = 5000.0 / sc.extent.metres_per_tile * k;
    let y0 = W + 18.0;
    s.push_str(&format!(
        "<line x1=\"12\" y1=\"{y0}\" x2=\"{:.1}\" y2=\"{y0}\" stroke=\"#111827\" stroke-width=\"3\"/><text x=\"{:.1}\" y=\"{:.1}\">5 km ({} m/tile, {}x{} tiles, north up)</text>\n",
        12.0 + km,
        18.0 + km,
        y0 + 4.0,
        sc.extent.metres_per_tile,
        sc.extent.map_size,
        sc.extent.map_size
    ));
    let mut x = 12.0;
    for ev in [Evidence::Observed, Evidence::Derived, Evidence::Inferred, Evidence::Hypothetical] {
        s.push_str(&format!(
            "<rect x=\"{x}\" y=\"{:.1}\" width=\"10\" height=\"10\" fill=\"{}\"/><text x=\"{:.1}\" y=\"{:.1}\">{:?}</text>\n",
            y0 + 14.0,
            colour(ev),
            x + 14.0,
            y0 + 23.0,
            ev
        ));
        x += 130.0;
    }
    s.push_str(&format!(
        "<text x=\"12\" y=\"{:.1}\">circle town, square industry, triangle landmark sign, line economy edge (dashed: HYPOTHETICAL)</text>\n",
        y0 + 42.0
    ));
    for (i, a) in sc.attribution.iter().enumerate() {
        let t: String = a.chars().take(130).collect();
        s.push_str(&format!("<text x=\"12\" y=\"{:.1}\" font-size=\"9\" fill=\"#4b5563\">{}</text>\n", y0 + 58.0 + 11.0 * i as f64, esc(&t)));
    }
    s.push_str("</svg>\n");
    s
}
