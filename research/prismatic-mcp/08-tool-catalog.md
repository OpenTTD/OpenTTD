# 08 — Tool Catalog

The catalogue is what a running server answers to `tools/list`; nothing here is a second copy of
it. Each instance lists only what it can do (a dedicated server has no window: no `openttd.view.*`,
`openttd.ui.*` or toolbar-menu tool). The tool families, by module:

| Family | Module | What it does | Approval |
|---|---|---|---|
| `openttd.view.*` | `prismatic_mcp_view.cpp` | the local viewport, news, questions to the player | none, local to the client |
| `openttd.ui.*`, `openttd.input.*` | `prismatic_mcp_ui.cpp` | the interface as data, armed input through the driver | none; input only when armed in the console |
| `openttd.ui.menu.run`, `openttd.ui.requests.answer` | `prismatic_mcp_menu.cpp` | the fork's toolbar menu and the requests asked through it | none, local to the client |
| `openttd.decision.validate`, `openttd.decision.list_action_types` | `prismatic_mcp_server.cpp` | check a typed action, list the action types | none, read-only |
| `openttd.decision.submit`, `openttd.game.set_pause` | `prismatic_mcp_server.cpp` | a typed action through the command system | the operator approves or denies in the console (`prismatic_mcp approve|deny <id>`), digest-bound, one use |

Every tool declares `annotations.readOnlyHint`: true for one that changes nothing, false otherwise
(with `destructiveHint` and `idempotentHint` where they apply). The coverage probe
(`tools/research/mcp-coverage-check --probe`) calls only the read-only ones, so an
undeclared tool would go untested; the approval-gated ones are exercised by
`tools/research/mcp-live-check`, approve and deny both. The action types are named once,
`detail::ACTION_TYPES` (`prismatic_mcp_detail.h`): the validator, `list_action_types` and the
decision tools' input schemas (`action.type` enum) read that table.

## Planned tools (the original design; most of it exists under other names above)

**Read-only (Phase F):** `openttd.search_entities`, `openttd.get_entity`, `openttd.analyze_company`,
`openttd.analyze_vehicle`, `openttd.analyze_town`, `openttd.find_idle_vehicles`,
`openttd.export_snapshot`. Return observations + exact source entity URIs + explicit unknowns; never a
fabricated causal claim.

**Client-local UI (Phase F, privilege UI_CONTROL):** `openttd.ui.center_viewport`,
`openttd.ui.focus_entity`, `openttd.ui.open_company/vehicle/station/town`, `openttd.ui.highlight_*`,
`openttd.ui.capture_screenshot`. Main-thread, local presentation only; never touch synchronized state.

**Game mutation (Phase H, privilege GAME_MUTATION, approval-required):** `openttd.game.set_pause`,
`openttd.game.save`, `openttd.ai.start/reload/stop`, `openttd.vehicle.start_stop`,
`openttd.vehicle.send_to_depot`, `openttd.company.rename`. Each: validate mode → authorize → check
arming → validate ownership/server-authority → immutable request digest → approval → execute on main
thread via `Command<>::Post` → capture actual result → prevent duplicate execution → audit.

The read-only command *inventory* (which internal `Commands` exist, their sync/server class, and
adapter status) is the mapping in `01-openttd-system-map.md §5.4`; the future
`openttd://commands/catalog` resource surfaces it without making any raw command remotely executable.
