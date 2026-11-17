# 05 — Implementation Plan & Phase Status (Phase 4)

Honest status of every phase. `DONE` = implemented and compiled (and, where noted, run).
`PARTIAL` = foundation in place, explicit gaps. `STAGED` = designed, not yet implemented.
Per AGENTS.md, nothing is marked done that was not actually built and checked.

Status update (2026-11-17): phases D to H have landed in part or in full since this plan was
written; each phase below says what exists now and cites its source. The plan's original wording
for what is still open is kept.

## Phase A — Build gate & skeleton — **DONE**

- `OPTION_PRISMATIC_MCP_SERVER` at all three `cmake/Options.cmake` sites, with the
  research-instrumentation `FATAL_ERROR` guard; `src/prismatic_mcp/` gated subdirectory.
- Runtime state object, `Init/Shutdown/Poll/Start/Stop` lifecycle wired into `openttd.cpp`
  (`Init` after `NetworkStartUp`, `Poll` in `GameLoop`, `Shutdown` in `ShutdownGame`), all under
  `#ifdef OTTD_PRISMATIC_MCP_SERVER`.
- Console command `prismatic_mcp <status|start [port] [token-file]|stop|endpoint|token|rotate|approvals|approve <id>|deny <id>|decisions [n]>`
  under the research gate (`ConPrismaticMcp` in `src/console_cmds.cpp`).
- **Verified:** configures with the gate ON; `FATAL_ERROR` path exists for ON-without-research.

## Phase B — HTTP transport, auth, JSON-RPC core — **DONE (core), PARTIAL (SSE/sessions)**

- Loopback v4+v6 listener; raw HTTP/1.1 parser (partial reads handled; Content-Length bodies;
  case-insensitive headers; bounded body/header/connection limits).
- Bearer auth (256-bit CSPRNG, constant-time compare); `Origin` rejection; `POST`/`DELETE` handled.
- JSON-RPC: `initialize`, `notifications/initialized`, `ping`, `resources/list`, `resources/read`,
  `tools/list`, `tools/call`, `prompts/list`; batch rejected; error codes mapped.
- **Gaps (STAGED):** SSE (`GET` stream), a real session table with `MCP-Session-Id`/expiry,
  `Last-Event-ID` replay, chunked transfer. This build is auth-per-request and stateless.

## Phase C — Registry & core resources — **PARTIAL**

- Deterministic `std::array` catalogue, `RESOURCES` in `prismatic_mcp_server.cpp` (one row per
  resource; `resources/list` lists them). It began with `openttd://mcp/status`,
  `openttd://meta/build`, `openttd://game/state` and `openttd://companies`, each wrapped in the
  common envelope (schema_version, source_revision, snapshot tick/date, mode, redactions,
  truncated). `openttd://mcp/coverage` lists every published tool and resource with its call count.
- **Gaps (STAGED):** the full `MCPExposureDescriptor` registry (privilege/sensitivity/cost metadata),
  `openttd://mcp/catalog`, resource templates.

## Phase D — Native toolbar + Control Center window — **PARTIAL**

Designed in `01-openttd-system-map.md §5.6` (framerate_gui template, index-sync hazard list). The
first vertical shipped the **console** control surface instead to get a verifiable path with no
toolbar index-sync risk.

- **Done (I0072):** the fork's own main-toolbar button and dropdown menu, one table
  (`prismatic_mcp_menu.cpp`, wired in `src/toolbar_gui.cpp`), described by `openttd://ui/menu`, run
  by `openttd.ui.menu.run`, with questions and requests answered through `openttd://ui/requests`.
- **Staged:** the 9-view Control Center window.

## Phase E — Broad introspection (vehicles/towns/industries/stations/AI/settings/UI/debug/network) + pagination — **PARTIAL**

- **Done:** vehicles, towns, stations, depots, engines, the map (`openttd://map`, `map/terrain`,
  `map/ways`), the world graph (industries included), prices, rail types, bridges, the AIs and
  their settings (`openttd://ai`), the UI windows (`openttd.ui.windows`). See
  `exposure-coverage-matrix.md`.
- **Staged:** game settings, debug/performance, network clients. The opaque-cursor pagination +
  snapshot-consistency machinery (mission §17) is still not built: these resources answer whole,
  unpaginated.

## Phase F — Read-only & UI tools — **PARTIAL**

- **Done:** the view, UI and input tools (`openttd.view.*`, `openttd.ui.*`, `openttd.input.*`),
  local to the client, input only when armed (`08-tool-catalog.md`).
- **Staged:** the analysis tools of the original design (`openttd.search_entities`,
  `openttd.analyze_*`, `openttd.export_snapshot`).

## Phase G — Approval workflow (scopes, arming, digest-bound approval, replay prevention) — **DONE (core), PARTIAL (scopes)**

- **Done:** two-phase approval, binding to the exact canonical arguments, one-time use (replay
  refused), TTL and a bounded store, operator approve/deny in the console
  (`16-decision-contract.md`; state machine unit-tested since `e1f2ee7d3f`).
- **Partial:** `required_scope` is reported per action but not enforced per client; company-scoped
  authorization is staged.

## Phase H — Typed game mutations (pause/save/AI/vehicle via `Command<>::Post`) — **DONE (except save)**

- **Done:** `game.set_pause`; `command.<Name>` for every game command whose parameters convert from
  JSON (`prismatic_mcp_commands.cpp`), which covers the vehicle and company commands; `ai.start`,
  `ai.stop`, `ai.reload`, `ai.set_setting` (`prismatic_mcp_ai.cpp`); Script API calls with
  `execute: true` (`prismatic_mcp_api.cpp`). All go through `Command<>::Post` behind the approval
  gate.
- **Staged:** saving a game (not a game command, so not in the catalogue).

These were deliberately ordered *after* the approval workflow: no mutation tool was registered until
the two-phase approval + argument binding + replay prevention existed, so there was never a
mutation path ahead of its safety guarantees (ADR decision 6).

## Phase I — Prompts, SSE notifications, progress, cancellation — **STAGED**
## Phase J — Fuzz, determinism experiment, Inspector/conformance, in-game vertical, full evidence — **PARTIAL**

- **Done:** the read-only in-game connect vertical (see `10-in-game-runbook.md` / final report);
  the scripted live check `tools/research/mcp-live-check`; the A/B determinism experiment
  (control vs MCP-query run), **PASS** (`determinism-ab.md`, I0026).
- **Staged:** libFuzzer targets, the official conformance suite, multiplayer mutation test,
  native-GUI resolution matrix.

## Closing the adversarial-review "staged/N/A" items

| Review # | Closed by phase |
|---|---|
| 2, 11, 18, 19, 20 (mutation safety) | G + H |
| 6 (frame budget on huge queries) | E (pagination) |
| 13 (toolbar index sync) | D |
| SSE/session (§11.4-11.5) | B completion + I |
| determinism / fuzz / conformance | J |
