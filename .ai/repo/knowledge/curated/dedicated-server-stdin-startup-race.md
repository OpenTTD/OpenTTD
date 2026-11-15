---
schema: knowledge/v1
id: dedicated-server-stdin-startup-race
kind: knowledge
class: constraint
title: "The headless research run feeds console commands on stdin after a fixed delay, and a loaded machine can lose them"
description: "When a research run passes but its research_status line is missing, or when scheduling a run next to a heavy build."
status: verified
epistemics: observed
date: 2026-07-24
tags: [research-cli, dedicated-server, flaky, load]
about: tools/research/research
route: generator
provenance:
  origin: extracted
  derived_from:
    - commit:19b32cafd6c8742b0eeb266017541b07403fc75e
    - file:research/tooling.md
    - issue:I0006
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/upstream-sync.md
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/macos-missing-baseset-hangs-headless-runs.md
---

# Constraint

`run` pipes `(sleep 5; printf 'research_status\nquit\n')` into `openttd -D`. If the server is not
reading its console yet when the bytes arrive, the command can be silently lost even with the
5 s delay. The run then records no `research_status` line.

# How to apply

Do not schedule the headless run next to another heavy build. A missing line is first a
start-up race (or a missing base set), and only then an instrumentation defect.
