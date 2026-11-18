# The MCP hub: every instance, every operation, every event over one WebSocket

RESEARCH-ONLY, UNSTABLE. Issue I0058; rule `project.mcp-surfaces-are-derived`.

A cluster of OpenTTD instances (`tools/research/mcp-e2e --instances N`, the GUI client, the human
player's stack) has one control panel per stack and one MCP server per process. The hub puts them
behind a single WebSocket JSON-RPC surface, so a page, a Rhai script, a test or another AI reaches
any of them the same way. Nothing in it names an operation:

| What | Derived from |
|---|---|
| Instances | the instance registry (`$PRISMATIC_MCP_REGISTRY`, records `mcp-instance/v1`, dead owners dropped) and `--panel URL` |
| Operations of a panel | its `/openapi.json`: every path and method, `operationId`, body schema, query parameters, `x-mcp-annotations.readOnlyHint`, streaming routes |
| Operations of an MCP server | its `tools/list` (one `tool:<name>` each) and `resources/list` (one `resource:<uri>` each), read with the token file the record names |
| Events | each panel's bus (`/api/events?since=`), fanned in and tagged with the instance; for a game's MCP server without a panel, derived from two reads of `openttd://game/state` every 5 s (`game.month`, `game.paused`, `game.resumed`, `game.new_world`, `vehicles.count`, `companies.count`, each marked `derived_from`); the audit watch's `audit.*` |
| Event kinds the hub raises | the hub's rows of `tools/research/mcp-rhai/src/panel_events.rs` `KINDS` (by `hub`), the table the panel's kinds are in too |

The set refreshes every two seconds; a new or vanished instance is pushed to subscribers as a
`hub.instances` notification.

## Run it

```bash
cargo build --offline --release --manifest-path tools/research/mcp-rhai/Cargo.toml --bin mcp-hub
tools/research/mcp-rhai/target/release/mcp-hub --port 8790        # then open http://127.0.0.1:8790/
tools/research/mcp-rhai/target/release/mcp-hub --panel http://127.0.0.1:8800   # add a panel the registry lacks
tools/research/mcp-rhai/target/release/mcp-hub --bus-port 8791 --no-store       # a fixed bus port, no store
```

## The surface

`ws://127.0.0.1:<port>/ws` speaks JSON-RPC 2.0; `POST /hub/rpc` takes the same requests one at a time
(everything except streaming), and `GET /hub/instances` and `GET /hub/catalogue` serve the lists.
Each instance in `/hub/instances` carries its record's `supervisor`: the Majordomus task it runs
under, or null.

| Method | Params | Answer |
|---|---|---|
| `rpc.discover` | | the methods and their parameters |
| `hub.instances` | | every live instance: name, kind, access (`panel` or `mcp`), base, operation count, last error |
| `hub.catalogue` | `instance?` | every operation: instance, id, method, path, summary, tags, `read_only`, `schema`, `query`, `stream` |
| `hub.call` | `instance`, `id`, `body?`, `query?` | the operation's answer; a panel's HTTP error comes back as `{http_error, body}` |
| `hub.subscribe` | `instances?`, `kinds?` | then `hub.event` notifications `{instance, event}`; a kind matches by prefix (`game` takes `game.month`) |
| `hub.unsubscribe` | | |
| `hub.audit` | | per instance: duplicate station pairs, and its depots with the duplicate depot pairs |
| `hub.bus` | | the hub's own bus (also `GET /api/bus`): endpoint, schema, `instance` (`hub-<port>`), topics seen, events published, the store, and `kinds`: the hub's rows of the kinds table |
| `hub.db_tables` | | the hub's own store (also `GET /api/db/tables`): its file and every table with its columns |
| `hub.db_query` | `sql?` | one read-only statement on the hub's store (also `GET /api/db/query?sql=`): `{columns, rows, count, capped}`; without `sql` the newest 20 events |
| `algoglyph.kernels` | | every AlgoGlyph kernel with the input it reads (`flow -- kernels`) |
| `algoglyph.kernel` | `kernel`, `input` | that kernel's answer on that input (`flow -- kernel <id> -`) |
| `algoglyph.track_plan` | `instance`, `from {x, y, dir}`, `to {x, y, dir?}`, `costs?`, `rules?`, `railtype?` (a label of `openttd://railtypes`, e.g. `ELRL`, or its index; its entry is passed as `railtype`) | AlgoGlyph track-planning on that game's `openttd://map`, `map/ways`, `map/terrain`, `economy/prices` and `bridges`, passed verbatim: status, cost, `money` in the game's money, counts (curves, inclines, foundations, bridges, tunnels, cleared, crossings), pieces, runs |

The hub describes itself in `GET /openapi.json`, derived from its method table, with Swagger UI over
it at `GET /docs` (the panel's own `swagger-ui-dist`, so `npm ci` in `tools/research/mcp-panel` first). A
method row that names a `get` path is served there too, its params as optional query parameters:
`/hub/instances`, `/hub/catalogue`, `/hub/audit`, `/api/bus`, `/api/db/tables`, `/api/db/query`. The
OpenAPI operation, the GraphQL field and the Rhai function of each come from the same row.

## The hub's own bus and store

Every event the hub raises itself (the audit watch's `audit.*` below, and the `game.*`,
`vehicles.count` and `companies.count` it derives for an MCP server without a panel) goes three
ways from `State::raise` in `tools/research/mcp-rhai/src/hub.rs` (I0084): to the WebSocket
subscribers as before, and in the panel's prismatic-bus/v1 envelope (ADR 0033) to a ZeroMQ PUB
socket and a DuckDB store of the hub's own. Both sinks are the panel's code,
`tools/research/mcp-rhai/src/bus_sinks.rs`, not a copy of it.

| | |
|---|---|
| Endpoint | `tcp://127.0.0.1:<--bus-port>`, by default an ephemeral port; `GET /api/bus` of the hub names it |
| Frames | the topic (the event kind) and the envelope `{schema, seq, ts_ms, instance, session, seed, game_tick, game_date, kind, data}` |
| `instance` | the instance the event is about (`panel-8800`, `gui-…`), not the hub; the hub itself is the bus's and the store's `instance` in `GET /api/bus` and in `runs` |
| `seq` | the hub's own counter of raised events; `seed` is null (the hub does not know it) |
| `game_tick`, `game_date` | the event's own date, else the clock of the instance's last `openttd://game/state` read (MCP-only instances), else null |
| Store | `$PRISMATIC_STORE_DIR/hub-<port>.duckdb` (by default `~/.local/state/prismatic-mcp/store/`), the panel's tables `runs`, `events`, `samples`; committed within a second; `--no-store` leaves it out |
| Kinds | the hub's rows of `KINDS` (`GET /api/bus` `kinds`); a kind the table does not declare for the hub is streamed to subscribers but not published |

A panel event the hub only relays is in that panel's own store already: it never enters the hub's
bus or store, so nothing is stored twice. Follow the hub's bus with
`mcp-bus-tail --panel http://127.0.0.1:8790 --topic audit` (it reads the endpoint from `GET /api/bus`),
or query it: `curl -s 'http://127.0.0.1:8790/api/db/query' --get --data-urlencode "sql=select instance, kind, data from events where kind like 'audit.%'"`.
While the hub runs it holds the store file's lock, as a panel does: read through the route.

`id` is a panel's `operationId`, `tool:<name>` or `resource:<uri>`. A streaming route
(`text/event-stream`) is not called: subscribe instead.

## AlgoGlyph: the planning engine

AlgoGlyph (`$MCP_ALGOGLYPH_DIR`, else a checkout of origin/main at `~/dev/algoglyph-wt/main`, else
`~/dev/algoglyph`) is the planning engine; the hub only
forwards (`tools/research/mcp-rhai/src/hub_algoglyph.rs`). `algoglyph.track_plan` reads the five
resources of the named game back to back (track-planning checks the terrain's slopes against the
map's heights, so they must be one moment) and pipes them with `from` and `to` to the kernel; a
resource the game does not publish is left out and the kernel uses its defaults. A kernel runs at
most 120 s. Bridges, tunnels, diagonal (half-tile) track, inclines, foundations and level crossings
are the kernel's, and so is the price model: with `prices` the unit is the game's `build_rail`, and
bridges and tunnels follow `CmdBuildBridge` / `CmdBuildTunnel`.

```bash
curl -s http://127.0.0.1:8790/hub/rpc -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":1,
  "method":"algoglyph.track_plan","params":{"instance":"<name from hub.instances>",
  "from":{"x":68,"y":9,"dir":"NE"},"to":{"x":45,"y":9}}}'
```

Measured on a live 256x256 game (seed 7, construction cost low): a 22-tile straight was found in
3.5 s, cost 22, money 1650 (22 x the game's build_rail of 75); a 92-tile route across rough ground
in 2.8 s, with 6 curves, 4 inclines and 9 cleared tiles.

## GraphQL

`POST /graphql` takes `{query, variables}` and answers `{data, errors?}`; `GET /graphql/schema` is the
SDL. The schema is derived from the method table: every method that answers per request is a root
field named by its last segment (`instances`, `catalogue`, `audit`, `discover`), `hub.call` is the
one Mutation field (`call`), and the method's params are the field's arguments, as JSON. A selection
set keeps only the named keys of each result, so a client asks for exactly what it reads:

```graphql
query ($i: String) {
  instances { name operations }
  catalogue(instance: $i) { id read_only }
  audit { instance stations depots { depots reach network error } }
}
mutation { call(instance: "panel-8800", id: "status") }
```

It is a subset implemented in the hub (`tools/research/mcp-rhai/src/hub_graphql.rs`), so the hub needs
no new dependency: one operation per document, aliases, arguments, variables and literals. Fragments,
directives, introspection and subscriptions are refused by name; events stream over the WebSocket's
`hub.subscribe`.

## From Rhai

`mcp-rhai --hub http://127.0.0.1:8790` (or `MCP_HUB=...`) reads the hub's `/openapi.json` and
registers one function per method it lists, named `hub_<method>`: `hub_instances()`,
`hub_catalogue(#{instance: ...})`, `hub_call(#{instance, id, body})`, `hub_audit()`,
`hub_rpc_discover()`. Streaming methods need the WebSocket and are left out. A method the hub gains
is a Rhai function with no change to mcp-rhai.

```bash
mcp-rhai --hub http://127.0.0.1:8790 -e 'hub_audit().len()'
```

## The page and its components

`tools/research/mcp-panel/static/hub.html` is built only from `static/components.js`, which any
page of the panel can import:

- `HubClient`: the WebSocket client (requests matched to answers, notifications as events,
  reconnects and re-subscribes with the same filter);
- `catalogueView`: every operation of every instance, grouped by instance and tag and filtered by
  text; clicking one opens `operationForm`, a form derived from the operation's schema and query
  (enums as selects, booleans as checkboxes, objects as JSON), which calls it;
- `instancesView`: the instances as a table; clicking one narrows everything to it;
- `eventsView`: the live stream, each event's instance and kind clickable to follow only that;
- `jsonView`: results, with every `openttd://` URI a link that reads that resource.

The state (instance, open operation, filter, followed kinds) is in the URL hash, so a view is a link.
The page uses the panel's compiled Flowbite CSS and only classes it already contains.

## No duplicate buildings

`hub.audit` (also `GET /hub/audit`, `mcp-hub audit`, `tools/research/mcp-dup-check`, and the
"Duplicate buildings" card of the page) judges every instance's stations by one rule: **two stations
of one owner that share a facility and whose catchments overlap duplicate each other**, since both
serve the same tiles with the same kind of transport. A bus stop beside its own airport, another
owner's stop, or two stops of one owner whose catchments do not touch are not duplicates. Only a
company builds a duplicate: an owner of 15 or more (`MAX_COMPANIES`, `src/company_type.h`) is the
game's own, a town (15), nobody (16) or water (17). Two oil rigs a few tiles apart, each with its own
unowned heliport and dock, are the map and not a build; on seeds 24455 and 2468 they had been
reported as duplicates of owner 16. Depots are judged the same way.

The catchment is the game's own: `openttd://stations` publishes `catchment_radius`
(`Station::GetCatchmentRadius`) and `catchment_area`, the bounding box of the station's
`catchment_tiles`. For an instance on an older binary that publishes neither, the radius of each
facility comes from `src/station_type.h` (`CA_BUS`, `CA_TRUCK`, `CA_TRAIN`, `CA_DOCK`, and
`CA_UNMODIFIED` for an airport), and the report says "catchment from the fallback radius".

The same audit judges **depots**: two depots of one owner and one type duplicate each other when
the second lies within the reach a vehicle searches for a depot to be serviced at. That reach is the
game's own setting, `pf.yapf.maximum_go_to_depot_penalty` in tiles, which `openttd://depots`
publishes as `service_reach_tiles` with every depot's tile, type and entrance. For road depots the
distance is measured along the road network of `openttd://map/ways` (two tiles are joined when each
points at the other), so two depots on separate roads are not duplicates. Without ways, or for rail
and ship depots, the straight distance is used and the report says so.

It also reports **routes over capacity**, as an advisory warning that does not fail the check. The
game's own link graph (`cargo_link` edges of `openttd://world/graph`) carries each link's measured
capacity and usage; the vehicles' order lists say which vehicles run it, and their capacities what one
vehicle carries. A link needs max(1, ceil(usage / vehicle capacity)) vehicles, and is judged only once
mature (its measured capacity covers every assigned vehicle, so a new route's rising usage says
nothing). A route whose both directions need fewer vehicles than it runs has that many too many.

The hub also **watches** the audit: every `--audit-every` seconds (default 60, 0 off) it runs it and
announces what changed on the event stream, once per finding: `audit.duplicate_station`,
`audit.duplicate_depot` and `audit.over_capacity` when one appears, `audit.resolved` (naming what it
was) when it goes away. A finding is keyed by what it is, not by its numbers, so a pair that stays is
not announced again. Any subscriber sees them (`hub.subscribe {kinds: ["audit"]}`): the page's live
events, a Rhai script, a bot that wants to repair what it built twice; and they are on the hub's own
bus and in its store (above), so a decision can read later which findings a game had.

The check is generic: it does not know which bot built what, so it holds PrismaticAI, the planner's
bot and the human-motion player to the same rule.

## Proof

| Check | What it proves |
|---|---|
| `cargo test --offline --manifest-path tools/research/mcp-rhai/Cargo.toml --bin mcp-hub` | operations derived from OpenAPI and MCP, registry records and dead owners, filters, verdicts; and end to end against a fake panel: list, call over WebSocket and HTTP, a stream refused as a call, only the subscribed kind streamed; the hub's own sinks (I0084): a raised event's envelope, an audit event received by a ZeroMQ SUB client with the instance named and in the hub's store within a second, a relayed panel event on neither, a derived game event stored with its instance's clock, the kinds the hub's code raises equal to its rows of `KINDS`, and the bus and store routes rows of the method table |
| `node --test 'tools/research/mcp-panel/test/*.test.mjs'` | schema to fields, coercion, call parameters, URL state round trip, grouping, and the client's request matching, notifications and re-subscription |
| `tools/research/mcp-hub-page-check <hub-url>` | the page in a real browser: the WebSocket goes live, every listed instance is a row, a click on an operation opens its form and the call answers, the filter is kept in the address and survives a reload, the audit card renders, a live event arrives, no console error |
| `mcp-hub intent-check [--hub URL] ["<sentence>"]...` | the owner's requests in plain language through the hub: every instance with an `/intent` operation (openttd-80's `/api/intent`, I0069) answers each sentence (the owner's own examples by default, Czech with and without diacritics and English) with an action, its candidates, and a plan to build; a sentence whose subjects one instance resolves and another does not is a WARN on the second (a fallback answer, advisory; derived across the instances, I0078) |
| `tools/research/mcp-dup-check` | against whatever runs: no instance holds a duplicate pair; each pair named with both stations and the distance |
| `tools/research/mcp-cluster-check [--servers N]` | discovery for a cluster: N servers and N null-driver clients, each announcing its MCP; the hub lists all 2N, a SIGTERM withdraws a record, and a SIGKILL's stale record is dropped |
| `tools/research/mcp-hub-check` | against whatever runs: every instance discovered, every read-only operation that needs no input called through the WebSocket, one event streamed; a 503 ("no GUI client attached") is counted as skipped |

## Limits

- Loopback only and no authentication of its own: it holds MCP tokens read from the registry's token
  files, so whoever reaches the port reaches every instance. Keep it on 127.0.0.1.
- It calls what a person asks for, including mutating operations; the approval flow of each panel
  (ADR 0008) and the arming switch of interface input (ADR 0014) still apply behind it.
- A panel's events are its own bus and its own store; the hub's bus and store hold only what the hub
  raised. A batch not yet committed (at most about half a second) is lost when the hub is killed.
- A bare MCP server has no bus, so its events are only what two
  successive reads of `openttd://game/state` show (the Events row above) (`derive_state_events` in
  `tools/research/mcp-rhai/src/hub.rs`, each marked `derived_from`): a change between two reads
  is seen, anything that changed and changed back between them is not.
- The page is checked in a real browser by `tools/research/mcp-hub-page-check` (headless installed
  Chrome through playwright-core); the World view by `tools/research/mcp-viz-check`.

The hub page has a card for it, **Plan a rail line** (`trackPlanView` in `static/components.js`):
pick a game (every instance that publishes `openttd://map`), the two ends and a rail type (read from
the game's `openttd://railtypes`, with each type's cost a tile), and AlgoGlyph's answer is shown as
status, money and counts. The inputs are kept in the address (`#plan=x,y,DIR;x,y;RAILTYPE`) and
survive a reload. A panel's 404 for a resource the build lacks is read as not published, so the
kernel uses its defaults; any other HTTP error fails the call.

Rail types, measured on the same 256x256 game (construction cost low, build_rail 75): one route
of the same tiles priced on `RAIL` 6487, `ELRL` 9350 and `MGLV` 18051 (track at the type's
`build_cost` 75, 112 and 225; clearing and foundations are the same for every type); an unknown
label is refused with the game's list.
