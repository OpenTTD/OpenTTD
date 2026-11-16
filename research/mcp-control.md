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
| `POST /api/bots/setting` `{company, name, value}` | change a live setting through `openttd.ai.set_setting`, approval-gated |

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
`api_plan(#{limit, params})`, and `mcp-plan [--params JSON] [--defaults]`. The planner ranks town
pairs and gives each one a mode (bus, rail or air), plus the five best pairs of each mode so that
a plan can combine modes. Every assumption is a parameter with a default that the caller can
override: distance bands, minimum populations, reach, costs, `cost_weight` and the enabled
`modes`. The answer echoes the parameters it used. The heuristic is v1 and not yet calibrated
against the game's payment formula (see `tools/research/mcp-rhai/src/plan.rs`). On `/world` the
best pairs are drawn on the map in each mode's colour, and a click on a row moves your camera to
the first town.

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
