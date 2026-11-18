// Rail steward (RESEARCH-ONLY): keeps every rail line of the owner's bot paying. Every round it reads
// the game, decides per line, and hands the decision to the bot as a target; the bot carries it out
// (tools/research/prismatic-ai/PrismaticAI/lines.nut ReconcileLines).
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/rail_steward.rhai
//   or from the panel's Scripts page (/scripts): Start as a service, edits reload it.
//
// Per line (rail_lib.rhai target, every threshold in rail_lib policy()):
// - close a line that lost money on a whole year and on this one (line<slot>_trains = 0): the bot
//   sells its trains and clears its track, stations and depot, so it stops costing maintenance;
// - lengthen its trains while a trainload or more waits and the platform has room (line<slot>_wagons);
// - add a train, up to what the platforms hold (trains_max), once the trains are full length;
// - drop a train that lost money while almost nothing waits.
// What costs money is chosen with AlgoGlyph's knapsack-dp kernel over the money above the bot's
// cash_reserve (rail_lib choose; greedy when no AlgoGlyph is found). Locomotive types a faster one
// on the same rails has overtaken go to the game's own autoreplace (ScriptGroup.SetAutoReplace),
// which swaps them at the next depot visit: "keep the consists up to date".
//
// Every change goes through openttd.ai.set_setting or the Script API's execute, so it waits for the
// owner's approval unless they switched auto-approve on (knowledge auto-approve-is-the-owners-switch).
// A round's decisions are printed and posted to the Majordomus board as openttd:<instance>:steward.
// STEWARD_EVERY_S (default 120) is the round; STEWARD_DRY=1 decides and prints without changing.
import "rail_lib" as rl;

fn every_s() { let v = env("STEWARD_EVERY_S"); if v == "" { 120 } else { parse_int(v) } }
fn dry() { env("STEWARD_DRY") != "" }

/// The panel instance's name: its event store's file name (panel-<port>), else "panel".
fn instance() {
    let s = status();
    let store = s.store ?? "";
    if store == "" { return "panel"; }
    let name = store.split("/");
    let base = name[name.len() - 1];
    base.sub_string(0, base.index_of(".").max(0))
}

/// The value of a setting the running bot declares, or () when it declares none.
fn setting_of(bot, name) {
    for s in bot.settings ?? [] { if s.name == name { return s.value; } }
    ()
}

/// STATION_INVALID (ScriptBaseStation): as `via`, "via any station".
const STATION_INVALID = 65535;

/// Cargo waiting for a line at its stops (the most at either), by the Script API in test mode (it
/// reads, never changes). Under cargodist it is the cargo routed to the line's other stop. Under
/// manual distribution every waiting unit's next hop is STATION_INVALID (script_station.hpp), so
/// a query via the other stop sees nothing: there it is the cargo waiting via any station.
fn waiting_for(line) {
    let cargo = line.record.cargo ?? ();
    if cargo == () || line.a == () || line.b == () { return 0; }
    let dist = openttd_api_ScriptCargo_GetDistributionType(#{ args: [cargo] });
    let manual = dist.ok == true && dist.result == 0;
    let w = 0;
    for pair in [[line.a, line.b], [line.b, line.a]] {
        let via = if manual { global::STATION_INVALID } else { pair[1] };
        let r = openttd_api_ScriptStation_GetCargoWaitingVia(#{ args: [pair[0], via, cargo] });
        if r.ok == true && type_of(r.result) == "i64" && r.result > w { w = r.result; }
    }
    w
}

/// Rounds before a target the owner did not approve is asked again (an hour at the default round).
const RETRY_ROUNDS = 30;

/// Hand one target to the bot: one gated setting change per value that differs. Called as a method
/// on the memory of what was asked (`done.hand_over(...)`), because Rhai passes a map argument by
/// value and only `this` is the caller's own map: per setting, the value asked, its approval id and
/// the round it was asked in. The same value is not asked again while its approval is pending, nor
/// within RETRY_ROUNDS after the owner left it unapproved; a different value is asked at once.
fn hand_over(bot, t, round, pending) {
    let out = [];
    for what in ["trains", "wagons"] {
        let want = if what == "trains" { t.trains } else { t.wagons };
        if want < 0 { continue; }
        let name = `line${t.slot}_${what}`;
        let now = setting_of(bot, name);
        if now == () { out.push(`${name}: the bot declares no such setting (more lines than slots)`); continue; }
        let key = `${bot.id}:${name}`;
        if !rl::should_ask(this[key], now, want, round, pending, global::RETRY_ROUNDS) { continue; }
        if dry() { out.push(`${name} ${now} -> ${want} (dry run)`); continue; }
        let r = api_botSetting(#{ company: bot.id, name: name, value: want });
        this[key] = #{ want: want, id: r.approval_id, round: round };
        out.push(`${name} ${now} -> ${want}: ${r.status ?? r}${if r.approval_id != () { ` #${r.approval_id}` } else { "" }}`);
    }
    out
}

let p = rl::policy();
let done = #{};        // per setting: the value asked, its approval id, the round (hand_over)
let round = 0;
let renewed = [];      // autoreplace pairs already handed over
let me = instance();
print(`rail steward on ${me}: a round every ${every_s()} s${if dry() { ", dry run" } else { "" }}; policy ${p}`);
loop {
    try {
        let state = read("openttd://game/state");
        let map_w = state.data.map_width;
        let vehicles = read("openttd://vehicles").data.vehicles;
        let stations = read("openttd://stations").data.stations;
        let engines = read("openttd://engines").data.engines;
        let records = rl::records_from_events(events(0, "ai.log").events);
        let report = [];
        round += 1;
        let pending = [];
        for p in status().pending ?? [] { pending.push(p.id); }
        for bot in api_bots().running ?? [] {
            if bot.ai != "PrismaticAI" { continue; }
            let lines = rl::lines_of(bot.id, records, vehicles, stations, map_w);
            let items = [];
            for line in lines {
                let t = rl::target(line, waiting_for(line), p);
                if t != () { items.push(#{ target: t, value: rl::value(line, t, engines) }); }
            }
            let reserve = setting_of(bot, "cash_reserve") ?? 0;
            let pick = rl::choose(items, bot.money - reserve, Fn("kernel"));
            for t in pick.chosen {
                print(`  ${bot.name} line ${t.slot}: ${t.action}: ${t.why}`);
                for line in done.hand_over(bot, t, round, pending) { print(`    ${line}`); }
                report.push(`${t.action} line ${t.slot}`);
            }
            if pick.chosen.len() < items.len() { print(`  ${bot.name}: ${items.len() - pick.chosen.len()} more change(s) wait for money (${pick.solver})`); }
            for r in rl::renewals(bot.id, vehicles, engines, p) {
                let key = `${bot.id}:${r.from}->${r.to}`;
                if renewed.contains(key) { continue; }
                print(`  ${bot.name} renew: ${r.why}`);
                if !dry() {
                    // GROUP_ALL (0xFFFD): every vehicle of the company, as ScriptGroup declares it.
                    let a = openttd_api_ScriptGroup_SetAutoReplace(#{ company: bot.id, args: [65533, r.from, r.to], execute: true });
                    print(`    autoreplace ${r.from_name} -> ${r.to_name}: ${a.status ?? a}`);
                }
                renewed.push(key);
                report.push(`renew ${r.from_name}->${r.to_name}`);
            }
            print(`${state.game_date} ${bot.name}: ${lines.len()} line(s), ${items.len()} change(s) proposed, ${pick.chosen.len()} chosen (${pick.solver})`);
        }
        if !report.is_empty() { board_post(`openttd:${me}:steward`, `OpenTTD ${me} ${state.game_date} rail steward: ${report}`); }
    } catch (err) {
        print(`rail steward: ${err}`);
    }
    sleep(every_s() * 1000);
}
