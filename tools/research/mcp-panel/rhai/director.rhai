// Director (RESEARCH-ONLY): moves the owner's GUI client like a person would, over the client's own
// MCP server (openttd.view.*), driven by the panel's event feed: the camera glides to where the
// bot works on a minimum-jerk path with small human deviations, zooms in, points at the tile the
// bot surveys or builds with the cursor (the marked tile), and follows each bus it starts.
//
//   MCP_VIEW_TOKEN=<token file of the GUI client> mcp-rhai tools/research/mcp-panel/rhai/director.rhai
//
// tools/research/mcp-e2e starts it with the client it launches. Every target is derived: a tile
// from the bot's log line ("... at tile N"), a town from the client's openttd://towns.
import "ttd" as ttd;

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

// Glide the camera from (fx, fy) to (tx, ty) like a hand on a mouse; returns the destination.
fn glide(view, fx, fy, tx, ty) {
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
let towns = mcp_read(view, "openttd://towns").data.towns;
let largest = towns.reduce(|a, t| if a == () || t.population > a.population { t } else { a }, ());
let at = [w / 2, w / 2];
mcp_call(view, "openttd.view.zoom", #{ level: 2 });
if largest != () {
    at = glide(view, at[0], at[1], largest.x, largest.y);
    print(`camera -> ${largest.name}: the largest town, where the bot starts`);
}

let since = status().event_seq;
loop {
    let feed = events(since, "ai.log,game.new_world");
    since = feed.last_seq;
    for e in feed.events {
      // A client that went away (closed, restarting) is waited for, not fatal.
      try {
        if e.kind == "game.new_world" { w = map_width(view); continue; }
        if e.data.text.contains("built road at tile ") {
            // A road grows tile by tile: follow it with the cursor and a short pan, no zooming.
            let tile = parse_int(e.data.text.sub_string(e.data.text.index_of(" at tile ") + 9).split(" ")[0]);
            mcp_call(view, "openttd.view.mark", #{ tile: tile });
            mcp_call(view, "openttd.view.scroll_to", #{ tile: tile });
            at = [tile % w, tile / w];
            continue;
        }
        let t = e.data.text;
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
