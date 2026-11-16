---
schema: context/v1
id: fork.substrate.prismatic-mcp
kind: context
title: "PrismaticMCP (fork only): the embedded loopback MCP server, its view, UI and input tools, and the approval boundary"
description: "Read before changing src/prismatic_mcp/, its hooks in upstream files, or anything that lets an agent read or drive a running game."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/prismatic_mcp/**'
  - 'src/tests/test_prismatic_mcp.cpp'
---

# PrismaticMCP (fork only)

Evidence: read at f069e8f0f5; refreshed at cb3e08ea4d after `tools/knowledge/refs impact` named this map stale
(graph, depots, engines, window control, ClearApprovals). Fork code, not upstream OpenTTD: it exists only in this fork,
compiles only with `OPTION_PRISMATIC_MCP_SERVER=ON` (which requires
`OPTION_RESEARCH_INSTRUMENTATION=ON`, `cmake/Options.cmake`), and is labelled RESEARCH-ONLY. The
section on findings came from an independent read-only review by an external model, checked
against the source here with `tools/knowledge/refs check` (every citation resolved); each
finding keeps its label. What the module offers is documented in `research/prismatic-mcp/README.md`
and the generated catalogue (`research/prismatic-mcp/07-resource-catalog.md`,
`research/prismatic-mcp/08-tool-catalog.md`); this map says where it lives and what binds it.

## Purpose and ownership

Lets an agent read live game state and, within stated boundaries, act on a running OpenTTD process
over MCP (Streamable HTTP, JSON-RPC) on loopback. Decided in ADR 0008 (server, approval), ADR 0013
(a server in every process, local view tools) and ADR 0014 (armed interface input), all proposed.

## Entry points

- `src/openttd.cpp:openttd_main` → `src/prismatic_mcp/prismatic_mcp_server.cpp:Init` (no listener yet) **[fork]**.
- `src/openttd.cpp:GameLoop` → `src/prismatic_mcp/prismatic_mcp_server.cpp:Poll`, before `NetworkGameLoop`/`StateGameLoop` **[fork]**.
- `src/openttd.cpp:ShutdownGame` → `src/prismatic_mcp/prismatic_mcp_server.cpp:Shutdown` **[fork]**.
- Console `prismatic_mcp start|stop|token|rotate|approvals|approve|deny|decisions`: `src/console_cmds.cpp:ConPrismaticMcp` → `src/prismatic_mcp/prismatic_mcp_server.cpp:Start`, `ApproveById`, `RotateToken` **[fork]**.
- Request path: `src/prismatic_mcp/prismatic_mcp_server.cpp:ServiceConnection` → `HandleHttpRequest` (bearer, Origin) → `DispatchOne` → `HandleToolsCall` / resource readers.
- Catalogue: `src/prismatic_mcp/prismatic_mcp_server.cpp:ToolsList` (plus `src/prismatic_mcp/prismatic_mcp_view.cpp:Tools` and `src/prismatic_mcp/prismatic_mcp_ui.cpp:Tools`) and `src/prismatic_mcp/prismatic_mcp_server.cpp:RESOURCES`. Measure the counts from a live server (`tools/list`, `resources/list`); do not copy them.
- Mutation: `src/prismatic_mcp/prismatic_mcp_server.cpp:ValidateAction` → approval → `ExecuteAction` → `Command<Commands::Pause>::Post`. `src/prismatic_mcp/prismatic_mcp_server.cpp:ClearApprovals` is the one point where approvals (and anything else bound to a session or a world) are dropped: on Stop, RotateToken, a game-mode change, and a tick counter that went back.
- Interface (GUI processes): `src/prismatic_mcp/prismatic_mcp_ui.cpp:Tools` and `Call` (declared in `src/prismatic_mcp/prismatic_mcp_ui.h`): the windows as data (`openttd.ui.windows`, `ui.at`, `ui.locate`), generic window control (`ui.press` a widget by window class/number/caption and widget index/text/type, `ui.choose` a dropdown item, `ui.type`, `ui.close`, `ui.click_tile`), and armed raw input (`openttd.input.*`). View, news and questions: `src/prismatic_mcp/prismatic_mcp_view.cpp:Tools` (`src/prismatic_mcp/prismatic_mcp_view.h`).
- World serialisers: `src/prismatic_mcp/prismatic_mcp_world.cpp:Map`, `Stations` (with the game's catchment), `Depots` (with the service reach), `Engines`; declared in `src/prismatic_mcp/prismatic_mcp_world.h`.
- Topology: `src/prismatic_mcp/prismatic_mcp_graph.cpp:Ways` (`openttd://map/ways`: where a road vehicle can leave each tile, and rail tracks) and `World` (`openttd://world/graph`, world-graph/v1: towns, industries, stations, vehicles; order and cargo_link edges with the game's link-graph capacity and usage); declared in `src/prismatic_mcp/prismatic_mcp_graph.h`.
- Vehicles as data: `src/prismatic_mcp/prismatic_mcp_entities.cpp:Vehicles` (read-only; `src/prismatic_mcp/prismatic_mcp_entities.h`).
- Coverage counted by the dispatch itself: `src/prismatic_mcp/prismatic_mcp_coverage.h:Count`, `src/prismatic_mcp/prismatic_mcp_coverage.cpp:Report`
  (what clients exercised against what the server publishes; issue I0049).
- Instance registry: `src/prismatic_mcp/prismatic_mcp_registry.cpp:Announce`, `Withdraw` write and remove an
  owner-only `mcp-instance/v1` record per server (never the token itself; issue I0039); declared in `src/prismatic_mcp/prismatic_mcp_registry.h`.
- Upstream files with gated hooks: `src/dropdown.cpp` (`src/prismatic_mcp/prismatic_mcp_hooks.h:DropdownItems`), `src/screenshot.cpp:QueueQuietScreenshot`, `src/console_cmds.cpp`, `src/openttd.cpp`, `src/CMakeLists.txt`, `src/tests/CMakeLists.txt`, `cmake/Options.cmake`.

## Lifecycle

- **[verified]** One thread: the game-loop thread accepts, reads, parses, dispatches and answers; there is no worker thread and no hand-off queue (`src/prismatic_mcp/prismatic_mcp_server.cpp:Poll`, `ServiceConnection`). It runs under the video driver's game-state mutex (`src/video/video_driver.cpp:VideoDriver::GameLoop`).
- **[verified]** Start binds `127.0.0.1` (required) then `::1` (best effort); default port `src/prismatic_mcp/prismatic_mcp.h:DEFAULT_PORT`. Stop closes sockets, zeroes the token and clears every approval (so do `RotateToken` and a world whose tick counter restarted, `src/prismatic_mcp/prismatic_mcp_detail.h:WorldChanged`); provenance and input tracking survive it.
- **[verified]** `src/prismatic_mcp/prismatic_mcp_server.cpp:OnGameModeChanged` is empty and nothing calls it: a new world does not reset MCP state.
- Screenshots leave the game thread through `src/video/video_driver.hpp:VideoDriver::QueueOnMainThread`.

## Invariants

- **[verified]** The only persistent simulation change the module makes itself is `Command<Commands::Pause>::Post` in `src/prismatic_mcp/prismatic_mcp_server.cpp:ExecuteAction`; view, news, question and cursor changes are per-process presentation state (rule `project.game-state-mutates-only-through-commands`).
- **[verified]** Injected input runs as the local company through the client's own interface (`src/prismatic_mcp/prismatic_mcp_ui.cpp:AsDriver` → `src/window.cpp:HandleMouseEvents`, `HandleKeypress`), so what it changes is whatever command that interface posts; there is no allow-list.
- **[verified]** Input tools refuse unless the process started with `PRISMATIC_MCP_INPUT=1` (`src/prismatic_mcp/prismatic_mcp_ui.cpp:Armed`) and while the real mouse moved in the last `OWNER_GRACE` (`src/prismatic_mcp/prismatic_mcp_ui.cpp:OwnerHasMouse`); view and UI tools need a main window (`src/prismatic_mcp/prismatic_mcp_view.cpp:MainWindow`), so a dedicated server refuses them.
- **[verified]** Approval binds a 64-bit digest of the action and its arguments (`src/prismatic_mcp/prismatic_mcp_detail.h:ArgDigestHex`), not the caller, the session or the world.
- **[supported]** Nothing new is saved and no new command exists for MCP convenience (ADR 0008, point 5).

## Extension points

- A read-only resource: an entry in `src/prismatic_mcp/prismatic_mcp_server.cpp:RESOURCES` and its reader; regenerate the catalogue (`tools/research/generate_prismatic_docs.py`, checked by `tools/check-prismatic-docs.sh`).
- A GUI-local tool: `src/prismatic_mcp/prismatic_mcp_view.cpp:Tools` and `Call` (refuse without `MainWindow`).
- A game-changing action: a new type in `src/prismatic_mcp/prismatic_mcp_detail.h:IsKnownActionType`, validated in `ValidateAction`, executed in `ExecuteAction` through `Command<...>::Post`, never a direct pool write.
- Panel, OpenAPI and Rhai surfaces derive from the live catalogue (rule `project.mcp-surfaces-are-derived`): no hand-written list.

## Traps and forbidden operations

- **[verified]** A server-only command (`CommandFlag::Server`, e.g. `src/misc_cmd.h:CmdPause`) posted from a network client gets that client kicked (`src/network/network_server.cpp:ServerNetworkGameSocketHandler::ReceiveClientCommand`). On master `ExecuteAction` still posts `game.set_pause` without a role check; the guard (`ClientMayPost`, refusing a `CommandFlag::Server` action in a network client) is in flight on `research/mcp-full-auto`. Any new action must pass it. Knowledge candidate `a-server-only-command-from-a-network-client-kicks-it`.
- **[inferred]** `Post` in a network game only sends: reading the effect right after it (`ExecuteAction` observes `_pause_mode` at once) can report EXECUTED_UNVERIFIED for a command that later succeeds, or VERIFIED_SUCCESS because the state already matched.
- **[inferred, not run]** Armed key input reaches the console window (`src/console_gui.cpp`, `IConsoleCmdExec`), which accepts `prismatic_mcp approve <id>`: an armed controller can approve its own decisions. ADR 0014 records the arming switch as the boundary.
- **[verified]** `src/console.cpp:IConsolePrint` redirects to an rcon client when one issued the command, so `src/console_cmds.cpp:ConPrismaticMcp` refuses to print the bearer token while the console is redirected (`token`, and `start` without a token file).
- **[inferred]** The listener is serviced on the game thread with an immediate retry on a blocked send and no idle timeout (`src/prismatic_mcp/prismatic_mcp_server.cpp:ServiceConnection`): a slow reader can stall the game loop.
- A default build compiles none of this: a green `tools/gate.sh change` says nothing about it (knowledge `gated-code-is-invisible-to-a-default-build`).

## Tests and validation

- Unit: `src/tests/test_prismatic_mcp.cpp` covers the pure helpers of `src/prismatic_mcp/prismatic_mcp_detail.h` (digest, Content-Length, bearer, constant-time compare, action types). Sockets, dispatch, approval lifecycle, input and view are not unit-tested.
- Gate: `tools/gate.sh research` (builds with both options ON and runs `openttd_test '[prismatic_mcp]'`).
- Live: `tools/research/mcp-live-check`, `tools/research/mcp-panel-check`, `tools/research/mcp-view-check`, `tools/research/mcp-human-e2e --test`, the stack from `tools/research/mcp-e2e`.
- Debug: the server logs under `Facility::Net` (`-d net=1`).

## Decisions, rules and knowledge

- What `openttd.ui.windows` reports depends on `WindowClass` order, widget-enum indices and hidden toolbar buttons: [game windows](../game-windows/README.md).
- A dragged rail or road build reports success when any piece was built (knowledge candidate `a-dragged-build-succeeds-when-any-piece-was-built`): verify a dragged line through the map's ways, not the command result.
- `ctest -R` cannot select the `[prismatic_mcp]` Catch2 tag; how to run those tests is in [unit tests](../unit-tests/README.md).
- Upstream architecture: [ADR 0016](../../../adrs/0016-multiplayer-is-deterministic-lockstep-every-peer-runs-the-sa.md) (deterministic lockstep), [ADR 0017](../../../adrs/0017-game-objects-live-in-typed-pools-addressed-by-strong-ids-nev.md) (typed pools and strong IDs), [ADR 0023](../../../adrs/0023-the-map-is-a-flat-array-of-tiles-whose-behaviour-is-dispatch.md) (tile array and tile procs).
- Neighbouring maps: [game-windows](../game-windows/README.md), [rail-road-and-infrastructure](../rail-road-and-infrastructure/README.md).
- ADR 0004, ADR 0008, ADR 0013, ADR 0014 (all proposed); rules `project.research-code-is-gated-and-labelled`, `project.game-state-mutates-only-through-commands`, `project.mcp-surfaces-are-derived`, `project.public-api-is-exposed`.
- Knowledge `auto-approve-is-the-owners-switch`, `fixed-loopback-ports-collide`, `gated-code-is-invisible-to-a-default-build`, `dedicated-server-stdin-startup-race`.
- Research trail: `research/prismatic-mcp/README.md`, `research/prismatic-mcp/02-threat-model.md`, `research/prismatic-mcp/16-decision-contract.md`, `research/mcp-control.md`, `research/mcp-human.md`.

## Typical tasks

- To add a read-only resource for vehicles: start at `src/prismatic_mcp/prismatic_mcp_server.cpp:RESOURCES`, read pools on this thread with no locking, regenerate the catalogue, validate with `tools/gate.sh research` and `tools/research/mcp-live-check`.
- To add a vehicle action (e.g. send to depot): add the action type in `IsKnownActionType`, validate its arguments in `ValidateAction`, post the existing command in `ExecuteAction`; gate it on its `CommandFlag`s against the process role (the kick trap above); read the vehicles map (`.ai/repo/workflows/substrate/vehicles-and-pathfinding/README.md`) for which command owns the state.
- To add a GUI-only tool: `src/prismatic_mcp/prismatic_mcp_view.cpp:Tools`, refuse without `MainWindow`, issue no command.
- To rebase onto upstream: `tools/knowledge/refs impact "$MB"..upstream/master` names this map when a hooked upstream file moved; `src/dropdown.cpp` reproduces `DropdownWindow` layout internals and is the most fragile hook.
