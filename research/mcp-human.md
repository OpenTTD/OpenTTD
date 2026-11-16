# A person-like player: the interface as data, the mouse as input

RESEARCH-ONLY. The embedded MCP server (ADR 0008, `research/prismatic-mcp/`) runs in every
OpenTTD process: a dedicated server and each GUI client have their own. This document covers
the part that lets a bot play the way a person does, *on screen*. The visible cursor moves with
human motion, buttons are found by what they say, roads are dragged across tiles with the game's
own tools, and the camera glides to the work. The owner watching the client sees exactly what a
person at that mouse would produce, and can take over at any moment.

## Why through the interface

The company AI (`PrismaticAI`) and the decision tools act through the command system directly.
That is correct and governed, but nothing visible happens on the way: a road just appears.
Playing through the interface makes the engine's own UI the API:

- **Watchable.** The cursor, the tile highlight, the toolbar state and the drag are drawn by the
  game, so a recording or a person watching sees a player, not a teleport.
- **Derived, not hand-written.** The bot reads every window, widget, caption, tooltip and
  rectangle from the window system (`openttd.ui.windows`). It finds "Build road infrastructure"
  as a person does, so any window the game opens is usable without a new tool.
- **Honest.** A click becomes a command only where a person's click would. The command reaches
  the server through the network like any client's, under the same rules, costs and errors.

## The surface (GUI processes)

| Tool | Kind | What it does |
|---|---|---|
| `openttd.ui.windows` | read | Every window front to back: class, number, key, caption, rectangle. `widgets: true` adds each widget's type, screen rectangle, resolved text and tooltip, and disabled/lowered state. An edit box reports what was typed into it. A title bar without an index (the station pickers) still gives the caption and its close box. An open dropdown also lists its items (`items`), which are drawn rather than being widgets. `match: "words"` marks each item whose text has those words (`matches`), tested with the dropdown's own text filter. A depot window and a vehicle list (plain, or the groups window the toolbar opens) list the vehicles they draw (`vehicles`: `{vehicle, action, count, rect}`). The cells come from the windows' own hit tests: `drag` picks the whole vehicle up (a train by its front engine), `open` opens its window, `start_stop` is the depot's flag, `open_group` a row of shared orders. |
| `openttd.ui.at` | read | What is under a screen pixel: the window, the widget and, over a viewport, the map tile. |
| `openttd.ui.locate` | read | The screen pixel of a tile's centre in the main viewport, and whether a click there reaches the map (`clickable`). |
| `openttd.input.state` | read | Cursor, buttons, the armed switch, whether the owner holds the mouse or has paused the bot (`owner_paused`), the active placement tool and the tile selection, plus injected-event counters. |
| `openttd.input.yield` | owner | The owner's switch, armed or not: `paused: true` makes every input tool refuse until `paused: false`, and the player waits meanwhile. The toolbar's own menu ("Human player: paused") flips it. |
| `openttd.input.pointer` | armed | Move the visible cursor to a pixel through the driver's input path (hover, drag and tile highlight follow). |
| `openttd.input.button` | armed | Press or release left/right/middle at the cursor, with optional ctrl/shift. |
| `openttd.input.wheel` | armed | Turn the wheel at the cursor. |
| `openttd.input.key` | armed | Press a key as the keyboard driver does (an OpenTTD `WKC_*` keycode; letters are their upper-case code, `WKC_CTRL` is `0x4000`). |

**Programmatic control of any window.** These tools are instant, unlike `mcp-human`'s human
motion. They use the same armed driver path and each answers what changed:

| Tool | What it does |
|---|---|
| `openttd.ui.press` | Find a window by class, number or caption, and a widget in it by index, text or tooltip, or `type` (e.g. `close_box`). Raise the window if it is covered, click, and answer the windows opened and closed and the active tool. |
| `openttd.ui.choose` | Pick an item of the open dropdown by position, by result, or by what it says (`text`). The game applies a dropdown choice a few frames later, so wait for its effect. A toolbar menu that closes on release (finances, companies) needs drag-select: `openttd.ui.press` with `hold: true`, then `ui.choose`, which lets go over the item, as a person drags down a menu. |
| `openttd.ui.type` | Type text into the focused edit box. |
| `openttd.ui.close` | Close a window by its close box. |
| `openttd.ui.click_tile` | Click a map tile when it is on screen. An off-screen tile makes the view jump there and answers `scrolled: true`; call again. Applying the jump at once would redraw on the game thread, which crashed the client when tried. |

Every client surface derives these from `tools/list` without a hand-written entry: Rhai, the
panel's REST/OpenAPI and the WebSocket hub.

A list drawn in a matrix (for example the vehicles in the buy window) also reports
`row_height`, so a person-like controller can point at a row. The title-bar boxes (close,
sticky, shade, default size) carry no widget index. They are found by hit-testing the caption's
row and reported with `"index": null`. The resource
`openttd://vehicles` lists every primary vehicle with its owner, type, engine, tile,
stopped/in-depot/crashed state, cargo, profits and orders. It also gives its age and `max_age`
in calendar days (past `max_age` the game calls it old), reliability in percent, breakdowns since
the last service, and its current value. The player uses it to confirm that what
it did through the windows took effect.

Input feeds the same entry points a video driver calls (`HandleMouseEvents()`,
`HandleKeypress()`), as the local company, on the main thread. A dedicated server, which has no
interface, refuses every UI tool. Like every MCP tool, these are reachable from Rhai (derived
from `tools/list`) and over the panel's derived REST/OpenAPI.

## Coverage: measured by the server, not claimed by the scripts

Every MCP server counts what its clients actually call: each `tools/call` by tool name and each
`resources/read` by URI, in the dispatch itself (`prismatic_mcp_coverage.cpp`).
`openttd://mcp/coverage` compares those counts with the catalogue the server publishes at that
moment (`tools/list`, `resources/list`). It lists every entry with its call count, and the
entries never called.

`tools/research/mcp-coverage-check [--probe] [--report] <token-file>...` works on any instance:

1. **Probe** (`--probe`). It reads every published resource. It also calls every tool annotated
   with MCP's `readOnlyHint` (an annotation that tool owners derive from how each tool is
   handled), using arguments derived from the tool's `inputSchema`.
2. **Gate.** It then fails (exit 10) on any entry nothing exercised.

Nothing in the check names a tool. A tool added tomorrow is uncovered until a scenario uses it.

`mcp-human-e2e --test` runs the gate on both the server and the client after the player has
played. Its governance scenario lists action types, validates a pause, and submits it. The pause
stays pending because nobody approves it. The run then asserts that the game is still running.

## The world as data a planner can trust

Two read-only resources come from the engine's own map, pools and link graph
(`prismatic_mcp_graph.cpp`).

**`openttd://map/ways`** (schema `map-ways/v1`, in `tools/research/schema/map-ways.v1.schema.json`)
describes each tile as hex strings, one per row, plus two lists:

- **Road:** where a road vehicle can leave the tile (NW=1, SW=2, SE=4, NE=8). This covers road
  pieces, drive-through and bay stops, depot entrances, level crossings, and road bridge or
  tunnel heads.
- **Rail:** the tile's rail track bits.
- **Foundations:** the foundation the tile's content stands on (the engine's own value, 0 none),
  so a planner knows where track or road already sits on raised ground.
- **Crossings:** every bridge and tunnel once, with both heads, its length in middle tiles, what
  it carries, its direction and its owner.
- **Occupancy:** each tile with a train, road vehicle or ship on it at the moment of the read, with
  the vehicles' ids. A planner sees which pieces are in use before it rebuilds them.

Two tiles are joined only when each points at the other. Tile types cannot tell you that, which
is how a bus once got lost on a road that "looked connected". The player checks
depot → stop → stop in both directions on these bits before it buys a bus. If the route is
broken it lays the road again once, and refuses the service if it stays broken.

**`openttd://world/graph`** (schema `world-graph/v1`, in
`tools/research/schema/world-graph.v1.schema.json`) is one typed graph.

- **Nodes:** towns (with population), industries (what they produce and accept), stations
  (owner, plus waiting cargo and its rating) and vehicles (position and profit).
- **Edges:** `near_town`, `serves_town`, `order` (in order sequence) and `cargo_link`. The cargo
  links are the link graph's capacity and usage per cargo.

Planners, the shared route planner, Rhai or algoglyph read this one contract instead of
re-deriving it.

## Every instance announces itself

When an MCP server starts, it writes a record of itself to the instance registry. The registry is
`$PRISMATIC_MCP_REGISTRY`, by default `~/.local/state/prismatic-mcp/instances`; the code is
`prismatic_mcp_registry.cpp`. This happens whoever started the process: the control panel,
`mcp-human-e2e`, or a person at a terminal.

- **What a record holds.** Schema `mcp-instance/v1`
  (`tools/research/schema/mcp-instance.v1.schema.json`): one owner-only JSON file with the
  endpoint, the process kind (`server` or `gui`), the owner's pid and the path of the token
  file. It never holds the token itself.
- **When it goes away.** It is removed when the server stops, and on SIGTERM, SIGINT or SIGHUP
  in a process that sets no handler of its own (a GUI or null-driver client): the handler unlinks
  the record and dies of the signal as before. The dedicated driver keeps its own handler, which
  shuts down through the normal path. A record left by a hard kill is stale, and a reader drops
  any record whose owner process is gone.
- **Who reads it.** The panel's `GET /api/instances` reads the same directory, so a cluster of
  instances is discoverable without the launcher knowing about discovery.
- **What it does not hold.** A GUI process's live role (network client, host, single player)
  changes as it plays, so it is not in the record; ask the instance itself
  (`openttd://game/state`).

`mcp-human-e2e --test` runs against an isolated registry. It checks both instances' records:
schema, kind, endpoint, token-file path, no token, 0600 permissions, and an alive owner.

`tools/research/mcp-cluster-check [--servers N]` checks discovery for a cluster (I0046). It starts
N dedicated servers and one null-driver client per server (no window), each with its own MCP
server, against an isolated registry, and checks:

- the registry holds N server records, each with its game port, and N client records, all
  `mcp-instance/v1` with a token file, no token and a live owner;
- every announced endpoint answers with its token, and each client plays a company;
- `mcp-hub` over that registry lists exactly the 2N instances, all reachable;
- a client stopped with SIGTERM withdraws its record;
- a client killed with SIGKILL leaves a stale record, which the hub drops within its 2-second
  refresh.

## Safety: the owner's switch and the owner's hand

- **Armed per process.** Input tools refuse unless the process was started with
  `PRISMATIC_MCP_INPUT=1`. Only the owner's own client gets it, from the launcher. The read
  tools are always available.
- **The person wins.** When the real mouse moves, the cursor position no longer matches what
  the bot left there. Input then pauses for three seconds after the last real movement. The bot
  waits on every kind of input: pointer, buttons, keys and wheel. Grab the mouse and you are
  playing; let go and it carries on.
- **The owner is asked.** Before it starts, and before its first loan, the player asks a yes/no
  question in the game's own dialog (`openttd.view.ask`). With no answer in time it closes the
  dialog itself and records "no answer", not "no".
- **Network rules unchanged.** A client acts only for its own company. The server validates
  every command as it would a person's.

## The player: `tools/research/mcp-human`

A Rust program with no runtime dependencies beyond the client's token file
(`{"agent_token","endpoint"}`, written by `prismatic_mcp start <port> <file>`):

| Module | Role |
|---|---|
| `motion.rs` | Human pointing, seeded. Duration from Fitts' law, `MT = a + b·log2(D/W + 1)`. A minimum-jerk profile along a bowed Bézier path. Overshoot plus correction on long moves. Tremor away from the end point. Steady, watched drags. Think and dwell pauses. |
| `hand.rs` | The hand on the mouse: move, click, drag, find a widget by its text, pick a dropdown item, focus the camera on a tile and spot it on screen without scrolling. |
| `plan.rs` | Road routes: A* over `openttd://map` (clear land, trees, existing roads; no houses, water or steep tiles), with a turn penalty so routes come out as long straight runs. The runs are then cut into drags. |
| `play.rs` | Tasks. **Road:** open the road menu, pick the road type, pick autoroad, drag a route; a run that does not fit on screen is halved. **Stops and depot:** a drive-through bus stop on a straight town road, and a depot facing a road, each placed through its picker. **Bus:** buy one in the depot window (filter the list for "bus", pick the first row, Buy), give it orders with Go To on both stops, and start it. **Service:** all of the above for a pair of towns. **Look around.** |
| `economy.rs` | The company over a whole game (`game <minutes>`). Serve new town pairs while money allows. Add a bus to a service whose buses earn; past its second bus, only while passengers wait at its stops (`waiting_per_bus` per bus, from the world graph). Borrow through the finances window when cash runs short, asking the owner first and only while the bank still lends (`loan` < `max_loan` in `openttd://companies`, which also gives `loan_step`, the 10,000 one press of Borrow or Repay moves). Repay part of the loan through the same window once money is over `--repay-above` (default 150,000), with a margin over the borrowing threshold so it never borrows back what it repaid. Once money is over `--rail-above` (default 120,000), and with the shared planner (`MCP_PLAN`) and AlgoGlyph (`MCP_ALGOGLYPH_DIR`) at hand, build a rail service from the planner's candidates (`rail.rs`), up to two lines of at most 90 tiles. Between jobs, follow a bus with the camera and read the news. Town pairs come from the shared route planner (`mcp-plan --token`, the game's payment formula) when `MCP_PLAN` names it, and nearest neighbours otherwise. |
| `requests.rs` | What the owner asks from the toolbar's own menu (`openttd://ui/requests`, each with the tile under the view's centre). Every game turn first takes the open requests of its kinds, oldest first, and answers each through `openttd.ui.requests.answer`, which the game shows as news: `serve_here` builds a bus service between the two towns nearest that tile, `watch` follows a bus, and `plan_rail` builds the shared planner's rail line whose station is nearest that tile (`rail.rs`). Typed questions (`ask`) belong to the relay. |
| `actions.rs` | A plan first, then its execution (`execute <actions.json>`). The owner's actions (`repay_loan`, `borrow`, `serve_towns {a, b}`, `add_bus`, `watch {seconds}`, `build_rail_line {max_distance, min_distance}`, `renew_vehicle {vehicle}`, `sell_vehicle {vehicle}`, with `after` for order) go with the company's books (`cash`, `loan`, `max_loan`, `loan_step` from `openttd://companies`) to AlgoGlyph's `action-plan` kernel. The kernel validates the DAG and refuses a plan that would overdraw, borrow past the limit or repay a loan already repaid. It expands the plan into steps, one press each, with the cash after every step and a graph to draw. "Repay 50,000" becomes five presses of Repay. The player prints the plan as JSON (the Plan card draws its graph), shows its summary in the game, and does the steps in order through the interface. A plan with a kind the player cannot do yet is refused before anything runs, so a plan never half-runs. |
| `fleet.rs` | Keeping the fleet young and paying (`renew <vehicle>`, `sell <vehicle>`, `fleet`). A vehicle is due for renewal at its own `max_age`, or from `--renew-age DAYS`. It is due for sale when it lost money last year and so far this year and is at least two years old. The player opens the company's vehicle list from the toolbar, clicks the vehicle's row, presses Send to Depot, and waits until it stands stopped inside. For a renewal it then presses Buy a Copy in the vehicle's window (the copy gets the old orders) and starts the copy. Last, it opens the depot and drags the old vehicle onto the sell button (a train onto the sell-whole-train button). The game loop renews one due vehicle per turn when the money is there for a copy. It sells a losing one either way, and a sold bus no longer counts for its service. |
| `rail.rs` | A rail line the way a person builds one (`rail [<max distance>]`). The shared planner's rail candidates (`MCP_PLAN`, modes `rail` and `freight_rail`) carry a station site per end ({x, y, axis, length, end, exit}), shortest first. AlgoGlyph's `track-planning` kernel plans from one station's end tile, leaving by its exit, into the other's, entered from its exit side. Its input is the game's own `openttd://map`, `map/ways`, `map/terrain`, `economy/prices` and `bridges`, verbatim, as the hub's `algoglyph.track_plan` assembles it. Both stations are built through the station picker: orientation, one platform and the length are chosen by their order under a shared tooltip, because the picker reports no caption. The track goes down with the game's own tools: straight X/Y runs are dragged, half-tile pieces clicked on their half (above the centre is the upper track, as `viewport.cpp` decides), with a target small enough that the aim cannot cross halves. Every piece is then checked against the game's rail bits in `openttd://map/ways`, missing ones are laid once more, and the line fails if any is still missing or a walk over those bits does not join the two stations. Then comes a train. The depot is built behind the first station, facing its platform, at the planner's depot site when it gives one. In the depot's shop the player buys the engine and wagons the planner priced the line with ("Engine + N x Wagon"), filtering the list by name. The game couples each wagon to the lone engine in the depot. The train gets orders to both stations and is started, each step checked on `openttd://vehicles` (`parts`, `consist_capacity`). The shop is brought to the front before it is used, because the new engine's window opens over it. Bridges and tunnels in the plan (two head tiles each) are built with their own tools. A bridge is a drag from head to head, then the first bridge in the game's list. A tunnel is a click on its entrance; the game digs to the far side. Each is checked in the `crossings` layer, and both connectivity walks, the player's and the e2e's, cross from head to head (the tiles between carry no rail bits). `rail <max> [<min>]`, or the e2e's `--rail "MAX MIN"`, picks a line by distance. |
| Telling the owner | A company newspaper in the game after each new service (`openttd.view.news.show`), then a screenshot. The tile about to be worked on is marked (`openttd.view.mark`). |
| Screen manners | Clicks are scoped to the window they belong to. A covered window is first brought to the front by its title bar. A covered tile is revealed by shifting the view. Placement starts from a clean hand (Escape). Windows are closed by their close boxes after each job and after any failure. |

```bash
mcp-human --token <client-token.json> check                  # tools present, input armed
mcp-human --token <client-token.json> windows                # the interface as data
mcp-human --token <client-token.json> click-widget "zoom in" # find by what it says, click
mcp-human --token <client-token.json> road 125 52 90 60      # plan, then drag the runs
mcp-human --token <client-token.json> serve 6 12             # road, two stops, a depot, a bus
mcp-human --token <client-token.json> --seed 7 play 3        # look around, serve town pairs
```

`--speed 2` plays twice as fast; the same `--seed` moves the same way.

**One motion model, two implementations.** `mcp-human golden` prints the motion model's golden
vectors, committed as `tools/research/mcp-human/golden/human-pointing.v1.json`: 39 cases across
distances 0–2000 px, target widths 4–80, both overshoot branches and drags, with every sample.
A unit test fails when the model drifts from the file. AlgoGlyph ports the model as its
`human-pointing` kernel, with the same xorshift64* draws and the same rounding, and checks it
against the same file. With `MCP_ALGOGLYPH_DIR` naming an AlgoGlyph checkout that has the flow CLI,
the player (`algoglyph.rs`) runs `npm run --silent flow -- serve` once and takes every pointer
movement from the kernel, one JSON line per move. Each move draws a fresh seed from the player's own
stream, so the run stays reproducible. Without the checkout, with non-default model constants
(`--speed`), or after any kernel failure (said once in the log), the builtin model moves instead,
bit for bit the same. `cargo test` checks the live kernel against every golden case when
`MCP_ALGOGLYPH_DIR` is set, and skips that check otherwise.

## End to end

```bash
tools/gate.sh research                 # builds build-gate-research/ with both options ON
tools/research/mcp-human-e2e           # server + client (own MCPs), the human plays, stays up
tools/research/mcp-human-e2e --test    # ...and asserts on the server's map, exit 0/1
tools/research/mcp-human-e2e --minutes 30 --front   # a long game, its window raised to watch
```

The test checks the outcome on the **server**, which is authoritative. The client's new company
must own new road tiles, two bus stations and a running bus with two orders. The test also checks
that the cursor moved, that the client offers every UI and input tool, and that the server
refuses them.

Verified 2026-11-16 (build of `feature/human-ui-play` on top of the cluster and news commits,
both research options ON, macOS), with `--rounds 2`:

- The client joined as company 0 and served Patborough–Cadston and Sunway–Quardstone entirely
  through the interface.
- On the server: 77 road tiles, 6 bus stations, and 2 buses running with two orders each.
- 4521 pointer updates. All checks PASS.
- Three pairs were skipped and logged: a depot window that did not open, and two road runs whose
  ends stayed unclickable.

**What a person does when the screen is in the way.** A window that stays in front of the one the
player needs is closed. An edit box is cleared with its own clear button before typing, then read
back, because typing lands at the caret, which may sit before old text. A station's name sign can
cover a depot completely; the game hits signs before the ground, so no point on the depot is
clickable. The player then hides station names from the options menu, picking the entry by its
words ("station names displayed"), clicks the depot, and shows the names again.

**The owner reads along.** Besides the log, the player prints its decisions in the game's own `~`
console (`openttd.view.console.print`, level `plan`): each turn's money, loan and fleet, every
borrow and repay, each finished service, plans and their outcome, and its answers to the menu's
requests. Single clicks stay in the log.

## Limits and next steps

- **Economy loop.** The loop serves new pairs and adds buses to services that earn. It borrows
  only after asking the owner, and reuses its own stops and depots instead of building next to
  them. Verified with `--borrow-below 150000`: the player asked, got no answer, closed the
  question, then borrowed three times through the finances window (100,000 → 129,808). A 30-minute
  game ran short of cash on its own. Repaying is verified with `--repay-above 0`: the loan fell
  from 100,000 to 40,000 one press at a time, and the e2e reads that from the server's
  `openttd://companies`, not from the player's log.
- **Vehicle replacement.** `--fleet` (e2e) renews the company's first bus through the windows
  and then sells the copy. Verified 2026-11-16 with `--rounds 1 --fleet`, all 36 checks PASS. The
  vehicle list was opened from the toolbar and bus 5 sent to its depot, about 100 s away. Its copy 6
  took over the same engine and both orders, and 5 was dragged onto the sell button. Selling 6
  afterwards raised the money from 86,858 to 91,161, read from the server.
  In a 6-minute game with `--renew-age 45` the player decided this by itself: bus 5, 89 days old,
  was renewed by its copy 7 with both orders (35 PASS).
- **No visible cursor in screenshots.** OpenTTD's screenshots exclude the cursor sprite, which
  is drawn only on screen.
- **English text.** Widgets are found by English UI text (the default language). Another
  language needs its own phrases, or matching on the widget index the model also reports.
- **Planner only sees the map.** The route ignores town authority ratings, costs and rail
  crossings. A failed drag shows the game's own error message, and the bot continues.
