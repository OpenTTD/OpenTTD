---
schema: context/v1
id: fork.substrate.core-loop
kind: context
title: "Core loop: process lifecycle, the real-time pump, the game tick, timers and pause"
description: "Read before touching startup/shutdown, GameLoop/StateGameLoop, a video driver's MainLoop, a timer, the date, world generation or pause handling."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/openttd.cpp'
  - 'src/openttd.h'
  - 'src/video/**'
  - 'src/timer/**'
  - 'src/genworld*'
---

# Core loop

## Purpose and ownership

Owns the process lifecycle (`openttd_main` to `PostMainLoop`), the real-time pump in the video
drivers, the per-tick simulation step, the timer framework (tick, calendar, economy, realtime,
window), game-mode switching, world generation entry and the pause state. It does not own what runs
inside a tick (vehicles, tiles, economy) or the network frame logic, only their ordering.

## Entry points

- `src/openttd.cpp:openttd_main` - startup; ends in the video driver's `MainLoop`.
- `src/openttd.cpp:GameLoop` - one real-time tick: realtime timers, `SwitchToMode`, network
  background, then `NetworkGameLoop` (multiplayer) or `StateGameLoop` (single player), sound/music.
- `src/openttd.cpp:StateGameLoop` - one simulation tick; `src/openttd.cpp:SwitchToMode` applies a
  pending `SwitchMode` (new game, load, save, menu).
- `src/video/video_driver.cpp:VideoDriver::Tick` / `VideoDriver::GameThread` - the pump; the
  per-driver `MainLoop` (`src/video/dedicated_v.cpp:VideoDriver_Dedicated::MainLoop`,
  `src/video/null_v.cpp:VideoDriver_Null::MainLoop`, cocoa/win32/sdl3) calls them.
- `src/genworld.cpp:GenerateWorld`; headless: `src/genworld_gui.cpp:StartNewGameWithoutGUI`.
- `src/timer/timer.h:IntervalTimer`, `src/timer/timer.h:TimeoutTimer` - the only scheduling API.
- **[fork]** three `OTTD_PRISMATIC_MCP_SERVER` hunks in `src/openttd.cpp`: `PrismaticMCP::Init`
  after `NetworkStartUp`, `Poll` in `GameLoop` after `NetworkBackgroundLoop`, `Shutdown` in
  `ShutdownGame` (declared in `src/prismatic_mcp/prismatic_mcp.h:Poll`).

## Lifecycle

- Init: `openttd_main` -> `LoadFromConfig` -> driver selection -> `NetworkStartUp` ->
  `HandleBootstrap` -> base sets -> `GenerateWorld(GWM_EMPTY)` + `LoadIntroGame` ->
  `RequestNewGRFScan` -> `MainLoop`. `-D` selects the dedicated driver, whose `MainLoop` calls
  `StartNewGameWithoutGUI` unless `-g` already set `SwitchMode::LoadGame`/`StartHeightmap`.
- Per tick: `VideoDriver::GameLoop` advances `next_game_tick` by `GetGameInterval()` and calls
  `::GameLoop` under `game_state_mutex`. `StateGameLoop` order: `CheckCaches`,
  `AnimateAnimatedTiles`, calendar timers (+`RunVehicleCalendarDayProc` on a new day), economy
  timers, `TimerManager<TimerGameTick>::Elapsed(1)`, `RunTileLoop`, `CallVehicleTicks`,
  `CallLandscapeTick`, `AI::GameLoop`, `Game::GameLoop`, window tick events, `NewsLoop`.
- Paused (`_pause_mode.Any()` or `HasModalProgress()`): `StateGameLoop` returns early but still runs
  `Game::GameLoop` in `GameMode::Normal`.
- Teardown: `_exit_game` -> `PostMainLoop` (wait for saves, `SaveToConfig`) -> `ShutdownGame`.
- Save/load: date, tick counter, `_pause_mode` are in the `DATE` chunk (`src/saveload/misc_sl.cpp`).

## Invariants

- **[verified]** Simulation advances only in `StateGameLoop`; `GameLoop` runs it only when not
  networking, otherwise `NetworkGameLoop` drives it (`src/openttd.cpp:GameLoop`).
- **[verified]** Every tick-side action runs as `OWNER_NONE` (`AutoRestoreBackup cur_company` in
  `src/openttd.cpp:StateGameLoop`), and the function asserts `IsLocalCompany()` on exit.
- **[verified]** Only one calendar/economy timer may exist per (trigger, priority) and one tick
  timer per priority unless it is `None`, asserted only under `WITH_ASSERT`
  (`src/timer/timer_manager.h:TimerManager::RegisterTimer`); release builds then run colliding
  timers in pointer order. Those timers may call `Random()`
  (`src/timer/timer_game_common.h:TimerGame::Priority`).
- **[verified]** Realtime timers are wall-clock and advance only in `GameMode::Normal`
  (`src/openttd.cpp:GameLoop`); `Trigger::Autosave` keeps ticking during a pause only after a
  command set `PauseMode::CommandDuringPause` (`src/timer/timer_game_realtime.cpp:Elapsed`).
- **[verified]** Calendar time can be frozen or slowed by `economy.minutes_per_calendar_year`;
  economy time and the tick counter are not
  (`src/timer/timer_game_calendar.cpp:TimerManager::Elapsed`).
- **[verified]** World generation seeds `_random` from `game_creation.generation_seed` and runs as
  `OWNER_NONE` with `_generating_world` set (`src/genworld.cpp:_GenerateWorld`).

## Extension points

- New periodic game logic: a static `IntervalTimer<TimerGameEconomy>` (or `TimerGameCalendar`,
  `TimerGameTick`) next to the logic it drives, with a new `Priority` value in
  `src/timer/timer_game_common.h` (or `src/timer/timer_game_tick.h`) if it may call `Random()`.
- Wall-clock behaviour (UI, autosave): `IntervalTimer<TimerGameRealtime>` with a
  `TimerGameRealtime::Trigger`; GUI blinking: `src/timer/timer_window.h:TimerWindow`.
- A new pause reason: `src/openttd.h:PauseMode` plus the switch in `src/misc_cmd.cpp:CmdPause` (see
  Traps: the bitset is full).
- Cross-thread work onto game state: `src/video/video_driver.hpp:VideoDriver::QueueOnMainThread`.

## Traps and forbidden operations

- On threaded GUI drivers (cocoa, win32, sdl3, allegro) `::GameLoop` runs on the `ottd:game` thread
  under `game_state_mutex`, not the process main thread; drawing takes the same mutex in
  `VideoDriver::Tick`. The dedicated driver sets `is_game_threaded = false`
  (`src/video/video_driver.cpp:VideoDriver::StartGameThread`). Code in `GameLoop`, including the
  fork's `PrismaticMCP::Poll`, is serialized with drawing by that mutex.
- `VideoDriver_Null::MainLoop` runs exactly `ticks` iterations with no sleep: a harness, not
  real-time pacing.
- `src/openttd.h:PauseModes` is an `EnumBitSet<PauseMode, uint8_t>` whose eight bits are all taken,
  and `pause_mode` is saved as `VarFileType::U8` (`src/saveload/misc_sl.cpp`). A new `PauseMode`
  therefore needs a wider bitset and a version-gated saved field.

## Tests and validation

- No Catch2 test covers the loop or timers (`src/tests/` has none). `regression_ai` and
  `regression_game` (`cmake/scripts/Regression.cmake`) run `-vnull:ticks=30000` through
  `VideoDriver_Null::MainLoop`, so they exercise `GameLoop`/`StateGameLoop` end to end.
- Tier: `tools/gate.sh change`; `tools/gate.sh research` for the `OTTD_PRISMATIC_MCP_SERVER` hunks.
- Debug: `-d driver=1` (game-thread choice), `-d desync=2` (`src/cachecheck.cpp:CheckCaches` every
  tick); frame timings via `docs/logging_and_performance_metrics.md`.

## Decisions, rules and knowledge

- Rules: `project.game-state-mutates-only-through-commands`,
  `project.research-code-is-gated-and-labelled`, `project.never-report-unrun-validation-as-pass`.
- ADR 0008 (MCP serviced from `GameLoop()`), ADR 0004.
- Knowledge: `macos-missing-baseset-hangs-headless-runs`, `dedicated-server-stdin-startup-race`,
  `heightmap-flag-is-lowercase-g`, `gated-code-is-invisible-to-a-default-build`; upstream docs:
  `docs/desync.md`, `docs/logging_and_performance_metrics.md`.

## Typical tasks

- To add a monthly economic effect: start at `src/timer/timer_game_common.h:TimerGame::Priority`,
  add a static `IntervalTimer<TimerGameEconomy>` beside the logic, validate with
  `tools/gate.sh change`.
- To observe per-tick state for an experiment: start at `src/openttd.cpp:StateGameLoop`, read (never
  write) state behind a research guard, validate with `tools/gate.sh research`.
