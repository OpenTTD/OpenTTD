// Live tuner (RESEARCH-ONLY, research/battle-brain.md): keeps the live PrismaticAI on the battle's best
// measured settings. Every round it reads the genetic algorithm's standings from the LAN battle
// (127.0.0.1:8765/api/tuning: variants and anchors, each with its fitness over its matches), takes
// the best one that has enough matches, compares its settings with what the live bot runs
// (api_bots().running), and changes every knob that differs, one at a time, through set_bot: the
// approval-gated openttd.ai.set_setting, so each change waits for the owner's Approve (or
// auto-approve) and is recorded as a decision. It says each change in the game's chat.
//
// Run it as a service on the Scripts page (/scripts/prismatic_tune). Edit the constants below while
// it runs: the service reloads within a second. Every knob it may touch is a CONFIG_INGAME setting
// of PrismaticAI (info.nut), and it only touches the genes the GA's own space names.
import "ttd" as ttd;

const TUNING = "http://127.0.0.1:8765/api/tuning";
// The company id of the bot to tune (mcp-e2e starts PrismaticAI as company 0).
const COMPANY = 0;
// A variant needs at least this many matches before the tuner trusts its fitness.
const MIN_MATCHES = 4;
// Seconds between rounds, and how long one change may wait for approval.
const ROUND_S = 300;
const APPROVE_S = 120;

fn say(text) {
    text.replace("\"", "'");
    con_say(`"[tune] ${text}"`);
}

fn parse_settings(s) {
    let m = #{};
    for kv in s.split(",") {
        let p = kv.split("=");
        // Rhai's trim changes the string in place and returns (), so it never chains.
        if p.len() == 2 { let k = p[0]; let v = p[1]; k.trim(); v.trim(); m[k] = parse_int(v); }
    }
    m
}

// The best measured candidate: {label, settings, score, matches}, or ().
fn best(t) {
    let top = ();
    for v in t.variants {
        let f = v.fitness;
        if f != () && f.matches >= global::MIN_MATCHES && (top == () || f.score > top.score) {
            top = #{ label: v.label, settings: v.settings, score: f.score, matches: f.matches };
        }
    }
    top
}

// What the live bot runs now: setting name -> value.
fn live_settings() {
    let out = #{};
    for r in api_bots().running {
        if r.id == global::COMPANY {
            for s in r.settings { out[s.name] = s.value; }
        }
    }
    out
}

fn round() {
    let t = loopback_get(global::TUNING);
    let b = best(t);
    if b == () { print("tune: no variant with enough matches yet"); return; }
    let want = parse_settings(b.settings);
    let have = live_settings();
    let changed = 0;
    for name in want.keys() {
        if !(name in t.space) { continue; }          // only the GA's genes
        if !(name in have) { continue; }             // not a setting of this bot
        if have[name] == want[name] { continue; }
        print(`tune: ${name} ${have[name]} -> ${want[name]} (from ${b.label}, score ${b.score}, ${b.matches} matches)`);
        try {
            ttd::set_bot(global::COMPANY, name, want[name]);
            say(`${name} = ${want[name]} (best measured: ${b.label})`);
            changed += 1;
        } catch (err) {
            print(`tune: ${name} not changed: ${err}`);
        }
    }
    print(`tune: ${changed} change(s); the live bot follows ${b.label} (score ${b.score})`);
}

print(`prismatic_tune: company ${COMPANY}, every ${ROUND_S}s, from ${TUNING}`);
loop {
    try { round(); } catch (err) { print(`tune: round failed: ${err}`); }
    sleep(ROUND_S * 1000);
}
