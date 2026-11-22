// Towns as data (RESEARCH-ONLY): what a town's local authority rewards and punishes, what it then
// allows or refuses, and what a company can do about it, as pure functions over the derived model
// (town_model.rhai, generated from the engine sources by tools/research/town-model/derive.py) and
// what the game publishes (openttd://towns). No number here is written by hand: every threshold,
// step and limit is read from the model, so an engine change reaches the scripts by re-running the
// generator. `cargo test` in tools/research/mcp-rhai checks these functions offline (main.rs tests).
//
// A live town record, as openttd://towns gives it for one company (src/prismatic_mcp/
// prismatic_mcp_server.cpp ResourceTowns):
//   rating           the company's rating in the town (-1000..1000), or () when unknown
//   band             the band an AI sees (ScriptTown::GetRating enum), when the exact rating is not
//   active_stations  the company's stations near the town that loaded or unloaded recently
//   idle_stations    the company's stations near the town that did not
//   statue, exclusivity, exclusive_months, unwanted_months, fund_buildings_months, growth_rate
//                    as the town stores them
// Every decision says why in words and names the rule's source line.

import "town_model" as tm;

/// The model itself, for a caller that wants to show or walk it.
fn model() { tm::model() }

/// The rating constant `name` (e.g. "RATING_TREE_UP_STEP").
fn k(name) { tm::model().rating[name] }

/// The band (name, enum, lowest, highest) a rating falls in.
fn band_of(rating) {
    for b in tm::model().bands {
        if b.name != "TOWN_RATING_NONE" && rating >= b.lowest && rating <= b.highest { return b; }
    }
    ()
}

/// The band with the enum value an AI receives from AITown.GetRating.
fn band_from_enum(e) {
    for b in tm::model().bands { if b.enum == e { return b; } }
    ()
}

/// The lowest rating a town record can have: the exact rating when known, else its band's lowest.
fn rating_floor(town) {
    if type_of(town.rating) == "i64" { return town.rating; }
    if type_of(town.band) == "i64" { let b = band_from_enum(town.band); if b != () { return b.lowest; } }
    k("RATING_INITIAL")
}

/// Whether the authority allows a new station at this rating (CheckIfAuthorityAllowsNewStation).
fn allows_station(rating) { rating > tm::model().new_station_above }

/// Whether the authority allows removing a town road ("RoadRemove") or a town tunnel or bridge
/// ("TunnelBridgeRemove") at this rating and tolerance ("lenient", "tolerant"/"neutral", "hostile",
/// "permissive").
fn allows_removal(rating, check, tolerance) {
    let t = if tolerance == "tolerant" { "neutral" } else { tolerance };
    rating >= tm::model().remove_needs[check][t]
}

/// The rating after one ChangeTownRating-style change: a step towards `limit` that never crosses it
/// (src/town_cmd.cpp ChangeTownRating), clamped to the rating range.
fn changed(rating, delta, limit) {
    let r = rating;
    if delta < 0 { if r > limit { r = max(limit, r + delta); } } else { if r < limit { r = min(limit, r + delta); } }
    max(k("RATING_MINIMUM"), min(k("RATING_MAXIMUM"), r))
}

/// The rating after one month's UpdateTownRating: a low rating recovers by RATING_GROWTH_UP_STEP up
/// to RATING_GROWTH_MAXIMUM, then every active station of the company near the town adds
/// RATING_STATION_UP_STEP and every idle one RATING_STATION_DOWN_STEP, and the result is clamped.
fn after_month(rating, active, idle) {
    let r = rating;
    if r < k("RATING_GROWTH_MAXIMUM") { r = min(k("RATING_GROWTH_MAXIMUM"), r + k("RATING_GROWTH_UP_STEP")); }
    r += active * k("RATING_STATION_UP_STEP") + idle * k("RATING_STATION_DOWN_STEP");
    max(k("RATING_MINIMUM"), min(k("RATING_MAXIMUM"), r))
}

/// Months until the rating is above `target` with this many active and idle stations, or -1 when
/// it never gets there within `horizon` months.
fn months_until_above(rating, target, active, idle, horizon) {
    let r = rating;
    for m in 0..horizon {
        if r > target { return m; }
        r = after_month(r, active, idle);
    }
    if r > target { horizon } else { -1 }
}

/// The rating after clearing `tiles` tree tiles (levelling or building on them), each a
/// RATING_TREE_DOWN_STEP towards RATING_TREE_MINIMUM.
fn after_clearing_trees(rating, tiles) {
    let r = rating;
    for i in 0..tiles { r = changed(r, k("RATING_TREE_DOWN_STEP"), k("RATING_TREE_MINIMUM")); }
    r
}

/// Tree tiles to plant to bring the rating above `target`, or -1 when planting cannot (each tile is
/// RATING_TREE_UP_STEP, and planting stops raising it at RATING_TREE_MAXIMUM).
fn trees_to_get_above(rating, target) {
    if rating > target { return 0; }
    if target >= k("RATING_TREE_MAXIMUM") { return -1; }
    let step = k("RATING_TREE_UP_STEP");
    (target + 1 - rating + step - 1) / step
}

/// What the town rewards and what it punishes, from the model's effects: one entry per rule with
/// the trigger in the engine's own words, the delta, the limit and the source.
fn likes_and_dislikes() {
    let likes = [];
    let dislikes = [];
    for e in tm::model().effects {
        if type_of(e.delta) != "i64" { continue; }
        let x = #{ trigger: e.trigger, function: e.function, delta: e.delta, limit: e.limit, source: e.source };
        if e.delta > 0 { likes.push(x); } else { dislikes.push(x); }
    }
    #{ likes: likes, dislikes: dislikes }
}

/// One piece of advice: what to do, why, and the rule's source.
fn item(what, why, source) { #{ what: what, why: why, source: source } }

/// The link of the model with this id, or ().
fn link(id) {
    for l in tm::model().links { if l.id == id { return l; } }
    ()
}

/// The remedies when the town refuses new stations at rating `r` (below or at `need`).
fn refusal_advice(town, r, need, active, idle, src_station) {
    let out = [];
    let qual = "";
    if type_of(town.rating) != "i64" { qual = "at least "; }
    let why = `rating ${qual}${r} is not above ${need}: no new station in this town`;
    let trees = trees_to_get_above(r, need);
    if trees > 0 {
        let what = `plant ${trees} tree tiles near the town`;
        let up = k("RATING_TREE_UP_STEP");
        let cap = k("RATING_TREE_MAXIMUM");
        let more = `; each tile is +${up} up to ${cap}`;
        out.push(item(what, why + more, "src/tree_cmd.cpp CmdPlantTree"));
    }
    let months = months_until_above(r, need, active, idle, 120);
    if months >= 0 {
        let more = `; it recovers by itself with ${active} active and ${idle} idle stations`;
        out.push(item(`wait about ${months} months`, why + more, src_station));
    } else {
        let more = `; with ${idle} idle stations against ${active} active it never recovers`;
        out.push(item("serve or remove the idle stations", why + more, src_station));
    }
    out
}

/// What a company should do in a town, most urgent first: each item says what, why and which rule.
/// `town` is a live town record (see the header); `tolerance` the town_council_tolerance name.
fn advice(town, tolerance) {
    let out = [];
    if tolerance == "permissive" {
        out.push(item("build freely", "the local authority is disabled (town_council_tolerance permissive)",
            "src/town_cmd.cpp CheckIfAuthorityAllowsNewStation"));
        return out;
    }
    let need = tm::model().new_station_above;
    let r = rating_floor(town);
    let active = 0;
    if type_of(town.active_stations) == "i64" { active = town.active_stations; }
    let idle = 0;
    if type_of(town.idle_stations) == "i64" { idle = town.idle_stations; }
    let service = link("station_service_monthly");
    let src_station = "";
    if service != () { src_station = service.source; }
    if type_of(town.unwanted_months) == "i64" && town.unwanted_months > 0 {
        let um = town.unwanted_months;
        let what = `wait ${um} months`;
        out.push(item(what, "a failed bribe made the company unwanted: no town actions until it ends", "src/town_cmd.cpp TownActionBribe"));
    }
    if !allows_station(r) {
        for x in refusal_advice(town, r, need, active, idle, src_station) { out.push(x); }
    }
    if idle > 0 {
        let down = k("RATING_STATION_DOWN_STEP");
        let why = `each costs ${down} rating a month, and only an active one counts for the town's growth`;
        out.push(item(`serve or remove ${idle} idle station(s)`, why, src_station));
    }
    let tree_step = -k("RATING_TREE_DOWN_STEP");
    // Below RATING_GROWTH_MAXIMUM a rating no longer recovers by itself above that mark, so the
    // tree tiles left before a refusal are worth saying.
    if allows_station(r) && r < k("RATING_GROWTH_MAXIMUM") {
        let n = (r - need) / tree_step + 1;
        let why = `clearing ${n} tree tiles (levelling a site clears them) would make the town refuse new stations`;
        out.push(item("avoid clearing trees here", why, "src/tree_cmd.cpp ClearTile_Trees"));
    }
    let g = link("growth_by_active_stations");
    if g != () && active < 5 {
        let now = g.normal[min(active, 5)];
        let next = g.normal[min(active + 1, 5)];
        if next < now {
            let more = active + 1;
            let why = `the town grows every ${now} town ticks with ${active} active stations, ${next} with ${more}`;
            out.push(item("add an active station", why, g.source));
        }
    }
    let s = link("statue_cargo_rating");
    let has_statue = type_of(town.statue) == "bool" && town.statue;
    if s != () && active > 0 && !has_statue {
        let bonus = s.value;
        out.push(item("build a statue", `+${bonus} cargo rating at every station of the company in the town`, s.source));
    }
    out
}

/// The cost of a town action, given the game's town-action base price (the price table's
/// town_action entry): price * cost_factor >> 8 (src/town_cmd.cpp).
fn action_cost(name, base_price) {
    for a in tm::model().actions { if a.name == name { return (base_price * a.cost_factor) >> 8; } }
    ()
}
