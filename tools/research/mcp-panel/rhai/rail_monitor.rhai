// Rail monitor (RESEARCH-ONLY): watches the owner's bot's railway in the background and reports what
// is wrong to the Majordomus board, where every agent session reads it (majordomus_blackboard,
// majordomus_peers), as the named claim openttd:<instance>:rail.
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/rail_monitor.rhai
//   or from the panel's Scripts page (/scripts) as a service.
//
// What it reports (rail_lib.rhai problems), worst first:
//   crashed      a train crashed (a level crossing, a collision)
//   lost         the pathfinder found no way to the train's destination (openttd://vehicles lost):
//                a broken line or a turn the train may not take; such a train earns nothing
//   unserved     a line the bot built has no train
//   warning      the bot's own rail warnings since the last round: no rail path, a broken line, a
//                refused throat or piece, no depot (ai.log)
//   broken_down  a train stopped by a breakdown now (openttd://vehicles broken_down)
//   stopped      a train stopped outside a depot
//   old          a train past its maximum age (autorenew should have replaced it)
// It posts when the report changes, and at least every MONITOR_REPEAT_S (default 600) while
// problems last; MONITOR_EVERY_S (default 60) is the round. MCP_BOARD_DRY=1 prints instead.
import "rail_lib" as rl;

fn every_s() { let v = env("MONITOR_EVERY_S"); if v == "" { 60 } else { parse_int(v) } }
fn repeat_s() { let v = env("MONITOR_REPEAT_S"); if v == "" { 600 } else { parse_int(v) } }

fn instance() {
    let store = status().store ?? "";
    if store == "" { return "panel"; }
    let parts = store.split("/");
    let base = parts[parts.len() - 1];
    base.sub_string(0, base.index_of(".").max(0))
}

let me = instance();
let since = 0;
let last = "";
let quiet = 0;
print(`rail monitor on ${me}: a round every ${every_s()} s, reposted every ${repeat_s()} s while problems last`);
loop {
    try {
        let state = read("openttd://game/state");
        let map_w = state.data.map_width;
        let vehicles = read("openttd://vehicles").data.vehicles;
        let stations = read("openttd://stations").data.stations;
        let all = events(0, "ai.log");
        let records = rl::records_from_events(all.events);
        // Warnings only since the last round, so one failure is reported once.
        let fresh = [];
        for e in all.events { if e.seq > since { fresh.push(e); } }
        since = all.last_seq;
        let found = [];
        for bot in api_bots().running ?? [] {
            if bot.ai != "PrismaticAI" { continue; }
            let lines = rl::lines_of(bot.id, records, vehicles, stations, map_w);
            for p in rl::problems(bot.id, lines, vehicles, rl::rail_warnings(fresh)) {
                p.text = `${bot.name}: ${p.text}`;
                found.push(p);
            }
        }
        let text = rl::digest(me, state.game_date, found, 4);
        // The report without its date, so the same problems a day later are not news.
        let key = text.sub_string(text.index_of(" rail: "));
        quiet += every_s();
        if key != last || (!found.is_empty() && quiet >= repeat_s()) {
            print(text);
            board_post(`openttd:${me}:rail`, text);
            last = key;
            quiet = 0;
        }
    } catch (err) {
        print(`rail monitor: ${err}`);
    }
    sleep(every_s() * 1000);
}
