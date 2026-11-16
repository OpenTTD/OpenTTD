---
schema: knowledge/v1
id: a-regression-case-fails-on-any-stdout-or-changed-stderr-line
kind: knowledge
class: fact
title: "A regression case fails on any standard output and on any normalised stderr line that differs from its result.txt"
description: "When adding or changing a Script API call that the regression suite covers, or debugging a red regression_* case."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [scripting]
about: cmake/scripts/Regression.cmake
route: platform
provenance:
  origin: authored
  derived_from:
    - file:cmake/scripts/Regression.cmake
    - file:cmake/CreateRegression.cmake
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/scripting/README.md
---

# Statement

`cmake/scripts/Regression.cmake` runs the game headless with null drivers and `-d script=2`, treats
any stdout as "Unexpected output", and compares the normalised stderr with the case's `result.txt` line
by line. A new log line in a covered path is a failure until `result.txt` is updated deliberately.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote a-regression-case-fails-on-any-stdout-or-changed-stderr-line` with evidence, or `majordomus knowledge reject a-regression-case-fails-on-any-stdout-or-changed-stderr-line --reason ...`.
