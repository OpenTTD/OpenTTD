# PrismaticAI: the owner's company AI

**RESEARCH-ONLY.** A Squirrel company AI that the owner's planner and the Rhai scripts steer from
outside the game. It builds bus, intercity road, air, freight rail and passenger rail routes from
its settings. It now lives here, beside the panel, planner and scripts that drive it. Until
2026-11-18 it lived in the bridge repository (`~/dev/openttd-prismatic-bridge/ai/PrismaticAI`,
moved at that repository's `19c2cbe`, whose history keeps the earlier commits).

It is content, not engine code: OpenTTD loads it from the `ai/` folder of its personal directory,
like any AI from BaNaNaS. No CMake option builds or ships it, so no default or release build
contains it (rule `project.research-code-is-gated-and-labelled`). It also stays out of `bin/ai/`,
the folder upstream ships with every build.

## Install

```bash
tools/research/prismatic-ai/install          # ~/Documents/OpenTTD/ai/PrismaticAI -> this checkout
tools/research/prismatic-ai/install --check  # exit 0 when the game loads this checkout's bot
```

The personal folder gets a symlink, never a copy, so the next `start_ai PrismaticAI` loads an
edit made here. A real folder found there is moved aside to `ai-backup/PrismaticAI.<time>`, outside
`ai/`, because the game scans every folder of `ai/` and a backup left there is a second PrismaticAI. Run the
script from another worktree to point the game at that worktree's bot. `--personal DIR` covers
another personal directory: Linux uses `$XDG_DATA_HOME/openttd`, and a test run can use a scratch
`HOME`.

## Files

| File | What it does |
|---|---|
| `info.nut` | Name, API version and every setting. All settings are `CONFIG_INGAME`, so a running bot takes a change at once (`openttd.ai.set_setting`, gated by the owner's approval) |
| `main.nut` | The heartbeat: the planner's air, freight and passenger lines first, then the bot's own lines, buses, fleet, line reconciliation and loan |
| `rail.nut` | Rail lines: termini with a throat, the A* line search, laying, the walk check, depots, trains, platform length |
| `lines.nut` | Rail lines as desired state: line slots, the record the Rhai steward reads (with station ids), and the reconciliation of its targets; closing removes only what the line owns |
| `fleet.nut` | Road and air capacity follows waiting cargo and the link graph's planned flow; vehicles that lose money are sold |
| `roadstop.nut`, `intercity.nut`, `air.nut` | Bus stops and depots facing their road, intercity roads tile by tile, airports and aircraft |

## Rail lines a train can always run

Full rationale: [`research/rail-steward.md`](../../../research/rail-steward.md), knowledge
`rail-lines-a-train-can-turn-on`.

- **No 90-degree turn anywhere.** `pf.forbid_90_deg` is on by default (`src/table/settings/pathfinding_settings.ini`).
  `Turns90` rejects two corners in a row that turn the same way: in the search, on the first tile
  beyond each throat, on the last tile into it, and across a re-plan. The line also runs straight
  through each throat's first tile: the second platform joins by a sideways move, so a corner
  there would be a 90-degree turn on its route only.
- **Termini with two platforms** (`rail_platforms`, default 2). A two-tile throat merges the
  platforms into the single line, with a path signal on each platform's exit. A train leaves only
  when it can reserve its way into a free platform at the other end, so up to `trains_max`
  trains share one track without a passing loop.
- **The laid line is walked** (`VerifyRailLine`) before any train is bought. A missing piece is
  laid again, and a line that stays broken gets no train.
- **Every wagon is checked.** A wagon the game will not refit or couple (a NewGRF rule) is sold
  again (`AddWagon`), and a train that would carry nothing is not started.
- **The depot stands behind a platform's dead end.** If that fails, it goes beside a straight
  piece of the line, never on a corner.
- **Length follows demand** (`PlatformPlan`). Platform length and wagons come from the line's
  monthly supply, its distance and the locomotive's speed. They are capped by
  `vehicle.max_train_length` and `station.station_spread`, unless `rail_len` or `rail_wagons`
  override them.

## Steering from outside

| Setting | Set by | Meaning |
|---|---|---|
| `rail_a`, `rail_b`; `fr_*` … `fr16_*`; `air_a`, `air_b` | the panel, from the planner's portfolio | the lines to build |
| `own_lines`, `line_budget`, `rail_after` | the panel | how far the bot plans on its own, and when |
| `rail_platforms`, `rail_len`, `rail_wagons` | the owner, a duel's `--bot-settings` | the shape of new rail lines (0: derive) |
| `disclose_name` | the owner, for a real game against people | 1 appends " (AI)" to the company's name ([`research/real-game.md`](../../../research/real-game.md)) |
| `line<N>_trains`, `line<N>_wagons` (24 slots) | `tools/research/mcp-panel/rhai/rail_steward.rhai` | the target of rail line N: trains (0 closes it), wagons per train |

Every finished line logs a record once at build time and again every game month:

```text
PrismaticAI: rail line 3: label=COAL A Mine - B Power Station|a=26106|b=53365|st_a=0|st_b=1|cargo=1|platforms=2|len=3|depot=26109|trains_max=2|wagons_fit=5|tiles=233|state=open
```

`tools/research/mcp-panel/rhai/rail_lib.rhai` (`parse_record`) reads it. The steward and the
monitor share that reading.

## Checks

- `tools/research/mcp-duel --months 12 --seed 4242` plays a game with this bot. Run it against a
  scratch `HOME` whose `ai/PrismaticAI` points here, so other sessions' games keep their bot.
- `cargo test` in `tools/research/mcp-rhai` runs the Rhai side offline (`rail_*` tests).
