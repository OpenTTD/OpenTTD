// In-game narrator (RESEARCH-ONLY): follows the panel's event feed and reports what the owner's bot
// and the game do as chat messages from the server, so the owner sees them in the running game.
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/narrator.rhai
//
// Routes it opens also appear as a newspaper in the owner's window, with a viewport on the town.
// What is said is derived from the events (ai.log, company.*, decision.*, game.*), not scripted:
// a route the bot opens, a town it fails in, its knobs, companies founded or renamed, every MCP
// decision, a pause or resume, and a summary at every new month.
import "ttd" as ttd;

// Rhai's String.replace changes the string in place and returns (), so it never chains: each
// helper replaces on its own copy (arguments are passed by value) and returns that copy.
fn without_bot_name(t) {
    t.replace("PrismaticAI: ", "");
    t.replace("PrismaticAI ", "");
    t
}

fn say(text) {
    // The console takes one line; quotes inside the message would end it early.
    text.replace("\"", "'");
    con_say(`"[owner-bot] ${text}"`);
}

// A route the bot opened, whatever its mode: "built a bus route in <town> ...", "built a rail
// route <cargo> <a> - <b> ...", "built an air route ...".
fn opened_route(t) { (t.contains("built a ") || t.contains("built an ")) && t.contains(" route") }

// A newspaper in the owner's window for what matters most: a route the bot opened, with a
// viewport on its town when it has one (openttd.view.news.show through POST /api/news). Chat
// carries the rest.
fn headline(e) {
    if e.kind == "ai.log" && opened_route(e.data.text) {
        let t = e.data.text;
        let text = "Your bot opened a route: " + without_bot_name(t);
        if t.contains("built a bus route in ") {
            let rest = t.sub_string(t.index_of("built a bus route in ") + 21);
            let town_name = rest.split(" with ")[0];
            for town in read("openttd://towns").data.towns {
                if town.name == town_name { return #{text: text, town: town.id}; }
            }
        }
        return #{text: text};
    }
    if e.kind == "game.new_world" { return #{text: `A new world for your bot: ${e.data.towns} towns, ${e.data.date}`}; }
    ()
}

fn narrate(e) {
    let k = e.kind;
    let d = e.data;
    if k == "ai.log" {
        let t = d.text;
        if opened_route(t) || t.contains(": route ") || t.contains("behaviour:")
            || t.contains("no new route") || d.level == "error" || d.level == "squirrel_error" {
            return without_bot_name(t);
        }
        return ();
    }
    if k == "company.founded" { return `a company was founded (#${d.id})`; }
    if k == "company.renamed" { return `company #${d.id} is now ${d.to}`; }
    if k == "company.removed" { return `company #${d.id} is gone`; }
    if k == "decision.recorded" { return `MCP decision #${d.decision_id}: ${d.action_type} ${d.command_result} (${d.verdict})`; }
    if k == "game.paused" { return `the game was paused through MCP at ${d.date}`; }
    if k == "game.resumed" { return `the game resumed at ${d.date}`; }
    if k == "game.new_world" { return `a new world: ${d.towns} towns, ${d.date}`; }
    if k == "game.month" {
        let parts = [];
        for c in ttd::companies() { parts.push(`${c.name}: ${c.road} buses, value ${c.value}, money ${c.money}`); }
        return `${d.date}: ${d.vehicles} vehicles. ${parts.reduce(|a, b| if a == "" { b } else { a + "; " + b }, "")}`;
    }
    ()
}

con_setting("network.client_name owner-bot");
// The panel's address comes from whoever started this script (tools/research/mcp-e2e sets it).
let panel = env("MCP_PANEL_URL");
say(if panel == "" { "narrator online: I report what your bot does. Steer it in the control panel." }
    else { `narrator online: I report what your bot does. Steer it at ${panel}/` });
let since = status().event_seq;
loop {
    let feed = events(since, "ai.log,company,decision,game");
    for e in feed.events {
        let text = narrate(e);
        if text != () { say(text); print(`${e.kind}: ${text}`); }
        let news = headline(e);
        if news != () {
            try { api_newsShow(news); print(`news: ${news.text}`); } catch (err) { print(`news not shown: ${err}`); }
        }
    }
    since = feed.last_seq;
    sleep(2000);
}
