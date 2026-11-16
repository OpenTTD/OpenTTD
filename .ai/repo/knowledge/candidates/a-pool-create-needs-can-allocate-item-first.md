---
schema: knowledge/v1
id: a-pool-create-needs-can-allocate-item-first
kind: knowledge
class: constraint
title: "Every PoolItem::Create must follow a CanAllocateItem for that pool; an assert build checks it, a release build dies with a FatalError when the pool is full"
description: "When creating a pooled object (vehicle, town, sign, station, company, ...) in a command handler, the game loop or research code."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [core-utilities, commands]
about: src/core/pool_func.hpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/core/pool_type.hpp
    - file:src/core/pool_func.hpp
    - file:src/signs_cmd.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/core-utilities/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/commands/README.md
---

# Statement

`src/core/pool_type.hpp:Pool::CanAllocate` sets the pool's `checked` counter to `n` under
`WITH_ASSERT` (the default, `OPTION_USE_ASSERTS=ON`), and `GetNew(size)` in
`src/core/pool_func.hpp` asserts `checked != 0` and decrements it. So `T::Create` without a prior
`T::CanAllocateItem()` asserts even when the pool has room, and a second `Create` after
`CanAllocateItem()` (n = 1) asserts too: ask for `CanAllocateItem(n)` before creating n items.
Without asserts the missing check is silent until the pool is full, then `FatalError` ends the
process. Command handlers call `CanAllocateItem` in the test pass and return an error
(`src/signs_cmd.cpp:CmdPlaceSign` returns `STR_ERROR_TOO_MANY_SIGNS`), which is what makes a
full pool a refused command rather than a crash. `CreateAtIndex` (savegame loading) is exempt.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0063); awaiting the owner's review.
`majordomus knowledge promote a-pool-create-needs-can-allocate-item-first` with evidence, or `majordomus knowledge reject a-pool-create-needs-can-allocate-item-first --reason ...`.
