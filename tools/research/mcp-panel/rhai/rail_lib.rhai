// Rail lines as data (RESEARCH-ONLY): pure functions over what the game and the bot publish, with no
// side effect, so the steward (rail_steward.rhai) and the monitor (rail_monitor.rhai) share one
// reading and `cargo test` in tools/research/mcp-rhai checks it offline (main.rs tests).
//
// Inputs, all derived from the live game:
//   records    the bot's line records, ai.log lines "PrismaticAI: rail line <slot>: k=v|k=v|..."
//              (tools/research/prismatic-ai/PrismaticAI/lines.nut LineRecord)
//   vehicles   openttd://vehicles .vehicles (engine_id, lost, broken_down, profits, consist, orders)
//   stations   openttd://stations .stations (id, x, y, w, h)
//   engines    openttd://engines .engines (wagon, power_hp, railtypes, buildable_by)
// Nothing here names a line, a station, an engine or a number the data can give. The policy
// thresholds a decision needs are one map, POLICY below, documented where it is declared, and every
// decision says why in words.

/// The thresholds the steward decides by, each derived from a game rule where one exists.
fn policy() {
    #{
        // A line is judged on a full year: OpenTTD resets profit_this_year on 1 January, so a line
        // younger than this has no last year of its own (days).
        judge_after_days: 400,
        // Grow when this many full trainloads wait at the loading stop (cargo waiting / capacity).
        grow_loads: 1.0,
        // Shrink a line of several trains when less than this share of one load waits and the
        // newest train lost money last year.
        shrink_loads: 0.25,
        // Renew a locomotive type when a buildable one runs this much faster on the same rails.
        renew_speed_gain: 1.15,
    }
}

/// One line record from an ai.log text, or () when the text is not one. The keys are the record's
/// own (label, a, b, cargo, platforms, len, depot, trains_max, wagons_fit, tiles, state); numbers
/// are parsed, the label and state stay text.
fn parse_record(text) {
    let head = "PrismaticAI: rail line ";
    if !text.starts_with(head) { return (); }
    let rest = text.sub_string(head.len());
    let colon = rest.index_of(": ");
    if colon < 0 { return (); }
    let slot = parse_int(rest.sub_string(0, colon));
    let r = #{ slot: slot };
    for kv in rest.sub_string(colon + 2).split("|") {
        let eq = kv.index_of("=");
        if eq < 0 { continue; }
        let k = kv.sub_string(0, eq);
        let v = kv.sub_string(eq + 1);
        r[k] = if k == "label" || k == "state" { v } else { parse_int(v) };
    }
    if r.label == () || r.a == () || r.b == () { return (); }
    r
}

/// The newest record of each company's slot, from ai.log events (oldest first, as events() returns
/// them), each with the `company` the panel stamped on its event.
fn records_from_events(events) {
    let by_slot = #{};
    for e in events {
        let t = if e.data == () { "" } else { e.data.text ?? "" };
        let r = parse_record(t);
        if r == () { continue; }
        // The panel stamps every ai.log event with the company whose AI logged it.
        r.company = if e.data == () { () } else { e.data.company };
        by_slot[`${r.company}:${r.slot}`] = r;
    }
    let out = [];
    for k in by_slot.keys() { out.push(by_slot[k]); }
    out.sort(|x, y| if x.company == y.company { x.slot - y.slot } else if `${x.company}` < `${y.company}` { -1 } else { 1 });
    out
}

/// The station a record's `tile` names on a map `map_w` tiles wide, or (): the station whose own
/// tile it is (openttd://stations `tile`, the corner the bot built from), else the one with the
/// smallest catchment_area ({x, y, w, h}) that holds it. Catchments overlap, so the smallest is the
/// nearest station.
fn station_at(stations, tile, map_w) {
    for s in stations { if s.tile == tile { return s; } }
    let x = tile % map_w;
    let y = tile / map_w;
    let best = ();
    for s in stations {
        let a = s.catchment_area;
        if a == () || a.w == () { continue; }
        if x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h {
            if best == () || a.w * a.h < best.catchment_area.w * best.catchment_area.h { best = s; }
        }
    }
    best
}

/// The station with id `id`, or () (for -1 or a station that is gone).
fn station_by_id(stations, id) {
    if id == () || id < 0 { return (); }
    for s in stations { if s.id == id { return s; } }
    ()
}

/// The station ids a vehicle's orders name, in order (its goto_station orders; a depot or a
/// waypoint is not a stop).
fn order_stations(v) {
    let out = [];
    for o in v.orders ?? [] {
        if o.type == "goto_station" && o.destination != () { out.push(o.destination); }
    }
    out
}

/// Every rail line of `company` (its records whose first station the company owns), joined with the
/// trains whose orders name both of its stations. A line is #{slot, label, record, a, b (station
/// ids, () when the station is gone), trains: [vehicles], closed}.
fn lines_of(company, records, vehicles, stations, map_w) {
    let out = [];
    for r in records {
        // The record's station ids when it has them (a joined platform can lie in a smaller
        // neighbour's catchment), else its tiles.
        let sa = station_by_id(stations, r.st_a ?? -1) ?? station_at(stations, r.a, map_w);
        let sb = station_by_id(stations, r.st_b ?? -1) ?? station_at(stations, r.b, map_w);
        let closed = (r.state ?? "open") == "closed";
        if r.company != () && r.company != company { continue; }
        // A line whose first station is gone (closed and cleared) belongs to nobody any more.
        if sa == () || sa.owner != company { continue; }
        let a = sa.id;
        let b = if sb == () { () } else { sb.id };
        let trains = [];
        for v in vehicles {
            if v.type != "train" || v.owner != company { continue; }
            let st = order_stations(v);
            if a != () && b != () && st.contains(a) && st.contains(b) { trains.push(v); }
        }
        out.push(#{ slot: r.slot, label: r.label, record: r, a: a, b: b, trains: trains, closed: closed });
    }
    out
}

/// What a line earns and carries: the trains' summed profits, the oldest train's age (days), the
/// wagons of its first train (parts less the locomotive), one train's capacity, and the load the
/// trains carry now as a share of what they could.
fn line_stats(line) {
    let s = #{ trains: line.trains.len(), profit_last: 0, profit_this: 0, oldest: 0, wagons: 0,
               capacity: 0, load: 0.0, newest: () };
    let carried = 0;
    let cap = 0;
    for v in line.trains {
        s.profit_last += v.profit_last_year;
        s.profit_this += v.profit_this_year;
        if v.age > s.oldest { s.oldest = v.age; }
        if s.newest == () || v.age < s.newest.age { s.newest = v; }
        carried += v.consist_count;
        cap += v.consist_capacity;
    }
    if !line.trains.is_empty() {
        s.wagons = line.trains[0].parts - 1;
        s.capacity = line.trains[0].consist_capacity;
    }
    if cap > 0 { s.load = carried.to_float() / cap.to_float(); }
    s
}

/// The target for one line given `waiting` (cargo waiting for it at its loading stop) and the
/// policy: #{slot, trains, wagons, action, why} with trains/wagons -1 where nothing changes, or ()
/// when the line is fine as it is. Actions: "close", "lengthen", "add_train", "shrink".
fn target(line, waiting, p) {
    if line.closed { return (); }
    let s = line_stats(line);
    let r = line.record;
    if s.trains == 0 { return (); }
    // Losing money on a whole year and on this one: the line costs more than it brings.
    if s.oldest >= p.judge_after_days && s.profit_last < 0 && s.profit_this < 0 {
        return #{ slot: line.slot, trains: 0, wagons: -1, action: "close",
                  why: `${line.label}: lost ${s.profit_last} last year and ${s.profit_this} this year with ${s.trains} train(s) up to ${s.oldest} days old` };
    }
    if s.capacity <= 0 { return (); }
    let loads = waiting.to_float() / s.capacity.to_float();
    if loads >= p.grow_loads {
        let fit = r.wagons_fit ?? s.wagons;
        if s.wagons < fit {
            // The platform still has room: longer trains first, they cost no new locomotive.
            let per_wagon = (s.capacity / s.wagons.max(1)).max(1);
            let more = ((waiting + per_wagon - 1) / per_wagon).min(fit - s.wagons).max(1);
            return #{ slot: line.slot, trains: -1, wagons: s.wagons + more, action: "lengthen",
                      why: `${line.label}: ${waiting} waiting is ${loads} trainloads of ${s.capacity}; ${s.wagons} -> ${s.wagons + more} wagons (platform holds ${fit})` };
        }
        if s.trains < (r.trains_max ?? 1) {
            return #{ slot: line.slot, trains: s.trains + 1, wagons: -1, action: "add_train",
                      why: `${line.label}: ${waiting} waiting with full-length trains; ${s.trains} -> ${s.trains + 1} train(s) (platforms allow ${r.trains_max})` };
        }
        return ();
    }
    if s.trains > 1 && loads < p.shrink_loads && s.newest != () && s.newest.age >= p.judge_after_days && s.newest.profit_last_year < 0 {
        return #{ slot: line.slot, trains: s.trains - 1, wagons: -1, action: "shrink",
                  why: `${line.label}: ${waiting} waiting (${loads} loads) and train ${s.newest.id} lost ${s.newest.profit_last_year}; ${s.trains} -> ${s.trains - 1}` };
    }
    ()
}

/// What a target costs and is expected to bring in a year, for choose(): #{cost, gain}.
/// - Cost is what the change asks for now. A wagon is priced at the consist's own wagons:
///   (consist_value - value) / wagons, both depreciated values from openttd://vehicles, else the
///   cheapest wagon the engines list sells on the same rails. Lengthening adds that to every train
///   of the line. A new train is the locomotive's price now plus its wagons.
/// - Gain is the line's own earnings per unit of capacity times the capacity added, so it is
///   derived from what the line has done, never assumed.
/// Closing and shrinking free money: no cost, gain the loss they stop.
fn value(line, t, engines) {
    let s = line_stats(line);
    if t.action == "close" || t.action == "shrink" { return #{ cost: 0, gain: (-s.profit_last).max(1) }; }
    let train = line.trains[0];
    let loco = (); 
    for e in engines { if e.id == train.engine_id { loco = e; } }
    let loco_price = if loco == () { 0 } else { loco.price };
    let wagon_price = 0;
    if s.wagons > 0 && train.consist_value != () && train.consist_value > train.value {
        wagon_price = (train.consist_value - train.value) / s.wagons;
    }
    if wagon_price <= 0 {
        for e in engines {
            if e.type == "rail" && e.wagon == true && loco != () {
                let same = false;
                for rt in e.railtypes ?? [] { if (loco.railtypes ?? []).contains(rt) { same = true; } }
                if same && (wagon_price <= 0 || e.price < wagon_price) { wagon_price = e.price; }
            }
        }
    }
    let per_unit = if s.capacity > 0 { (s.profit_last.max(0) / s.trains.max(1)) / s.capacity } else { 0 };
    if t.action == "lengthen" {
        let add = t.wagons - s.wagons;
        let per_wagon = (s.capacity / s.wagons.max(1)).max(1);
        return #{ cost: (wagon_price * add * s.trains).max(1), gain: (per_unit * per_wagon * add * s.trains).max(0) };
    }
    // add_train: a clone of the first train, bought new
    #{ cost: (loco_price + wagon_price * s.wagons).max(1), gain: (s.profit_last.max(0) / s.trains.max(1)).max(0) }
}

/// Choose the actions to take with `budget` money: closings and shrinks always (they free money),
/// the rest by AlgoGlyph's knapsack-dp kernel on cost and gain (`solve` is Fn("kernel"), or ()
/// offline), falling back to the best gain per cost first. `items` are #{target, value}. Returns
/// #{chosen: [targets], solver}.
fn choose(items, budget, solve) {
    let chosen = [];
    let paid = [];
    for it in items {
        if it.value.cost <= 1 && (it.target.action == "close" || it.target.action == "shrink") { chosen.push(it.target); }
        else if it.value.gain > 0 { paid.push(it); }
    }
    if paid.is_empty() || budget <= 0 { return #{ chosen: chosen, solver: "none" }; }
    // Integer weights for the kernel: the budget in at most 200 steps.
    let unit = ((budget + 199) / 200).max(1);
    let weights = [];
    let values = [];
    for it in paid { weights.push(((it.value.cost + unit - 1) / unit).max(1)); values.push(it.value.gain); }
    let cap = budget / unit;
    if solve != () {
        try {
            let r = call(solve, "knapsack-dp", #{ weights: weights, values: values, capacity: cap });
            for i in r.items { chosen.push(paid[i].target); }
            return #{ chosen: chosen, solver: "algoglyph knapsack-dp" };
        } catch (e) { }
    }
    // Greedy: gain per cost, while the budget lasts.
    paid.sort(|x, y| if y.value.gain * x.value.cost > x.value.gain * y.value.cost { 1 } else { -1 });
    let left = budget;
    for it in paid { if it.value.cost <= left { chosen.push(it.target); left -= it.value.cost; } }
    #{ chosen: chosen, solver: "greedy gain per cost" }
}

/// Locomotive renewals for `company`: for each locomotive type its trains run, the fastest buildable
/// locomotive on the same rails that is at least renew_speed_gain faster (ties: more power).
/// Returns [#{from, to, from_name, to_name, why}]: what ScriptGroup.SetAutoReplace(GROUP_ALL, from,
/// to) hands to the game's own autoreplace, which swaps them at the next depot visit.
fn renewals(company, vehicles, engines, p) {
    let by_id = #{};
    for e in engines { by_id[`${e.id}`] = e; }
    let used = #{};
    for v in vehicles {
        if v.type == "train" && v.owner == company && v.engine_id != () { used[`${v.engine_id}`] = true; }
    }
    let out = [];
    for k in used.keys() {
        let old = by_id[k];
        if old == () || old.wagon == true { continue; }
        let best = ();
        for e in engines {
            if e.type != "rail" || e.wagon != false || e.id == old.id { continue; }
            if !(e.buildable_by ?? []).contains(company) { continue; }
            let same = false;
            for rt in e.railtypes ?? [] { if (old.railtypes ?? []).contains(rt) { same = true; } }
            if !same { continue; }
            if e.max_speed_kmh.to_float() < old.max_speed_kmh.to_float() * p.renew_speed_gain { continue; }
            if best == () || e.max_speed_kmh > best.max_speed_kmh || (e.max_speed_kmh == best.max_speed_kmh && e.power_hp > best.power_hp) { best = e; }
        }
        if best != () {
            out.push(#{ from: old.id, to: best.id, from_name: old.name, to_name: best.name,
                        why: `${old.name} ${old.max_speed_kmh} km/h -> ${best.name} ${best.max_speed_kmh} km/h, ${best.power_hp} hp` });
        }
    }
    out
}

/// What the monitor reports for `company`: every train that is lost (no way to its destination),
/// broken down, crashed, past its maximum age or stopped outside a depot; every line with no
/// train; and every ai.log warning about rail since the last look. [#{kind, id, text}], worst first.
fn problems(company, lines, vehicles, warnings) {
    let out = [];
    for v in vehicles {
        if v.type != "train" || v.owner != company { continue; }
        if v.crashed { out.push(#{ kind: "crashed", id: v.id, text: `train ${v.id} crashed at tile ${v.tile}` }); }
        if v.lost == true { out.push(#{ kind: "lost", id: v.id, text: `train ${v.id} is lost at tile ${v.tile} (no way to order ${v.current_order})` }); }
        if v.broken_down == true { out.push(#{ kind: "broken_down", id: v.id, text: `train ${v.id} broken down at tile ${v.tile} (reliability ${v.reliability}%)` }); }
        if v.stopped && !v.in_depot && !v.crashed { out.push(#{ kind: "stopped", id: v.id, text: `train ${v.id} stopped outside a depot at tile ${v.tile}` }); }
        if v.max_age > 0 && v.age > v.max_age { out.push(#{ kind: "old", id: v.id, text: `train ${v.id} is ${v.age} days old (max ${v.max_age})` }); }
    }
    for l in lines {
        if !l.closed && l.trains.is_empty() { out.push(#{ kind: "unserved", id: l.slot, text: `line ${l.slot} ${l.label} has no train` }); }
    }
    for w in warnings { out.push(#{ kind: "warning", id: (), text: w }); }
    let rank = #{ crashed: 0, lost: 1, unserved: 2, warning: 3, broken_down: 4, stopped: 5, old: 6 };
    out.sort(|x, y| rank[x.kind] - rank[y.kind]);
    out
}

/// The bot's rail warnings in ai.log events: a line it could not route or lay, a broken line, a
/// refused piece. [text].
fn rail_warnings(events) {
    let out = [];
    for e in events {
        let t = if e.data == () { "" } else { e.data.text ?? "" };
        if t.contains("no rail path") || t.contains(" is broken at tile ") || t.contains("rail at tile") && t.contains("failed")
            || t.contains("throat of the station") || t.contains("no rail depot") {
            out.push(t);
        }
    }
    out
}

/// One line for the board: counts by kind, then the worst few texts.
fn digest(instance, date, problems, keep) {
    if problems.is_empty() { return `OpenTTD ${instance} ${date} rail: no problems`; }
    let counts = #{};
    for p in problems { counts[p.kind] = (counts[p.kind] ?? 0) + 1; }
    let parts = [];
    for k in counts.keys() { parts.push(`${counts[k]} ${k}`); }
    let worst = [];
    for i in 0..problems.len().min(keep) { worst.push(problems[i].text); }
    `OpenTTD ${instance} ${date} rail: ${parts}; ${worst}`
}
