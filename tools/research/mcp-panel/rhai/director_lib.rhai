// The director's event logic (RESEARCH-ONLY), apart from the camera so it can be tested offline
// (cargo test in tools/research/mcp-rhai). Nothing here names a structure: the kind of a bot log line
// is derived from its own text.

// The kind of a log line: its text before " at tile ", without digits. Every "built rail at tile N"
// is one kind; "built rail station in X at tile N" is another.
fn kind(text) {
    let t = text;
    let i = t.index_of(" at tile ");
    if i >= 0 { t = t.sub_string(0, i); }
    let out = "";
    for c in t.chars() { if c < '0' || c > '9' { out += c; } }
    out
}

// The tile a line names after " at tile ", or -1.
fn tile_of(text) {
    let i = text.index_of(" at tile ");
    if i < 0 { return -1; }
    let rest = text.sub_string(i + 9).split(" ")[0];
    parse_int(rest)
}

// The text of an event, or "" when it carries none (game.new_world, panel events).
fn text_of(e) {
    if e.data == () { return ""; }
    if e.data.text == () { return ""; }
    e.data.text
}

// Coalesce one batch of events: a run of consecutive lines of one kind becomes its last line, with
// `repeats` set to the length of the run. A camera that acts on each line of a 119-tile rail line
// falls minutes behind and replays construction that is long finished; acting on the last keeps it
// where the bot is now.
fn coalesce(events) {
    let out = [];
    let n = events.len();
    let run = 0;
    for i in 0..n {
        let e = events[i];
        run += 1;
        let same_next = false;
        if i + 1 < n {
            let next = events[i + 1];
            let text = text_of(e);
            let next_text = text_of(next);
            if next.kind == e.kind && text != "" && next_text != "" {
                same_next = kind(next_text) == kind(text);
            }
        }
        if same_next { continue; }
        e.repeats = run;
        out.push(e);
        run = 0;
    }
    out
}

// Is this line one piece of a linear structure being laid tile by tile (road, rail, anything)? It is
// when its run repeats in this batch, or when it continues the kind the camera last acted on.
fn is_linear(e, last_kind) {
    let text = text_of(e);
    text.contains("built ") && text.contains(" at tile ") && (e.repeats > 1 || kind(text) == last_kind)
}
