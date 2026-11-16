# Driving OpenTTD from outside: MCP panel, developer console and Rhai

RESEARCH-ONLY. The embedded MCP server (ADR 0008, `research/prismatic-mcp/`) is the channel;
this is the tooling that drives a running game through it. Everything binds to loopback.

## Use it now

1. Once: `tools/gate.sh research` (the MCP-enabled binary) and
   `cargo build --release` in `tools/research/mcp-rhai`. The base sets (OpenGFX, OpenSFX, OpenMSX)
   go in `~/Documents/OpenTTD/baseset/`, `PrismaticAI` in `~/Documents/OpenTTD/ai/`.
2. `tools/research/mcp-e2e`: the game, MCP, your bot, the window at http://127.0.0.1:8800/ and a
   spectator client. Auto-approve is off: every change waits for your Approve.
3. In the window: **My bot** sets the knobs of the AI that plays for you; **Control** pauses and
   resumes; `~` opens the developer console; **API (OpenAPI)** opens Swagger UI.
4. A whole game played for you: `tools/research/mcp-rhai/target/release/mcp-rhai
   tools/research/mcp-panel/rhai/full_game.rhai`. It is a one-year game (2049), about 13 real
   minutes, with quarterly reports, a mid-game pause that waits for your Approve, and the final
   result.
5. From anything else: REST at `/api/*` (documented in `/openapi.json`), events at
   `/api/events/stream`, Rhai scripts with `import "ttd" as ttd;`.

## One command: the full end-to-end run

```bash
tools/gate.sh research     # once: builds build-gate-research/ with both research options ON
tools/research/mcp-e2e     # server + MCP + PrismaticAI + control window + GUI client
```

`mcp-e2e` starts a headless dedicated server from the MCP-enabled binary on 127.0.0.1:3979 (the
panel owns its console), starts the MCP server and the company AI `PrismaticAI`, opens the control
window at http://127.0.0.1:8800/ and joins an OpenTTD GUI client as a spectator so the game is
visible. Ctrl-C stops all of it. Options: `--auto-approve`, `--no-gui`, `--no-browser`, ports,
`--seed`. Prerequisites: a base graphics set (`research/baseset-prerequisite-macos.md`; OpenSFX
and OpenMSX for sound and music), and `PrismaticAI` in `~/Documents/OpenTTD/ai/` (from
`~/dev/openttd-prismatic-bridge/ai/`).

## The control window (`tools/research/mcp-panel`)

`server.py` (Python standard library) holds the bearer token in memory only, redacts it from the
console log it shows, and serves the Flowbite Pro admin shell in `static/` (CSS built with
`tailwindcss -i input.css -o static/app.css`, Flowbite 4 under `node_modules/`, not committed).

| Surface | What it does |
|---|---|
| Stat cards, companies, decisions | `openttd://game/state`, `openttd://companies`, `openttd://decisions`, every second |
| Pause / Resume | `openttd.game.set_pause` through the approval flow; pending approvals get an Approve button |
| Auto-approve (full auto) | the operator's switch: the panel approves its own pending decisions; off by default |
| MCP catalogue | `resources/list` and `tools/list`; click a resource to read it |
| Developer console (`~`) | OpenTTD console commands, `/mcp <method> [json]`, `/read <uri>`, `/pause`, `/resume`, `/approve <id>`, `/auto on/off`, `/rhai <script>` (every line streamed as the script prints it), `/tail [kinds]` and `/untail` (the live event feed). Output is highlighted by content: errors, warnings, passes, events, debug lines, and JSON pretty-printed with coloured keys and values |

Loopback HTTP API, for any external controller (scripts, agents, Prismatic):

| Endpoint | Body | Answer |
|---|---|---|
| `GET /api/status` | | server and MCP up, pending approvals, auto-approve, events |
| `GET /api/overview` | | state, companies, decisions |
| `GET /api/catalogue` | | `initialize`, `resources/list`, `tools/list` |
| `GET /api/resource?uri=<uri>` | | one resource's payload |
| `POST /api/rpc` | `{"method", "params"}` | raw MCP JSON-RPC with the panel's token |
| `POST /api/console` | `{"command"}` | one OpenTTD console line and its output |
| `POST /api/pause` | `{"pause": bool}` | the approval-gated pause |
| `POST /api/approve` | `{"id"}` | the operator's approval, then the resubmission |
| `POST /api/auto-approve` | `{"on": bool}` | |
| `POST /api/rhai` | `{"script"}` | a Rhai script's output |
| `POST /api/rhai/stream` | `{"script"}` | the same, streamed line by line (SSE `out`, `err`, `exit`) |

A POST with a foreign `Origin` is refused.

### One declaration, every surface: OpenAPI, Swagger UI and derived routes

The routes above are declared once, in `make_routes()` of `server.py`. The router dispatches from
that table, and `GET /openapi.json` (OpenAPI 3.1) documents it. Swagger UI, served from the local
`swagger-ui-dist`, is at `GET /docs`. On every request for the document, the live MCP catalogue is
read again and one route is derived per entry:

| Derived route | From | Body / answer |
|---|---|---|
| `POST /api/tools/<tool name>` | each `tools/list` entry | the tool's `inputSchema`; the tool's JSON answer |
| `GET /api/resources/<path>` | each `resources/list` entry (`openttd://<path>`) | the resource's payload |

A tool the game adds, such as the command-table catalogue of the C++ side, becomes a documented
REST route, a Swagger operation and a Rhai function without a change here.
`tools/research/mcp-panel-check` proves it against a running panel: every tool and resource has its
route, the schemas are carried, the routes answer, Swagger UI is served, and the event feed works.

### Events: SSE and REST over one bus

Every event has a sequence number, a time, a `kind` and `data`, and goes onto one bus:

| Kind | Source |
|---|---|
| `game.observed`, `company.observed` | the first MCP read of a game |
| `game.month`, `game.paused`, `game.resumed`, `game.new_world` | derived from the difference between two MCP reads of `openttd://game/state` |
| `vehicles.count` | the vehicle count changed |
| `company.founded`, `company.removed`, `company.renamed` | the difference between two reads of `openttd://companies` |
| `decision.recorded`, `decision.updated` | the difference between two reads of `openttd://decisions` |
| `ai.log` | AI script log lines (`company`, `level`, `text`), parsed from the server console |
| `console.line` | every other server console line (token redacted) |
| `panel.<kind>` | the panel's own events (operator approvals, console commands, Rhai runs) |

Surfaces: `GET /api/events?since=<seq>&kinds=game,company` (REST; a kind matches itself and its
`kind.` children) and `GET /api/events/stream?since=<seq>&kinds=...` (Server-Sent Events, `id:` is
the sequence number, `event:` the kind). In Rhai: `events(since)`, `events(since, "game,company")`,
and `ttd::wait_event(kind, seconds)`.

## Watching it live, and notifications

- `mcp-e2e` joins the GUI client full screen at the display's size (`--window WxH` for a window),
  as a spectator named `owner-view`.
- `rhai/narrator.rhai` follows the event feed and reports in the running game, as chat from the
  server (`owner-bot`): every route the bot opens or fails to open, its knobs, companies founded or
  renamed, every MCP decision, pause and resume, and a summary at every new month. Run it beside the
  game: `mcp-rhai tools/research/mcp-panel/rhai/narrator.rhai`.
- The panel window shows the same events as toasts, live from `/api/events/stream`, and an
  approval that waits for you stays on screen.
- The GUI client runs its own MCP server: `mcp-e2e` gives it a personal directory of its own
  (OpenTTD reads it from `$HOME`), whose `scripts/autoexec.scr` runs
  `prismatic_mcp start <free port> <token file>`; the token file is mode 0600,
  `{"agent_token", "endpoint"}`.
- `rhai/director.rhai` moves that client like a person would, over its MCP: the camera glides to
  where the bot works along a minimum-jerk path (10t^3 - 15t^4 + 6t^5) with a small deviation that
  peaks mid-way, faster in the middle and slower at both ends; it zooms in at the site, points the
  cursor (`openttd.view.mark`, the marked tile) at every tile the bot surveys or builds, and
  follows each bus the bot starts for a moment. Targets are derived from the bot's log lines
  (`... at tile N`) and the client's `openttd://towns`. `mcp-e2e` starts it with the narrator.
- `PrismaticAI` logs the tile of every build step and waits 20 ticks between steps, so each one is
  seen as it happens. With `intercity=1` (the default) it connects two towns with a road it builds
  tile by tile (A*, round slopes and water), and the director follows the road as it grows.
- `mcp-e2e` supervises the GUI client, the director and the narrator: whichever exits while the
  panel runs is started again, and the director waits out a client that is away instead of
  stopping.
- View tools, GUI processes only, no approval (they change only the local viewport):
  `openttd.view.scroll_to {tile | x,y | town | vehicle, instant}`, `openttd.view.follow {vehicle | null}`,
  `openttd.view.zoom {level}`, `openttd.view.mark {tile | x,y | town | vehicle | clear}` (the bot's
  cursor), `openttd.view.state`. Resource `openttd://towns`.
- Rhai reaches any OpenTTD process's own MCP server: `mcp_call(token_file, tool, args)`,
  `mcp_read(token_file, uri)`.

## The owner's bot

The bot is an AI company that plays for the owner, within knobs the owner sets in the panel's
**My bot** card, over `/api/bots*`, or from Rhai (`ttd::start_bot`, `ttd::set_bot`). The settings
form is derived from the game. It comes from `openttd.ai.list` once the game offers it, and until
then from each AI's `info.nut` (`AddSetting` blocks: name, description, range, default,
`CONFIG_INGAME`).

| Route | What it does |
|---|---|
| `GET /api/bots` | the AIs, their settings, and the AI companies running now |
| `POST /api/bots/start` `{ai, settings}` | start the bot (`openttd.ai.start` when the game has it, else `start_ai`) |
| `GET /api/bots/choices` | what a town, industry or cargo setting may name: towns from `openttd://towns`, industries and cargos from `openttd://world/graph` where the game serves it |
| `POST /api/bots/setting` `{company, name, value}` | change a live setting through `openttd.ai.set_setting`, approval-gated; without that tool it answers `{error, fallback: "start"}` |

`/api/bots` has one shape whatever the source: each setting carries `min`, `max`, `step`,
`default`, `flags` (`boolean`, `in_game`, `developer`), `labels`, `value` and `editable`, and
`running` lists each AI company with the values its script reads now (from `openttd.ai.list`;
from `info.nut` only the defaults). `live` says whether the game can list and set them.

The card draws one control per setting from what the setting declares, never from a list kept by
hand: a `boolean` flag is a toggle, labels are a select, a description naming a `(town id`,
`(industry id` or `(cargo id` is a searchable picker over the live world (a plain number box when
the game cannot list that kind), and any other range is a slider paired with a number box. A
running bot shows the same controls bound to its current values with an Apply each; a change reads
*waiting for approval #N* until the operator approves it, *applied* only once `openttd.ai.list`
reads the new value back, and then offers an undo, which is the reverse change asking again.

`PrismaticAI` (`~/dev/openttd-prismatic-bridge/ai/PrismaticAI`) reads `aggressiveness` (buses per
route), `max_routes` (towns served) and `cash_reserve` (money kept) again at every heartbeat. Its
log lines reach the event feed as `ai.log`, because the panel runs the server with `-d script=4`.

Planned, not yet in the game (C++ side, branch `research/mcp-full-auto`): `openttd.ai.*` tools,
client mode (a network client's MCP server posts commands as that client's company, for network
games), and operator-token approval over MCP. A policy that approves on its own is auto-approve,
and that is the owner's call (knowledge `auto-approve-is-the-owners-switch`).

## Rhai (`tools/research/mcp-rhai`)

`cargo build --release` in `tools/research/mcp-rhai`, then
`mcp-rhai [--panel URL] (<file.rhai> | -e '<script>')`, or `/rhai ...` in the developer console.

- Built-in: `rpc`, `read`, `overview`, `status`, `console`, `pause`, `approve`, `auto_approve`,
  `events`, `sleep`, `to_json`, `tools`, `commands`, `help`.
- Derived at start-up, never listed by hand: one function per MCP tool in `tools/list` (dots
  become underscores: `openttd_game_set_pause(#{pause: true})`), and one per OpenTTD console
  command in `list_cmds` (`con_companies()`, `con_newgame("42")`; 83 today), and one per operation
  of the panel's OpenAPI document (`api_<operationId>`: `api_bots()`, `api_botStart(#{...})`,
  `api_tool_openttd_game_set_pause(#{pause: true})`). A tool, console command or panel route added
  to the game is a Rhai function the next time a script starts.
- `import "ttd" as ttd;` loads `tools/research/mcp-panel/rhai/ttd.rhai` (`MCP_RHAI_PATH` overrides),
  the high-level commands: `state`, `companies`, `wait_running`, `new_game`, `start_ai`,
  `set_pause` (waits for the operator's approval, never approves itself), `report`, `watch`.
- `rhai/demo.rhai` is the end-to-end demo: new world, PrismaticAI plays, watched through MCP,
  paused and resumed through the approval flow, final report.

## Every route works: the generic route check

`tools/research/mcp-route-check [panel-url]` reads the panel's own `/openapi.json` and checks every
operation in it, so a new route, server tool or GUI-client tool is covered the moment it exists.
Nothing is listed by hand.

- Every GET answers 200 with JSON, or PNG for the screenshot.
- Every POST whose schema has required fields answers 400 to an empty body. The panel validates
  each body against the declared schema (required fields, types, enums) before any handler runs,
  for its own routes and for the derived `/api/tools/...` and `/api/client/...` ones, so a bad call
  can never reach the game.
- Every MCP tool annotated `readOnlyHint` (carried into OpenAPI as `x-mcp-annotations`) is called
  and has to succeed.
- Operations that change something and need no input are not called blindly. The report lists them
  by name, so it is clear what was not exercised.

On a dedicated server the `openttd.view.*` tools are not in the catalogue, because it has no window.
`openttd.view.state`, `news.list` and `ask.answers` carry `readOnlyHint`.

## News in the owner's window, both ways

The game's newspapers and information windows are wired to MCP. Anyone can read what the player
was shown and show them something: the owner in the panel, the bot's narrator, a Rhai script,
Prismatic or another AI.

| Surface | What it does |
|---|---|
| `openttd.view.news.list`, `GET /api/news?limit=N`, Rhai `api_news(#{limit: 5})` | the news this client has shown, newest first: text, type (`company_info`, `arrival_company`, `subsidies`, ...), style, date |
| `openttd.view.news.show`, `POST /api/news {text, style?, tile / x,y / town / vehicle}` | a news message in the owner's window. With a target it gets a viewport there (style `thin`); `normal` is a newspaper and `small` an information window. It is local to that client: GUI state, not game state, never sent to the server |
| `openttd.view.news.close`, `POST /api/news/close` | close the news window |
| event `news.shown` (`/api/events?kinds=news`, SSE) | every new item, from the game or from a caller |
| main page, News card | the list, plus a form to send a message with an optional town viewport |
| `narrator.rhai` | every route the bot opens appears as a newspaper with a viewport on its town |

### Every window and widget, generically: the GUI client as a derived surface

The panel derives one route for every tool on the attached GUI client's live `tools/list`:
`POST /api/client/<tool>`, with the tool's `inputSchema` as the body. The routes are in OpenAPI under
"gui client tools" and in Rhai as `api_client_<tool>`. That covers `openttd.ui.windows` (every
window front to back, with each widget's rectangle, text, tooltip and state), `openttd.ui.at`,
`openttd.ui.locate`, `openttd.input.*`, `openttd.view.*`, and any tool the client gains later,
without a line changed here. Window class names come from `src/window_type.h` through
`GET /api/ui/classes`.

`/ui` is a generic mirror of the owner's screen: the live picture, with every window outlined and
named, and every widget a hotspot whose label is its text or tooltip. A click on a widget presses
it in the game through `openttd.input.pointer` and `openttd.input.button`, but only when the owner
armed input: `mcp-e2e --arm-input` sets `PRISMATIC_MCP_INPUT=1` for the GUI client, and it is off
by default. Unarmed, the page only reads, and a click shows what is under it (`openttd.ui.at`).
The news window, the yes/no dialog and every other panel are therefore reachable through the same
generic path as the hand-made `news` and `ask` tools.

### Talking with the owner in the game: company newspapers and questions

| Surface | What it does |
|---|---|
| `POST /api/news {text, style: "company", company, title?}` | the company newspaper with that company's manager face, as in "New transport company launched!". `title` is `launch`, `trouble`, `merger`, `bankrupt` or `none`, taken from the game's own strings. An unknown company is refused |
| `openttd.view.ask`, `POST /api/ask {question, caption?}`, Rhai `ttd::ask_owner(question, seconds)` | a yes/no question in the game's own query dialog. It opens without keyboard focus, so only a deliberate click answers it |
| `openttd.view.ask.answers`, `GET /api/ask`, event `ask.answered` | every question with its answer: `yes`, `no` (also a closed dialog), `replaced` (a newer question took its place), or `null` while it is open |
| News card, right column | the questions and their answers, live |
| `steward.rhai` (supervised by `mcp-e2e`) | every three minutes it offers the planner's best pair you have not answered yet ("Show it to you?"). On yes your camera visits both towns and a newspaper sums it up. Only a real yes or no retires a pair. It asks about allowing more routes only when the game has `openttd.ai.set_setting`, and even then the change waits for your approval in the panel |

## A route planner with modes of transport, every assumption a parameter

`GET /api/plan?limit=N&params=JSON`, Rhai `world_plan(n)`, `world_plan_with(n, #{...})` or
`api_plan(#{limit, params})`, and `mcp-plan [--params JSON] [--defaults]`. `mcp-plan` and `mcp-graph`
take `--panel URL` (the default) or `--token <file>`, the `{agent_token, endpoint}` file that
`prismatic_mcp start` writes. With a token they read the resources straight from that MCP server,
for a stack without a panel such as `mcp-human-e2e`, and give the same output. The planner ranks town
pairs and gives each one a mode (bus, rail or air), plus the five best pairs of each mode so that
a plan can combine modes. Every assumption is a parameter with a default that the caller can
override: distance bands, minimum populations, `speed_kmh`, `overhead_periods`, `supply_factor`,
`payment`, costs, `payback_months` and the enabled `modes`. The answer echoes the parameters it
used. Model v3 takes the game's own formulas: the payment curve, vehicle movement in tiles,
passenger generation and base prices. They are documented with their sources in the knowledge
record `cargo-payment-and-vehicle-speed`. It also uses the vehicles on offer right now from the
resource `openttd://engines`: price, running cost, effective speed, capacity, plane type and who may
buy them. For each mode it takes the best passenger vehicle and picks the fleet size that maximises
the score, within `max_vehicles`, the demand and a `budget` parameter. Each option reports its
transit periods, income per passenger, monthly income and profit, build cost, investment, fleet
and score; `vehicles_from` says whether live vehicles were used. When the panel starts the bot, it
passes the best air pair, budgeted to `difficulty.max_loan`, only if that pair scores above zero,
together with the plane count. On `/world` the
best pairs are drawn on the map in each mode's colour, and a click on a row moves your camera to
the first town.

## The bot plays the plan, and the A/B result

When the panel starts the bot, it asks planner v3 for the best air pair and the best rail pair
(budget `difficulty.max_loan`). It passes the pair to the bot as the settings `air_a` and `air_b` (town ids)
and records the choice as an `ai` event ("planner: air A - B, N tiles, score S"). PrismaticAI
(`ai/PrismaticAI/air.nut` in the bridge repository) flies that pair first:

- each airport goes where it accepts the most passengers over its catchment;
- if the town authority or the land refuses a site, the next one is tried;
- the airport is the biggest type the game offers;
- the planes are the best capacity times speed that fit, and a big plane never goes to a small
  airport.

Rail works the same way. The panel passes the best rail pair as `rail_a` and `rail_b`, only when
planner v3 scores it above zero. `ai/PrismaticAI/rail.nut` then builds it:

- each station's axis points at the other town, on the site that accepts the most passengers, with
  a free exit tile;
- the line is found by A* over buildable tiles: slopes stand on foundations, steep ones are avoided,
  turns happen only on flat land, and a turn costs extra;
- each piece is laid with `BuildRail(prev, tile, next)` into the stations' end tiles;
- the depot stands beside the line and joins it in both directions;
- one train runs on the single track, the fastest locomotive with `rail_wagons` passenger
  carriages, so nothing can deadlock without signals;
- the bot borrows up to the loan limit before it builds, as the planner's budget assumes, and
  removes the stations again when no line can be found.

The first live line, Rindingbury to Senford, is 309 tiles with a Ginzu 'A4'. The bot then goes on
with its bus routes. There is one profitability model, the planner, and the
bot carries it out. `mcp-e2e --no-plan-bot` starts the bot without the pair.

`tools/research/mcp-ab [--months 12] [--seed N] [--label text]` measures the effect. It runs two
headless games from the same seed side by side: A with the planner's pair, B without. Every game
month it records each company's value, money, loan and vehicles from the console's `companies`
command, and it writes `research/results/ab-<time>-seed<N>.{json,md}`. A result is a run that
completed, not a claim.

## Planner v4: freight, and a portfolio chosen by AlgoGlyph

- **Freight pairs.** Every producing industry in `openttd://world/graph` (with last month's
  production) is paired with every industry that accepts its cargo within the freight band. Each
  pair is priced with that cargo's own payment rate and transit periods: `openttd://engines` now
  carries a `cargos` table, the inputs of `GetTransportedGoodsIncome`, and `time_factor_for(t, td1,
  td2)` follows the source for any cargo. Two options are priced, trucks and a freight train,
  using vehicles on offer for that cargo now. Freight travels loaded one way and empty back, so a
  vehicle carries one load per round trip.
- **Portfolio.** From the best option of every passenger and freight candidate, the planner picks
  the set of investments with the highest total score that fits the budget, a 0/1 knapsack with
  money in thousands. When `MCP_ALGOGLYPH_DIR` names a checkout of AlgoGlyph's flow runner,
  AlgoGlyph's `knapsack-dp` kernel solves it. `mcp-e2e` finds one at
  `~/dev/algoglyph-wt/feature/flow-cli` or `~/dev/algoglyph`. Otherwise the same dynamic programme
  runs built in, and a unit test pins it to AlgoGlyph's answer on a known case. The answer names
  the solver, the kernel (id, plate, work) and its input. `/world` shows the portfolio and its
  solver, and a click on an entry moves your camera there.
- **The bot plays the portfolio.** The panel passes the first chosen freight rail pair (`fr_from`,
  `fr_to`, `fr_cargo`), passenger rail pair and air pair. The bot places the freight station where
  the industry produces into it at the mine, and where the cargo is accepted at the power station.
  Its train waits for a full load at the mine. Live: coal by rail from Little Trenhill City Coal
  Mine to Tunfingbridge Power Station, 243 tiles. The portfolio was chosen by AlgoGlyph knapsack-dp
  (3 investments, 293k of 300k).

### Every Rhai output and every plan answer in your ~ console

`openttd.view.console.print {text, level}` (GUI client) prints into the owner's `~` console in a
colour per level: info yellow, warning light blue, error red, debug light brown, rhai green, plan
gold, plain silver. The panel sends every line a Rhai script prints there, through `/api/rhai`
and `/api/rhai/stream` alike: `[rhai] …` in green, errors in red. Answers to `~` plan requests
(`[prismatic-plan] <n> …`) go there in gold. A queue keeps the lines in order, and nothing is lost
when no client is attached.

### Station sites for an executor

Every rail candidate in `/api/plan`, freight and passengers, in the portfolio too, carries
`stations: {from, to}`. Each site is `{tile, x, y, axis, length, end, exit: {x, y, dir}}`, found by
PrismaticAI's own rule (`src/sites.rs` mirrors `rail.nut` `BuildRailStationAt`): the axis points
toward the other end, the rectangle is buildable and flat (all corner heights equal), and the free,
flat exit tile beyond the end faces the other end. `dir` is the side the line leaves by, the input
AlgoGlyph track-planning takes. openttd-07's person-like player lays the track pieces from there:
`mcp-human rail` builds both stations at these sites, plans and lays the line, and walks it
(a 64-piece coal line on seed 31, both sites right first time).

Each site also carries `depot: {x, y, tile, dir}`. That is the free, flat tile beyond the
station's back end, opposite the exit, with the depot's entrance facing the platform. A rail
station connects at both ends of its axis, so a train leaves the depot straight onto the platform.
`dir` is the side the depot opens to, the same as the exit's. Sites with a free depot tile are
preferred, and `depot` is null only when no site nearby has one. On the main stack all 30 sites of
a plan had one.

`tools/research/mcp-duel [--months 24]` is a duel in one game: the planned bot and the same bot
unplanned compete for the same towns and industries on one map, and the higher company value wins.
It writes `research/results/duel-*.{json,md}`.

### What to build: AlgoGlyph transport-investment over the live game

`GET /api/investment?params=JSON` (and `mcp-plan --invest`, `src/invest.rs`) hands the planner's
own candidates to AlgoGlyph's `transport-investment` kernel. Towns and industries become places,
planned pairs become demands (passenger supply × `supply_factor` two-way, freight production one
way, each with its cargo's payment rate and transit periods), and the game's vehicles become modes,
with per-tile and per-end costs and distance bands from the planner's parameters. A mode gives its
vehicles keyed by cargo (`vehicles: {cargo: vehicle}`). One run therefore covers every cargo, and a
two-leg route through a hub is open to freight too. The kernel ranks each demand's routes by return
on investment (`roi = 12 × profit / investment`), and each route's fleet is the count that scores
best, the same rule the planner uses. Each demand enters the planner's knapsack with its
highest-score route, and the kernel's ROI pick stays in `why`. The answer carries the planner's own
portfolio alongside.

The two models agree. On the main stack (2026-11-16, AlgoGlyph `feature/exact-prices`) the
kernel's portfolio was 1.001 of the planner's: the same three coal lines, two of them equal to the
unit. Earlier the kernel sized a fleet to the whole volume, and a map came out at 0.88. That case
went to AlgoGlyph, which adopted the best-score rule and keyed vehicles by cargo. `mcp-view-check`
requires the kernel's portfolio to be at least 95% of the planner's. It may be more, through a
two-leg route the planner does not consider. `mcp-plan --invest-inputs` prints the kernel input, so
a case can be reproduced.

A producer's cargo is counted once. Each freight candidate assumes it carries the producer's whole
production, so the portfolio used to take two lines from one coal mine and count its coal twice.
Now only the best line of each (cargo, producer) enters the knapsack, and the others are listed
under `shadowed`. Passenger pairs are not split that way.

The flow CLI ends with `process.exit()`, which drops whatever a pipe has not yet taken, so a kernel
answer above 64 KiB arrived cut off. Both runners (`cargo::algoglyph` and the panel's action plan)
read the answer from a file instead.

Construction costs follow the game's prices when it offers `openttd://economy/prices`
(`plan::with_prices`). Each default cost is scaled by its `Price` entry's price now divided by its
base price, which covers inflation and the construction-cost setting together. A cost the caller
set is kept, and `params.costs_from_prices` lists the costs that came from the game.

### Line cost on the real map: AlgoGlyph track-planning

`GET /api/plan?track=N` (`mcp-plan --track N`, `src/linecost.rs`) takes the N best rail lines of
each list (freight, passenger rail) and prices each one with AlgoGlyph's `track-planning` on the
game's own map, ways, terrain, prices and bridges, instead of distance × rail tile cost. The input is
the one `hub_algoglyph.rs` `track_input` assembles, read once and shared by every line. The
endpoints are the ones the rail executor lays from (`mcp-human` `plan_track`): each station site's
end tile, leaving by its exit and arriving against the far one's.

- **A line with a route:** its build cost becomes the track's money plus the stations, and its
  investment and score follow.
- **A line without a route:** it loses its rail option, and the candidate falls back to its next
  best mode.

The portfolio is then chosen on the priced lines. Each priced option carries `track` with status,
money, the old estimate, counts (tiles, curves, bridges, inclines, cleared tiles) and pieces. When
the game publishes `openttd://economy/prices` the answer is in game money. Without it, the
kernel's cost is in plain-track tile units and becomes money through `rail_tile`; `money_from`
says which. When AlgoGlyph is present, the panel's bot start prices three lines of each list this
way and reports them in the `~` console. PrismaticAI lays track over land, level crossings and bridges, with no
tunnel (knowledge `the-bots-rail-pathfinder-lays-plain-track-only`). Each bridge is accepted by the
game itself in test mode before it enters the path. So the bot start prices six lines of each list
and gives the bot only lines priced with a route that needs no tunnel. An unpriced
line is not handed over blind, and `~` says why a chosen line was passed over. Without that, a
duel's planned company built no train in 36 months. A station site is taken only when its exit
leads on, the same rule in `sites.rs` and in the bot: ahead or to either side is land or a road.

The bot gets up to four freight lines, `fr_*` and then `fr2_*` to `fr4_*`, in the planner's order and
one per producer. It builds the first at once. Each next one is built when its bank balance plus
loan headroom reach `line_budget` (default 80000). More trains on one single-track line without
signals would deadlock, so idle money goes into more lines. On seed 20208 the bot built the second
coal line (254 tiles) right after the first, at 47703 money and 190000 headroom.

On the main stack (2026-11-16, low construction costs, so `rail_tile` 75 from the prices) the real
lines cost 5-19% more than the straight-line estimate: a 172-tile coal line 15,341 against 12,900,
with 2 bridges, 11 inclines and a level crossing. One passenger pair had no rail route at all and
fell back to air.

### Plan before acting: a DAG, then the presses

`POST /api/actionplan {actions, cash?, loan?, max_loan?, loan_step?, company?}` sends the actions
a plan needs (`{id, kind, label?, cost?, income?, amount?+step?, repeat?, after?:[ids]}`) to
AlgoGlyph's `action-plan` kernel. The kernel orders them as a DAG and expands each one into
executable presses, so "repay 50k" becomes 5 × "repay 10k" as one plan. It tracks cash and loan
after every press and refuses the whole plan (HTTP 422) if it would overdraw or exceed the loan
limit. Cash and loan default to the bot company's, read from the console's `companies` line, and
the loan limit to `difficulty.max_loan`. The main page's Plan card draws the DAG with ECharts
(waves as columns, dependencies as arrows) and lists the presses before anything runs. `mcp-e2e`
looks for the AlgoGlyph runner in the newest stacked branch first (`feature/action-plan`, then
`track-planning`, `flow-cli`, main).

### Steering the plan from the panel

`/world` has the planner's parameters as controls: the budget, the payback horizon in months, and
the enabled modes (bus, rail, air, truck, freight rail). Every change replans at once through
`/api/plan`, and the parameters stay in the address, so a configured plan is a link. Freight modes
take part in the same `modes` filter as the passenger ones.

## The owner's sentences: plans in plain language

`POST /api/intent {text}` (Rhai `api_intent`, `mcp-intent [--panel | --token] "<sentence>"`)
answers a sentence over the live game:

| Sentence | Answer |
|---|---|
| najdi nejproduktivnější les | the forests ordered by last month's production |
| najdi nejbližší les + pilu | forest and sawmill pairs that trade wood, nearest first |
| vystav trať mezi nejproduktivnějším lesem a pilou | the pair from the most productive forest, and the bot settings `fr_from`, `fr_to`, `fr_cargo` that build it |
| vybuduj nejlukrativnější trať | planner v4's candidates by score (freight and passengers), and the settings for the first |
| find the nearest coal mine and power station | the same in English |

None of the nouns is typed by hand. Every industry type and cargo name in every language file of
the game (`src/lang/*.txt`, all case forms such as Czech `.gen` and `.loc`) is joined to the English
name the world graph uses, through its string id. The sentence's language is the file whose names
present in this game match the most distinct words of the sentence. A tie between languages that
share a name ("les" is Czech, Slovak and Slovenian), or a sentence that names nothing ("vybuduj
nejlukrativnější trať"), is decided by the sentence's spelling: character trigram profiles of every
language file's strings, smoothed toward the frequency over all languages. English variants count
as English. The answer says how it decided (`language_from`) and which words each of the three
best languages matched (`language_scores`). Matching uses that language and English, only names
present in this game (industry types on the map, cargos on offer), with stemming for declension and
diacritics folded. A word that sets the action or the ordering is never also a name ("postav", build,
is not "pošta", mail).
The only hand vocabulary is three orderings (nearest, most productive, most profitable) and two
actions (find, build). A build is carried out once its settings reach the bot, through
`openttd.ai.set_setting` (approval-gated) or the next `start_ai`. Without a budget in the request, the
panel adds `difficulty.max_loan`, so the portfolio is solved and its solver is named
(AlgoGlyph knapsack-dp when available).

From the `~` console: openttd-59's verb `plan <text>` prints `[prismatic-plan] <n> <text>`. The
panel's log watcher answers each request number once, into the same console with `echo` lines
prefixed `[prismatic-plan] <n>`: the reading, the three best candidates, and for a build the
settings. The dedicated server runs one stdin line per `select()`, so the panel writes console
lines one at a time, 0.25 s apart (knowledge `dedicated-console-reads-one-buffered-line-per-select`).
`mcp-view-check` proves the round trip.

## A cluster of instances: discovery and calls across it

Every panel registers itself, and the GUI client it attaches, in a shared registry
(`$PRISMATIC_MCP_REGISTRY`, default `~/.local/state/prismatic-mcp/instances/`). It writes one
JSON file per instance, atomically and with mode 0600, and removes it when the panel stops. An
entry whose owning process is gone is dropped on the next discovery.

| Surface | What it does |
|---|---|
| `GET /api/instances`, Rhai `api_instances()` | every registered instance with its role and endpoint, each checked with a live probe (a panel's `/api/status`, a client's MCP `ping`) |
| `POST /api/instances/call {name, method, params}`, Rhai `api_instancesCall(...)` | one MCP JSON-RPC call on any instance: a panel's dedicated server, or a GUI client with its token |
| main page, Cluster card | the same list, live, with a link to each instance's own panel |
| one binary snapshot per stack | `mcp-e2e` copies the binary next to the original (`openttd-e2e-<pid>`) and runs the server and the client from it. A rebuild of the shared build directory then cannot reach a client the supervisor restarts. Before this, the server, still on the old revision, refused every restarted client with "wrong revision" |
| `mcp-e2e --instances N` | N-1 more headless stacks with their own bots; a port that is taken (Majordomus holds 8741, another stack holds 8800) is replaced by a free one |

`mcp-view-check` reads `openttd://game/state` from every live panel through this panel and
expects a 404 for an unknown instance. Majordomus does not discover these instances yet.

## The owner's window in the panel, and clicks that drive it

`mcp-e2e` attaches the GUI client to the panel (`POST /api/view/attach` with the client's token
file) and brings its window to the front. From then on:

| Surface | What it does |
|---|---|
| `GET /api/view/screenshot` | what your game window shows right now, as PNG; the main page and `/world` show it as a live view, about once a second |
| `POST /api/view/tool {name, arguments}` | any tool of the GUI client's own MCP server (`openttd.view.scroll_to`, `mark`, `follow`, `zoom`, `state`, `screenshot`), proxied by the panel |
| `/world`, click a tile or a graph node | your camera goes there (`scroll_to`) and the cursor marks it (`mark`); the director takes the camera back on its next move |

`openttd.view.screenshot` queues the capture on the drawing thread (`QueueQuietScreenshot` in
`src/screenshot.cpp`, RESEARCH-ONLY), writes a temporary file and renames it, and shows no
"screenshot saved" message. A capture taken directly from the game thread crashed the Cocoa client:
the video buffer was unlocked under the drawing thread. `tools/research/mcp-view-check
[panel-url]` checks all of this against a running stack: five screenshots in a row, with the client
still alive afterwards, the proxied view tools, the map, stations and graph, and the OpenAPI entries.

Approvals stay the owner's act: `POST /api/console` refuses `prismatic_mcp approve`, `mcp approve`
and the auto-approve lines (HTTP 403) and records the refusal as an operator event. Approvals go
through `POST /api/approve`, which is the panel's Approve button. Withdrawing your own request
(`deny`) is still allowed. `mcp-panel-check` tests both cases.

## The world as data: map, stations, road graph

| Surface | What it is |
|---|---|
| `openttd://map` (`GET /api/resources/map`) | the whole map: one string per row for tile types (`C R r H T S W V I B O`), heights (two hex digits per tile) and owners (`0-9a-e` companies, `T` town, `N` none, `W` water, `D` deity, `.` not ownable); the encoding is part of the answer; 256x256 is about 266 KB |
| `openttd://stations` | every station: id, name, tile, x/y, owner, facilities, town |
| `openttd://engines` | every engine some company can buy now: type, name, intro year, max and effective speed (aircraft divided by `plane_speed`), capacity, cargo, price, running cost a year, reliability, plane type, the companies that may buy it |
| `openttd://towns` | every town: id, name, tile, x/y, population |
| `GET /api/graph`, Rhai `world_graph()`, `mcp-graph` | the road network as a graph: nodes are junctions, dead ends, stations and each town's nearest road tile, edges are road corridors with their length in tiles, and components say which towns are connected; derived in Rust (`tools/research/mcp-rhai/src/graph.rs`) from the three resources |
| `/world` | the World view: the map on a canvas (types, heights or owners, towns and stations) and the road graph in ECharts on map coordinates, with a component filter, live refresh, maximize and Escape, and every parameter in the address |

A planner reads these and decides. A Rhai script, Prismatic over HTTP, or, next, an algoglyph
kernel (I0041), for routes, profitability and expansion.

## How much of the public C++ API is exposed

`tools/research/mcp-rhai/target/release/mcp-api-coverage --repo . [--panel URL] [--json] [--report]`
derives OpenTTD's public API from the sources (the `Commands` enum, the registered console commands,
the Script API static functions) and checks what the running game exposes through MCP and OpenAPI.
Rule `project.public-api-is-exposed` requires 100% in every dimension. On 2026-11-15 it reported
commands 0/146, console 83/86 and Script API 0/660: the rule is not met yet (issue I0036).

## Limits

- The only game-state change MCP offers is `openttd.game.set_pause`. Everything else a script
  does to the game goes through the server console (`newgame`, `start_ai`, ...), not MCP.
  Typed actions derived from the command table are planned on the C++ side.
- `openttd://companies` returns an empty `name` and no value; `ttd::companies()` fills them from
  the console's `companies` output.
- The verdict of an executed pause is taken on the same tick it was posted, so a network server
  answers `EXECUTED_UNVERIFIED`; `ttd::set_pause` checks the effect itself.
- The panel's console output is the server log since the command was sent, so concurrent
  activity can interleave.
