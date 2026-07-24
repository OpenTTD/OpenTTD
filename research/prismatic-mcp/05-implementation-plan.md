# 05 — Implementation Plan & Phase Status (Phase 4)

Honest status of every phase. `DONE` = implemented and compiled (and, where noted, run).
`PARTIAL` = foundation in place, explicit gaps. `STAGED` = designed, not yet implemented.
Per AGENTS.md, nothing is marked done that was not actually built and checked.

## Phase A — Build gate & skeleton — **DONE**

- `OPTION_PRISMATIC_MCP_SERVER` at all three `cmake/Options.cmake` sites, with the
  research-instrumentation `FATAL_ERROR` guard; `src/prismatic_mcp/` gated subdirectory.
- Runtime state object, `Init/Shutdown/Poll/Start/Stop` lifecycle wired into `openttd.cpp`
  (`Init` after `NetworkStartUp`, `Poll` in `GameLoop`, `Shutdown` in `ShutdownGame`), all under
  `#ifdef OTTD_PRISMATIC_MCP_SERVER`.
- Console command `prismatic_mcp <status|start|stop|endpoint|token|rotate>` under the research gate.
- **Verified:** configures with the gate ON; `FATAL_ERROR` path exists for ON-without-research.

## Phase B — HTTP transport, auth, JSON-RPC core — **DONE (core), PARTIAL (SSE/sessions)**

- Loopback v4+v6 listener; raw HTTP/1.1 parser (partial reads handled; Content-Length bodies;
  case-insensitive headers; bounded body/header/connection limits).
- Bearer auth (256-bit CSPRNG, constant-time compare); `Origin` rejection; `POST`/`DELETE` handled.
- JSON-RPC: `initialize`, `notifications/initialized`, `ping`, `resources/list`, `resources/read`,
  `tools/list`, `prompts/list`; batch rejected; error codes mapped.
- **Gaps (STAGED):** SSE (`GET` stream), a real session table with `MCP-Session-Id`/expiry,
  `Last-Event-ID` replay, chunked transfer. This build is auth-per-request and stateless.

## Phase C — Registry & core resources — **PARTIAL**

- Deterministic `std::array` catalogue with 4 live resources: `openttd://mcp/status`,
  `openttd://meta/build`, `openttd://game/state`, `openttd://companies`, each wrapped in the common
  envelope (schema_version, source_revision, snapshot tick/date, mode, redactions, truncated).
- **Gaps (STAGED):** the full `MCPExposureDescriptor` registry (privilege/sensitivity/cost metadata),
  `openttd://mcp/catalog`, resource templates.

## Phase D — Native toolbar + Control Center window — **STAGED**

Designed in `01-openttd-system-map.md §5.6` (framerate_gui template, index-sync hazard list). The
first vertical ships the **console** control surface instead to get a verifiable path with no
toolbar index-sync risk. The toolbar button + 9-view window are the next UI increment.

## Phase E — Broad introspection (vehicles/towns/industries/stations/AI/settings/UI/debug/network) + pagination — **STAGED**

High-cardinality resources require the opaque-cursor pagination + snapshot-consistency machinery
(mission §17) before they can be exposed safely; that machinery is designed but not built.

## Phase F — Read-only & UI tools — **STAGED**
## Phase G — Approval workflow (scopes, arming, digest-bound approval, replay prevention) — **STAGED**
## Phase H — Typed game mutations (pause/save/AI/vehicle via `Command<>::Post`) — **STAGED**

These are deliberately ordered *after* the approval workflow: no mutation tool is registered until
the two-phase approval + argument-digest binding + replay prevention exist, so there is never a
mutation path ahead of its safety guarantees (ADR decision 6).

## Phase I — Prompts, SSE notifications, progress, cancellation — **STAGED**
## Phase J — Fuzz, determinism experiment, Inspector/conformance, in-game vertical, full evidence — **PARTIAL**

- **Done:** the read-only in-game connect vertical (see `10-in-game-runbook.md` / final report).
- **Staged:** libFuzzer targets, the A/B determinism experiment (control vs MCP-query run, hash
  compare), the official conformance suite, multiplayer mutation test, native-GUI resolution matrix.

## Closing the adversarial-review "staged/N/A" items

| Review # | Closed by phase |
|---|---|
| 2, 11, 18, 19, 20 (mutation safety) | G + H |
| 6 (frame budget on huge queries) | E (pagination) |
| 13 (toolbar index sync) | D |
| SSE/session (§11.4-11.5) | B completion + I |
| determinism / fuzz / conformance | J |
