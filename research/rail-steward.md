# Rail lines that pay: geometry, a steward and a monitor

**RESEARCH-ONLY.** How the owner's bot builds rail that trains can always run, and what keeps
each line paying afterwards. The parts:

- the bot (PrismaticAI, [`tools/research/prismatic-ai/`](../tools/research/prismatic-ai/README.md))
  builds the line;
- the Rhai steward
  ([`rail_steward.rhai`](../tools/research/mcp-panel/rhai/rail_steward.rhai)) decides what each
  line should run;
- the Rhai monitor
  ([`rail_monitor.rhai`](../tools/research/mcp-panel/rhai/rail_monitor.rhai)) reports what is
  wrong to the Majordomus board;
- the panel's Scripts page edits and runs these scripts, and reloads a running script when its
  file changes.

Every number is derived from the game or from the line's own record, and the scripts name no line,
station or engine.

## The problem: a train that cannot turn earns nothing

The owner saw trains that could not turn around and lines that lost money. The game's rules
explain it:

- **90-degree turns are forbidden by default.** `pf.forbid_90_deg` defaults to `true`
  (`src/table/settings/pathfinding_settings.ini`). The track follower then cuts every trackdir that
  crosses the one the train arrives on (`follow_track.hpp`, `TrackdirCrossesTrackdirs`).
  - On a 4-neighbour grid, a 90-degree turn is two corner pieces on adjacent tiles that turn the
    same way: the moves `d1, d2, d3` with `d3 == -d1`.
  - Alternating corners (`d3 == d1`) are a 45-degree diagonal, which is allowed.
  - A line holding one forbidden turn is two dead ends to a train. The train finds no path, gets
    lost, turns at the end of the track, and never reaches its destination.
- **The old search checked this only in the middle of the line.** At the first tile beyond a
  station, the direction "before" was taken as the current one. A turn right after the exit and a
  turn right before the far station were therefore never compared with their neighbours.
- **A re-plan around a refused piece started blind.** It started as if from a straight platform,
  so its first new corner could make a 90-degree turn with the last piece already laid.
- **Depots were joined to the line on any tile, corners included.** The two joining pieces then
  meet the line's own corner at 90 degrees, and a train leaving the depot may take neither way.
- **One platform per station limited a line to one train.** Two trains on one track without
  signals would meet head-on, so capacity could grow only by wagons.

## The geometry the bot builds now

`tools/research/prismatic-ai/PrismaticAI/rail.nut`:

```mermaid
flowchart LR
  subgraph station["terminus (two platforms)"]
    pm["main platform … end_m"]
    ps["second platform … end_s"]
  end
  pm --> t1m["T1m<br/>path signal, facing out"]
  ps --> t1s["T1s<br/>path signal, facing out"]
  t1s --> m2["M2<br/>bend (a diagonal, never 90°)"]
  t1m --> m["M<br/>junction"]
  m2 --> m
  m --> l0["L0<br/>first tile of the line"]
  l0 --> other["single track to the other terminus"]
```

| Rule | Where | Why |
|---|---|---|
| `Turns90(d1, d2, d3)` rejects `d1 != d2 && d2 != d3 && d3 == -d1` | `FindRailPath` on every step, the step one beyond `from` (using `from_end`), the goal step into `to_end`, and a re-plan (`from_in`) | the game's own 90-degree rule; also faster: no 90-degree slowdown where it is allowed |
| Two platforms and a two-tile throat (`rail_platforms`, default 2) | `BuildRailStationAt`, `TerminusGeometry`, `BuildThroat` | The second platform bends onto the main row at `M` as a diagonal, never at 90 degrees. The site check covers platforms and throat together: flat, buildable, and an exit that leads on |
| Straight through each throat's first line tile (`L0`) and into the far one | `FindRailPath` `from_straight` / `to_straight`, `ExitLeadsOn(…, straight)` | The second platform joins the line by a sideways move (M2 to M). A corner on `L0` toward its side would be a 90-degree turn on the second platform's route only, invisible from the main one (found by Codex's independent review, 2026-11-18) |
| A path signal on each platform's exit (`T1m`, `T1s`) | `BuildThroat` | A train leaves only when it can reserve its whole way into a free platform at the other end (a dead end counts as a safe position, `pbs.cpp` `IsSafeWaitingPosition`). It never waits on the single track, so up to `trains_max` = the smaller end's platforms share it with no passing loop |
| The laid line is walked | `VerifyRailLine` (`AIRail.AreTilesConnected`) | A gap shows up as a warning with its tile, never as a lost train. A missing piece is laid once more |
| The depot behind a platform's dead end | `BuildRailDepotBehind` | Trains reach it through their own platform and leave inside the signals. As a fallback, beside a straight piece only (`BuildRailDepotBeside`) |
| Platform length and wagons from demand | `PlatformPlan` | `wagons = ceil(supply × trip_days / 30 / trains / capacity)`, where the trip is both ways at the locomotive's speed (0.036 tiles a day per km/h) plus a week at the stations. The platform holds the locomotive and the wagons at half a tile each, capped by `vehicle.max_train_length` and `station.station_spread`. `BuildTrain` checks the real length (`AIVehicle.GetLength`) |

## The steward: each line as desired state

The bot builds and the steward decides. The bot then reconciles the line to the steward's target
(`lines.nut` `ReconcileLines`).

1. **Record.** Every line logs one record at build time and again every game month:
   `PrismaticAI: rail line <slot>: label=…|a=…|b=…|cargo=…|platforms=…|len=…|depot=…|trains_max=…|wagons_fit=…|tiles=…|state=…`.
   The panel stamps each ai.log event with its company.
2. **Read** (`rail_lib.rhai`). The steward joins the records with `openttd://vehicles` (trains
   whose orders name both stations), `openttd://stations` and `openttd://engines`. Waiting cargo
   comes from the Script API in test mode (`ScriptStation.GetCargoWaitingVia`), which reads and
   changes nothing.
   - Under cargodist it is the cargo routed to the other stop.
   - Under manual distribution every waiting unit's next hop is `STATION_INVALID`, so it is the
     cargo waiting via any station.
   - Line records carry their station ids (`st_a`, `st_b`), so a joined platform inside a
     neighbour's catchment still finds its own station.
3. **Decide** (`target`, thresholds in `policy()`):
   - **close** a line that lost money last year and again this year, once its oldest train is
     past `judge_after_days` (400: profit resets on 1 January);
   - **lengthen** while a trainload or more waits and the platform has room (`wagons_fit`);
   - **add a train** once the trains are full length, up to `trains_max`;
   - **shrink** by one train when almost nothing waits and the newest train lost money.
4. **Choose** (`choose`). Closing and shrinking are always chosen. The rest goes through
   AlgoGlyph's `knapsack-dp` kernel over the money above the bot's `cash_reserve`.
   - Cost is what the change asks for now. A wagon is priced at the line's own wagons
     (`consist_value` less the front vehicle's `value`, from `openttd://vehicles`), and lengthening
     pays for every train of the line. A new train is the locomotive's price now plus its wagons.
   - Gain is the line's own profit per unit of capacity times the capacity added.
   - Without AlgoGlyph the steward chooses greedily by gain per cost, and the answer says which
     solver ran.
5. **Hand over.** A setting already asked for the same value is not asked again while its approval
   is pending, nor within `RETRY_ROUNDS` after the owner left it unapproved. A new value is asked
   at once. Each target is a setting of the bot, `line<slot>_trains` (0 closes the line)
   and `line<slot>_wagons`, set through `openttd.ai.set_setting`. That call waits for the owner's
   approval unless the owner switched auto-approve on (knowledge
   `auto-approve-is-the-owners-switch`).
6. **Reconcile** (the bot, every heartbeat):
   - a train with a different wagon count goes to the depot, is resized there and is started
     again;
   - a missing train is cloned with shared orders;
   - a surplus train is sold;
   - a closing line sells its trains, then removes its track, its own platforms and throats, and
     its depot, so it stops costing maintenance. The removal never touches what is not the line's:
     - a level crossing loses only its rail (`AIRail.RemoveRail`), and the town keeps its road;
     - a station loses only this line's platforms (`AIRail.RemoveRailStationTileRectangle`).
       Demolishing a station tile would remove every platform of the station, a joined line's
       included (`station_cmd.cpp` `RemoveRailStation`);
   - a wagon the game will not refit or couple (a NewGRF rule) is sold again, and a train that
     would carry nothing is not started (`AddWagon`).
7. **Renew.** For each locomotive type the company runs, the steward looks for a buildable one on
   the same rails that is at least `renew_speed_gain` (1.15) times faster. It hands the pair to the
   game's own autoreplace (`ScriptGroup.SetAutoReplace(GROUP_ALL, old, new)` through the Script
   API, gated). The game swaps the engines at the next depot visit, and the company's autorenew
   replaces old ones as before.

`STEWARD_DRY=1` decides and prints without handing anything over. `STEWARD_EVERY_S` sets the
round (default 120 s). A round's decisions are posted to the board as `openttd:<instance>:steward`.

## The monitor: what is wrong, on the board

`rail_monitor.rhai` runs a round every `MONITOR_EVERY_S` (default 60 s) and reports, worst first:

| Kind | From |
|---|---|
| `crashed` | `openttd://vehicles` `crashed` |
| `lost` | `openttd://vehicles` `lost` (`VehicleFlag::PathfinderLost`, the "is lost" news): no way to the destination, a broken line or a forbidden turn |
| `unserved` | a line record with no train |
| `warning` | the bot's rail warnings since the last round: no rail path, a broken line, a refused throat or piece, no depot |
| `broken_down` | `openttd://vehicles` `broken_down` (`breakdown_ctr == 1`, stopped now) |
| `stopped` | a train stopped outside a depot |
| `old` | a train past its maximum age |

The monitor posts to the Majordomus board as the named claim `openttd:<instance>:rail`. It posts
when the report changes, and at least every `MONITOR_REPEAT_S` (600 s) while problems last.
Every agent session sees the post in `majordomus_blackboard` and `majordomus_peers`.
`board_post(claim, text)` is the Rhai side of `tools/research/mcp-rhai/src/board.rs`, the client
`mcp-bus-blackboard` also uses. `MCP_BOARD_DRY=1` prints the post instead of sending it.

## The Scripts page: an editor, services and hot reload

The panel serves `/scripts` and `/scripts/<name>`
([`static/scripts.html`](../tools/research/mcp-panel/static/scripts.html),
[`scripts.js`](../tools/research/mcp-panel/static/scripts.js),
[`src/panel_scripts.rs`](../tools/research/mcp-rhai/src/panel_scripts.rs)). The page is part of
the panel's one shell.

- **Library.** Every `.rhai` file of `$MCP_RHAI_PATH` (default `tools/research/mcp-panel/rhai`),
  with what its own text says:
  - its header comment as documentation;
  - its imports;
  - the scripts that import it, which makes it a library;
  - its service state.
- **Editor.**
  - Rhai highlighting, line numbers and an outline of its functions with their `///` docs.
  - A live parse check that marks the error's line (`POST /api/script/check`).
  - Save takes only text that parses (`POST /api/script/save`). The file is written in place, so
    the repository's diff shows the change and committing stays the owner's.
  - Keys: Ctrl/Cmd+S saves, Ctrl/Cmd+Enter runs, Escape stops a run, Tab indents.
- **Run once.** The editor's text runs in the panel's process, and its output streams to the page
  (`POST /api/rhai/stream`).
- **Service.**
  - The saved script runs in the background (`POST /api/services/start`, `/stop`), and its log
    streams into the page (`GET /api/services/log`).
  - **Hot reload:** a watcher looks every second, and a running service whose file, or any library
    it imports, changed is restarted on the new text. This works for a Save on the page and for an
    edit in any editor. A change that does not parse leaves the old run going and says why in the
    log.
  - A file changed on disk is loaded into the page when the page holds no unsaved edit, and
    offered otherwise.

The routes are declared once (`src/panel.rs` `routes()`), so they are also in `/openapi.json`,
Swagger UI at `/docs`, and every Rhai script as `api_scripts()`, `api_serviceStart(#{name})` and
so on.

## Surfaces that changed

| Surface | Change |
|---|---|
| `openttd://vehicles` | `lost`, `broken_down`, `engine_id` per vehicle |
| `openttd://engines` | for trains: `wagon`, `power_hp`, `railtypes` (labels) |
| Panel HTTP, OpenAPI, Rhai `api_*` | `/api/scripts`, `/api/script`, `/api/script/check`, `/api/script/save`, `/api/services`, `/api/services/start`, `/api/services/stop`, `/api/services/log`; the page `/scripts` |
| Rhai built-ins | `board_post(claim, text)` |
| Bot settings | `rail_platforms`, `rail_len`, `line<N>_trains`, `line<N>_wagons`; `rail_wagons` 0 now means derive |

## What was checked

- **A 12-month duel on seed 4242** (`research/results/duel-20261118T063729-seed4242.md`). The
  bot ran with the new geometry, before the steward and line slots existed.
  - Six rail lines were built, each with two-platform termini at both ends, a throat with path
    signals, and a depot behind a platform.
  - None logged "no rail path", a broken line or a refused throat.
  - At month 12 the planned company had 6 trains and was worth 55,349, against 7,840 for the
    unplanned one.
  - This is one run. A tuning claim needs held-start pairs (rule
    `project.a-tuning-claim-needs-held-start-pairs`), and that A/B against the previous geometry is
    still to run.
- **The bot reconciling its line targets**, in a 12-month duel on seed 4242 with
  `--bot-settings line1_wagons=3,line2_trains=2`
  (`research/results/duel-20261118T071045-seed4242.md`). These are start settings, which are not
  gated, so the owner's approval gate was never bypassed.
  - `openttd://vehicles` read during the run showed line 1's train shortened from 5 wagons to 3
    (4 parts).
  - Line 2 ran two trains on its single track, one moving while the other waited in its platform,
    and neither was lost or crashed.
  - A target above the platform's `wagons_fit` (6 for 5) was capped and caused no resizing loop
    (the run on port 8871).
  - The duel record keeps a selection of the bot's log, not the reconciliation lines: the evidence
    is the game state read during the run.
- **`cargo test` in `tools/research/mcp-rhai`.** The `rail_*` tests run `rail_lib.rhai` on
  fixtures: records, line assembly, every target, the choice within a budget, renewals, and the
  monitor's ranking and digest. The `panel_scripts` tests cover names, docs, imports, the parse
  check, libraries, and a service that hot-reloads on a library edit, keeps running on an edit
  that does not parse, and stops on request.
- **`tools/research/mcp-scripts-check <panel-url>`** drives the Scripts page in a real Chrome.
  On a stack of this worktree (2026-11-18) it passed 16 of 16 checks:
  - the library list and the marked script;
  - highlighting and line numbers;
  - a typed parse error marked on its line, and Save refused with 422, leaving the file untouched;
  - a new script created from the page;
  - an edit on disk loaded into the editor;
  - a service started with its log streaming in, and reloaded by an edit;
  - Run once streaming its output and exit;
  - no page error, and no horizontal scroll at 390 px.
- **The steward and the monitor on a live game** (`STEWARD_DRY=1`, `MCP_BOARD_DRY=1`, the duel's
  panel).
  - Both read every resource and the line records.
  - The young line got no change, as `judge_after_days` says.
  - The monitor's post went to the board as `openttd:panel-<port>:rail` when it ran as a panel
    service.
  - Its first live run found that `openttd://stations` gives a station's `tile` and
    `catchment_area`, not a rectangle of its own. `station_at` now matches the station's own tile
    first, and a test pins that shape.
- **`node --test tools/research/mcp-panel/test/*.test.mjs`.** `scripts.test.mjs` covers:
  - tokens that join back to every library script character for character;
  - token kinds, escaping, the outline, indenting, stream frames and the address;
  - the page against its script.

## Limits

- **Two platforms, not N.** The throat merges exactly two. A third platform needs a longer
  ladder, and `TerminusGeometry` is where it would go.
- **A closed line leaves no gap in the slots.** Its slot is not reused, and lines beyond slot 24
  get no targets.
- **The bot keeps its lines in memory** (`RAIL_LINES`). An AI restart forgets them, and the
  records in the event store are the only trace.
- **The steward's gain is a line's past earnings per capacity.** A line with no profit yet earns
  nothing in the knapsack, so it is lengthened only after a year.
- **The bot's own per-vehicle sell rule still runs** (`fleet.nut`: older than two years and
  losing money both years). The steward's close rule judges the whole line earlier.
