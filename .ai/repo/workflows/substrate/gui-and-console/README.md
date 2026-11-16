---
schema: context/v1
id: fork.substrate.gui-and-console
kind: context
title: "GUI and console: windows, widgets, viewport, input, hotkeys and the in-game console"
description: "Read before adding or changing a window, a widget tree, viewport behaviour, a hotkey or a console command, or when a feature must also work on a dedicated server."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/window*'
  - 'src/widget.cpp'
  - 'src/widget_type.h'
  - 'src/widgets/**'
  - 'src/viewport*'
  - 'src/tilehighlight_*'
  - 'src/hotkeys.*'
  - 'src/console*'
  - 'src/dropdown*'
  - 'src/screenshot.cpp'
  - 'src/screenshot.h'
  - 'src/video/dedicated_v.*'
  - 'src/video/null_v.*'
---

# GUI and console

## Purpose and ownership

Owns everything a local person sees and types: the window stack, nested widget trees, the
viewport camera and tile highlighting, mouse and keyboard dispatch, hotkeys, and the console
(GUI window, stdin on a dedicated server, rcon target). It holds no game state of its own and
changes game state only by posting commands.

## Entry points

- `src/window_gui.h:Window` - base of every window; behaviour lives in the virtual `On*` events (`OnClick`, `OnKeyPress`, `OnHotkey`, `OnInvalidateData`, `OnGameTick`, `OnRealtimeTick`, `OnPlaceObject`, `DrawWidget`, `UpdateWidgetSize`, `GetWidgetString`).
- `src/window_gui.h:WindowDesc` - static description (position, `ini_key`, `WindowClass`, widget parts, hotkeys); every instance registers in `src/window.cpp:_window_descs`.
- `src/window_gui.h:AllocateWindowDescFront` - open-or-focus helper; windows build their tree with `Window::InitNested` (`CreateNestedTree` + `FinishInitNested`).
- `src/widget_type.h:NWidget` / `EndContainer` / `SetStringTip` - the `NWidgetPart` list a `WindowDesc` points at, turned into a tree by `src/widget.cpp:MakeWindowNWidgetTree`. Widget ids are enums in the `src/widgets/` directory (e.g. `src/widgets/console_widget.h:ConsoleWidgets`).
- `src/window.cpp:HandleMouseEvents` / `HandleKeypress` - input dispatch. Keys go to a focused editbox, then front-to-back through each window's `HotkeyList` and `OnKeyPress`, then the main toolbar, then `src/hotkeys.cpp:HandleGlobalHotkeys`.
- `src/viewport.cpp:HandleViewportClicked` - a click on the map: vehicle select, `PlaceObject` (to the window that called `src/viewport.cpp:SetObjectToPlace`), sign, then landscape.
- `src/viewport.cpp:ScrollWindowTo`, `src/viewport.cpp:ScrollMainWindowToTile`, `src/smallmap_gui.cpp:ScrollMainWindowTo` (defined in the smallmap file, declared in `src/viewport_func.h`), `src/main_gui.cpp:DoZoomInOutWindow` - camera and zoom.
- `src/console.cpp:IConsoleCmdExec` - tokenises a line and runs a command (after its hook) or an alias; the single path for GUI console, dedicated stdin, rcon, admin rcon and `exec` scripts.
- `src/console_cmds.cpp:IConsoleStdLibRegister` - registers every built-in command with `IConsole::CmdRegister(name, proc, hook)`.
- `src/console_gui.cpp:IConsoleWindow` - the GUI console window; `src/console_gui.cpp:IConsoleSwitch` toggles it.
- `src/video/dedicated_v.cpp:DedicatedHandleKeyInput` - stdin line to `IConsoleCmdExec` on a dedicated server.
- **[fork]** `src/console_cmds.cpp:ConResearchStatus` (`research_status`, under `OTTD_RESEARCH_INSTRUMENTATION`) and `src/console_cmds.cpp:ConPrismaticMcp` (`prismatic_mcp`, under `OTTD_PRISMATIC_MCP_SERVER`), both registered at the end of `IConsoleStdLibRegister`.
- **[fork]** `src/dropdown.cpp:DropdownItems` (declared in `src/prismatic_mcp/prismatic_mcp_hooks.h:DropdownItems`) reads an open `DropdownWindow` read-only; `src/screenshot.cpp:QueueQuietScreenshot` queues a silent capture on the main thread. Both gated by `OTTD_PRISMATIC_MCP_SERVER`. `src/prismatic_mcp/prismatic_mcp_ui.cpp` drives the GUI by setting `_cursor` and calling `HandleMouseEvents`.

## Lifecycle

- Init: `src/openttd.cpp:openttd_main` calls `InitWindowSystem`; after the NewGRF scan, `src/openttd.cpp:AfterNewGRFScan::OnNewGRFsScanned` runs `LoadHotkeysFromConfig`, `WindowDesc::LoadFromConfig` (`hotkeys.cfg`, `windows.cfg`; paths in `src/fileio.cpp:_windows_file`), `IConsoleInit` (which calls `IConsoleStdLibRegister`) and execs `autoexec.scr`. `src/openttd.cpp:PostMainLoop` saves both files again.
- Per tick: `src/video/video_driver.cpp:VideoDriver::Tick` runs `GameLoop`, and on a draw tick (holding `game_state_mutex`) `DrainCommandQueue`, the driver's input poll, `InputLoop`, `UpdateWindows`. `src/window.cpp:InputLoop` deletes closed windows, then `HandleMouseEvents`. `src/window.cpp:UpdateWindows` runs `TimerWindow` timers, `OnRealtimeTick`, scheduled resizes and invalidations, then `DrawDirtyBlocks` and `UpdateViewportPosition`.
- Game ticks: `src/openttd.cpp:StateGameLoop` calls `src/window.cpp:CallWindowGameTickEvent`, i.e. every window's `OnGameTick` runs inside the simulation step.
- Teardown: `src/window.cpp:Window::Close` hides the window and resets `_thd` placement if it owned it; the object is destroyed later in `Window::DeleteClosedWindows`. `UnInitWindowSystem` on exit.
- Save/load: windows are never saved in a savegame; only `windows.cfg` (size/sticky per `ini_key`) and `hotkeys.cfg`.

## Invariants

- **[verified]** GUI code changes game state only through `Command<...>::Post` (e.g. `src/rail_gui.cpp` posts `Commands::BuildRailDepot` with `CcRailDepot`). In multiplayer `Post` only sends: `src/command.cpp:CommandHelperBase::InternalPostBefore` sets `only_sending`, so the callback fires later, on the issuing client only, when the command executes.
- **[verified]** Any callback passed to `Post` must be in `_callback_tuple` in `src/network/network_command.cpp`; `src/command_func.h:Post` asserts `IsNetworkRegisteredCallback`.
- **[verified]** `ini_key` values are unique, a window has an `ini_key` iff it has a defsize or sticky box, and every widget part list is closed and builds (`src/tests/test_window_desc.cpp`).
- **[supported]** `OnInvalidateData` called from command scope (`gui_scope = false`) may not test-run commands nor assume `_current_company == _local_company`; it runs again in GUI scope at the next redraw (comment on `src/window.cpp:InvalidateWindowData`).
- **[verified]** A dedicated server still runs window timers, resizes and invalidations but draws nothing: `src/window.cpp:UpdateWindows` returns before `DrawDirtyBlocks` when `_network_dedicated`. The GUI sources are compiled into every build (listed unconditionally in `src/CMakeLists.txt`); `OPTION_DEDICATED` (`cmake/Options.cmake`, defines `DEDICATED`) drops only the video, sound and music backends (`src/video/CMakeLists.txt`) and defaults the blitter to `null` (`src/blitter/factory.hpp:GetBlitterFactory`).
- **[verified]** Console names ignore underscores: `IConsole::CmdRegister` and `CmdGet` key on `RemoveUnderscores(name)` in `src/console.cpp`. A proc returning `false` is re-called with empty `argv` to print help.
- **[verified]** On a dedicated server `src/console.cpp:IConsolePrint` writes to stdout and the admin port, never the GUI; during rcon it writes only to the requester.
- **[inferred]** `OnGameTick` must not touch game state or `Random()`: it runs inside `StateGameLoop`, but the set of open windows differs per client (from `CallWindowGameTickEvent` and the lockstep rule).

## Extension points

- New window: widget enum in a `src/widgets/*_widget.h` file, a `static constexpr NWidgetPart` list and a `static WindowDesc` in the feature's `*_gui.cpp`, a `WindowClass` value in `src/window_type.h:WindowClass`, open it with `AllocateWindowDescFront`.
- New hotkey set: a `static inline HotkeyList hotkeys{"<ini_group>", {...}}` member referenced from the `WindowDesc` (pattern in `src/toolbar_gui.cpp` and `src/main_gui.cpp`); `Window::OnHotkey` treats a hotkey number as a widget id to click.
- New console command: a `static bool Con<Name>(std::span<std::string_view> argv)` in `src/console_cmds.cpp`, registered in `IConsoleStdLibRegister`, with a hook such as `ConHookServerOnly` or `ConHookNoNetwork` (`src/console_internal.h:ConsoleHookResult`). Research-only commands sit under their `#ifdef`.
- New command callback for a GUI action: declare it in the command's `*_cmd.h`, define it in the `*_gui.cpp`, append it to `_callback_tuple` in `src/network/network_command.cpp`.

## Traps and forbidden operations

- Widget enums are exported to scripts: `src/script/api/script_window.hpp.in` pulls every `src/widgets/*_widget.h` enum through its `@enum` lines. Renumbering or renaming a widget changes the `ScriptWindow` API. [verified from the `@enum` line and `src/script/api/CMakeLists.txt`]
- Do not act on `Post`'s return value as if the command had run; in multiplayer it only means "sent". Follow-up work belongs in the callback.
- Holding Shift turns a `Post` into a cost estimate (`InternalPostBefore`, `estimate_only`) unless the command has `CommandFlag::NoEst`; synthetic input must not leave Shift pressed.
- A console hook sees the executing process, not the rcon caller: rcon runs `ConHookServerOnly` commands as the server.
- `src/video/null_v.cpp:VideoDriver_Null::MainLoop` never calls `DrainCommandQueue`, so `QueueOnMainThread` work (including the fork's `QueueQuietScreenshot`) never runs under `-v null`.
- `src/video/dedicated_v.cpp:VideoDriver_Dedicated::MainLoop` reads stdin only when input is waiting; console input sent before startup can be lost (knowledge `dedicated-server-stdin-startup-race`).

## Tests and validation

- `src/tests/test_window_desc.cpp` (`WindowDesc - *` cases): `tools/gate.sh smoke -R WindowDesc`, then `tools/gate.sh change`. Fork console commands and hooks: `tools/gate.sh research`, which proves the research console command is present.
- No automated test opens a window or drives input; a visual change needs a GUI run (or the MCP UI tools) and is reported as such.
- Debug: `-d console=N` (`IConsoleCmdExec` logs each line at Info and tokens at Trace2); `-d misc=N` for vehicle-click logging in `HandleViewportClicked`; `-d driver=N` for the video driver.

## Decisions, rules and knowledge

- The feature windows built on this framework (toolbar, main, smallmap, graphs, company, error, picker windows): [game windows](../game-windows/README.md).
- Upstream architecture: [ADR 0020](../../../adrs/0020-player-facing-text-is-compiled-from-english-txt-by-strgen-an.md) (strings and translations), [ADR 0022](../../../adrs/0022-platform-backends-are-drivers-registered-in-factories-and-ch.md) (runtime-selected drivers).
- Neighbouring maps: [game-windows](../game-windows/README.md), [graphics-and-sprites](../graphics-and-sprites/README.md), [strings-and-languages](../strings-and-languages/README.md).
- Rules: `project.game-state-mutates-only-through-commands`, `project.research-code-is-gated-and-labelled`, `project.mcp-surfaces-are-derived`, `project.never-report-unrun-validation-as-pass`.
- ADR 0004 (research access to internals, gated and removable), ADR 0008 (embedded MCP server; the fork's UI and view hooks serve it).
- Knowledge: `dedicated-server-stdin-startup-race`, `gated-code-is-invisible-to-a-default-build`, `macos-missing-baseset-hangs-headless-runs`.
- Upstream docs: none specific; `CODINGSTYLE.md` for naming.

## Typical tasks

- To add a button to a window: start at the widget enum in its `src/widgets/*_widget.h`, add the part to the window's `NWidgetPart` list, handle it in `OnClick`; validate with `tools/gate.sh smoke -R WindowDesc` and a GUI run.
- To make a GUI action change the game: start at `src/command_func.h:Post`, pass a registered `Cc*` callback for follow-up; validate in single player and, if it matters, a two-process network game.
- To add a console command: start at `src/console_cmds.cpp:IConsoleStdLibRegister`, choose a hook, keep it read-only or route mutation through `Post`; validate with `tools/gate.sh change` (or `research` when gated) and a `-D` run that feeds the command on stdin.
- To move the camera or highlight a tile programmatically: start at `src/viewport.cpp:ScrollMainWindowToTile` and `src/viewport.cpp:SetRedErrorSquare` / `_thd` (`src/tilehighlight_type.h:TileHighlightData`); validate in a GUI build, as the dedicated build draws nothing.
