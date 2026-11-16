---
schema: knowledge/v1
id: cheats-are-not-recorded-in-the-gamelog
kind: knowledge
class: fact
title: "Classic cheats leave no gamelog entry; a savegame records them only as been_used flags in the CHTS chunk"
description: "When reconstructing a savegame's history for research provenance, or adding a cheat."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [goals-story-news-and-history, saveload]
about: src/gamelog.h
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/gamelog.h
    - file:src/gamelog.cpp
    - file:src/cheat_gui.cpp
    - file:src/saveload/cheat_sl.cpp
    - file:src/settings.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/goals-story-news-and-history/README.md
---

# Statement

`src/gamelog.h:GamelogActionType` declares `Cheat`, but no code calls
`_gamelog.StartAction(GamelogActionType::Cheat)`; only an assert in `src/gamelog.cpp:Gamelog::Mode`
mentions it. `src/cheat_gui.cpp:CheatWindow` sets `*ce->been_used = true` and writes the cheat's
variable directly; `src/saveload/cheat_sl.cpp` saves those `been_used` flags. When, how often and to
what value a cheat was used is not recorded anywhere. Sandbox settings in the same window are
different: they go through `SetSettingValue`, and `src/settings.cpp:IntSettingDesc::ChangeValue`
logs them as a `Setting` change. Provenance that needs cheat use must read `CHTS`, not the gamelog.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote cheats-are-not-recorded-in-the-gamelog` with evidence, or `majordomus knowledge reject cheats-are-not-recorded-in-the-gamelog --reason ...`.
