# 12 — Continuation Plan & Adversarial Review (Phase 2)

## Current architecture (retained, not rewritten)

One canonical MCP stack in `src/prismatic_mcp/` (transport + JSON-RPC + resources), serviced on the
main thread from `GameLoop()->Poll()`. Continuation adds — in the same module, no second stack — a
typed decision/approval/mutation layer and a provenance trace. Console command `prismatic_mcp` extended
(not duplicated).

## Implemented this session (Phase 2E core, verified live)

- **Typed action catalogue** (`game.set_pause`) + `openttd.decision.validate` (non-mutating),
  `openttd.decision.submit`, `openttd.decision.list_action_types`, `openttd.game.set_pause` via
  `tools/list`/`tools/call`.
- **Two-phase, digest-bound approval**: submit → `pending_approval` (+ FNV-1a argument digest); operator
  `prismatic_mcp approve <id>`; resubmit → executes via `Command<Commands::Pause>::Post` on the main
  thread. Approval is **one-time** (replay → new pending) and **argument-bound** (changed args → new
  digest → not covered).
- **Observed delta + honest verdict**: after execution the tool reads `_pause_mode`; it reports
  `EXECUTED_UNVERIFIED` when the effect isn't yet observable (dedicated = networked = deferred command)
  and `VERIFIED_SUCCESS` only when the observed effect matches intent — never conflating HTTP 200 or
  command dispatch with strategic success.
- **Provenance**: bounded `openttd://decisions` resource + `prismatic_mcp decisions` console trace
  (decision→approval→command_result→verdict, no secrets).

## Staged (designed, honestly not built this session)

Native toolbar entry + Control Center window (mapped in `01`/`14`; large GUI + index-sync effort);
event journal + snapshots/deltas + topology graphs (`15`); full company-scope binding with budgets;
the autonomous multiplayer lab/arena (`17`); MCP SSE notifications/subscriptions; Catch2 tests,
Inspector/conformance, determinism experiment. See `continuation-gap-matrix.json`.

## Adversarial review (mission §4, 20 questions)

| # | Challenge | This build |
|---|---|---|
| 1 | Toolbar coupled to MCP internals? | Toolbar not implemented; console surface calls only the public `PrismaticMCP::` API — no internal coupling. |
| 2 | UI functional when server stopped? | Console commands report "stopped"/"(server not running)" gracefully; no crash. |
| 3 | Event capture changes determinism? | No event hooks added yet; reads are const. |
| 4 | Full-world scan each frame? | `Poll()` only services sockets; resources build on request, bounded. |
| 5 | Topology stalls main loop? | Topology not implemented. |
| 6 | Stale snapshot → deleted object? | Resources rebuild live each request; no cached entity pointers. |
| 7 | Client acts as wrong company? | `game.set_pause` is SERVER_ADMIN (no company). Company-scoped mutation is staged; the digest+approval gate is in place for when it lands. |
| 8 | Replay an approved action? | **Verified defended** — approval consumed on execute; replay → new pending. |
| 9 | Lab armed after player joins? | Lab not implemented; auto-disarm conditions designed in `17`. |
| 10 | HTTP success == executed command? | **Verified** — tool distinguishes pending/executed; `isError` reflects command_result. |
| 11 | Executed == strategic success? | **Verified** — verdict separates `EXECUTED_UNVERIFIED` from `VERIFIED_SUCCESS`. |
| 12 | Two clients control one company? | Single-controller rule designed; not exercised (no company mutation yet). |
| 13 | One client controls many companies w/o scope? | Company-scope binding staged; default is no-company-mutation. |
| 14 | Reconnect inherits stale privileges? | Token rotation invalidates bearer; approvals are process-local, not per-reconnect. |
| 15 | Save/load invalidates op state wrongly? | `OnGameModeChanged()` hook exists; approvals are ephemeral (staged: clear on world change). |
| 16 | New UI breaks dedicated build? | No GUI added; module compiles in the dedicated-capable build. |
| 17 | Toolbar indices misalign? | No toolbar change → no index hazard. |
| 18 | Unbounded event/approval history? | `MAX_APPROVALS=64`, `MAX_PROVENANCE=128` ring-trim. |
| 19 | Client infers hidden MP data? | Only server-visible company data exposed; network-client resource withheld. |
| 20 | Experiment claims causality from correlation? | Arena not implemented; verdict model already refuses unobserved success. |

## Rollback

All continuation code is inside the compile gate; disabling `OPTION_PRISMATIC_MCP_SERVER` removes it
entirely (OFF baseline build verified green after these changes).
