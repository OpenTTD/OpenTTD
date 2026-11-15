// High-level commands for driving OpenTTD from outside, over the embedded MCP server and the
// server console (RESEARCH-ONLY; ADR 0008). Import with:  import "ttd" as ttd;
//
// Reads go through MCP resources. Game-state changes go through the approval-gated MCP tools,
// or, where no MCP tool exists yet, through the server console as the operator would type them.
// A change that needs approval is never approved by a script: it waits for the operator (the
// panel's Approve button, or auto-approve when the owner switches it on).

/// The live game state: date, tick, map, counts, paused.
fn state() {
    let s = read("openttd://game/state");
    #{ date: s.game_date, tick: s.game_tick, paused: s.data.paused,
       vehicles: s.data.num_vehicles, companies: s.data.num_companies, towns: s.data.num_towns,
       map: `${s.data.map_width}x${s.data.map_height}` }
}

/// Every company, from MCP, completed with what the console reports (name, value, vehicles by
/// type: T trains, R road vehicles, P aircraft, S ships).
fn companies() {
    let out = [];
    let lines = con_companies();
    for c in read("openttd://companies").data.companies {
        let row = #{ id: c.id, ai: c.is_ai, money: c.money, founded: c.inaugurated_year,
                     name: c.name, value: (), trains: (), road: (), air: (), ships: () };
        for line in lines {
            if line.starts_with(`#:${c.id + 1}(`) {
                let a = line.index_of("Company Name: '") + 15;
                let b = line.index_of("'", a);
                row.name = line.sub_string(a, b - a);
                let v = line.index_of("Value: ") + 7;
                row.value = parse_int(line.sub_string(v, line.index_of(" ", v) - v));
                let t = line.index_of("(T:");
                let parts = line.sub_string(t + 1, line.index_of(")", t) - t - 1).split(", ");
                row.trains = parse_int(parts[0].sub_string(2));
                row.road = parse_int(parts[1].sub_string(2));
                row.air = parse_int(parts[2].sub_string(2));
                row.ships = parse_int(parts[3].sub_string(2));
            }
        }
        out.push(row);
    }
    out
}

/// Wait until the game answers MCP reads with a running simulation, up to `seconds`.
fn wait_running(seconds) {
    for i in 0..seconds {
        try {
            let s = state();
            if !s.paused && s.tick > 0 { return s; }
        } catch { }
        sleep(1000);
    }
    throw `the game did not come up within ${seconds} s`;
}

/// Start a new world (the server regenerates the map; joined clients rejoin).
fn new_game(seed) {
    con_newgame(`${seed}`);
    // The server regenerates the map; reads fail or show the old world until it is back.
    sleep(4000);
    wait_running(120)
}

/// Start a company AI with a settings string, e.g. ttd::start_ai("PrismaticAI", "aggressiveness=75").
fn start_ai(name, settings) {
    if settings == "" { con_start_ai(name) } else { con_start_ai(`${name} ${settings}`) }
}

/// Pause or resume through MCP: submit, wait for the operator's approval, resubmit, and check the
/// effect. Returns the executed answer, or throws when no approval arrives within `seconds`.
fn set_pause(on, seconds) {
    // Through the panel, so the request shows up under "Pending approvals" with an Approve
    // button; the panel resubmits it once approved.
    let first = pause(on);
    if first.status == "executed" { return first; }
    let id = first.approval_id;
    print(`  waiting for the operator to approve #${id} (panel Approve, or auto-approve)`);
    for i in 0..seconds {
        sleep(1000);
        if status().pending.filter(|p| p.id == id).is_empty() {
            for j in 0..10 {
                let s = state();
                if s.paused == on { return #{ status: "executed", approval_id: id, paused: s.paused }; }
                sleep(500);
            }
            throw `approval #${id} was consumed but the game is not ${if on { "paused" } else { "running" }}`;
        }
    }
    throw `approval #${id} did not arrive within ${seconds} s`;
}

/// One line about the game and each company.
fn report() {
    let s = state();
    let line = `${s.date}  tick ${s.tick}  vehicles ${s.vehicles}  companies ${s.companies}  ${if s.paused { "PAUSED" } else { "running" }}`;
    for c in companies() {
        line += "\n    " + `#${c.id} ${c.name}  money ${c.money}  value ${c.value}  road ${c.road} trains ${c.trains} air ${c.air} ships ${c.ships}`;
    }
    line
}

/// Print a report every `every` seconds, `times` times.
fn watch(times, every) {
    for i in 0..times {
        print(report());
        sleep(every * 1000);
    }
}

/// Wait up to `seconds` for the first event whose kind is or starts with `kind` (e.g.
/// "company.founded", "game"), published after this call. Returns the event, or throws.
fn wait_event(kind, seconds) {
    let since = status().event_seq;
    for i in 0..seconds {
        let feed = events(since, kind);
        if !feed.events.is_empty() { return feed.events[0]; }
        sleep(1000);
    }
    throw `no ${kind} event within ${seconds} s`;
}

/// The AIs that can play for the owner and their settings (from openttd.ai.list when the game has
/// it, else each AI's info.nut), and the AI companies running now.
fn bots() { api_bots() }

/// Start the owner's bot: an AI with a settings map, e.g.
/// ttd::start_bot("PrismaticAI", #{ aggressiveness: 75, max_routes: 6, cash_reserve: 20000 }).
fn start_bot(ai, settings) { api_botStart(#{ ai: ai, settings: settings }) }

/// Change one live (CONFIG_INGAME) setting of a running bot; approval-gated, waits for the operator
/// like every other change. Needs openttd.ai.set_setting in the game's tools/list.
fn set_bot(company, name, value) { api_botSetting(#{ company: company, name: name, value: value }) }

/// The bot's own log lines (ai.log events), newest last.
fn bot_log(limit) {
    let e = events(0, "ai.log").events;
    let n = e.len();
    if n > limit { e.extract(n - limit) } else { e }
}
