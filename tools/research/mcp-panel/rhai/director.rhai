// Director (RESEARCH-ONLY): moves the owner's GUI client like a person would, over the client's own
// MCP server (openttd.view.*), driven by the panel's event feed: the camera glides to where the
// bot works as a hand moves a mouse, zooms in, points at the tile the bot surveys or builds with the
// cursor (the marked tile), and follows each bus it starts.
//
// The glide is AlgoGlyph's human-pointing kernel (kernel(), through $MCP_ALGOGLYPH_DIR), the model
// the bot's own cursor moves by in mcp-human: Fitts' law duration, a minimum-jerk curved path, an
// overshoot corrected on long moves. Without AlgoGlyph it is the built-in minimum-jerk glide below.
//
//   MCP_VIEW_TOKEN=<token file of the GUI client> mcp-rhai tools/research/mcp-panel/rhai/director.rhai
//
// tools/research/mcp-e2e starts it with the client it launches. Every target is derived: a tile
// from the bot's log line ("... at tile N"), a town from the client's openttd://towns.
//
// Each batch of events is coalesced first (director_lib.rhai): a run of one kind of line, such as
// the 119 "built rail at tile N" of one rail line, is acted on once, at its last tile. Acting on each
// line put the camera minutes behind the bot, replaying construction long finished as if it were
// being built again, with a zoom pulse per tile. A piece of a linear structure is followed with a pan
// and the cursor; only a single structure (a station, a depot, a stop) gets the zoom pulse.
import "ttd" as ttd;
import "director_lib" as d;

let view = env("MCP_VIEW_TOKEN");
if view == "" { throw "set MCP_VIEW_TOKEN to the GUI client's token file"; }

fn map_width(view) { mcp_read(view, "openttd://game/state").data.map_width }

fn town_named(view, name) {
    for t in mcp_read(view, "openttd://towns").data.towns { if t.name == name { return t; } }
    ()
}

// Minimum-jerk profile: the position a hand reaches at fraction t of a movement.
fn min_jerk(t) { t * t * t * (10.0 - 15.0 * t + 6.0 * t * t) }

// A pseudo-random wobble in [-1, 1], seeded by the step, so a glide is not a ruler line.
fn wobble(i, seed) { let v = ((i * 7919 + seed * 104729) % 2001) - 1000; v.to_float() / 1000.0 }

// The glide along AlgoGlyph's human-pointing kernel. The kernel reads screen pixels, so tiles are
// scaled by 32 (a tile's width at normal zoom) and back, which also keeps its pixel tremor below a
// tile; a target of 4 tiles. At most 28 camera steps, each waiting as long as the samples it stands for.
fn kernel_glide(view, fx, fy, tx, ty) {
    let seed = (tx * 31 + ty) % 997;
    let s = kernel("human-pointing", #{ from: [fx * 32, fy * 32], to: [tx * 32, ty * 32], width: 128.0, seed: seed, drag: false }).samples;
    let n = s.len();
    let stride = ((n + 27) / 28).max(1);
    let wait = 0;
    for i in 0..n {
        wait += s[i][2];
        if (i + 1) % stride == 0 || i == n - 1 {
            let x = (s[i][0].to_float() / 32.0).round().to_int().max(1);
            let y = (s[i][1].to_float() / 32.0).round().to_int().max(1);
            mcp_call(view, "openttd.view.scroll_to", #{ x: x, y: y });
            sleep(wait);
            wait = 0;
        }
    }
    mcp_call(view, "openttd.view.scroll_to", #{ x: tx, y: ty });
    [tx, ty]
}

// Glide the camera from (fx, fy) to (tx, ty) like a hand on a mouse; returns the destination.
fn glide(view, fx, fy, tx, ty) {
    try { return kernel_glide(view, fx, fy, tx, ty); } catch { }
    let dx = (tx - fx).to_float(); let dy = (ty - fy).to_float();
    let dist = (dx * dx + dy * dy).sqrt();
    let steps = (dist / 5.0).to_int().max(4).min(28);
    let seed = (tx * 31 + ty) % 997;
    for i in 1..=steps {
        let t = i.to_float() / steps.to_float();
        let e = min_jerk(t);
        // The deviation peaks mid-way and vanishes at both ends, as a hand corrects onto a target.
        let dev = (t * (1.0 - t) * 4.0) * (dist / 18.0).min(6.0);
        let x = (fx.to_float() + dx * e + dev * wobble(i, seed)).round().to_int().max(1);
        let y = (fy.to_float() + dy * e + dev * wobble(i + 17, seed)).round().to_int().max(1);
        mcp_call(view, "openttd.view.scroll_to", #{ x: x, y: y });
        // Faster in the middle, slower at the ends.
        sleep((140.0 - 90.0 * (t * (1.0 - t) * 4.0)).to_int());
    }
    mcp_call(view, "openttd.view.scroll_to", #{ x: tx, y: ty });
    [tx, ty]
}

let w = map_width(view);
try {
    kernel("human-pointing", #{ from: [0, 0], to: [32, 0], width: 128.0, seed: 1, drag: false });
    print("camera moves by AlgoGlyph's human-pointing kernel");
} catch (e) {
    print(`camera moves by the built-in glide (${e})`);
}
let towns = mcp_read(view, "openttd://towns").data.towns;
let largest = towns.reduce(|a, t| if a == () || t.population > a.population { t } else { a }, ());
let at = [w / 2, w / 2];
mcp_call(view, "openttd.view.zoom", #{ level: 2 });
if largest != () {
    at = glide(view, at[0], at[1], largest.x, largest.y);
    print(`camera -> ${largest.name}: the largest town, where the bot starts`);
}

let since = status().event_seq;
let last_kind = "";
loop {
    let feed = events(since, "ai.log,game.new_world");
    since = feed.last_seq;
    for e in d::coalesce(feed.events) {
      // A client that went away (closed, restarting) is waited for, not fatal.
      try {
        if e.kind == "game.new_world" { w = map_width(view); continue; }
        if d::is_linear(e, last_kind) {
            // A road or a rail line grows tile by tile: follow its newest tile with the cursor and a
            // short pan, no zooming, and never replay the tiles before it.
            let tile = d::tile_of(e.data.text);
            mcp_call(view, "openttd.view.mark", #{ tile: tile });
            mcp_call(view, "openttd.view.scroll_to", #{ tile: tile });
            at = [tile % w, tile / w];
            last_kind = d::kind(e.data.text);
            continue;
        }
        let t = e.data.text;
        last_kind = d::kind(t);
        if t.contains(" at tile ") {
            let tile = parse_int(t.sub_string(t.index_of(" at tile ") + 9).split(" ")[0]);
            let x = tile % w; let y = tile / w;
            if t.contains("started bus ") {
                let v = parse_int(t.sub_string(t.index_of("started bus ") + 12).split(" ")[0]);
                mcp_call(view, "openttd.view.follow", #{ vehicle: v });
                print(`camera follows bus ${v}`);
                sleep(2500);
                mcp_call(view, "openttd.view.follow", #{ vehicle: () });
                continue;
            }
            mcp_call(view, "openttd.view.zoom", #{ level: 1 });
            at = glide(view, at[0], at[1], x, y);
            mcp_call(view, "openttd.view.mark", #{ tile: tile });
            print(`cursor -> (${x},${y}): ${t}`);
            if t.contains("built ") { mcp_call(view, "openttd.view.zoom", #{ level: 0 }); sleep(600); mcp_call(view, "openttd.view.zoom", #{ level: 1 }); }
            continue;
        }
        // A town without a tile: glide there and look around.
        let name = ();
        if t.contains("built a bus route in ") {
            let a = t.index_of("built a bus route in ") + 21; let b = t.index_of(" with ", a);
            if b > a { name = t.sub_string(a, b - a); }
        } else if t.contains("could not find") && t.contains(" near ") {
            name = t.sub_string(t.index_of(" near ") + 6);
        }
        if name != () {
            let town = town_named(view, name);
            if town != () {
                mcp_call(view, "openttd.view.zoom", #{ level: 2 });
                at = glide(view, at[0], at[1], town.x, town.y);
                mcp_call(view, "openttd.view.mark", #{ clear: true });
                print(`camera -> ${town.name}: ${t}`);
            }
        }
      } catch (err) {
        print(`client view not reachable, waiting: ${err}`);
        sleep(3000);
      }
    }
    sleep(400);
}
