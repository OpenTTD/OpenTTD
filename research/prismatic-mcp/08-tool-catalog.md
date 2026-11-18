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
| `openttd.cmd.<Name>` | `prismatic_mcp_commands.cpp`, `prismatic_mcp_params.hpp` | one tool per game command (`Commands` enum) whose parameters convert from JSON, derived from the command table; validated by a side-effect-free test run, posted through the network-safe command path, the verdict read with `openttd.decision.get` | the same approval as `submit` |
| `openttd.ai.list`, `openttd.ai.set_setting`, `openttd.ai.start`, `openttd.ai.stop`, `openttd.ai.reload` | `prismatic_mcp_ai.cpp` | the company AIs and their settings (`openttd://ai`); a live setting changes without a restart | `list` none; the rest the same approval as `submit` |
| `openttd.decision.get` | `prismatic_mcp_server.cpp` | one decision's provenance: the command result and the verdict taken after it ran | none, read-only |
| `openttd.api.Script<Class>.<Function>` | `prismatic_mcp_api.cpp` | one tool per static function the build publishes to scripts, read back from a private Game Script engine (and an AI engine for what only AIs get); a call goes through the game's own script binding (`../mcp-api.md`) | none without `execute` (runs in `GSTestMode`, changes nothing); with `execute: true` the same approval as `submit`, as action `api.Script<Class>.<Function>` |

Every tool declares `annotations.readOnlyHint`: true for one that changes nothing, false otherwise
(with `destructiveHint` and `idempotentHint` where they apply). A view tool that declares nothing
gets the local-GUI default (not read-only, not destructive, idempotent when it sets a state), and a
unit test holds every client tool to the rule. `mcp-coverage-check --probe --exercise` calls the
others too, once, on a throwaway game. The coverage probe
(`tools/research/mcp-coverage-check --probe`) calls only the read-only ones, so an
undeclared tool would go untested; the approval-gated ones are exercised by
`tools/research/mcp-live-check`, approve and deny both. The fixed action types are named once,
`detail::ACTION_TYPES` (`prismatic_mcp_detail.h`), and every exposable game command adds
`command.<Name>`; `ActionTypes()` (`prismatic_mcp_server.cpp`) joins the two, and the validator,
`list_action_types` and the decision tools' input schemas (`action.type` enum) read it.

Parameters are spelled positionally, each by its declared type: an integer, enum or id as a number,
a bool, a string, and three structured types: a text (`EncodedString`) as a plain string (made valid,
at most 1024 bytes, carried as the game's raw-string message), a vehicle list as
`{type, vtype, company, index}`, a cargo source as `{id, type}`, an order as its wire fields
`{type, flags, dest, refit_cargo?, wait_time?, travel_time?, max_speed?}` (checked by the command
as from any client), and any choice of types (`std::variant`, e.g. a news reference) as `null`
(its empty alternative) or `{index, value}`, derived from the alternatives. A command with a
parameter that has no JSON spelling is not a tool: `openttd://commands` lists it with `exposable: false` and the types in the way
(`unsupported_types`). The console spells the same calls: `bot` (the AIs) and `plan` (a request in
your own words, handed to the panel's intent resolver).

A build with the MCP server registers one more network command callback (`CcPrismaticMcp`, last in
`network_command.cpp`'s table). An official server does not know that index, so such a build must
not post commands into a public game; play there with the unmodified client
([`../vanilla-client.md`](../vanilla-client.md), ADR 0030).

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
adapter status) is the mapping in `01-openttd-system-map.md §5.4`. The `openttd://commands`
resource now serves it, derived from the command table (category, flags, parameter schemas,
`exposable`); every command it marks exposable is an approval-gated `openttd.cmd.<Name>` tool, so
no command runs without an operator approval.
