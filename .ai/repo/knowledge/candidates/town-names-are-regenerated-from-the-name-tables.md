---
schema: knowledge/v1
id: town-names-are-regenerated-from-the-name-tables
kind: knowledge
class: constraint
title: "A town's generated name is not saved; it is regenerated from townnameparts and the compiled name tables on every read"
description: "When editing src/table/townname.h or a town-name generator, or comparing town names across builds or saves."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [towns, original-data-and-game-init, determinism]
about: src/table/townname.h
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/townname.cpp
    - file:src/saveload/town_sl.cpp
    - file:src/town_cmd.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/original-data-and-game-init/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/towns-industries-stations/README.md
---

# Statement

A town without a custom name saves only the 32-bit `townnameparts` seed
(`src/saveload/town_sl.cpp`). `src/townname.cpp:GetTownName` rebuilds the string on each call
through `GenerateTownNameString`, whose generators index the `_name_*` arrays of
`src/table/townname.h`. `src/townname.cpp:VerifyTownName` compares these regenerated strings for
uniqueness, so `src/town_cmd.cpp:CmdFoundTown` and world generation accept or reject seeds by the
table contents. Editing a name array renames towns in every existing save, changes generation
from the same seed, and makes two builds with different tables disagree on a found-town command.
The regression suite prints English town names, so such an edit fails `regression_regression`.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0063); awaiting the owner's review.
`majordomus knowledge promote town-names-are-regenerated-from-the-name-tables` with evidence, or `majordomus knowledge reject town-names-are-regenerated-from-the-name-tables --reason ...`.
