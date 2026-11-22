---
schema: knowledge/v1
id: levelling-an-airport-site-clears-trees-and-costs-town-rating
kind: knowledge
class: fact
title: "Levelling an airport site clears its tree tiles, each costs town rating, and neither a guard nor replanting paid off"
description: "Before changing how PrismaticAI levels airport sites: why a levelled site can be refused with ERR_LOCAL_AUTHORITY_REFUSES, and the measured result of skipping such sites."
status: verified
epistemics: observed
date: 2026-11-22
tags: [prismatic-ai, ai-battle, airport, terraform, town-rating, negative-result]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/prismatic-ai/PrismaticAI/air.nut
    - file:src/terraform_cmd.cpp
    - file:src/tree_cmd.cpp
    - file:src/town_cmd.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/a-bot-change-that-moves-money-in-every-game-replays-as-chaos.md
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/prismatic-ai-without-a-steward-keeps-one-train-per-line.md
  - type: relates_to
    target: file:.ai/repo/workflows/ai-battle-last-place-diagnosis.md
---

# Fact

`CmdTerraformLand` (src/terraform_cmd.cpp) clears every tile around a moved corner through the
tile's terraform proc, and a tree tile's proc is `LandscapeClear`; `ClearTile_Trees`
(src/tree_cmd.cpp) then changes the closest town's rating by `RATING_TREE_DOWN_STEP` (-35) per
tile. A new station needs a rating above `RATING_VERYPOOR` (-200,
`CheckIfAuthorityAllowsNewStation`, src/town_cmd.cpp). From the initial 500, about 20 cleared
tree tiles make the town refuse the airport whose site was just levelled and paid for. In the
replay logs kept on 2026-11-22, 52 of 146 levelled-and-refused sites were refused with
`ERR_LOCAL_AUTHORITY_REFUSES`.

An AI cannot read the exact rating: `AITown.GetDetailedRating` is game-script only (`@api -ai`),
and `AITown.GetRating` gives the band (APPALLING up to -400, VERY_POOR up to -200, ...;
NONE means the initial 500).

A guard tried on 2026-11-22 counted the tree tiles with a corner inside the site at another
height than the level and skipped the site when the band's lowest value minus 35 per tree tile
reached -200. Over 12 `ablate --match` replays (the 8 of the chaos record plus 1425f2c3 m-0004,
m-0014, 99064377 m-0051, c55320eb m-0009) it changed 4 games and lost in all 4: -26.8%, -4.9%,
-6.2%, -65.8% (799899b1 m-0039, 13.7M to 4.7M), where it skipped 33 sites in a game whose
baseline had one authority refusal. Skipping a site also leaves `air_level_once` unspent, so the
search goes on to other sites and the game takes another path. It was not committed.

The same diagnosis (1425f2c3 m-0015, g318.brainSafe, value 2.58M with no aircraft) found that
`LevelKeeping` (8c8d025388) opened its own `AIAccounting`; `ScriptAccounting`'s destructor
restores the outer count, so the caller logged "levelled ... for 0". The function now takes the
caller's accounting; the replay is identical to the unit and logs 16947 and 3683.

# How to apply it

- Read a "levelled ... and the airport was still refused: ERR_LOCAL_AUTHORITY_REFUSES" line as
  tree clearing, not as a bad site; the town refuses every other site of that search too.
- Do not reintroduce a tree-count guard without a better model: the band's lower bound is up to
  200 points pessimistic and the guard changes the search path in many games.
- Replanting after the refusal was tried next (2026-11-22): a band of trees three tiles wide
  around the levelled site (`AITile.PlantTreeRectangle`, +7 rating per tile up to 220), then one
  more `BuildAirport`. It worked as a mechanism (5 of the 12 replays planted, for 1500 to 9390,
  and 4 then built the airport), but the games came out -21.0%, +0.2%, -34.2%, +14.5%, -24.2%
  (-6.4% geometric over the 12), and on 1425f2c3 m-0015 2.27M against 2.58M with 4 aircraft
  instead of none. The airports a forested town refuses are not worth winning back; m-0015's
  weakness is not its missing aircraft. A second sample of 30 other matches (random from the
  2026-11-22 editions, all on 8c8d025388) changed 9 games, 5 up and 4 down (-18.6% to +22.3%),
  0.0% geometric over the 30: over 42 replays replanting is neutral to slightly negative, so it is
  not committed.
