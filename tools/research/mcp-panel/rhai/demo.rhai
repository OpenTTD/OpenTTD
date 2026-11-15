// End-to-end demo (RESEARCH-ONLY): a game driven entirely from outside, over MCP and the server
// console, from a new world to a final report.
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/demo.rhai
//   or, in the panel's developer console:  /rhai import "demo" as d; d::run(30, 6)
//
// 1. a new world, 2. PrismaticAI founds a company and plays, 3. the run is watched through MCP
// resources, 4. the game is paused and resumed through the approval-gated MCP tool (the operator
// approves in the panel, or auto-approve does), 5. a final report.
import "ttd" as ttd;

fn run(watch_times, every_seconds) {
    print("== 1. new world (newgame through the server console)");
    let seed = (ttd::state().tick % 100000) + 7;
    print(ttd::new_game(seed));

    print("== 2. PrismaticAI founds a company and plays on its own");
    print(ttd::start_ai("PrismaticAI", "aggressiveness=75"));
    sleep(5000);

    print("== 3. watching the run through MCP resources");
    ttd::watch(watch_times, every_seconds);

    print("== 4. pause through the approval-gated MCP tool, then resume");
    print(ttd::set_pause(true, 300));
    let a = ttd::state();
    sleep(3000);
    let b = ttd::state();
    print(`  paused: tick ${a.tick} -> ${b.tick} over 3 s (frozen: ${a.tick == b.tick})`);
    print(ttd::set_pause(false, 300));
    sleep(3000);
    print(`  resumed: tick now ${ttd::state().tick}`);

    print("== 5. final report");
    print(ttd::report());
    print(`decisions recorded: ${read("openttd://decisions").data.decisions.len()}`);
}

run(10, 6);
