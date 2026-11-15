---
schema: knowledge/v1
id: macos-missing-baseset-hangs-headless-runs
kind: knowledge
class: fact
title: "On macOS a missing base graphics set hangs every headless run instead of failing it"
description: "When a regression ctest case times out at 120 s, or a research run exits 124 with no research_status line."
status: verified
epistemics: observed
date: 2026-11-15
tags: [macos, regression, baseset, ctest, environment]
about: research/baseset-prerequisite-macos.md
route: documentation
provenance:
  origin: extracted
  derived_from:
    - commit:b86f1cb7f7f6b399221df460776f60641694c438
    - file:research/baseset-prerequisite-macos.md
    - issue:I0008
    - issue:I0021
relations:
  - type: documents
    target: file:.ai/repo/project/issues/I0008.yaml
  - type: supports
    target: file:.ai/repo/project/issues/I0021.yaml
  - type: relates_to
    target: file:tools/gate.sh
---

# Fact

`HandleBootstrap()` raises a fatal user error when no base graphics set is found, and on macOS
that error is a modal Cocoa dialog (`NSAlert runModal`) even under `-vnull -snull -mnull` or
`-D`. The process waits for a click that never comes.

# Observed

- 2026-07-24: the four `regression_*` ctest cases timed out until OpenGFX was installed; then
  each ran in 0.13 to 0.35 s.
- 2026-11-15, after `~/Documents/OpenTTD/` had disappeared from the machine:
  `tools/gate.sh change` gave 94/98, with `regression_regression` and `regression_stationlist`
  timing out and `regression_gs` and `regression_gs_compat` reporting "did not output
  anything". The research-debug headless run exited 124 with no `research_status`. After
  `opengfx-8.0.tar` was copied into `~/Documents/OpenTTD/baseset/`, both passed: 98/98, and
  `research_status ... tick=1392`.

# How to apply

Before reading a regression timeout as a code defect, check
`ls ~/Documents/OpenTTD/baseset/`. The prerequisite lives outside the repository and no gate
checks it.
