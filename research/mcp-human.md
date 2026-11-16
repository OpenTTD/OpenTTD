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
| `openttd.ui.windows` | read | Every window front to back: class, number, key, caption, rectangle. `widgets: true` adds each widget's type, screen rectangle, resolved text and tooltip, and disabled/lowered state. An open dropdown also lists its items (`items`), which are drawn rather than being widgets. |
| `openttd.ui.at` | read | What is under a screen pixel: the window, the widget and, over a viewport, the map tile. |
| `openttd.ui.locate` | read | The screen pixel of a tile's centre in the main viewport, and whether a click there reaches the map (`clickable`). |
| `openttd.input.state` | read | Cursor, buttons, the armed switch, whether the owner holds the mouse, the active placement tool and the tile selection, plus injected-event counters. |
| `openttd.input.pointer` | armed | Move the visible cursor to a pixel through the driver's input path (hover, drag and tile highlight follow). |
| `openttd.input.button` | armed | Press or release left/right/middle at the cursor, with optional ctrl/shift. |
| `openttd.input.wheel` | armed | Turn the wheel at the cursor. |
| `openttd.input.key` | armed | Press a key as the keyboard driver does (an OpenTTD `WKC_*` keycode; letters are their upper-case code, `WKC_CTRL` is `0x4000`). |

A list drawn in a matrix (for example the vehicles in the buy window) also reports
`row_height`, so a person-like controller can point at a row. The title-bar boxes (close,
sticky, shade, default size) carry no widget index. They are found by hit-testing the caption's
row and reported with `"index": null`. The resource
`openttd://vehicles` lists every primary vehicle with its owner, type, engine, tile,
stopped/in-depot/crashed state, cargo, profits and orders. The player uses it to confirm that what
it did through the windows took effect.

Input feeds the same entry points a video driver calls (`HandleMouseEvents()`,
`HandleKeypress()`), as the local company, on the main thread. A dedicated server, which has no
interface, refuses every UI tool. Like every MCP tool, these are reachable from Rhai (derived
from `tools/list`) and over the panel's derived REST/OpenAPI.

## Safety: the owner's switch and the owner's hand

- **Armed per process.** Input tools refuse unless the process was started with
  `PRISMATIC_MCP_INPUT=1`. Only the owner's own client gets it, from the launcher. The read
  tools are always available.
- **The person wins.** When the real mouse moves, the cursor position no longer matches what
  the bot left there. Input then pauses for three seconds after the last real movement and the
  bot waits (`owner_has_mouse`). Grab the mouse and you are playing; let go and it carries on.
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

## End to end

```bash
tools/gate.sh research                 # builds build-gate-research/ with both options ON
tools/research/mcp-human-e2e           # server + client (own MCPs), the human plays, stays up
tools/research/mcp-human-e2e --test    # ...and asserts on the server's map, exit 0/1
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

## Limits and next steps

- **One service per pair, no economy loop yet.** Choosing profitable routes, adding buses
  where demand is, replacing old ones and managing the loan are I0045's remaining part.
- **No visible cursor in screenshots.** OpenTTD's screenshots exclude the cursor sprite, which
  is drawn only on screen.
- **English text.** Widgets are found by English UI text (the default language). Another
  language needs its own phrases, or matching on the widget index the model also reports.
- **Planner only sees the map.** The route ignores town authority ratings, costs and rail
  crossings. A failed drag shows the game's own error message, and the bot continues.
