---
schema: context/v1
id: fork.substrate.settings
kind: context
title: "Settings: declaration tables, settingsgen, config files, savegame and network sync"
description: "Read before adding or changing a setting, its flags or callbacks, the config-file layout, or any code that writes a setting at runtime."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/settings*'
  - 'src/settingsgen/**'
  - 'src/table/settings/**'
  - 'src/table/settings.h.*'
  - 'src/settingentry_gui.*'
  - 'src/saveload/settings_sl.cpp'
---

# Settings

## Purpose and ownership

Owns how a setting is declared (`.ini` tables compiled by `settingsgen`), stored (`_settings_game`,
`_settings_newgame`, `_settings_client`), read from and written to `openttd.cfg` / `private.cfg` /
`secrets.cfg`, saved in the `PATS` chunk, changed at runtime (GUI, console, scripts) and
synchronized in network games. Setting semantics belong to the subsystem that reads the value.

## Entry points

- `src/table/settings/` directory - one `*_settings.ini` per table; each section header
  (`[SDT_VAR]`, `[SDT_BOOL]`, `[SDTC_VAR]`, ...) picks a template from the file's `[templates]`.
- `src/table/settings.h.preamble:SDT_VAR` - the macros; `#var` becomes the setting name, which is
  both the config key and the savegame key.
- `src/settingsgen/settingsgen.cpp:ProcessIniFile` - build-time generator writing
  `generated/table/settings.h` in the build tree
  (`src/table/settings/CMakeLists.txt:TABLE_INI_SOURCE_FILES`).
- `src/settings_internal.h:SettingFlag`, `src/settings_internal.h:SettingDesc`,
  `src/settings_internal.h:IntSettingDesc` - descriptor types and flags.
- `src/settings.cpp:SetSettingValue` - the runtime write path for GUI, console and admin.
- `src/settings.cpp:CmdChangeSetting` / `CmdChangeCompanySetting` - the commands
  (`src/settings_cmd.h:CmdChangeSetting`); `src/settings.cpp:IConsoleSetSetting` for `setting` and
  `setting_newgame`.
- `src/settings.cpp:LoadFromConfig` / `SaveToConfig`;
  `src/saveload/settings_sl.cpp:GetSettingsDesc`.
- No fork hunk in the tracked files.

## Lifecycle

- Startup: `openttd_main` calls `LoadFromConfig(true)` (only `startup = true` settings); the rest
  load in `LoadFromConfig()` after the NewGRF scan (`OnNewGRFsScanned` in `src/openttd.cpp`), and
  again on a server restart when `network.reload_cfg` is set (`SwitchToMode`). `HandleSettingDescs`
  loads generic tables into `_settings_newgame`, private and secrets tables from their own files,
  `_company_settings` into `_settings_client.company`.
- New game / load: `src/openttd.cpp:MakeNewgameSettingsLive` copies `_settings_newgame` into
  `_settings_game`; a load then overwrites the keys present in `PATS`
  (`src/saveload/settings_sl.cpp:LoadSettings`) and clamps every int setting to its range.
- Runtime: `SetSettingValue` -> `PerCompany`: `Commands::ChangeCompanySetting`; `NoNetworkSync`:
  direct `ChangeValue` on newgame and current; otherwise `Commands::ChangeSetting` (single player or
  server only). `IntSettingDesc::ChangeValue` clamps, runs `pre_check`, writes, runs
  `post_callback`, logs `NoNetwork`/`Sandbox` changes to the gamelog, then `SaveToConfig` if
  `_save_config`.

## Invariants

- **[verified]** Only the tables in `src/settings.cpp:GetSaveLoadSettingTable` (difficulty, economy,
  game, linkgraph, locale, pathfinding, script, world) go into the savegame; gui, multimedia,
  network, news and the private/secrets tables never do.
- **[verified]** Within those, `NotInSave` settings are skipped, and `NoNetworkSync` settings are
  skipped on load by a network client, which keeps its own value
  (`src/saveload/settings_sl.cpp:GetSettingsDesc`).
- **[verified]** `CmdChangeSetting` is `CommandFlag::Server`, accepts only int settings valid in the
  current savegame version, and rejects `NoNetworkSync` via `IsEditable(true)`
  (`src/settings.cpp:SettingDesc::IsEditable`).
- **[verified]** String settings must be `NoNetworkSync` and have no command (`assert` in
  `src/settings.cpp:SetSettingValue`). `PerCompany` is `ST_COMPANY`, `NotInSave` `ST_CLIENT`, else
  `ST_GAME` (`src/settings.cpp:SettingDesc::GetType`).
- **[supported]** Flag meanings: header comments of `src/settings_internal.h:SettingFlag`, e.g.
  `NoNetworkSync` "Do not synchronize over network (but it is saved if ... NotInSave is not set)".

## Extension points

- New setting: add the member to the struct in `src/settings_type.h` (e.g. `GameSettings`), a
  section in the matching `*_settings.ini` in the `src/table/settings/` directory with
  `from = SaveLoadVersion::<new>` when it is saved, strings `STR_CONFIG_SETTING_*` in
  `src/lang/english.txt`, and an entry in `src/settingentry_gui.cpp:GetSettingsTree` for the GUI.
- New `.ini` table: list it in `src/table/settings/CMakeLists.txt:TABLE_INI_SOURCE_FILES`, export it
  from `src/settings_table.h`, and add it to `GenericSettingTables` (config) and, if saved,
  `GetSaveLoadSettingTable` in `src/settings.cpp`.
- Config-file renames: a new `IniFileVersion` value plus an `IsConversionNeeded` block in
  `src/settings.cpp:LoadFromConfig`.

## Traps and forbidden operations

- Never hand-edit the generated `table/settings.h`; edit the `.ini` and rebuild.
- Renaming a setting's `var` renames its savegame and config key; old saves then skip the value with
  an `sl` warning and the field keeps the loader's config value.
- A saved setting absent from an older savegame takes the loading machine's `_settings_newgame`
  value unless `AfterLoadGame` pins it (pattern: `src/saveload/afterload.cpp:AfterLoadGame`,
  `minutes_per_calendar_year`).
- Writing `_settings_game` directly bypasses the command, the clamp and the callbacks, and
  desynchronizes a network game; use `SetSettingValue` or `Command<Commands::ChangeSetting>`.
- Any runtime change rewrites `openttd.cfg` unless the process was started with `-x` (`_save_config`
  in `src/openttd.cpp`).

## Tests and validation

- No Catch2 test for settings. `regression_ai`/`regression_game` start with
  `-c regression/regression.cfg -x` and read settings through `AIGameSettings`/`GSGameSettings`.
- Tier: `tools/gate.sh change` (rebuild runs `settingsgen`, and the `[validation]` section emits
  `static_assert`s such as `DoesMaxFitVariable`).
- Debug: `-d sl=2` for skipped `PATS` keys; console `list_settings`, `setting <name>`.

## Decisions, rules and knowledge

- Upstream architecture: [ADR 0019](../../../adrs/0019-settings-are-declared-once-in-ini-tables-that-settingsgen-co.md) (settings tables), [ADR 0018](../../../adrs/0018-every-savegame-ever-written-stays-loadable-saved-fields-are-.md) (savegame versioning).
- Neighbouring maps: [platform-and-files](../platform-and-files/README.md).
- Rules: `project.savegame-fields-are-version-gated`,
  `project.game-state-mutates-only-through-commands`.
- ADR 0007 (the bridge steers through console commands and AI settings); knowledge
  `bridge-control-is-coarse-ai-restart`.
- Upstream docs: `docs/savegame_format.md` (`__<shortname>` prefix for patch-pack keys).

## Typical tasks

- To add a game setting: start at the `.ini` for its group under `src/table/settings/`, then the
  struct in `src/settings_type.h`, strings and `src/settingentry_gui.cpp:GetSettingsTree`, validate
  with `tools/gate.sh change` and a load of an older `regression/regression/test.sav`.
- To change a setting from research code: call `src/settings.cpp:SetSettingValue` (or post
  `Commands::ChangeSetting`) behind a research guard, validate with `tools/gate.sh research`.
- To make a setting client-only: add `SettingFlag::NotInSave` (and `NoNetworkSync`), and check
  `src/saveload/settings_sl.cpp:GetSettingsDesc` for the load effect.

