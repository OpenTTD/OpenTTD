// Steward (RESEARCH-ONLY): your bot talks to you inside the game. It asks yes/no questions in the
// game's own query dialog (openttd.view.ask), and your answer steers what happens next.
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/steward.rhai
//
// - Every few minutes it offers the planner's best pair that you have not seen yet (GET /api/plan):
//   "bus between A and B, 33 tiles, already joined by road. Show it to you?" On yes your camera
//   goes to both towns, marks them, and a newspaper with a viewport sums it up. On no that pair is
//   never offered again. If nobody answers, it asks again later.
// - When the bot has opened all the routes it may and has spare money, it asks whether to allow
//   two more. It only asks this when the game has the live-settings tool openttd.ai.set_setting,
//   and the change still waits for your approval in the panel.
// Every question and every answer is also an event (ask.answered) on the panel's feed.
import "ttd" as ttd;

const ASK_EVERY_S = 180;
const ANSWER_WITHIN_S = 90;

fn view(name, args) { api_viewTool(#{ name: name, arguments: args }) }

fn show_pair(p) {
    view("openttd.view.scroll_to", #{ x: p.a.x, y: p.a.y });
    view("openttd.view.mark", #{ x: p.a.x, y: p.a.y });
    sleep(3500);
    view("openttd.view.scroll_to", #{ x: p.b.x, y: p.b.y });
    view("openttd.view.mark", #{ x: p.b.x, y: p.b.y });
    sleep(2500);
    ttd::news(`Planner: ${p.mode} between ${p.a.name} and ${p.b.name}, ${p.distance} tiles, score ${p.score}.`,
              #{ town: p.b.id });
}

fn pick(plan, declined) {
    for p in plan.pairs {
        let k = `${p.a.id}-${p.b.id}`;
        if !declined.contains(k) { return p; }
    }
    for mode in plan.by_mode.keys() {
        for p in plan.by_mode[mode] {
            let k = `${p.a.id}-${p.b.id}`;
            if !declined.contains(k) { return p; }
        }
    }
    ()
}

fn routes_from_log() {
    // The bot's heartbeat: "... routes=4/4 ... cash_reserve=20000 ..." and money=69026.
    for e in ttd::bot_log(40) {
        let t = e.data.text;
        if t.contains("heartbeat:") && t.contains("routes=") {
            let r = t.split("routes=")[1].split(" ")[0].split("/");
            let money = parse_int(t.split("money=")[1].split(" ")[0]);
            let reserve = parse_int(t.split("cash_reserve=")[1].split(" ")[0]);
            return #{ open: parse_int(r[0]), max: parse_int(r[1]), money: money, reserve: reserve };
        }
    }
    ()
}

let can_set = tools().contains("openttd_ai_set_setting");
print(`steward online; live bot settings ${if can_set { "available" } else { "not in this build: no expansion questions" }}`);
let seen = [];
let expansion_asked = false;
sleep(30000);
loop {
    try {
        let r = if can_set && !expansion_asked { routes_from_log() } else { () };
        if r != () && r.open >= r.max && r.money > 3 * r.reserve {
            expansion_asked = true;
            let a = ttd::ask_owner(`Your bot runs ${r.open}/${r.max} routes and has ${r.money}. Allow two more routes?`, ANSWER_WITHIN_S);
            print(`expansion: ${a}`);
            if a == "yes" {
                for c in ttd::bots().running { print(ttd::set_bot(c.id, "max_routes", r.max + 2)); }
                ttd::news("Allowed two more routes; approve the change in the panel.");
            }
        } else {
            let p = pick(world_plan(10), seen);
            if p != () {
                let a = ttd::ask_owner(`Planner: ${p.mode} between ${p.a.name} and ${p.b.name}, ${p.distance} tiles${if p.road_connected { ", already joined by road" } else { "" }}. Show it to you?`, ANSWER_WITHIN_S);
                print(`${p.a.name}-${p.b.name}: ${a}`);
                if a == "yes" { show_pair(p); }
                // Only a real answer retires the pair; unanswered or replaced, it is offered again.
                if a == "yes" || a == "no" { seen.push(`${p.a.id}-${p.b.id}`); }
            }
        }
    } catch (err) {
        print(`steward: ${err}`);
    }
    sleep(ASK_EVERY_S * 1000);
}
