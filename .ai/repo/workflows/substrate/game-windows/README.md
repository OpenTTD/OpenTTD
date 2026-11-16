---
schema: context/v1
id: fork.substrate.game-windows
kind: context
title: "Game windows: main window, toolbars, status bar, intro, query and error windows, pickers, smallmap, graphs, company windows, framerate and help"
description: "Read before adding or changing a feature window, a toolbar button or menu entry, a global hotkey, an error or query dialog, a graph, a picker, or a framerate measurement point."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/main_gui.cpp'
  - 'src/toolbar_gui.*'
  - 'src/statusbar_gui.*'
  - 'src/intro_gui.cpp'
  - 'src/misc_gui.cpp'
  - 'src/osk_gui.cpp'
  - 'src/picker*'
  - 'src/querystring_gui.h'
  - 'src/smallmap_gui.*'
  - 'src/graph_gui.*'
  - 'src/company_gui.*'
  - 'src/company_manager_face.h'
  - 'src/livery.h'
  - 'src/framerate_*'
  - 'src/help_gui.*'
  - 'src/date_gui.*'
  - 'src/error*'
  - 'src/bootstrap_gui.cpp'
  - 'src/slider*'
  - 'src/sortlist_type.h'
  - 'src/textfile_*'
  - 'src/screenshot_gui.*'
  - 'src/gui.h'
  - 'src/table/animcursors.h'
  - 'src/table/company_face.h'
---

# Game windows

# Game windows

Read at f8b1d52e80. No fork hunk in the tracked files (`git log upstream/master..HEAD` over them
is empty); the fork reads them from `src/prismatic_mcp/prismatic_mcp_ui.cpp`.

## Purpose and ownership

The feature windows on top of the window framework, which is the
[GUI and console map](../gui-and-console/README.md) (read it first; not repeated here): main
window, toolbars, status bar, title screen, dialogs, pickers, smallmap, graphs, company windows,
framerate, help, plus `FatalError`/`UserError`. Goal, story, league, news, sign, cheat and highscore
windows: [goals, story, news and history](../goals-story-news-and-history/README.md).

## Entry points

- `src/main_gui.cpp:SetupColoursAndInitialWindow` creates `MainWindow`, then
  `ShowSelectGameWindow` or `src/main_gui.cpp:ShowVitalWindows` (`AllocateToolbar`, plus
  `ShowStatusBar` outside the editor). `src/main_gui.cpp:MainWindow` owns the `"global"`
  `HotkeyList` (quit, console, center, transparency, chat, Ctrl+B/I/O overlays).
- `src/toolbar_gui.cpp:AllocateToolbar`: `MainToolbarWindow` or `ScenarioEditorToolbarWindow`
  (class `MainToolbar`, number 0), buttons built in code by `src/toolbar_gui.cpp:MakeMainToolbar`
  via `NWidgetFunction`. Ids in `src/widgets/toolbar_widget.h:ToolbarNormalWidgets`, hotkeys in
  `src/toolbar_gui.h:MainToolbarHotkeys` (group `"maintoolbar"`). Dispatch:
  `src/toolbar_gui.cpp:_toolbar_button_procs` (click), `src/toolbar_gui.cpp:_menu_clicked_procs`
  (dropdown), `src/toolbar_gui.cpp:MainToolbarWindow::OnHotkey`, `OnPlaceObject` for menu tools.
- `src/statusbar_gui.cpp:StatusBarWindow` (date, money, news ticker);
  `src/intro_gui.cpp:SelectGameWindow` (title screen; `ReadIntroGameViewportCommand` turns
  `T<n> <flags> <delay>` signs into a camera script).
- Dialogs: `src/misc_gui.cpp:ShowQueryString`, `src/querystring_gui.h:QueryString`,
  `src/osk_gui.cpp:ShowOnScreenKeyboard`, `src/misc_gui.cpp:GuiShowTooltips`,
  `src/misc_gui.cpp:ShowLandInfo`, `src/misc_gui.cpp:ShowEstimatedCostOrIncome`,
  `src/date_gui.cpp:ShowSetDateWindow`, `src/screenshot_gui.cpp:ScreenshotWindow`.
- Errors: `src/error_gui.cpp:ShowErrorMessage` (its `CommandCost` overload is how a failed `Post`
  reaches the player), `src/error.h:WarningLevel`; fatal paths `src/error_func.h:FatalError`,
  `src/error_func.h:UserError`, `src/error.cpp:AssertFailedError`.
- `src/bootstrap_gui.cpp:HandleBootstrap`, from `src/openttd.cpp:openttd_main` when no base
  graphics set exists: offers a download or fails.
- Pickers: `src/picker_gui.h:PickerCallbacks` (rail stations, road stops, objects, houses),
  `src/picker_gui.h:PickerWindow`; favourites via `src/picker_gui.cpp:PickerLoadConfig`.
- `src/smallmap_gui.cpp:SmallMapWindow`, `src/smallmap_gui.cpp:ScrollMainWindowTo` (every "go to"
  button), `src/graph_gui.cpp:BaseGraphWindow`, `src/company_gui.cpp:CompanyWindow`,
  `CompanyFinancesWindow`, `SelectCompanyLiveryWindow`, `SelectCompanyManagerFaceWindow`,
  `BuyCompanyWindow`; data `src/livery.h:Livery`, `src/company_manager_face.h:FaceSpec`,
  `src/table/company_face.h`.
- Framerate: `src/framerate_type.h:PerformanceElement`, `src/framerate_type.h:PerformanceMeasurer`,
  `src/framerate_type.h:PerformanceAccumulator`; `src/framerate_gui.cpp:FramerateWindow`,
  `src/framerate_gui.cpp:ConPrintFramerate` (console `fps`, `src/console_cmds.cpp:ConFramerate`;
  `fps_wnd` is `src/console_cmds.cpp:ConFramerateWindow`).
- Shared parts: `src/sortlist_type.h:GUIList`, `src/slider_func.h:DrawSliderWidget`,
  `src/textfile_gui.h:TextfileWindow` (by `src/textfile_type.h:TextfileType`),
  `src/help_gui.cpp:HelpWindow`; many `Show*` declarations sit in `src/gui.h`.

## Lifecycle

- Declare: widget enum, `NWidgetPart` list, `static WindowDesc` and a `Show*` function, as in the
  GUI and console map; the toolbar instead builds its tree in `MakeMainToolbar`.
- Act: `OnClick`/`OnDropdownSelect`/`OnQueryTextFinished`/`OnPlaceObject` post a command (e.g.
  `Commands::IncreaseLoan` in `CompanyFinancesWindow`); the effect comes back through the command
  callback and the handler's `InvalidateWindowData` → `OnInvalidateData`.
- Refresh: `IntervalTimer<TimerWindow>` members (wall clock), or game-time timers such as
  `StatusBarWindow::daily_interval` on `TimerGameCalendar`.
- Framerate: `src/openttd.cpp:StateGameLoop` measures `GameLoop` (or marks elements `Paused`),
  subsystems accumulate into theirs, `src/window.cpp:UpdateWindows` measures `Drawing` and drains
  the mixer queue (`src/framerate_gui.cpp:ProcessPendingPerformanceMeasurements`).

## Invariants

- **[verified]** Feature windows change game state by `Command<...>::Post` (loan, livery, face, HQ,
  give money, rename, buy company in `src/company_gui.cpp`; pause in
  `src/toolbar_gui.cpp:ToolbarPauseClick`, which refuses on a non-server client). ADR 0016.
- **[verified]** GUI-side randomness uses `_interactive_random` and posts the result:
  `SelectCompanyManagerFaceWindow` randomises, then posts `Commands::SetCompanyManagerFace`;
  `src/company_cmd.cpp` uses `_random` in command and load paths.
- **[verified]** Upstream exceptions that write state directly rely on no network game: the title
  screen deletes the signs it parsed (`delete Sign::Get`), and the cheat window (other map). The
  `_DEBUG`-only `Alt+1` hotkey posts `Commands::MoneyCheat` only when `!_networking`.
- **[verified]** Toolbar dispatch is positional: `_toolbar_button_procs[widget]` and
  `_menu_clicked_procs[widget]` follow `ToolbarNormalWidgets` with no `static_assert`, and each
  tooltip is `STR_TOOLBAR_TOOLTIP_PAUSE_GAME + widget` (`MakeMainToolbar`).
- **[verified]** `MainToolbarWindow::OnPaint` disables construction buttons for spectators and
  Goal/Story while their pools are empty; on a narrow screen `NWidgetMainToolbarContainer` hides
  buttons with `current_x = 0` and `ToolbarSwitchClick` flips `_toolbar_mode`.
- **[verified]** Framerate timing (`src/framerate_gui.cpp:GetPerformanceTimer`, wall clock) feeds
  only `_pf_data`, never the simulation; `Sound` is measured on the mixer thread behind a mutex.
- **[verified]** `ShowErrorMessage` also prints non-`Info` levels to the console (dedicated logs).

## Extension points

- New window: the [GUI and console](../gui-and-console/README.md) recipe; append the class to
  `src/window_type.h:WindowClass` and widgets to the end of their enum.
- New toolbar button: a `ToolbarNormalWidgets` value before `WID_TN_SWITCH_BAR`; the same index in
  `_toolbar_button_procs` and `_menu_clicked_procs`; a row in `_toolbar_button_sprites`; a tooltip
  at the matching offset from `STR_TOOLBAR_TOOLTIP_PAUSE_GAME`; a slot in every array of
  `NWidgetMainToolbarContainer::GetButtonArrangement`; optionally a `MainToolbarHotkeys` value.
- New global hotkey: a `GHK_*` value and `Hotkey` in `MainWindow::hotkeys`, handled in `OnHotkey`.
- New framerate element: a value before `End` in `PerformanceElement` and an entry in every
  `EnumIndexArray` over it in `src/framerate_gui.cpp` (`_pf_data`, `MEASUREMENT_NAMES`, order list).

## Traps and forbidden operations

- Widget enums and `WindowClass` are script API (`@enum` lines in
  `src/script/api/script_window.hpp.in`): never insert in the middle (knowledge
  `widget-enums-are-part-of-the-script-api`).
- A toolbar value out of step compiles and runs another button's proc with another tooltip
  (candidate `toolbar-dispatch-tables-are-indexed-by-widget-id`).
- **[inferred]** A window's `TimerGameCalendar`/`TimerGameEconomy` timer fires inside the tick on
  only the clients with that window open: keep its callback presentation-only.
- During world generation `MainWindow::OnHotkey` drops every global hotkey except quit
  (`HasModalProgress`), synthetic keys included. `UserError`/`FatalError` never return.
- `HandleBootstrap` fails at once without a screen depth (dedicated, null blitter); knowledge
  `macos-missing-baseset-hangs-headless-runs`.
- **[fork]** What agents see is derived from these windows: `WindowJson` reports `window_class`,
  `window_number`, `ini_key`, caption, and each widget of `widget_lookup` by enum index with
  `src/prismatic_mcp/prismatic_mcp_ui.cpp:TypeName`, rectangle, `GetWidgetString` text, tooltip and
  disabled/lowered state; title-bar boxes have no index. The panel names classes by parsing
  `src/window_type.h` (`tools/research/mcp-panel/server.py:window_classes`, implicit order,
  aliases). So renumbering renames what agents see, a window without `ini_key` has an empty key, a
  hidden toolbar button has zero width, and dropdown items come from
  `src/prismatic_mcp/prismatic_mcp_hooks.h:DropdownItems`. Input is injected only when armed
  (ADR 0014); see the [PrismaticMCP map](../prismatic-mcp/README.md).

## Tests and validation

- `src/tests/test_window_desc.cpp` checks every `WindowDesc` (ini_key, parts):
  `tools/gate.sh smoke -R WindowDesc`, then `tools/gate.sh change`.
- No test opens a window or checks the toolbar tables: a visible change needs a GUI run or
  `tools/research/mcp-view-check` (it also checks class names from `src/window_type.h`).
- Framerate: `fps` (also on a dedicated server), `fps_wnd` in a GUI build; figures explained in
  `docs/logging_and_performance_metrics.md`. Debug: `-d misc=N`, `-d driver=N`, Ctrl+O outlines.

## Decisions, rules and knowledge

- ADR 0016 (lockstep), ADR 0020 (strings; the tooltip offset depends on string order), ADR 0028
  (base sets, bootstrap), ADR 0008 and ADR 0014 (MCP, armed input), ADR 0004.
- Rules: `project.game-state-mutates-only-through-commands`, `project.mcp-surfaces-are-derived`,
  `project.never-report-unrun-validation-as-pass`.
- Knowledge: `widget-enums-are-part-of-the-script-api`, `post-in-a-network-game-only-sends`,
  `armed-input-can-approve-its-own-decisions`, `toolbar-dispatch-tables-are-indexed-by-widget-id`.
- Siblings: [commands](../commands/README.md), [core loop](../core-loop/README.md),
  [economy and companies](../economy-and-companies/README.md).

## Typical tasks

- To add a company-window button: its enum in `src/widgets/company_widget.h`, the part, a `Post` in
  `CompanyWindow::OnClick`; validate with `tools/gate.sh smoke -R WindowDesc` and a GUI run.
- To add a toolbar menu: every place in the extension point; hover it at full and narrow width.
- To measure a subsystem: a `PerformanceElement` and a `PerformanceAccumulator`; check `fps` (`-D`).
- To let an agent find a new window: give it an `ini_key` and a caption; check `openttd.ui.windows`
  through `tools/research/mcp-view-check`.
