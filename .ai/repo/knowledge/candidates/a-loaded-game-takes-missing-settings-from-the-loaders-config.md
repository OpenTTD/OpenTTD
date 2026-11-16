---
schema: knowledge/v1
id: a-loaded-game-takes-missing-settings-from-the-loaders-config
kind: knowledge
class: fact
title: "A setting a savegame does not carry keeps the loading machine's openttd.cfg value, so the same save can load differently on two machines"
description: "When reproducing an experiment from an older save, or comparing runs across machines."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [saveload, settings]
about: src/misc.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/misc.cpp
    - file:src/saveload/saveload.cpp
    - file:src/saveload/afterload.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/saveload/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/settings/README.md
---

# Statement

Loading calls `src/misc.cpp:InitializeGame` with settings reset, which makes the new-game settings
live before the chunks load (`MakeNewgameSettingsLive`). A value the save lacks keeps the local config's
value unless `src/saveload/afterload.cpp:AfterLoadGame` pins it. Record the config with the experiment.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote a-loaded-game-takes-missing-settings-from-the-loaders-config` with evidence, or `majordomus knowledge reject a-loaded-game-takes-missing-settings-from-the-loaders-config --reason ...`.
