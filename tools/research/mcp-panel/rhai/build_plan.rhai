// Carry out a sentence (RESEARCH-ONLY): the half that `plan <text>` leaves undone.
//
//   tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/build_plan.rhai
//   or, in the panel's developer console:
//       /rhai import "build_plan" as b; b::run("vystav trať mezi nejproduktivnějším lesem a pilou")
//
// `plan <text>` in the ~ console ANSWERS a sentence: it prints the reading and the candidates, and
// for a build it prints the settings that would do it. Nothing carries those settings to the bot,
// so the owner has to read three numbers off the console and type three `bot set` lines. This
// script is that last step, and nothing more:
//
//   api_intent(text)            the same resolver the console verb uses
//     -> reading + build.settings
//   api_botSetting(...)         one call per setting, on the running bot
//     -> each returns pending_approval #id, because openttd.ai.set_setting is gated
//   the owner approves          `prismatic_mcp approve <id>` in the console, or the panel
//     -> the bot builds the line on its next heartbeat
//
// IT DOES NOT APPROVE ANYTHING. The gate is the owner's setting ("Keep it off"), and a script that
// approved its own changes would make that setting a decoration. It stops at the pending ids and
// prints them.
//
// Everything it needs is derived: the company from the running bot, the settings from the answer,
// the setting names from what the bot declares. The only thing typed here is the sentence.
import "ttd" as ttd;

// The company whose bot we are steering: the first running bot the panel reports.
// /api/bots -> {source, ais, running[], live}; running[] = {id, name, money, ai, running, settings[]}
fn running_bot() {
    let bots = api_bots();
    let running = bots.running ?? [];
    if running.is_empty() { return (); }
    running[0]
}

/// Apply the settings an answer proposes to the running bot, and report what came back.
/// Returns #{pending: [approval ids], refused: [setting names the bot does not declare],
///            lines: [one human line per call], bot: name}. Applies nothing else and approves
/// nothing: every call is openttd.ai.set_setting, which is gated.
fn apply(a) {
    let settings = (a.build ?? #{}).settings ?? #{};
    let out = #{ pending: [], refused: [], lines: [], bot: "" };
    if settings.is_empty() { return out; }
    let bot = running_bot();
    if type_of(bot) == "()" {
        out.lines.push("no bot is running: start one (bot start PrismaticAI) and ask again");
        return out;
    }
    out.bot = bot.name;
    let declared = [];
    for s in (bot.settings ?? []) { declared.push(s.name); }
    for name in settings.keys() {
        if !declared.contains(name) {
            out.refused.push(name);
            out.lines.push(`${name}: refused, the bot declares no such setting`);
            continue;
        }
        let r = api_botSetting(#{ company: bot.id, name: name, value: settings[name] });
        // the gated reply is {status: "pending_approval", approval_id, argument_digest,
        // expires_at_tick, message}. Reading the wrong key here printed "nothing is waiting for
        // you" while three changes waited -- a false clean, so the key is taken from the reply.
        let id = r.approval_id ?? ();
        if type_of(id) == "()" {
            out.lines.push(`${name} = ${settings[name]}: ${r}`);
        } else {
            out.pending.push(id);
            out.lines.push(`${name} = ${settings[name]}: pending approval #${id}`);
        }
    }
    out
}

fn run(text) {
    print(`== sentence: ${text}`);

    // 1. the reading. Printed first and always, because a sentence that names nothing does not
    // error -- it falls back to the planner's candidates, and `how` is the only thing that says so.
    let answer = api_intent(#{ text: text });
    let intent = answer.intent ?? #{};
    let a = answer.answer ?? #{};
    print(`   read as: ${intent.action ?? "?"} ${intent.ordering ?? ""} -- ${a.how ?? "(no how: do not trust this)"}`);
    let subjects = [];
    for s in (intent.subjects ?? []) { subjects.push(s.name); }
    print(`   subjects: ${if subjects.is_empty() { "(none named)" } else { subjects }}`);
    print(`   candidates: ${(a.candidates ?? []).len()}, solver ${a.solver ?? "?"}`);

    if (intent.action ?? "find") != "build" {
        print("   a find sentence: nothing to carry out. Say 'vystav'/'vybuduj' to build.");
        return;
    }

    let build = a.build ?? #{};
    let settings = build.settings ?? #{};
    if settings.is_empty() {
        print("   the answer proposes no settings; nothing to apply.");
        return;
    }
    print(`   would build with: ${settings}`);

    // 2. the bot that will do it, and the settings it actually declares. A plan naming a setting
    // the bot does not have reads perfectly and does nothing, so it is refused here rather than
    // applied into silence.
    let bot = running_bot();
    if type_of(bot) == "()" {
        print("   no bot is running: start one (bot start PrismaticAI) and run this again.");
        return;
    }
    let declared = [];
    for s in (bot.settings ?? []) { declared.push(s.name); }
    print(`   bot: ${bot.name} (company ${bot.id}), ${declared.len()} settings declared`);

    let outcome = apply(a);
    let pending = outcome.pending;
    let refused = outcome.refused;
    for line in outcome.lines { print(`   ${line}`); }

    if !refused.is_empty() {
        print(`   REFUSED, the bot declares no such setting: ${refused}`);
    }
    if pending.is_empty() {
        print("== nothing is waiting for you.");
    } else {
        print(`== ${pending.len()} change(s) waiting for your approval: ${pending}`);
        print("   approve them in the panel, or in the ~ console:");
        for id in pending { print(`       prismatic_mcp approve ${id}`); }
        print("   the bot builds the line on its next heartbeat after the last approval.");
    }
}

// Imported by ask_relay.rhai, so NOTHING may run at import time. As a script it takes the
// sentence from the environment:
//   BUILD_PLAN_TEXT="vystav trať mezi nejproduktivnějším lesem a pilou" \
//     tools/research/mcp-rhai/target/release/mcp-rhai tools/research/mcp-panel/rhai/build_plan.rhai
let from_env = env("BUILD_PLAN_TEXT");
if from_env != "" { run(from_env); }
