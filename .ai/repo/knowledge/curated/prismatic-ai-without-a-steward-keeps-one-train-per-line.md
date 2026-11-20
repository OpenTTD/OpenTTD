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
Caveat (2026-11-19, later): those 20 pairs came from editions before 46dd8814f9, which replayed
the same 6-8 seeds, so the result is partly fitted to those maps. Once every generation met
new maps, the champion was even with `PrismaticAI pre-champion` (4 of 10 pairs). Pre-champion
stays an anchor until 16 or more pairs on new maps exist, and then the promotion rule decides.
The battle brain also learns only from the newest 10 editions (BRAIN_EDITIONS).
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

The new setting `air_keep_last` (on by default) keeps the last plane of an air route. It is pinned in `tuning.fixed`, not a gene: the battle brain kept proposing 0 because most of its rows predate the gene, and `g39.brainSafe` and `g41.brainSafe` ended last with it off. A setting that fixes a bug stays out of the GA's space.
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

**A double-track line bankrupted the company (2026-11-19).** In edition 150ed917,
`g30.brainSafe1` ended last with a value of 1. In the 20-year replay of seed 1516196607 (company
4, `--slot 4`), the bot laid both tracks of a 192-tile line, 58 signals included, on the whole
300k loan. The locomotive was then refused for ERR_NOT_ENOUGH_CASH, and the company went
bankrupt in 1961 with two buses.

Track B is now laid only when the money still buys the train afterwards: track A's actual cost
plus the train's price must be covered (`rail.nut`). Otherwise the line runs single track. The
same replay survives at 452k with 46 vehicles. The default bot over 20 years:

| Seed | Before | After |
|---|---|---|
| 1516196607 | 7.6M | 9.3M |
| 930967890 | 15.3M | 16.0M |
| 31337 | 12.7M | 15.3M |

**The GA's elite became the default by its interval (2026-11-19, later).** One configuration (as
g54.0, g57.0 and g58.0) was paired with the default on 40 new maps:
- geometric mean ratio 1.12, with a 95% bootstrap interval of 1.03-1.21 (t = 2.7),
- 25 wins (62%).

Its genes: aggressiveness 50, max_routes 30, no cash reserve, no borrowing for investment,
catchment_stops 2, invest_air_cap 6, invest_road_cap 14, invest_ppv_div 5, rail_twin_max 2,
air_level_max 10, air_max_tiles 480, the in-bot steward on.

An 80% win share is out of reach for a steady +10-15% gain in a five-AI field, so promotion now
also accepts at least 30 pairs whose interval starts above 1.0 (`battle.py promotions`).

This deviates from the advisory rule `project.a-tuning-claim-needs-held-start-pairs`: no
held-start duel was run. The held-start pairs of the earlier defaults could not resolve effects
under ~20%, so the 40-seed paired battle is the stronger evidence here. The former defaults
play on as the anchor `PrismaticAI pre-elite`, so the switch keeps being re-measured.

**The late cash pile is not the gap (2026-11-19, night).** Every PrismaticAI configuration's fleet
stopped growing at 40-80 vehicles from about 1970, with cash making up ~97% of company value,
while AAAHogEx grew from 228 to 434 vehicles. A 20-year replay of the default (seed 930967890)
showed why: after 3 air routes the autopilot found no fresh town pair, because other companies
fly from every big town and towns with our airport were excluded.

Two settings, both GA genes and off by default, lift the cap:
- `air_reuse`: new routes may fly from our airports, as hubs.
- `air_grow_money`: one more air route per that much idle money.

Together they roughly tripled the fleet (50 planes against 18) but lowered value on all three
seeds, by 5-22%. Planes bought late do not pay back before the match ends, and they count
depreciated, below the cash spent on them. `air_reuse` alone: +15% on one seed, no change on
two. The gap to AAAHogEx opens early (income compounding), not in the idle cash of the last
decade.

Noise: the same default replay of seed 31337 ended at 18.2M before an unrelated script change
and at 11.6M after it, so one seed alone cannot decide anything.

**Airport levelling never levelled the whole site (2026-11-20).** `BuildAirportNear` passed
`AITile.LevelTiles` the far corner (x+w-1, y+h-1). Level-land works on tile corners: `TileHeight`
is a tile's north corner (`terraform_cmd.cpp` `CmdLevelLand`), so a w x h airport needs the corners
up to (x+w, y+h). The far edge stayed sloped, and every levelled site was paid for and then
refused for ERR_FLAT_LAND_REQUIRED.

What it cost in two arena matches:
- Seed 113258830: `g71.brainExplore` levelled nine sites for ~150k and went bankrupt in 1961
  without a vehicle.
- Seed 756662687: `g74.3` never had an airport and ended with 13 vehicles against 456.

The fix:
- levels the right rectangle;
- levels only when the test pass levels it fully (a test that fails part-way prices only part);
- keeps funds for two airports and a plane after levelling, with a fourfold margin, because
  the test pass underprices: 172k real against 53k estimated on one site.

Correct levelling costs more, so the GA's air_level_max of 10, fitted to the cheap broken
levelling, had become too low. Default air_level_max over 20 years:

| Seed | 10 | 20 | 35 |
|---|---|---|---|
| 930967890 | 4.6M | 12.8M | 13.4M |
| 1516196607 | 19.1M | 20.6M | 19.0M |
| 31337 | 15.0M | 15.0M | 16.5M |

The default is 35. Replays on the two arena seeds: `g71.brainExplore` survives at 4.4M, and
`g74.3` ends with 41 planes, 105 vehicles and 7.6M.

**When both openers fail, the bot borrows for its road routes (2026-11-20).** On seed 988774025
(company 3), `g78.brainExplore` failed to build any airport or rail line. `RepaySurplus` then
paid the loan down to twice the reserve after every heartbeat, and a road route is built only
from bank money above the reserve. The company kept 3 buses on about 1000 in the bank, with
100000 of loan unused, and went bankrupt.

Now a company with no air or rail route and fewer than 20 vehicles borrows up to its reserve
plus 30000 for the next road route. Anything unspent is repaid at the end of the heartbeat. The
replay survives (14 vehicles). The default's 20-year values on seeds 930967890, 31337 and
1516196607 are identical to before, because the guard never fires for it.

**Two money leaks, now gated by genes (2026-11-20).** On seed 1439892342 (company 4), GA
`g91.3` ended at value 1 after five years, holding the whole loan. Two leaks drained it:
- Own freight lines failed one after another: the stations had no way out ("1 nodes"), and
  each attempt still cost its stations and part of its track.
- Five levelled airport sites were refused anyway. A corner shared with a house or road does
  not move, so even correct levelling can leave a site sloped.

Two settings, off by default and GA genes, now limit these:
- `own_fail_pause`: no more own lines for the game year after that many failures in a row.
- `air_level_once`: an airport search levels nothing more after one levelled site was refused.

With both on, the replay ends at 294k instead of 266k. On three default seeds the effect could
not be told from noise: one seed fell from 13.4M to 8.7M, while the others moved by -10% and
-3%. So the tournament decides.

**Levelling only shallow sites (2026-11-20).** On seed 1787809485 (company 2) the default itself
ended last with a value of 1. It levelled two airport sites and both were refused:
- the first cost 59120 against an estimate of 31435;
- the second cost 152982 against an estimate of 32604.

A big height step makes level-land move corners outside the site as well, and houses and roads
there stop it. `air_level_step` (default 2, a GA gene) now levels only sites whose corners lie
within that many height steps. Results over 20 years:

| Seed | Step 1 | Step 2 | Step 8 (the old behaviour) |
|---|---|---|---|
| 1787809485 | 9.9M | 9.9M | value 1 |
| 930967890 | 5.0M | 8.0M | 13.4M |
| 31337 | 14.7M | 15.8M | 15.8M |

Step 2 avoided the last place but cost 40% on seed 930967890, so the step is not monotone
across maps. The default is now adaptive (`air_level_step` 0): any step until a levelled site is
refused in the game, then 1. It ended at 8.7M, 12.0M and 17.2M on the same three seeds, better
than step 2 on each.

A GA mutation went truly bankrupt in edition b8b04ae2 (seed 289036769, g145.3). The opener bought
six planes on a 100k start and kept 6k. With `invest_borrow=0` and an air route, no borrowing path
fired, so the balance stayed negative quarter after quarter until 1962-04. Two guards now hold
independent of genes. `StayAfloat` borrows back to a quarter of the fleet's running costs whenever
the balance falls below it, and `RepaySurplus` never repays below that floor. `BuildAirRoute`
stops buying planes once the funds left would not run them for half a year. Replayed for 20
years, g145.3 survives at 1.06M. The default meets the floor twice in that game.

The guard cannot rescue a company that has already borrowed its whole loan. In edition 6f7457c4,
g152.3 (seed 464398984, `air_level_max=5`, `air_planes=6`, `aggressiveness=100`) went bankrupt
with the guard in place. Replayed in its start slot, it kept the loan at 270k to 290k of a 300k
maximum. It had six planes and about fifteen bus routes, all built on borrowed money, and an income
that never covered them. The guard borrowed twice, then had no headroom left. That failure belongs
to the gene combination, which the GA selects out, not to a missing borrowing path. The default
has no bankruptcy in the editions since the guard.

A GA candidate can leave the population before the interval rule has enough pairs to decide it.
When it does, it plays on as a trial contestant under its exact settings string, and its pairs
pool, because pairing is keyed by settings. g150.0 became the default that way (2026-11-20): 36
pairs, ratio 1.154, interval low end 1.046. It flies more routes on shallow levelling and builds
fewer, better-funded road routes.

# How to apply it

- Do not read PrismaticAI's battle results as what the bot does when the panel steers it. The
  default and solo-max contestants measure the bot without its steward. `PrismaticAI self-steward`
  measures the steward's policy run inside the bot.
- A change to `lines.nut`, `fleet.nut`, `steward.nut` or the own-lines loop in `main.nut` must
  pass `tools/research/ai-battle/battle.py fitness`. `tools/gate.sh change` runs it on such a
  change, and it fails on a collapsing fleet of the contestant it holds healthy.
- During a run, read the dashboard's alerts or a record's `anomalies`. A `fleet_drop` on a
  contestant is the signature of this fact.
