---
schema: knowledge/v1
id: the-savegame-string-range-never-gets-new-ids
kind: knowledge
class: constraint
title: "Strings in english.txt's ##id 0x6000 region are saved by number; inserting or moving a line there breaks savegames"
description: "When adding, removing or reordering strings in src/lang/english.txt, especially near STR_SV_*."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [strings-and-languages, saveload]
about: src/lang/english.txt
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/lang/english.txt
    - file:src/strgen/strgen_base.cpp
    - file:src/saveload/station_sl.cpp
    - file:src/saveload/strings_sl.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/strings-and-languages/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/saveload/README.md
---

# Statement

A `StringID` is positional: `src/strgen/strgen_base.cpp:StringReader::HandleString` gives each
master string the next index after the last `##id` pragma, so the ID follows the line's position,
not its name. `src/lang/english.txt` fences the `##id 0x6000` and `##id 0x6020` blocks
(`STR_SV_*`) with "These strings may never get a new id, or savegames will break!", because
savegames store those numbers: `src/saveload/station_sl.cpp` saves `string_id` as a `StringID`,
and `src/saveload/strings_sl.cpp:RemapOldStringID` maps old numbers onto `STR_SV_*`. Add new
strings outside that region; inside it, never insert, delete or reorder a line.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote the-savegame-string-range-never-gets-new-ids` with evidence, or `majordomus knowledge reject the-savegame-string-range-never-gets-new-ids --reason ...`.
