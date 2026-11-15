// Director (RESEARCH-ONLY): moves the camera of the owner's GUI client to what the bot does, over
// the client's own MCP server (openttd.view.*), driven by the panel's event feed.
//
//   MCP_VIEW_TOKEN=<token file of the GUI client> mcp-rhai tools/research/mcp-panel/rhai/director.rhai
//
// tools/research/mcp-e2e starts it with the client it launches. Every target is derived: the town
// of a route comes from the bot's log line, its tile from the client's openttd://towns.
import "ttd" as ttd;

let view = env("MCP_VIEW_TOKEN");
if view == "" { throw "set MCP_VIEW_TOKEN to the GUI client's token file"; }

fn town_named(view, name) {
    for t in mcp_read(view, "openttd://towns").data.towns {
        if t.name == name { return t; }
    }
    ()
}

fn look_at(view, town, why) {
    let r = mcp_call(view, "openttd.view.scroll_to", #{ town: town.id });
    print(`camera -> ${town.name} (${r.x},${r.y}): ${why}`);
}

// Start where the action is: the largest town, a working zoom level.
let towns = mcp_read(view, "openttd://towns").data.towns;
let largest = towns.reduce(|a, t| if a == () || t.population > a.population { t } else { a }, ());
mcp_call(view, "openttd.view.zoom", #{ level: 1 });
if largest != () { look_at(view, largest, "the largest town, where the bot starts"); }

let since = status().event_seq;
loop {
    let feed = events(since, "ai.log,game.new_world");
    for e in feed.events {
        if e.kind == "game.new_world" {
            let towns = mcp_read(view, "openttd://towns").data.towns;
            let largest = towns.reduce(|a, t| if a == () || t.population > a.population { t } else { a }, ());
            if largest != () { look_at(view, largest, "a new world"); }
            continue;
        }
        let t = e.data.text;
        // "built a bus route in <Town> with ..." and "route n/m in <Town>"
        let name = ();
        if t.contains("built a bus route in ") {
            let a = t.index_of("built a bus route in ") + 21;
            let b = t.index_of(" with ", a);
            if b > a { name = t.sub_string(a, b - a); }
        } else if t.contains(" in ") && t.contains(": route ") {
            name = t.sub_string(t.index_of(" in ") + 4);
        } else if t.contains("could not find") && t.contains(" near ") {
            name = t.sub_string(t.index_of(" near ") + 6);
        }
        if name != () {
            let town = town_named(view, name);
            if town != () { look_at(view, town, t); }
        }
    }
    since = feed.last_seq;
    sleep(1000);
}
