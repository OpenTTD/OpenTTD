// Ask relay (RESEARCH-ONLY): the questions the owner asks in the game, through the fork's toolbar
// menu ("Ask the planner..."), answered by the planner and shown back in the game.
//
//   MCP_VIEW_TOKEN=<token file of the GUI client> mcp-rhai --panel <url> tools/research/mcp-panel/rhai/ask_relay.rhai
//
// The game queues each question in openttd://ui/requests as a request of kind "ask" (src/prismatic_mcp/prismatic_mcp_menu.cpp).
// This script reads the queue over the client's own MCP server, sends each unanswered question to the
// panel's /api/intent (the planner, api_intent here), and answers it with openttd.ui.requests.answer,
// which records the answer and shows it in the game as a news message. Nothing is named here: the
// answer is built from what the planner returns.
//
// A BUILD sentence is also CARRIED OUT, which is what makes one sentence in the ~ console enough:
// build_plan::apply sends the settings the planner proposed to the running bot through
// openttd.ai.set_setting, and the answer shown in the game names the approvals now waiting. It
// approves nothing -- the gate is the owner's setting, and the bot builds after the owner approves.
// A find sentence is answered and nothing is applied.

import "build_plan" as build_plan;

let view = env("MCP_VIEW_TOKEN");
if view == "" { throw "set MCP_VIEW_TOKEN to the GUI client's token file"; }

// One line per candidate the planner returns, as much as the answer carries.
fn summarise(r) {
    let it = r.intent;
    let a = r.answer;
    let subjects = [];
    for s in it.subjects { subjects.push(if type_of(s) == "map" { s.name } else { `${s}` }); }
    let head = `${it.action} ${it.ordering} ${subjects}`;
    let lines = [head];
    if "solver" in a && a.solver != () { lines.push(`solver: ${a.solver}`); }
    let n = 0;
    for c in a.candidates {
        if n >= 3 { break; }
        let name = if "name" in c { c.name } else if "a" in c && "b" in c { `${c.a.name} - ${c.b.name}` } else { to_json(c) };
        lines.push(`${n + 1}. ${name}`);
        n += 1;
    }
    if "build" in a && a.build != () { lines.push("a plan to build is ready: approve it in the panel"); }
    lines.reduce(|acc, l| if acc == "" { l } else { acc + "\n" + l }, "")
}

print("ask relay: waiting for questions asked in the game");
loop {
    try {
        let reqs = mcp_read(view, "openttd://ui/requests").data.requests;
        for q in reqs {
            // Only the typed questions: the other kinds (serve_here, plan_rail, watch) are mcp-human's.
            if q.answered || q.kind != "ask" { continue; }
            print(`question ${q.id}: ${q.text}`);
            let r = api_intent(#{ text: q.text });
            let answer = if "http_error" in r { `The planner could not answer: ${r.body}` } else { summarise(r) };
            // A build sentence is carried out here, not merely reported: the settings go to the
            // running bot and the answer tells the owner what is waiting for their approval.
            if !("http_error" in r) && (r.intent.action ?? "find") == "build" {
                let done = build_plan::apply(r.answer);
                if !done.pending.is_empty() {
                    answer += `\n${done.pending.len()} change(s) sent to ${done.bot}, waiting for your approval: `;
                    for id in done.pending { answer += `#${id} `; }
                    answer += "(approve in the panel, or: prismatic_mcp approve <id>)";
                } else if !done.lines.is_empty() {
                    answer += `\n${done.lines[0]}`;
                }
                for line in done.lines { print(`  apply: ${line}`); }
            }
            mcp_call(view, "openttd.ui.requests.answer", #{ id: q.id, answer: answer });
            print(`answered ${q.id}`);
        }
    } catch (err) {
        print(`client or panel not reachable, waiting: ${err}`);
        sleep(3000);
    }
    sleep(1500);
}
