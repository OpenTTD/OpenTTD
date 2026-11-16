---
schema: context/v1
id: fork.substrate.commands
kind: context
title: "Commands: the path every player, script and network change to game state takes, its traits, phases and costs"
description: "Read before adding or changing a command, a Cmd* handler, a DEF_CMD_TRAIT line or any code that calls Command<>::Post or ::Do."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/command.cpp'
  - 'src/command_func.h'
  - 'src/command_type.h'
  - 'src/*_cmd.h'
  - 'src/*_cmd.cpp'
---

# Commands

Read at f069e8f0f5. Reviewed adversarially on 2026-11-16 by an external model through
`tools/knowledge/consult` (skill `consult-an-external-model`); its corrections to the lifecycle, the
script route, pause handling and desync logging were verified in the source here before they were
applied.

## Purpose and ownership

Owns the `Commands` enum, the per-command traits, the master command table, the test-then-execute
protocol, cost accounting (`CommandCost`), pause gating and the hand-off of a command to the
network. Handlers (`Cmd*` functions) belong to their subsystems; this document covers the contract
they must honour. The wire protocol and script wrappers are owned elsewhere.

## Entry points

- `src/command_type.h:Commands` - the enum of every command; `End` must stay last.
- `src/command_type.h:DEF_CMD_TRAIT` - binds a `Commands` value to its proc, `CommandFlags` and
  `CommandType`; one line per command in a `*_cmd.h` header (e.g. `src/misc_cmd.h:CmdPause`).
- `src/command_func.h:Command` - `Command<Commands::X>` alias of `CommandHelper`; the API callers
  use.
- `src/command_func.h:CommandHelper::Post` - top-level, network-safe entry for players, AI/GS,
  console and the fork's MCP server; `PostFromNet` is the network re-entry.
- `src/command_func.h:CommandHelper::Do` - nested call from inside a handler or from the game loop.
- `src/command.cpp:_command_table` - generated from the traits; `GetCommandFlags`, `GetCommandName`,
  `IsCommandAllowedWhilePaused`.
- Routing pointers (owned by other docs): `src/network/network_command.cpp:NetworkSendCommand`,
  `src/script/api/script_object.hpp:ScriptDoCommandHelper`.
- No fork hunk in the tracked files. Fork callers post existing commands only
  (`Command<Commands::Pause>::Post` in `src/prismatic_mcp/prismatic_mcp_server.cpp`).

## Lifecycle

- `Post` -> `InternalPost`: out-of-map tile check, `InternalPostBefore` (estimate only when Shift is
  held for the local company, outside world generation, not from the network and not `NoEst`; pause
  gate via `IsCommandAllowedWhilePaused`, which always allows the scenario editor), fill `ClientID`
  args, then `Execute`.
- Scripts do not take `Post`: `src/script/api/script_object.hpp:ScriptObject::ScriptDoCommandHelper`
  calls `src/command_func.h:CommandHelper::Unsafe`, which enters `Execute` directly, so the pause gate
  and estimate handling of `InternalPostBefore` do not apply to the AI/GS route.
- `Execute`: `InternalExecutePrepTest` (switch to spectator for `Server`/`Spectator` commands,
  reject invalid companies, enter test mode) -> test call ->
  `InternalExecuteValidateTestAndPrepExec`. In a network game and not from the network, the command
  is sent with `NetworkSendCommand` and returns an empty result; otherwise it runs again with
  `DoCommandFlag::Execute` and `InternalExecuteProcessResult` charges money, records
  `last_build_coordinate`, sets `PauseMode::CommandDuringPause` if paused (not for
  `CommandType::ServerSetting` commands), and flushes signals.
- `Do`: test pass, then an execute pass only when `DoCommandFlag::Execute` is given and the test
  succeeded; nested calls with `Execute` skip the test. `Do` does none of `Execute`'s preparation
  (spectator switch, test/execute cost assertion, command logging).

## Invariants

- **[verified]** Test and execute passes must return the same cost and error state unless the trait
  has `CommandFlag::NoTest`; asserted in
  `src/command.cpp:CommandHelperBase::InternalExecuteProcessResult`.
- **[verified]** `Post`/`Execute` must be top level: `assert(counter.IsTopLevel())` in
  `src/command_func.h:CommandHelper::Execute`; inside a handler use `Command<>::Do`.
- **[verified]** `Server` and `Spectator` commands run as `COMPANY_SPECTATOR`; any other command in
  `GameMode::Normal` needs a valid `_current_company` (or `OWNER_DEITY` with `CommandFlag::Deity`)
  (`src/command.cpp:CommandHelperBase::InternalExecutePrepTest`).
- **[verified]** Which commands work while paused is decided per `CommandType` against
  `construction.command_pause_level` (`src/command.cpp:IsCommandAllowedWhilePaused`).
- **[verified]** Commands issued during world generation are executed locally, never sent
  (`_generating_world` in
  `src/command.cpp:CommandHelperBase::InternalExecuteValidateTestAndPrepExec`).
- **[supported]** "The test-run of a command alters the gamestate" and "altered ... without using
  commands" are listed desync causes (`docs/desync.md`).
- **[verified]** A callback passed to `Post` must be registered in
  `src/network/network_command.cpp:_callback_tuple` (asserted via `IsNetworkRegisteredCallback`).

## Extension points

- New command: add a value before `End` in `src/command_type.h:Commands`, declare
  `CommandCost CmdX(DoCommandFlags flags, ...)` and a `DEF_CMD_TRAIT` line in the subsystem's
  `*_cmd.h`, implement it guarding all side effects with `flags.Test(DoCommandFlag::Execute)`. A new
  `*_cmd.h` must be included by `src/command.cpp` and `src/network/network_command.cpp`, whose
  tables instantiate `CommandTraits` for every value up to `End`.
- Returning extra data: a `std::tuple<CommandCost, ...>` return type; a `Money` second element is
  the extra-cash convention (`src/command_func.h:CommandHelper::ExtractAdditionalMoney`).
- New flag/type: `src/command_type.h:CommandFlag`, `src/command_type.h:CommandType` (the latter also
  needs a row in `IsCommandAllowedWhilePaused`'s `static_assert`ed lookup table).

## Traps and forbidden operations

- Never mutate pools, map or company state outside a `Cmd*` handler's execute pass in response to a
  player, script or network action. The exceptions are the engine's own deterministic paths that every
  peer runs identically: the game loop, world generation and loading (`src/saveload/saveload.cpp:DoLoad`
  → `src/misc.cpp:InitializeGame`); see `docs/desync.md`.
- `*_cmd.cpp`/`*_cmd.h` naming is not a reliable index: `src/tile_cmd.h`, `src/aircraft_cmd.h`,
  `src/ship_cmd.h` hold no command traits, and handlers also live in `src/settings.cpp`,
  `src/story.cpp`, `src/goal.cpp`, `src/engine.cpp`. Find a handler with
  `git grep -n 'DEF_CMD_TRAIT(Commands::X' -- src`.
- In any network game (server included) `Post` returns success once the test pass succeeds and the
  command is queued; the effect arrives in a later frame. Report dispatch separately from the
  observed effect (`project.game-state-mutates-only-through-commands`).
- `Commands` values go over the wire as `uint16` (`src/network/network_command.cpp`); the enum is
  `uint8_t`. Measure headroom with `git grep -h 'DEF_CMD_TRAIT(Commands::' -- src | wc -l`.
- `docs/desync.md` still names `DoCommandP`; that function no longer exists, read it as `Post`.

## Tests and validation

- No Catch2 test targets commands. `regression_ai`/`regression_game` issue commands through the
  script API (`regression/regression/main.nut`, `regression/gs/main.nut`).
- Tier: `tools/gate.sh change`; `tools/gate.sh smoke -R regression_` while iterating.
- Debug: `-d desync=1` logs each command that reaches `Execute` with encoded args
  (`src/command.cpp:CommandHelperBase::LogCommandExecution`); nested `Do` calls and commands rejected
  before `Execute` are not logged.

## Decisions, rules and knowledge

- Upstream architecture: [ADR 0016](../../../adrs/0016-multiplayer-is-deterministic-lockstep-every-peer-runs-the-sa.md) (deterministic lockstep).
- Rules: `project.game-state-mutates-only-through-commands`,
  `project.research-code-is-gated-and-labelled`, `project.never-report-unrun-validation-as-pass`.
- ADR 0008 (all mutation through `Command<>::Post`, no new command for MCP convenience), ADR 0007.
- Knowledge: `bridge-control-is-coarse-ai-restart`.
- Upstream docs: `docs/desync.md`, `docs/debugging_desyncs.md`.

## Typical tasks

- To add a command: start at `src/command_type.h:Commands`, add the trait in the subsystem
  `*_cmd.h`, include that header in `src/command.cpp` and `src/network/network_command.cpp` if new,
  validate with `tools/gate.sh change`.
- To change a handler's cost or checks: start at the `Cmd*` function found via its `DEF_CMD_TRAIT`,
  keep test and execute equal (or set `NoTest`), validate with `-d desync=1`.
- To issue a command from research code: use `Command<Commands::X>::Post` from the game loop behind
  a research guard, verify the effect separately, validate with `tools/gate.sh research`.
- To allow a command while paused: change its `CommandType` or `IsCommandAllowedWhilePaused`, never
  bypass `InternalPostBefore`.
