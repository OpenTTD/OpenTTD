---
schema: context/v1
id: fork.substrate.newgrf
kind: context
title: "NewGRF: GRF scanning, staged loading, per-action and per-feature handlers, callbacks and sprite-group resolution"
description: "When a change touches GRF loading or configuration, an action or Action 0 property handler, a callback/resolver, NewGRF storage, or GRF matching in savegames and multiplayer."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/newgrf.cpp'
  - 'src/newgrf.h'
  - 'src/newgrf_*'
  - 'src/newgrf/**'
  - 'src/saveload/newgrf_sl.*'
---

## Purpose and ownership

Parses NewGRF files (trusted binary mods) into engines, stations, houses, industries, cargo,
rail/road types, objects, badges and sprites, and answers callbacks at run time through sprite
groups and resolvers. Owns the GRF configuration lists (`_grfconfig`, `_grfconfig_newgame`,
`_grfconfig_static` in `src/newgrf_config.h`) and their identity (GRF ID + MD5). All upstream code.

## Entry points

- `src/newgrf_config.cpp:ScanNewGRFFiles` - finds `.grf` files; `FillGRFDetails` runs the `FileScan` (and for static GRFs the `SafetyScan`) stage and `CalcGRFMD5Sum`.
- `src/newgrf.cpp:LoadNewGRF` - loads every config in `_grfconfig` through stages `LabelScan` -> `Init` -> `Reserve` -> `Activation` (`src/newgrf.h:GrfLoadingStage`), then `AfterLoadGRFs`.
- `src/newgrf.cpp:LoadNewGRFFile` / `DecodeSpecialSprite` - per pseudo-sprite dispatch via `InvokeGrfActionHandler` to `src/newgrf/newgrf_internal.h:GrfActionHandler` specialisations (one newgrf_act<N>.cpp per action in the `src/newgrf/` directory).
- `src/newgrf/newgrf_act0.cpp:FeatureChangeInfo` - Action 0; per-feature `src/newgrf/newgrf_internal.h:GrfChangeInfoHandler` in newgrf_act0_<feature>.cpp (e.g. `src/newgrf/newgrf_act0_trains.cpp`), dispatched by `InvokeGrfChangeInfoHandler` in `GrfSpecFeature` order.
- `src/newgrf_spritegroup.h:ResolverObject`, `ScopeResolver`, `SpriteGroup` - run-time evaluation of Action 2/3 chains; `src/newgrf_spritegroup.cpp:SpriteGroup::Resolve`.
- Feature resolvers: e.g. `src/newgrf_engine.cpp:GetVehicleCallback` (`src/newgrf_engine.h:VehicleResolverObject`), `src/newgrf_station.cpp`, `src/newgrf_house.cpp`, `src/newgrf_industries.cpp`. Callback ids: `src/newgrf_callbacks.h:CallbackID`, failure `CALLBACK_FAILED`.
- `src/saveload/afterload.cpp:ReloadNewGRFData` - in-game reload (GUI apply, console `reload_newgrfs`, cheats, `src/engine.cpp`).
- **[fork]** No fork hunk touches NewGRF code.

## Lifecycle

- Startup: `ScanNewGRFFiles` fills `_all_grfs`; settings load validates each configured GRF with `FillGRFDetails` (`src/settings.cpp`).
- Game start / load: `src/saveload/afterload.cpp` calls `IsGoodGRFConfigList` on the saved `NGRF` list (`src/saveload/newgrf_sl.cpp:NGRFChunkHandler`), then loads sprites/GRFs. `src/newgrf.cpp:ResetNewGRFData` clears GRF-derived state at the start of every `LoadNewGRF`.
- Network: `LoadNewGRF` temporarily sets calendar/economy dates to the starting year and `TimerGameTick::counter` to 0 (restored by `AutoRestoreBackup`) so every client loads GRFs identically.
- Run time: callbacks resolve on demand from the game loop, commands and drawing; results feed caches such as `Vehicle::grf_cache` (`src/vehicle_base.h`).
- Teardown/reload: `ResetNewGRFData` then `LoadNewGRF`; `ReloadNewGRFData` also rebuilds vehicle chains and calls `StartupEngines`.
- Save/load of identities: GRF list `NGRF`; entity id maps via `src/saveload/newgrf_sl.h:NewGRFMappingChunkHandler` (`HIDS`, `IIDS`, `TIDS`, `OBID`) over `src/newgrf_commons.h:OverrideManagerBase`; engines via `EIDS`.

## Invariants

- **[supported]** GRF files are trusted; a crafted file can exploit the parser (comment on `src/newgrf.cpp:DecodeSpecialSprite`). Reading past a pseudo-sprite disables the GRF (`DisableGrf`).
- **[verified]** A GRF's identity is `src/newgrf_config.h:GRFIdentifier` (GRF ID + MD5). For container-v2 files `src/newgrf_config.cpp:CalcGRFMD5Sum` hashes only the header and data section (`GRFGetSizeOfDataSection`), not the sprite section.
- **[verified]** A joining client needs every non-static server GRF with an exact MD5 match (`src/network/network_server.cpp:ServerNetworkGameSocketHandler::SendNewGRFCheck`, `src/network/network_client.cpp:ClientNetworkGameSocketHandler::ReceiveServerCheckNewGRFs` with `FindGRFConfigMode::Exact`).
- **[verified]** Loading a savegame accepts a same-ID "compatible" GRF only offline; with `_networking` any result other than `GRFListCompatibility::AllGood` aborts the load (`src/saveload/afterload.cpp`, `IsGoodGRFConfigList`). A missing GRF offline pauses the game (`PauseMode::Error`).
- **[verified]** Static GRFs are excluded from the network check, must pass the `SafetyScan` (`src/newgrf.cpp:GRFUnsafe` marks them unsafe), and are disabled when they influence a non-static GRF (`DisableStaticNewGRFInfluencingNonStaticNewGRFs`).
- **[verified]** At most `NETWORK_MAX_GRF_COUNT` non-static GRFs load (`src/newgrf.cpp:LoadNewGRF`).
- **[verified]** NewGRF persistent storage changes persist only inside the game loop or a command's execute pass; test passes and other contexts are reverted at the next `src/newgrf_storage.cpp:BasePersistentStorageArray::SwitchMode` (`AreChangesPersistent` in `src/newgrf_storage.h`).
- **[supported]** Cache mismatches are the usual cause of NewGRF-specific desyncs (`docs/desync.md`, section 2.1).
- **[inferred]** Changing the active GRF set in a running game is a client-local operation outside the command system; it is offered only when not networking (`src/toolbar_gui.cpp` passes `!_networking` to `ShowNewGRFSettings`; console hook `ConHookNewGRFDeveloperTool` requires no network).

## Extension points

- New Action 0 property: the `switch (prop)` in the feature's newgrf_act0_<feature>.cpp (e.g. `src/newgrf/newgrf_act0_trains.cpp`), with the property enum in `src/newgrf_properties.h` where one exists.
- New feature: add to `src/newgrf.h:GrfSpecFeature`, add a `GrfChangeInfoHandler<Feature>` file, extend the `funcs` table in `src/newgrf/newgrf_act0.cpp` (must stay in feature order), list the file in `src/newgrf/CMakeLists.txt`.
- New action handler stage: specialise `GrfActionHandler<0xNN>` stage methods; the `funcs` table in `src/newgrf.cpp` must stay in action order.
- New callback: add the id to `src/newgrf_callbacks.h:CallbackID`, call it through the feature resolver, report unexpected results with `src/newgrf_commons.cpp:ErrorUnknownCallbackResult`.
- New variable: extend the feature's `ScopeResolver::GetVariable` override (e.g. in `src/newgrf_engine.cpp`).

## Traps and forbidden operations

- A GRF-derived value cached in game objects without a recompute path in `src/cachecheck.cpp:CheckCaches` hides desyncs.
- Using the current date or tick in a loading-stage handler: `LoadNewGRF` fakes them only in network games, so single-player and network loads differ by design.
- Writing NewGRF storage from GUI or MCP code: the write is silently reverted (see `SwitchMode`).
- `reload_newgrfs` is documented in-game as "might crash OpenTTD" (`src/console_cmds.cpp`); do not use it in measured runs.
- Do not reorder `GrfSpecFeature` or `GrfLoadingStage`: both index dispatch tables.

## Tests and validation

- No unit test in `src/tests/` and no folder under `regression/` targets NewGRF parsing or callbacks. Validation is a build plus `tools/gate.sh change`, then a game with the GRF loaded, reported as such (not as a passing test).
- Debug: `-d grf=N` (`GrfMsg` in `src/newgrf.h`; level 7 = `Trace1` logs each action dispatch), `-d desync=2` (`CheckCaches` recomputes `grf_cache`, logs discarded storage changes), the NewGRF inspect window (`src/newgrf_debug.h:ShowNewGRFInspectWindow`), console `newgrf_profile` (`src/newgrf_profiling.h:NewGRFProfiler`) with `newgrf_developer_tools` on.

## Decisions, rules and knowledge

- Rail/road type slots and label remapping after load: [rail, road and infrastructure](../rail-road-and-infrastructure/README.md); knowledge candidate `a-missing-type-label-loads-as-the-base-type`.
- Upstream architecture: [ADR 0028](../../../adrs/0028-graphics-sounds-and-music-come-from-replaceable-base-sets-an.md) (base sets and NewGRF content), [ADR 0018](../../../adrs/0018-every-savegame-ever-written-stays-loadable-saved-fields-are-.md) (savegame versioning).
- Neighbouring maps: [graphics-and-sprites](../graphics-and-sprites/README.md), [sound-and-music](../sound-and-music/README.md).
- Rules: `project.savegame-fields-are-version-gated`, `project.game-state-mutates-only-through-commands`, `project.never-report-unrun-validation-as-pass`.
- ADR 0001 (fork stays a research target; upstream behaviour is the substrate).
- Upstream docs: `docs/desync.md`, `docs/savegame_format.md`.

## Typical tasks

- To find why a GRF is disabled: start at `src/newgrf.cpp:DisableGrf` callers for the error string, run with `-d grf=3`.
- To trace a vehicle callback: start at `src/newgrf_engine.cpp:GetVehicleCallback`, then `src/newgrf_spritegroup.cpp:SpriteGroup::Resolve`; inspect with the NewGRF inspect window, validate under `-d desync=2`.
- To diagnose "NewGRF mismatch" on join: compare MD5s via `ReceiveServerCheckNewGRFs` with `-d grf=1,net=3`.
- To add an Action 0 property: edit the feature's newgrf_act0_<feature>.cpp in the `src/newgrf/` directory, keep saved results version-gated, validate with `tools/gate.sh change`.
