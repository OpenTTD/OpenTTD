// A whole game played for the owner (RESEARCH-ONLY): from a new world to the game's end, through
// MCP and the server console, with the owner's bot as the owner's company.
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/full_game.rhai
//
// A dedicated server cannot fast-forward, so the game is a short one: it starts in START_YEAR and
// ends at the end of END_YEAR (OpenTTD's own game_creation.ending_year), about 13 real minutes a
// year. The owner influences the bot from the panel while it plays: its knobs (My bot), and the
// mid-game pause, which waits for the owner's Approve and is skipped, and said so, without it.
import "ttd" as ttd;

fn year(date) { parse_int(date.sub_string(0, 4)) }

fn run(cfg) {
    let START_YEAR = cfg.start_year; let END_YEAR = cfg.end_year; let BOT = cfg.bot; let knobs = cfg.knobs;
    print(`== a ${END_YEAR - START_YEAR + 1}-year game, ${START_YEAR} to the end of ${END_YEAR}`);
    con_setting_newgame(`game_creation.starting_year ${START_YEAR}`);
    con_setting_newgame(`game_creation.ending_year ${END_YEAR}`);
    let w = ttd::new_game((ttd::state().tick % 100000) + 11);
    print(`   new world: ${w.date}, ${w.towns} towns, map ${w.map}`);

    print(`== the owner's bot: ${BOT} ${knobs}`);
    let since = status().event_seq;
    print(ttd::start_bot(BOT, knobs));
    let founded = ttd::wait_event_since(since, "company.founded", 90);
    print(`   ${founded.kind}: company #${founded.data.id}`);

    let last_quarter = "";
    let paused_once = false;
    loop {
        let s = ttd::state();
        let quarter = s.date.sub_string(0, 7);
        let month = parse_int(s.date.sub_string(5, 2));
        if (month == 1 || month == 4 || month == 7 || month == 10) && quarter != last_quarter {
            last_quarter = quarter;
            print(`-- ${s.date}`);
            print(ttd::report());
            for e in ttd::bot_log(2) { print(`   bot: ${e.data.text}`); }
        }
        if !paused_once && month >= 7 && year(s.date) == END_YEAR {
            paused_once = true;
            print("== mid-game: the owner pauses through MCP (Approve in the panel)");
            try {
                print(ttd::set_pause(true, 60));
                let a = ttd::state(); sleep(3000); let b = ttd::state();
                print(`   frozen while paused: ${a.tick == b.tick} (tick ${a.tick} -> ${b.tick})`);
                print(ttd::set_pause(false, 120));
            } catch (err) {
                print(`   not approved, the game went on: ${err}`);
            }
        }
        if year(s.date) > END_YEAR { break; }
        sleep(5000);
    }

    print("== the game is over");
    print(ttd::report());
    let c = ttd::companies();
    let best = c.reduce(|acc, x| if acc == () || x.value > acc.value { x } else { acc }, ());
    print(`   the owner's company: ${best.name}, value ${best.value}, money ${best.money}, road vehicles ${best.road}`);
    print(`   decisions recorded: ${read("openttd://decisions").data.decisions.len()}`);
}

run(#{ start_year: 2049, end_year: 2049, bot: "PrismaticAI",
      knobs: #{ aggressiveness: 75, max_routes: 8, cash_reserve: 20000, heartbeat_seconds: 30 } });
