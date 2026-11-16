---
schema: context/v1
id: fork.substrate.goals-story-news-and-history
kind: context
title: "Goals, story, news and history: GS goals, story pages and league tables, the news queue, signs, cheats, the gamelog and highscores"
description: "Read before changing a goal, story page, league table, news item or sign (command, pool or window), a cheat, anything that writes or reads the gamelog, or the highscore table."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/goal*'
  - 'src/story*'
  - 'src/league*'
  - 'src/news*'
  - 'src/signs*'
  - 'src/cheat*'
  - 'src/gamelog*'
  - 'src/highscore*'
  - 'src/table/settings/news_display_settings.ini'
  - 'src/saveload/goal_sl.cpp'
  - 'src/saveload/story_sl.cpp'
  - 'src/saveload/league_sl.cpp'
  - 'src/saveload/signs_sl.cpp'
  - 'src/saveload/cheat_sl.cpp'
  - 'src/saveload/gamelog_sl.cpp'
---

# Goals, story, news and history

Read at f8b1d52e80. No fork hunk in the tracked files (`git log upstream/master..HEAD` over them is
empty).

## Purpose and ownership

Three kinds of state that sit beside the simulation. Saved and synced game objects a Game Script
(or, for signs, a player) creates through commands: goals, story pages and elements, league tables
and elements, signs. Per-process presentation state: the news queue and its display settings, the
highscore file. And the savegame's own history: the cheat flags and the gamelog. Their windows are
here too; the window framework is the [GUI and console map](../gui-and-console/README.md), the
toolbar entries the [game windows map](../game-windows/README.md), the script classes the
[scripting map](../scripting/README.md).

## Entry points

- Pools: `src/goal_base.h:Goal`, `src/story_base.h:StoryPage`, `src/story_base.h:StoryPageElement`,
  `src/league_base.h:LeagueTable`, `src/league_base.h:LeagueTableElement`, `src/signs_base.h:Sign`.
- Commands (traits in `src/goal_cmd.h`, `src/story_cmd.h`, `src/league_cmd.h`, `src/news_cmd.h`,
  `src/signs_cmd.h`): `src/goal.cpp:CmdCreateGoal`, `src/goal.cpp:CmdGoalQuestion`,
  `src/goal.cpp:CmdGoalQuestionAnswer`, `src/story.cpp:CmdCreateStoryPage`,
  `src/story.cpp:CmdShowStoryPage`, `src/story.cpp:CmdStoryPageButton`,
  `src/league_cmd.cpp:CmdCreateLeagueTable`, `src/league_cmd.cpp:CmdCreateLeagueTableElement`,
  `src/news_gui.cpp:CmdCustomNewsItem`, `src/signs_cmd.cpp:CmdPlaceSign`,
  `src/signs_cmd.cpp:CmdRenameSign`, `src/signs_cmd.cpp:CmdMoveSign`.
- Script side: `src/script/api/script_goal.cpp`, `src/script/api/script_story_page.cpp`,
  `src/script/api/script_league.cpp`, `src/script/api/script_news.cpp` (all `@api game`,
  `EnforceDeityMode`), `src/script/api/script_sign.cpp` (AI and GS).
- Windows: `src/goal_gui.cpp:GoalListWindow`, `src/goal_gui.cpp:ShowGoalQuestion`,
  `src/story_gui.cpp:StoryBookWindow`, `src/league_gui.cpp:PerformanceLeagueWindow` (built-in
  company ranking), `src/league_gui.cpp:ScriptLeagueWindow`, `src/signs_gui.cpp:SignListWindow`,
  `src/signs_gui.cpp:SignWindow`, `src/cheat_gui.cpp:CheatWindow`, `src/highscore_gui.cpp:EndGameWindow`,
  `src/highscore_gui.cpp:HighScoreWindow`, `src/news_gui.cpp:NewsWindow`,
  `src/news_gui.cpp:MessageHistoryWindow`.
- News: `src/news_func.h:AddNewsItem` and its wrappers (`AddVehicleNewsItem`, `AddTileNewsItem`,
  `AddIndustryNewsItem`, `AddCompanyNewsItem`), `src/news_gui.cpp:NewsLoop`,
  `src/news_func.h:DeleteVehicleNews` / `DeleteStationNews` / `DeleteIndustryNews`,
  `src/news_type.h:NewsType`, `src/news_type.h:NewsReference`.
- Cheats: `src/cheat_type.h:Cheats` (global `_cheats`, reset by `src/cheat.cpp:InitializeCheats`),
  `src/cheat_gui.cpp:ShowCheatWindow`; the only cheat command is `src/misc_cmd.h:CmdMoneyCheat`.
- Gamelog: `src/gamelog.h:Gamelog` (global `_gamelog`), `src/gamelog.h:GamelogActionType`,
  `src/gamelog.h:GamelogChangeType`, internals in `src/gamelog_internal.h:LoggedAction`.
- Highscore: `src/highscore.h:HighScore`, `src/highscore.cpp:SaveHighScoreValue`,
  `src/highscore.cpp:SaveToHighScore` (file `hs.dat` in the config dir, set in `src/fileio.cpp`).
- Saved in chunks `GOAL`, `STPA`/`STPE`, `LEAT`/`LEAE`, `SIGN`, `CHTS`, `GLOG`
  (`src/saveload/goal_sl.cpp` ... `src/saveload/gamelog_sl.cpp`).

## Lifecycle

- GS object: GS calls `ScriptGoal::New` → `ScriptObject::Command<Commands::CreateGoal>::Do` → on the
  server the handler checks `OWNER_DEITY`, creates the pool item in the execute pass and invalidates
  `WindowClass::GoalList` (and the toolbar when the first goal appears) on every peer.
- Question: `CmdGoalQuestion` executes everywhere, but only the targeted client or company opens
  `GoalQuestionWindow`; the answer button posts `Commands::GoalQuestionAnswer` from that company; its
  execute closes the window for that company and, on the server only, queues
  `ScriptEventGoalQuestionAnswer` for the GS. Story buttons follow the same path
  (`CmdStoryPageButton` → `ScriptEventStoryPageButtonClick`).
- News: game code calls `AddNewsItem` during the tick on every peer; the item is pushed on the local
  `std::list`, capped by `MAX_NEWS_AMOUNT`; `NewsLoop` (from `src/openttd.cpp:StateGameLoop`) ages
  out old items monthly and moves ticker and newspaper per the item's `news_display.*` setting.
  `src/misc.cpp:InitializeGame` clears the queue, so a loaded game starts with none.
- Gamelog: `src/misc.cpp:InitializeGame` resets it and logs `Start` (revision, mode, GRF list);
  `src/saveload/saveload.cpp:DoLoad` wraps `AfterLoadGame` in a `Load` action, where
  `src/saveload/afterload.cpp` calls `TestRevision`, `TestMode`, `Oldver`, GRF compat/removal;
  `src/newgrf_gui.cpp` logs `GRF` changes; `src/settings.cpp:IntSettingDesc::ChangeValue` logs
  `Setting`; `src/crashlog.cpp` logs `Emergency`; `src/vehicle.cpp` logs a `GRFBug`. `AfterLoadGame`
  ends with `_gamelog.PrintDebug(Severity::Error)`.
- Highscore: `LoadFromHighScore` at startup and `SaveToHighScore` at exit (`src/openttd.cpp`); the
  yearly `_check_end_game` timer in `src/highscore_gui.cpp` opens `EndGameWindow` in the year after
  `game_creation.ending_year`.

## Invariants

- **[verified]** `CommandFlag::Deity` means "deity may run it", not "only deity": every goal, story
  and league handler except the two button/answer commands, plus `CmdCustomNewsItem`, returns
  `CMD_ERROR` unless `_current_company == OWNER_DEITY`. Signs accept any company; `CompanyCanEditSign`
  protects deity signs outside the editor.
- **[verified]** Presentation side effects inside an execute pass are keyed on the local process
  (`_local_company`, `_network_own_client_id`): `CmdGoalQuestion`, `CmdShowStoryPage` and
  `CmdCustomNewsItem` (which returns early for another company) open windows or add news only where
  they apply. Game state they touch is identical everywhere (ADR 0016).
- **[verified]** The news queue is not saved and not synced (no news chunk in `src/saveload/`;
  `news_display.*` are `NotInSave`, `NoNetworkSync`); simulation code must never read it.
- **[verified]** `CmdRenameSign` with empty text deletes the sign; the intro screen deletes title-game
  signs directly (`src/intro_gui.cpp`), the only direct pool write in a window.
- **[verified]** Story sort counters are not saved; `src/saveload/story_sl.cpp` rebuilds them as max
  plus one on load. Pre-`SaveLoadVersion::Storybooks` story pools are discarded in `AfterLoadStoryBook`.
- **[verified]** Classic cheats write globals directly from the window (`_cheats.*.value`, the date
  via `TimerGameCalendar::SetDate`, `map_height_limit`); they are reachable only offline (toolbar
  entry and `MTHK_CHEATS` require `!_networking`; `MoneyCheat` is `CommandFlag::Offline`).
- **[verified]** Cheats are not in the gamelog: `GamelogActionType::Cheat` is declared but nothing
  starts it; the record is the `been_used` flags in `CHTS`. Sandbox settings in the same window go
  through `SetSettingValue` and are logged as `Setting` (flag `SettingFlag::Sandbox`), as are
  `NoNetwork` settings; ordinary synced settings are not logged.
- **[verified]** `Gamelog::Change` drops a change when no action is active; `StartAction` asserts no
  action is open. Each action records `TimerGameTick::counter`.
- **[verified]** The revision entry is `g`/`u`/`m` + git hash for unreleased builds
  (`GetGamelogRevisionString`), so a fork build is identifiable in every save it touched.
- **[verified]** `SP_MULTIPLAYER` highscores are per game and never written to `hs.dat`; the
  `HighScore::score` type is the file format.

## Extension points

- New GS-owned object kind: pool in a `*_base.h`, commands with `CommandFlag::Deity` plus an explicit
  `OWNER_DEITY` check, a chunk with version gating, an `@api game` class.
- New news type: a value before `End` in `src/news_type.h:NewsType`, a row in `_news_type_data`
  (`src/news_gui.cpp`), an `[SDTC_OMANY]` entry in `src/table/settings/news_display_settings.ini` whose
  `var` equals the row's name, its strings.
- New cheat: append a `Cheat` to `src/cheat_type.h:Cheats` (never reorder), a version-gated field in
  `src/saveload/cheat_sl.cpp`, a row in `_cheats_ui`. Prefer a command if it must work online.
- New gamelog change: a `GamelogChangeType` value, a `LoggedChange` subclass in
  `src/gamelog_internal.h`, and its save description in `src/saveload/gamelog_sl.cpp`.

## Traps and forbidden operations

- A `_news_type_data` name that no setting matches fails only when the item is shown:
  `NewsTypeData::GetDisplay` asserts the lookup (knowledge candidate
  `a-news-type-binds-its-display-setting-by-name`).
- `AddNewsItem` returns at once in `GameMode::Menu` and news shows only with a status bar: a headless
  or title run produces no news to observe.
- Do not post a deity command from player code expecting success; scripts reach them only through the
  GS, which runs only on the server (knowledge
  `scripts-run-only-on-the-server-and-the-game-script-runs-while-paused`).
- League tables have no remove command (only elements are removed) and `LeagueTableID` is a `uint8_t`
  (`src/league_type.h`): a GS that creates a table per event exhausts the pool.
- Never remove or reorder `Cheats` members or gamelog enum values: both are saved by position or
  value (comment in `src/cheat_type.h`; rule `project.savegame-fields-are-version-gated`).

## Tests and validation

- No Catch2 test covers this area. `regression_ai` exercises `AISign` (`regression/regression/main.nut`);
  no regression case covers goals, story, league or news.
- Tier: `tools/gate.sh change`; any chunk change also needs an old-save load (see the
  [saveload map](../saveload/README.md)).
- Debug: `-d gamelog=N` prints the log on every closed action (Debug1) and after each load (Error
  level); console `gamelog` prints it; `openttd -q <save>` prints revision, modified flag and GRFs
  without loading the game; crash logs embed it.

## Decisions, rules and knowledge

- ADR 0016 (lockstep), ADR 0017 (pools), ADR 0018 (savegame compatibility), ADR 0019 (news display
  settings), ADR 0021 (GS API), ADR 0025 (calendar vs economy dates in news ageing and the end-game timer).
- Rules: `project.game-state-mutates-only-through-commands`, `project.savegame-fields-are-version-gated`,
  `project.never-report-unrun-validation-as-pass`.
- Knowledge: `scripts-run-only-on-the-server-and-the-game-script-runs-while-paused`,
  `a-server-only-command-from-a-network-client-kicks-it`, candidates
  `cheats-are-not-recorded-in-the-gamelog`, `a-news-type-binds-its-display-setting-by-name`.
- Research provenance: the gamelog travels inside every savegame a run writes; cite it with the
  research manifest (`research/tooling.md`) rather than inferring a save's history.

## Typical tasks

- To add a GS goal feature: start at `src/goal.cpp:CmdCreateGoal`, keep the `OWNER_DEITY` check,
  expose it in `src/script/api/script_goal.cpp`; validate with `tools/gate.sh change`.
- To change how news is shown: start at `src/news_gui.cpp:NewsLoop` and `_news_type_data`; validate in
  a GUI run (the MCP `openttd.view.news.list` tool reads this process' queue only).
- To check where a save came from: `openttd -q <save>`, then `gamelog` in its console; record both
  in the experiment manifest.
- To touch a cheat: start at `src/cheat_gui.cpp:CheatWindow`, keep it offline-only, keep the `been_used`
  flag set; validate by saving and reloading.
