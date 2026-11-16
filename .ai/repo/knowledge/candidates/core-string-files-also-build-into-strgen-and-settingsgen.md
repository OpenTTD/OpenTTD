---
schema: knowledge/v1
id: core-string-files-also-build-into-strgen-and-settingsgen
kind: knowledge
class: constraint
title: "src/core string files, utf8 and getoptdata also compile into the host tools strgen and settingsgen, where game headers are not available"
description: "When adding an include, a Debug() call or a game dependency to src/core/string_*.cpp, src/core/utf8.cpp or src/misc/getoptdata.cpp."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [core-utilities, build]
about: src/core/string_consumer.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/strgen/CMakeLists.txt
    - file:src/settingsgen/CMakeLists.txt
    - file:src/core/string_consumer.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/core-utilities/README.md
---

# Statement

`src/strgen/CMakeLists.txt` and `src/settingsgen/CMakeLists.txt` build their own executables
from `src/core/string_builder.cpp`, `src/core/string_consumer.cpp`,
`src/core/string_inplace.cpp`, `src/core/utf8.cpp`, `src/misc/getoptdata.cpp`, `src/string.cpp`,
`src/error.cpp` and fmt's `format.cc`, with `STRGEN` or `SETTINGSGEN` defined. These tools run
before the game is compiled (they generate strings and settings tables), so a game header or
symbol added to those files breaks the build of the tools, not of `openttd`. Guard it the way
`src/core/string_consumer.cpp:StringConsumer::LogError` does: `FatalErrorI` under
`STRGEN`/`SETTINGSGEN`, `DebugPrint` otherwise.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote core-string-files-also-build-into-strgen-and-settingsgen` with evidence, or `majordomus knowledge reject core-string-files-also-build-into-strgen-and-settingsgen --reason ...`.
