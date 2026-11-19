---
schema: knowledge/v1
id: prismatic-ai-without-a-steward-keeps-one-train-per-line
kind: knowledge
class: fact
title: "PrismaticAI run without the panel keeps one train per rail line and hoards its earnings, so its fleet shrinks"
description: "Before reading any result of PrismaticAI started without the MCP panel (the LAN AI battle, a plain start_ai): its rail lines never grow and fleet.nut sells loss-making road vehicles without replacing them."
status: verified
epistemics: observed
date: 2026-11-19
tags: [prismatic-ai, steward, ai-battle, fleet, diagnosis]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/prismatic-ai/PrismaticAI/lines.nut
    - file:tools/research/prismatic-ai/PrismaticAI/fleet.nut
    - file:tools/research/prismatic-ai/PrismaticAI/main.nut
    - file:tools/research/prismatic-ai/PrismaticAI/steward.nut
    - file:tools/research/mcp-panel/rhai/rail_lib.rhai
    - file:tools/research/ai-battle/battle.py
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/duels-run-side-by-side-are-noisy.md
  - type: relates_to
    target: file:.ai/repo/adrs/0035-company-ais-are-ranked-in-headless-null-driver-matches-on-ever.md
---

# Fact

A rail line of PrismaticAI runs what the Rhai steward sets: `line<N>_trains` and `line<N>_wagons`,
default -1, which means no target (`lines.nut:ReconcileLines`). The steward lives in the MCP panel
(`rail_steward.rhai` with `rail_lib.rhai`). Started without the panel, by `start_ai` in the LAN AI
battle, the bot keeps the one train each line was built with. Once it has planned `own_lines`
lines, nothing else spends its money. `fleet.nut` sells road vehicles that lose money or sit idle
and does not replace them.

Observed on 2026-11-19 in the battle's edition b2ff6a45, in every match the orchestrator's
self-monitoring watched: solo-max (`own_lines=8`) reaches 20-29 vehicles by its second or third
year, then falls to 11-17 (`fleet_drop` alerts) while its bank balance rises into the millions.
With default settings (`own_lines=0`, `max_routes=4`) PrismaticAI stays at 2-4 vehicles.

On the same seed and binary, with the working tree of 2026-11-19, the opt-in in-bot steward
(`self_steward=1`, `steward.nut`) roughly doubled to quintupled solo-max's value at year 10. On two
seeds it ran 30 vehicles where solo-max ran 9-12, with 10 and 8 trains added and one line closed.
That is two seeds, unpaired with a held start, and in a headless battle rather than a duel, so it
is no reason to change a default (rule `project.a-tuning-claim-needs-held-start-pairs`). The
battle's records compare the two contestants on every seed of an edition.

Since 2026-11-19 the unplanned bot defaults to `autopilot=1`, which turns on this steward, own
lines and intercity roads when no plan was handed over, by the owner's order that PrismaticAI is
never last (rule `project.prismatic-ai-is-never-last`). `autopilot=0` gives the old unplanned
bot. The battle's control contestant `PrismaticAI solo-max` runs that way.

The winner's lead opens in the first two quarters, and it is carried by aircraft. In edition
91a72662 (rev df1e05e7d8) AAAHogEx ran 10 planes and about 340k of air profit a year by January
1961, while no PrismaticAI configuration owned an aircraft in any match and its first vehicle often
came after the first quarter. Its rail earned more per train (29k against 4k in 1962), but it
never scaled. A single-track line holds two trains (`rail_platforms` is at most 2), so rail grows
only by more lines. Since this was found, the autopilot builds its own air routes first
(`autopilot.nut`). Three PrismaticAI configurations on one map took each other's airport towns, so
the battle plays one of them per match (`lineup_for`).

In the first air edition (361b802f) three further failures showed up, all fixed in the bot:
- Airports were refused by `ERR_STATION_TOO_MANY_STATIONS_IN_TOWN` (365 sites in one game) and
  `ERR_LOCAL_AUTHORITY_REFUSES`. The bot now joins an adjacent station of its own and stops
  surveying a town whose authority refuses (`AIR_BAD_TOWNS`).
- `fleet.nut`'s "almost nothing waits" rule sold planes on routes earning about 67k a plane a
  year, because aircraft pick up every passenger. A route whose judged vehicles earned money last
  year is no longer thinned (`RouteEarns`).
- Idle money still did not buy vehicles. With the steward on, `InvestSurplus` adds a vehicle to
  the route earning most per vehicle while that is above a third of its price. On one seed the
  12-year value rose by about a fifth, with 14-16 planes instead of 2-4.

Next, with airports placeable, the limit was where they may go. `ERR_STATION_TOO_MANY_STATIONS_IN_TOWN`
is the authority refusing another airport. The winner flies from month one and takes the biggest
towns' slots first, and the bot then surveyed 25 sites in each refusing town. It now stops at the
first refusal, skips towns that already hold another company's airport, and picks from the 25
biggest towns (up to 10 routes). On seed 930967890 in 1965 that gave 19 planes against AAAHogEx's 20
and 2.15M against 2.64M. The remaining gap is rail (14 trains against 28) and income per quarter.

What AAAHogEx does that PrismaticAI does not, from its source, ranked by growth effect:
1. Debt-financed reinvestment with cash near zero.
2. One ROI-ranked candidate pool across all modes and distances.
3. Early air with a full load at both ends.
4. Double-track rail with uncapped train cloning.
5. Road-stop rings joined to each passenger station.
6. Line extension.
7. Pruning.
8. Statues.

Four of these became bot settings (catchment rings, statues, full load, borrowing) and stay off by
default. The recorded single-seed ablation (`research/results/ablation-20261119T062938-seed930967890.md`)
gave catchment rings +4%, full load -2%, borrowing 0 and statues -45%. Transplanted alone into a bot
built otherwise, they do not carry AAAHogEx's edge. The battle's genetic algorithm explores them on
many seeds. The unified pool (item 2) is not ported yet. Double-track rail (item 4) is the opt-in
setting `rail_double` since 2026-11-19. Its recorded 3-seed, 10-year ablations gave -24.2%, -15.5%
and +16.4%, and rail trains did not grow (knowledge `rail-lines-a-train-can-turn-on`, section
*Double track*). It stays off by default.

Two further mechanisms were measured with recorded 3-seed, 10-year ablations
(`research/results/ablation-20261119T06*`):
- `air_add_waiting=10` with `catchment_stops=8` (a plane more while both airports have passengers
  waiting, and road-stop rings): +59.5%, +46.9% and -1.9%. On the 6-year fitness lineup it lost.
  The default stays off, and the battle plays it as the group anchor `PrismaticAI air10+catch`.
- `rail_twin_max` (a second line between the same industries when a line is saturated):
  +13.3%, -6.7% and +4.7% alone. Combined with air10+catch it was worse on all three seeds
  (+19.9%, +29.7%, -18.0%), so it is off by default and a GA gene.

In the battle (20 years, editions from fd41eaa1 on), the anchor `PrismaticAI air10+catch` played
the default on 36 common seeds. It had a +14.8% higher mean value and was better on 27 of 36. So
since 2026-11-19 the bot's defaults are `air_add_waiting=10` and `catchment_stops=8`. This is
a default change on battle evidence (paired, deterministic, 36 seeds), not on the held-start duel
pairs that rule `project.a-tuning-claim-needs-held-start-pairs` asks for; those have not been run.

**A plan from the panel switched growth off.** Held-start duels by openttd-1e (seed 4242, 120 months)
found the planned bot A ending with ~20 vehicles and no plane, while the unplanned autopilot
rival ran ~190 vehicles with 100 planes and won. `Behaviour()` turned the whole autopilot off once
a plan existed. Since 2026-11-19 `autopilot=1` keeps the growth layer on top of a plan (`grows`:
in-bot steward, own air routes after the planner's pair, idle money buying vehicles), while the
planner's lines and own_lines/max_routes/cash_reserve stand. The planner's air pair is now built on
borrowed money; on cash alone it had built both airports and no plane. A route that gets no plane
takes its airports down again.

Recorded ablations (`research/results/ablation-20261119T094*`, 10 years, a plan of the air pair
`air_a=0,air_b=1`) gave +219%, +103% and +61% for growth on a plan against the old planned bot.
This is the reason the planner (AlgoGlyph kernels through the panel) lost to the bot's own
autopilot: its plan replaced growth instead of seeding it.

Double track (`rail_double`), after its add rule fills a free source platform, was paired with the
default in the battle on 74 common seeds over 20 years: +14.8% mean value, better in 56 of 74,
while single 10-year seeds swung from -29% to +48%. Its gain comes late. Since 2026-11-19 it is
the default, on the same basis and with the same caveat as air10+catch (battle evidence, not
held-start duel pairs).

**The GA's champion became the default.** `PrismaticAI GA g14.0` (= g15.0; its genes came from the
battle brain's safe proposal g13.brainSafe) beat the default on 20 of 20 common seeds over 20 years,
with +30.9% mean value. Variants with a higher GA score on 6-7 matches did worse in pairs (g15.3:
0.903), so the paired comparison, not the GA score, decides a default. Since 2026-11-19 the
defaults are its genes:
- aggressiveness 20,
- air_add_waiting 0, air_max_tiles 420, air_planes 1, air_routes_max 3, air_towns 50,
- invest_air_cap 12, invest_borrow 1, invest_min 50 (thousand), invest_ppv_div 2, invest_road_cap 20,
- line_budget 90000,
- the autopilot's own-lines floor 10.
max_routes and cash_reserve stay under the autopilot's floors. In short: fewer air routes, and
investment that starts earlier, accepts lower returns, borrows, and lets routes grow further.

**A hilly map left it last with buses (2026-11-19).** In edition a872adde, both of PrismaticAI's
last places came from one map, OpenTTD seed 1807087905. The other AIs ran 48-544 vehicles there,
and the default PrismaticAI ran 8. A verbose replay (`battle.py ablate --log-dir`, script level 4)
showed two openers failing on the same terrain:
- Every double-track path search stopped after 5 nodes. The straight exits and the second
  track's reserved tiles left no way out. The bot then removed both stations. The same pair
  single track (`rail_double=0`) built and ran 7 trains.
- Every airport site was sloped, and levelling one cost more than the fixed 15k cap. The log
  showed these as "refused: ERR_NONE", because the test pass had cleared the error.

Fixes:
- A double-track line with no path is now retried single track (`rail.nut`).
- The levelling cap is the setting `air_level_max`, a GA gene. With no air route yet and two
  failed pairs, it is multiplied by 4 (`autopilot.nut`).

Replaying 3 years of that seed raised the default's value from 1 to 318k (8 trains, 2 planes).
Seeds 930967890 and 31337 were unchanged over 6 years. A cap of 60 everywhere cost those seeds
38% and 8%, which is why it is raised only after the opener fails.

**The seller killed air for good (2026-11-19).** In edition 6779ebec, two GA variants ended last
on two maps, both without a single plane. Replays need the arena's start order, because the first
company to start takes the first airport sites (`battle.py ablate --slot N`). With that order, the
20-year replay of seed 725554694, company 2, reproduced the arena exactly: 62 vehicles and no
plane.

Every plane lost money in 1964-66, and the fleet seller also sold each route's last one. With no
plane left to clone, the company never flew again. The default had kept one plane on its first
route and grew it back to 12 planes at 23k a year each.

The new setting `air_keep_last` (on by default, a GA gene) keeps the last plane of an air route.
Default bot over 20 years, before -> after:

| Seed | Before | After |
|---|---|---|
| 930967890 | 15.6M | 15.3M |
| 31337 | 13.6M | 12.7M |
| 496519251 | 3.2M | 5.9M |
| 725554694 (slot 2) | 5.9M | 4.5M |

The sum is unchanged and the worst seed is up 39%. g26.0 on seed 725554694 went from 1.3M with
no plane to 3.4M with 16 planes. Single seeds are a direction, not a verdict; the GA decides the
gene on paired matches.

A caveat on any single replay: with `air_keep_last=0`, the committed code ends that seed at 2.2M
with 8 planes, not at 1.3M with none like the code before it
(`research/results/ablation-20261119T140023-seed725554694.md`). The only difference between the
two is extra script work (counting planes). A script's operations decide on which tick its
actions land, so any code change perturbs the whole game. A single-seed difference therefore
mixes the change with this noise, which is why the GA and the promotion rule judge on many
paired seeds.

# How to apply it

- Do not read PrismaticAI's battle results as what the bot does when the panel steers it. The
  default and solo-max contestants measure the bot without its steward. `PrismaticAI self-steward`
  measures the steward's policy run inside the bot.
- A change to `lines.nut`, `fleet.nut`, `steward.nut` or the own-lines loop in `main.nut` must
  pass `tools/research/ai-battle/battle.py fitness`. `tools/gate.sh change` runs it on such a
  change, and it fails on a collapsing fleet of the contestant it holds healthy.
- During a run, read the dashboard's alerts or a record's `anomalies`. A `fleet_drop` on a
  contestant is the signature of this fact.
