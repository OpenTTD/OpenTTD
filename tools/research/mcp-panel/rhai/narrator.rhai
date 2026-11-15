// In-game narrator (RESEARCH-ONLY): follows the panel's event feed and reports what the owner's bot
// and the game do as chat messages from the server, so the owner sees them in the running game.
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/narrator.rhai
//
// What is said is derived from the events (ai.log, company.*, decision.*, game.*), not scripted:
// a route the bot opens, a town it fails in, its knobs, companies founded or renamed, every MCP
// decision, a pause or resume, and a summary at every new month.
import "ttd" as ttd;

fn say(text) {
    // The console takes one line; quotes inside the message would end it early.
    con_say(`"[owner-bot] ${text.replace("\"", "'")}"`);
}

fn narrate(e) {
    let k = e.kind;
    let d = e.data;
    if k == "ai.log" {
        let t = d.text;
        if t.contains("built a bus route") || t.contains(": route ") || t.contains("behaviour:")
            || t.contains("no new route") || d.level == "error" || d.level == "squirrel_error" {
            return t.replace("PrismaticAI: ", "").replace("PrismaticAI ", "");
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
say("narrator online: I report what your bot does. Steer it at http://127.0.0.1:8800/");
let since = status().event_seq;
loop {
    let feed = events(since, "ai.log,company,decision,game");
    for e in feed.events {
        let text = narrate(e);
        if text != () { say(text); print(`${e.kind}: ${text}`); }
    }
    since = feed.last_seq;
    sleep(2000);
}
