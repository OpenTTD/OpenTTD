// Live arena (RESEARCH-ONLY): keeps a lineup of company AIs playing in the running game and
// reports the standings in the game's chat and as a newspaper. It is the visible counterpart of
// the headless LAN battle (research/ai-battle/README.md), and uses the same contestants.
//
//   Run it as a service on the panel's Scripts page (/scripts/arena), or once:
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/arena.rhai
//
// The lineup below is the desired state. Every round the script compares it with the AI
// companies running now (api_bots().running) and starts what is missing, one AI per round so
// that each new company appears where the game puts it. As a service it restarts within a second
// when this file is saved, so editing LINEUP changes the running game live: add an entry and
// that AI joins. The script never stops or removes a company. Which AI owns a company comes from
// the game itself (openttd.ai.list), not from start order.
//
// Starting an AI goes through the server console (start_ai), as the operator would type it; it
// changes who plays, not the game's rules. The chat line and the newspaper only display text.
import "ttd" as ttd;

// The desired lineup: an AI name as the game lists it, and the settings string start_ai hands
// it ("" for the AI's defaults). Two entries with the same AI mean two companies of it.
const LINEUP = [
    #{ ai: "PrismaticAI",     settings: "" },
    #{ ai: "AAAHogEx",        settings: "" },
    #{ ai: "LuDiAI AfterFix", settings: "" },
    #{ ai: "CivilAI",         settings: "" },
    #{ ai: "AdmiralAI",       settings: "" },
    // The battle's second PrismaticAI contestant, "PrismaticAI solo-max" (battle.json).
    #{ ai: "PrismaticAI",
       settings: "aggressiveness=100,max_routes=30,intercity=1,own_lines=8,cash_reserve=5000" },
    // "PrismaticAI self-steward": solo-max with the in-bot steward on (ba51ba6022).
    #{ ai: "PrismaticAI",
       settings: "aggressiveness=100,max_routes=30,intercity=1,own_lines=8,cash_reserve=5000,self_steward=1" },
];

// Seconds between rounds, and rounds between two standings reports.
const ROUND_S = 20;
const REPORT_EVERY = 6;

fn say(text) {
    // The console takes one line; a quote inside the message would end it early.
    text.replace("\"", "'");
    con_say(`"[arena] ${text}"`);
}

// How many companies of each AI run now, and the running rows themselves.
fn running() { api_bots().running }

fn count_of(rows, ai) {
    let n = 0;
    for r in rows { if r.ai == ai { n += 1; } }
    n
}

// The first lineup entry that has fewer companies than it asks for, or ().
fn missing(rows) {
    let wanted = #{};
    for e in global::LINEUP {
        let k = e.ai;
        wanted[k] = if k in wanted { wanted[k] + 1 } else { 1 };
        if count_of(rows, k) < wanted[k] { return e; }
    }
    ()
}

// The name in double quotes: the only quoting the console tokenizer understands, and needed for
// an AI name with a space ("LuDiAI AfterFix").
fn start(e) {
    let name = `"${e.ai}"`;
    ttd::start_ai(name, e.settings);
    say(`${e.ai} joins the arena` + if e.settings == "" { "" } else { ` (${e.settings})` });
}

// Companies ranked by value (OpenTTD's own figure: assets plus cash minus loan), each with the AI
// that runs it.
fn standings() {
    let ai_of = #{};
    for r in running() { ai_of[`${r.id}`] = r.ai; }
    let rows = [];
    for c in ttd::companies() {
        let k = `${c.id}`;
        rows.push(#{ id: c.id, ai: if k in ai_of { ai_of[k] } else { "human" }, name: c.name,
                     value: if c.value == () { 0 } else { c.value }, money: c.money,
                     vehicles: (c.trains ?? 0) + (c.road ?? 0) + (c.air ?? 0) + (c.ships ?? 0) });
    }
    rows.sort(|a, b| b.value - a.value);
    rows
}

// Prints the standings and says them in chat; returns the leading AI ("" with no company), so the
// caller can put a change of leader in the newspaper.
fn report() {
    let s = ttd::state();
    let rows = standings();
    let text = `${s.date}  standings:`;
    let place = 1;
    for r in rows {
        let line = `${place}. ${r.ai}  value ${r.value}  vehicles ${r.vehicles}`;
        text += "\n  " + line;
        say(line);
        place += 1;
    }
    print(text);
    if rows.is_empty() { "" } else { rows[0].ai }
}

ttd::wait_running(120);
print(`arena: lineup of ${LINEUP.len()} AIs, a round every ${ROUND_S}s`);
let round = 0;
let leader = "";
loop {
    let e = missing(running());
    if e != () { start(e); }
    if round % REPORT_EVERY == 0 {
        let now = report();
        if now != "" && now != leader {
            // A newspaper needs a GUI client joined to the server; without one it only fails here.
            try { ttd::news(`Arena: ${now} takes the lead`); } catch (err) { print(`news: ${err}`); }
            leader = now;
        }
    }
    round += 1;
    sleep(ROUND_S * 1000);
}
