---
schema: context/v1
id: fork.substrate.scripting
kind: context
title: "Scripting: the Squirrel VM, the generated AI/GS API, AI and Game Script lifecycles, and the regression suite"
description: "When a change touches a Script API class, the script VM or scheduler, AI/GS loading and versioning, or the regression scripts."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/script/**'
  - 'src/ai/**'
  - 'src/game/**'
  - 'src/3rdparty/squirrel/**'
  - 'bin/ai/**'
  - 'bin/game/**'
  - 'regression/**'
  - 'cmake/CreateRegression.cmake'
  - 'cmake/scripts/Regression.cmake'
  - 'cmake/scripts/SquirrelExport.cmake'
  - 'cmake/scripts/SquirrelIncludes.cmake'
---

## Purpose and ownership

Embeds Squirrel (`src/script/squirrel.cpp:Squirrel`), exposes the C++ Script API under
`src/script/api/` to two script kinds - company AIs (`src/ai/`) and the single Game Script
(`src/game/`) - schedules them with an opcode budget, and turns every state-changing API call into
a regular command. `regression/` drives that API end to end under ctest. All upstream code.

## Entry points

- `src/ai/ai_core.cpp:AI::GameLoop` / `src/game/game_core.cpp:Game::GameLoop` - per-tick drivers called from `src/openttd.cpp:StateGameLoop`.
- `src/script/script_instance.cpp:ScriptInstance::GameLoop` - suspend bookkeeping, constructor/`Load`/`Start` on first run, then `Squirrel::Resume` with the opcode budget.
- `src/script/api/script_object.hpp:ScriptObject::ScriptDoCommandHelper` (used as `ScriptObject::Command<Commands::X>::Do(...)`) - the only bridge from an API call to a command; `src/script/api/script_object.cpp:ScriptObject::DoCommandPrep` / `DoCommandProcessResult` wrap it.
- `src/ai/ai_instance.cpp:CcAI`, `src/game/game_instance.cpp:CcGame` - network command callbacks that resume a suspended script (`ScriptInstance::DoCommandCallback`, `ScriptInstance::Continue`).
- `src/ai/ai_core.cpp:AI::StartNew` (from `src/company_cmd.cpp` and afterload), `src/game/game_core.cpp:Game::StartNew` (from `src/genworld.cpp` and afterload).
- `src/script/script_scanner.hpp:ScriptScanner` - finds `info.nut`/`library.nut`; `AI::Rescan`, `Game::Rescan`.
- `src/script/api/script_event.cpp:ScriptEventController::InsertEvent` with `AI::NewEvent` / `Game::NewEvent` - game -> script event queue.
- **[fork]** No fork hunk here. The fork's Prismatic bridge drives a company AI over rcon (ADR 0007); `src/prismatic_mcp/` does not call the Script API.

## Lifecycle

- Init: `AI::Initialize` / `Game::Initialize` (`src/openttd.cpp`, `src/misc.cpp`) create scanners. A new instance runs `AIInstance::RegisterAPI` (`SQAI_RegisterAll`) / `GameInstance::RegisterAPI` (`SQGS_RegisterAll`), then `ScriptInstance::LoadCompatibilityScripts`.
- Per tick, not paused: `AI::GameLoop` then `Game::GameLoop`. AIs tick only every `2^(4-competitor_speed)` frames, each under `_current_company = c->index`; the GS runs as `OWNER_DEITY`. While paused only `Game::GameLoop` runs (`src/openttd.cpp:StateGameLoop`).
- Command path: `DoCommandProcessResult` throws `Script_Suspend`. Single player: suspend for the command delay. Network: suspend negatively until `CcAI`/`CcGame` -> `ScriptInstance::Continue`.
- Teardown: `AI::Stop`, `AI::Uninitialize`, `Game::Uninitialize`; `ScriptInstance::Died` releases the VM.
- Save/load: chunks `AIPL` (`src/saveload/ai_sl.cpp`), `GSDT`/`GSTR` (`src/saveload/game_sl.cpp`), `SRND` script randomizers (`src/saveload/randomizer_sl.cpp`). `ScriptInstance::Save` serialises the script's `Save()` table (bounded by `MAX_SL_OPS`, nesting `SQUIRREL_MAX_DEPTH`).

## Invariants

- **[verified]** Scripts run only where they may issue commands: `AI::GameLoop` returns on clients and on servers without `ai_in_multiplayer`; `Game::GameLoop` returns on clients. Script VM state is therefore not lockstep state.
- **[supported]** In a desync replay, scripts are not run, only their recorded commands; their saved data differs (`docs/desync.md`, section 3.3).
- **[verified]** Every state change from a script goes through `::Command<Tcmd>::Unsafe` inside `ScriptDoCommandHelper::Execute`; tile arguments out of map bounds are rejected first, and `ClientID` arguments are filled for `CommandFlag::ClientID` commands.
- **[verified]** No DoCommand in a constructor, `Save()`, `Load()` or a valuator: `ScriptObject::DoCommandPrep` throws `Script_FatalError` when `ScriptObject::CanSuspend` is false (`ScriptObject::DisableDoCommandScope` wraps the constructor and `Load`).
- **[verified]** In network play a command callback whose arguments differ from the stored last command terminates the script (`ScriptInstance::DoCommandCallback`, `ScriptObject::CheckLastCommand`).
- **[verified]** Script randomness never touches the game's `_random`: `src/script/api/script_object.cpp:ScriptObject::GetRandomizer` uses per-owner randomizers seeded from a copy of `_random` in `ScriptObject::InitializeRandomizers`.
- **[verified]** Budgets: `script_max_opcode_till_suspend` per resume, `MAX_CONSTRUCTOR_OPS`/`MAX_SL_OPS`/`MAX_GET_OPS` (`src/script/script_info.hpp`), `MAX_VALUATE_OPS` (`src/script/api/script_list.hpp`), memory via `script_max_memory_megabytes` in `src/script/squirrel.cpp:ScriptAllocator`. Settings live in `src/table/settings/script_settings.ini`.
- **[verified]** API downgrade order: `ScriptInstance::LoadCompatibilityScripts` loads compat_<v>.nut from the newest-but-one version down to the script's own API version; the last entry of `src/ai/ai_info.hpp:AIInfo::ApiVersions` / `src/game/game_info.hpp:GameInfo::ApiVersions` is the current API and has no compat file.

## Extension points

- New API function: add a `static` method to the class header script_<x>.hpp in the `src/script/api/` directory (e.g. `src/script/api/script_vehicle.hpp`) under a class tagged `@api ai game` (member-level `@api -game`/`@api -ai` removes it from one API), implement in script_<x>.cpp with `EnforcePrecondition`/`EnforceCompanyModeValid` (`src/script/api/script_error.hpp`) and `ScriptObject::Command<...>::Do`. `cmake/scripts/SquirrelExport.cmake` generates the `.sq.hpp` binding; no hand registration.
- Error mapping: an enum entry commented `// [STR_ERROR_...]` in the API header is turned into `ScriptError::RegisterErrorMap` by `SquirrelExport.cmake`.
- `@suspendable` doc tag: the binding is wrapped in a Squirrel re-call loop (`src/script/squirrel.cpp:Squirrel::AddMethod`).
- API changelog: `src/script/api/ai_changelog.hpp`, `src/script/api/game_changelog.hpp`.
- New API version: append to `ApiVersions` in `src/ai/ai_info.hpp` and `src/game/game_info.hpp`, add compat_<previous>.nut in both the `bin/ai/` and `bin/game/` directories, list them in `bin/ai/CMakeLists.txt` / `bin/game/CMakeLists.txt`, bump `GetAPIVersion` in the regression `info.nut` files.
- New regression case: a folder under `regression/` with `info.nut`, `main.nut`, `result.txt`, `test.sav`, a `CMakeLists.txt` calling `create_regression("ai"|"game" ...)` (`cmake/CreateRegression.cmake`), and an `add_subdirectory` in `regression/CMakeLists.txt`.

## Traps and forbidden operations

- Bypassing `ScriptObject::Command` (writing a pool from an API `.cpp`) breaks multiplayer and the fork rule `project.game-state-mutates-only-through-commands`.
- Calling `Random()`/`_random` from API code advances the server's game RNG but not the clients' (they do not run scripts), which desyncs; use `ScriptObject::GetRandomizer`. [inferred from the server-only `AI::GameLoop` and the separate randomizers]
- A breaking API change without a shim in the previous compat_*.nut silently breaks older AIs; `regression/gs_compat` only proves the compat scripts load.
- The regression run fails on ANY extra stdout or any line diff (`cmake/scripts/Regression.cmake`); a new `print` in an API path changes `result.txt` expectations.
- Generated headers live under the build tree (generated/script/api/ in the build directory); editing them is lost on rebuild.
- An API header without `@api` emits a CMake warning and is published to no API.

## Tests and validation

- Unit: `src/tests/test_script_admin.cpp` (Squirrel <-> JSON for `ScriptAdmin`).
- Regression (ctest names `regression_<folder>`, `RUN_SERIAL`): `regression/regression` (AI, broad API), `regression/stationlist` (AI), `regression/gs` (GS), `regression/gs_compat` (GS on API 1.2 compat). Each runs `openttd -x -c regression/regression.cfg -g test.sav -snull -mnull -vnull:ticks=30000 -d script=2 -Q` and diffs stderr with `result.txt`.
- Tier: `tools/gate.sh change`; `tools/gate.sh smoke -R regression_` for a loop. Base set required, else PARTIAL (knowledge `macos-missing-baseset-hangs-headless-runs`).
- Debug: `-d script=N` (the regression uses 2). Fork coverage: `mcp-api-coverage` measures Script API exposure from the script_*.hpp headers in `src/script/api/` (rule `project.public-api-is-exposed`).

## Decisions, rules and knowledge

- Rules: `project.game-state-mutates-only-through-commands`, `project.public-api-is-exposed`, `project.mcp-surfaces-are-derived`, `project.never-report-unrun-validation-as-pass`.
- ADR 0007 (bridge drives a company AI over rcon), ADR 0008 (embedded MCP server).
- Knowledge: `bridge-control-is-coarse-ai-restart`, `road-stops-and-depots-need-their-entrance-road`, `macos-missing-baseset-hangs-headless-runs`.
- Upstream docs: `docs/desync.md`, `docs/admin_network.md`.

## Typical tasks

- To expose an existing command to scripts: start at the matching script_<x>.hpp in `src/script/api/`, add a static method calling `ScriptObject::Command<Commands::X>::Do`, extend `regression/regression/main.nut` and `result.txt`, validate with `tools/gate.sh change`.
- To debug a script that stalls: start at `src/script/script_instance.cpp:ScriptInstance::GameLoop` (suspend counter), then `CcAI`/`CcGame`; run with `-d script=4`.
- To change API behaviour compatibly: edit the API `.cpp`, add the old behaviour to compat_<prev>.nut in `bin/ai/` (and `bin/game/`), note it in the changelog headers, validate with `regression_gs_compat` and `regression_regression`.
- To add a script event: subclass in `src/script/api/script_event_types.hpp`, insert via `AI::NewEvent`/`Game::NewEvent`, validate with `tools/gate.sh change`.
