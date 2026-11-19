---
schema: knowledge/v1
id: the-bots-rail-pathfinder-lays-plain-track-only
kind: knowledge
class: constraint
title: "PrismaticAI's rail pathfinder: land, level crossings, bridges and tunnels, each taken only as the game allows it"
description: "When choosing which planned rail line the owner's bot builds, or reading why a bot company has no trains."
status: verified
epistemics: observed
date: 2026-11-17
tags: [bot, rail, pathfinding, planner, track-planning]
about: tools/research/prismatic-ai/PrismaticAI/rail.nut
route: platform
provenance:
  origin: authored
  derived_from:
    - file:research/results/duel-20261116T074419-seed20208.md
    - file:tools/research/mcp-rhai/src/panel_bot.rs
    - commit:60aef3caa5529bbe769e8c5c423615aefb7d196a
    - file:tools/research/mcp-rhai/src/linecost.rs
    - file:src/rail_cmd.cpp
    - file:research/results/duel-20261116T082722-seed20208.md
    - file:research/results/duel-20261117T012840-seed31337.md
    - file:src/vehicle_base.h
    - file:research/results/duel-20261117T032759-seed31337.md
    - file:research/results/duel-20261117T041705-seed31337.md
    - file:research/results/duel-20261117T045857-seed31337.md
    - file:research/results/duel-20261117T053732-seed31337.md
    - file:research/results/duel-20261117T100202-seed31337.md
    - file:tools/research/mcp-rhai/src/yards.rs
    - file:research/results/duel-20261117T155736-seed31337.md
    - file:research/results/duel-20261117T180225-seed31337.md
---

# Constraint

PrismaticAI's `rail.nut` `FindRailPath` (in this repository since 2026-11-18, `tools/research/prismatic-ai`;
the bridge-repository commits named below are its earlier history) is an A* search over buildable
tiles. A water, road or rail tile ends a branch, and the search never places a bridge, a tunnel or
a level crossing. A planned line whose only routes need one of these fails: the bot builds both
stations, logs "no rail path between the stations; removing them", and falls back to buses.

In the 36-month duel on seed 20208 (2026-11-16) this left the planned company without a train.
AlgoGlyph track-planning had found routes for the same line, but they used bridges and a crossing.

Turns had a second limit, now fixed. A turn was allowed on flat land only, so a station whose exit
and neighbours were all sloped was unreachable even over plain land: a 180-tile coal line on the
same seed failed that way twice. Since bridge repository 2e3f116, `PieceFits` takes the game's own
rule (`GetRailFoundation` with build_on_slopes, `rail_cmd.cpp`): any piece on flat land, a straight
piece on any slope that is not steep, and a corner piece only where
`_valid_tracks_without_foundation` or `_valid_tracks_on_leveled_foundation` allow it. The pieces
on both exits are checked against their stations. Just allowing every slope that is not steep was
wrong: the game refused the first piece with ERR_LAND_SLOPED_WRONG.

Since bridge repository a0f2734 the bot crosses a straight road at right angles on flat land (a
level crossing, never turning on it). It takes a station site only when the exit leads on: on the
same seed, a station between houses had its exit facing a road junction. Since bridge repository d3c39ba the bot also bridges what blocks a straight run two tiles ahead
(water, a road it cannot cross, rail, a valley). The far head is the first land 3 to 11 tiles on
whose bridge the game accepts in test mode (`AITestMode` plus `AIBridge.BuildBridge`). On seed
20208 the passenger line Mefingpool–Sontburg was then built with two bridges. Since d3c39ba's
successor 34bd520 it may also tunnel where the next tile rises in the line's direction (the far
portal from `AITunnel.GetOtherTunnelEnd`, at most 15 tiles, accepted in test mode). No map so far
has needed one, so a built tunnel has not been seen.

Since bridge repository b4e20b5 the search takes **diagonal track**. Costs are in thirds of a straight
piece, the game's own travel measure (`vehicle_base.h`: a straight piece is 16 steps of
TILE_AXIAL_DISTANCE 192, a corner piece 8 steps of 2 × TILE_CORNER_DISTANCE). A corner piece that
continues a diagonal (alternating corners) costs 2 against 3, a turn is paid once where the diagonal
starts, and a U within two tiles is not taken. The estimate must stay above the diagonal cost (7 per
tile left). At 6 every staircase toward the goal had the same f, the search spread over the whole
rectangle between the stations, and lines that the old search found failed. On seed 31337, lines were
48–76% corner pieces, and company value went from 250716 to 454403 on the same plan
(duel-20261117T012840). A failed search logs whether the node budget ran out or the reachable tiles
did. "every reachable tile tried, 1 nodes" means the station exit has no usable neighbour. A failed
piece (ERR_AREA_NOT_CLEAR) is re-planned around since bridge f5f29d5: from the last laid tile, with
the refused tile avoided. It first ran on seed 31337, where a coal line was finished 42 tiles around an
occupied tile instead of being dropped (duel-20261117T100202).

Three later findings on seed 31337 (bridge repository, branch feat/live-behaviour, pushed to aea2cd5):

- **A station exit is checked by the search's own first-step rules** (7984f1c). A turn off the exit
  must fit the exit's slope (PieceFits). Without that check a town station passed the site check, and
  the search then stopped after one node. The passenger line was then built, which had failed on every
  earlier run.
- **Lines are built in order of score** (panel `fr_*` sorted, `rail_after`; bridge 75d114e). The bot
  borrows for each line in the order it is given. A passenger pair scoring 113003 had been built
  second, ahead of freight lines scoring up to 240547. Built best first, the same seven lines ended at
  558331 against 505369 (+10.5%).
- **Two platforms side by side where two planned lines meet did not pay** (3840fc6, 6f78532, both
  reverted). One yard worked (Lentburg); the other was walled in by the first line even with a
  four-tile corridor kept. With a fallback station, the line there lost its train twice. Results were
  −16% and −48% against no yards. Before another try, prove the corridor offline on
  `openttd://map` with the first line's path.
  **That proof exists since 2026-11-17** (`tools/research/mcp-rhai/src/yards.rs`; `mcp-plan --track N --yards N`,
  `GET /api/plan?track=N&yards=N`, the World page's "Prove yards"): the better line's route is laid
  by AlgoGlyph track-planning, its tiles and both stations are blocked (`blocked` in the track input),
  and the other line must still route from a platform one tile beside the first, same axis and exit,
  on buildable flat land. First reading on the live 1957 game of seed 31337's panel: three pairs
  shared a station site, one was proven (a coal mine's yard, side −1, the second line's track 45
  cheaper than alone) and two were refuted because no flat buildable platform fits beside the first.
  A retry of yards in the bot hands over only proven pairs, with the side the proof found.
  The first proven yard was built on 2026-11-17 by the person-like executor, not the bot
  (`mcp-human yard`, seed 4242: two lines into Drundstone from side-by-side platforms, both trains
  running). The executor lays the planned track itself, so the proof holds for it; the bot's own
  pathfinder can still wall a yard in.

- **Lines trains can run, and two platforms per terminus** (2026-11-18, this repository): no
  90-degree turn at either end or across a re-plan, a path-signalled two-platform throat, a depot
  behind a platform, the laid line walked before a train is bought. See
  [[rail-lines-a-train-can-turn-on]] and `research/rail-steward.md`. This supersedes the reverted
  yards above for lines into one site: each line has its own two-platform terminus.

- **More lines when money waits** (panel MAX_FREIGHT_LINES 16; bridge 2c01584). With eight lines the
  loan was repaid by month 30 and 308074 sat idle at month 36. Thirteen lines handed over, eleven
  built in 36 months: 597075, +6.9% over eight lines (spread between two runs of one bot is about 1% one duel at a time; under load it is 10% or more, knowledge duels-run-side-by-side-are-noisy).
  After that, laying track takes the game time, not the plan or the money.

- **Its own lines once the plan is built** (bridge 31fdb68, `own_lines`; the panel passes 8). They cost
  value until about month 40, because company value does not count track. **Judge any late investment on
  a ten-year duel:** a 36-month duel showed the same lines at −20% and −5%. A duel's `bot_settings` (e.g.
  own_lines=8) gives such an A/B without a code change. The first ten-year reading (+10.4% on seed 31337,
  duel-20261117T155736 against T180225) came from before held starts and was noise. Held-start pairs over
  ten years, own_lines=8 against 0:

  | Seed | own_lines=0 | own_lines=8 | Change | Trains (0 / 8) | Duels |
  |---|---|---|---|---|---|
  | 31337 | 4479471 | 4433637 | −1.0% | 14 / 15 | duel-20261117T225145, T234725 |
  | 4242 | 2770405 | 4022718 | +45.2% | 11 / 18 | duel-20261118T011804, T011802 |
  | 20208 | 2824858 | 3690922 | +30.7% | 9 / 15 | duel-20261118T012204, T012206 |

  Own lines pay where the planner hands over few lines (9 or 11 trains), and are neutral where it already
  fills the map (14 trains). The panel's default stays 8 (`OWN_LINES` in panel_bot.rs). I0087's rule that
  derives it from stored duel outcomes (own_lines 0 on seed 31337, 16 on 4242 and 20208) did not beat 8 on
  held-start pairs, and the repeats of those pairs a day later differed by 1-11% per arm. Yesterday's +45% and
  +31% are therefore one draw per seed (knowledge `own-lines-derived-from-stored-duels-did-not-beat-the-constant`).

# How to apply

- Price rail lines with track-planning (`mcp-plan --track N`) before the bot gets one. The panel's
  bot start prices six of each list, hands over any line with a route, and does not hand over an
  unpriced line while others were priced.
- A bot company with T0 in `companies` after a rail plan: look for "no rail path" in its log
  before blaming the planner's scores. To find which rule blocks the line, replay the search
  offline on `openttd://map` and `openttd://map/terrain`, with the bot's own stations put back.
- The bot's own search may still miss a route that track-planning finds: the two searches differ.
  Replay the bot's search offline (above) to see which rule blocks it.
