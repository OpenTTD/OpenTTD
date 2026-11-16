---
schema: context/v1
id: fork.substrate.saveload
kind: context
title: "Saveload: savegame chunks, field tables, versions, compat and after-load conversion"
description: "Read before adding, changing or removing anything that is written to a savegame, or touching chunk handlers, SaveLoadVersion or AfterLoadGame."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/saveload/**'
  - 'docs/savegame_format.md'
---

# Saveload

## Purpose and ownership

Owns the savegame container (format tag, version, compression filters), the chunk registry, the
per-chunk field descriptions (`SaveLoad` tables), the version enum, the compat tables for pre-table
savegames, pointer fix-up and `AfterLoadGame` conversion. Also the TTD/TTO old loader
(`src/saveload/oldloader.cpp`). What is saved is decided by each `*_sl.cpp`; how is decided here.

## Entry points

- `src/saveload/saveload.cpp:SaveOrLoad` - top-level save/load by filename; `SaveWithFilter` /
  `LoadWithFilter` for streams (the server's map dump, the client's `SafeLoad` in
  `src/openttd.cpp:SafeLoad`); `DoAutoOrNetsave`, `DoExitSave`.
- `src/saveload/saveload.cpp:ChunkHandlers` - the static list of every `*_chunk_handlers` table.
- `src/saveload/saveload.h:ChunkHandler` - base class; `Save`, `Load`, `FixPointers`, `LoadCheck`.
- `src/saveload/saveload.h:SaveLoad` - a field description; built with `SaveLoad::Variable`,
  `String`, `Reference`, `Array`, `Vector`, `Struct`, `StructList` and `SLE_OBJECT_ADDRESS` /
  `SLE_GLOBAL_ADDRESS` (e.g. `src/saveload/signs_sl.cpp:_sign_desc`).
- `src/saveload/saveload.h:SaveLoadVersion` - every savegame version, "must not be reordered".
- `src/saveload/saveload.cpp:SlTableHeader` / `SlCompatTableHeader` - header write/read and name
  matching; `src/saveload/compat/` holds the `*_sl_compat.h` original field orders.
- `src/saveload/afterload.cpp:AfterLoadGame` - post-load conversion keyed on
  `IsSavegameVersionBefore`.
- No fork hunk under `src/saveload/`; fork research state is process-local and never saved.

## Lifecycle

- Save: `DoSave` sets `_sl_version = SAVEGAME_VERSION`, runs `SlSaveChunks` synchronously into a
  `MemoryDumper` on the game thread, then compresses and writes on the `ottd:savegame` thread when
  `threaded`; `ProcessAsyncSaveFinish` (called from `GameLoop`) joins it.
- Load: `DoLoad` reads tag + version (rejects `_sl_version > SAVEGAME_VERSION` and the patchpack
  range `StartPatchpacks..EndPatchpacks`), calls `InitializeGame(256, 256, true, true)`, then
  `SlLoadChunks`, `SlFixPointers`, and `AfterLoadGame`; failure returns `SaveLoadResult::ReInit`.
- Load-check (save list preview): `SlLoadCheckChunks` fills `_load_check_data`, no pools.

## Invariants

- **[verified]** `version_from` is inclusive and `version_to` exclusive
  (`src/saveload/saveload.cpp:SlIsObjectValidInSavegame`); `SAVEGAME_VERSION` is `MaxVersion - 1`
  (`src/saveload/saveload.cpp:SAVEGAME_VERSION`), so a new enum value before `MaxVersion` is a bump.
- **[verified]** In table chunks fields are matched by `name`, not position. A saved key with no
  matching entry is skipped with an `sl` warning; a type mismatch is `SlErrorCorrupt`; two entries
  valid for one version with the same name assert (`src/saveload/saveload.cpp:SlTableHeader`).
- **[verified]** A chunk id in the file without a registered handler is
  `SlErrorCorrupt("Unknown chunk type")` (`src/saveload/saveload.cpp:SlLoadChunks`); removing a
  handler breaks old saves.
- **[verified]** Compat tables are consulted only for non-table chunks; every compat name must exist
  in the `SaveLoad` table (`src/saveload/saveload.cpp:SlCompatTableHeader`).
- **[verified]** Before chunks load, `InitializeGame(..., reset_settings=true)` copies
  `_settings_newgame` into `_settings_game` (`src/misc.cpp:InitializeGame`); any value a save does
  not carry keeps that until `AfterLoadGame` sets it.
- **[supported]** "Every revision must load every older savegame" and the field-removal ban are the
  fork's rule `project.savegame-fields-are-version-gated`; the table header format is in
  `docs/savegame_format.md` ("ChunkType::Table / ChunkType::SparseTable").

## Extension points

- New field in an existing chunk: append a
  `SaveLoad::Variable<...>("key", SLE_OBJECT_ADDRESS(T, member), SaveLoadVersion::NewLabel)` to the
  chunk's table; add `NewLabel` immediately before `MaxVersion` in
  `src/saveload/saveload.h:SaveLoadVersion`; set the old-save value in `AfterLoadGame` under
  `IsSavegameVersionBefore(SaveLoadVersion::NewLabel)`. Do not touch the compat table.
- Retire a field: close it with a `to` version; keep a load path (or a `SLC_NULL` entry in the
  compat table for pre-table data) so older saves still read.
- New chunk: a `ChunkHandler` subclass with a unique 4-letter id and `ChunkType::Table`, exported as
  `extern const ChunkHandlerTable _x_chunk_handlers`, listed in
  `src/saveload/saveload.cpp:ChunkHandlers`, source added to `src/saveload/CMakeLists.txt`. New
  table chunks call `SlTableHeader` directly.

## Traps and forbidden operations

- Never reorder, renumber or remove `SaveLoadVersion` values or an active field's key.
- Renaming a field's key without a version gate silently drops its value on load (skipped with a
  warning, not an error).
- A fork-only `SaveLoadVersion` would collide with a future upstream value; upstream already refuses
  a numeric range used by patchpacks (`StartPatchpacks`/`EndPatchpacks`). For a fork-only field in
  an existing chunk, `docs/savegame_format.md` ("Guidelines for network-compatible patch-packs")
  advises a `__<shortname>_` key prefix, which unpatched clients skip.
- Do not read pools or chunk state from `Load` that another chunk restores later; cross-chunk links
  are resolved in `FixPointers` and conversions belong in `AfterLoadGame`.
- Only compression and disk I/O are threaded; `SlSaveChunks` has finished before `DoSave` returns,
  so a threaded save never reads game state concurrently (`src/saveload/saveload.cpp:DoSave`).

## Tests and validation

- No Catch2 saveload test. `regression_ai` and `regression_game` load checked-in `test.sav` files of
  older versions (e.g. `regression/gs/test.sav`) via `-g`, which runs `AfterLoadGame` paths (read a
  file's version: `xxd -s 4 -l 2 -p regression/gs/test.sav`).
- Tier: `tools/gate.sh change`; `tools/gate.sh full` for a version bump.
- Debug: `-d sl=2` (skipped-field warnings; `sl=3` adds per-chunk notices); `-d desync=1` writes a
  `dmp_cmds_*.sav` periodically from `StateGameLoop`.

## Decisions, rules and knowledge

- Rules: `project.savegame-fields-are-version-gated`,
  `project.game-state-mutates-only-through-commands`.
- ADR 0003 (migration-rollback ban rejected), ADR 0008 (no savegame change for MCP), ADR 0011
  (rebasing onto upstream, which keeps adding versions).
- Knowledge: `heightmap-flag-is-lowercase-g`, `upstream-rebase-2026-11-15`.
- Upstream docs: `docs/savegame_format.md`, `docs/desync.md`.

## Typical tasks

- To persist a new member: start at the owning `*_sl.cpp` table and
  `src/saveload/saveload.h:SaveLoadVersion`, add the `AfterLoadGame` default, validate with
  `tools/gate.sh change` and a load of an older `regression/regression/test.sav`.
- To fix an old-savegame conversion: start at `src/saveload/afterload.cpp:AfterLoadGame`, find the
  `IsSavegameVersionBefore` block for that version, validate with `-d sl=2`.
- To find which chunk owns a value: `git grep -n 'SLE_OBJECT_ADDRESS(<Type>' -- src/saveload`.
- To inspect a savegame's format: `docs/savegame_format.md`, then
  `src/saveload/saveload.cpp:DoLoad`.
