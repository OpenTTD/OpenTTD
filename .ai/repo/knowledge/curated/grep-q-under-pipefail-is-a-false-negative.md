---
schema: knowledge/v1
id: grep-q-under-pipefail-is-a-false-negative
kind: knowledge
class: lesson
title: "Under set -o pipefail, `producer | grep -q` reports a match as a failure"
description: "When a gate or research script checks a binary or a log with strings/cat piped into grep -q."
status: verified
epistemics: observed
date: 2026-11-15
tags: [bash, pipefail, gate, false-negative]
about: tools/gate.sh
route: enforcement
provenance:
  origin: authored
  derived_from:
    - issue:I0022
    - file:tools/gate.sh
relations:
  - type: documents
    target: file:.ai/repo/project/issues/I0022.yaml
---

# Lesson

`grep -q` exits at the first match. The producer (`strings`) then writes into a closed pipe and
dies of SIGPIPE (status 141), and `set -o pipefail` makes the pipeline fail. The first run of
`tools/gate.sh research` refused a binary that did carry `research_status` (2 matches):
`set -o pipefail; strings build-gate-research/openttd | grep -q '^research_status'` gave 141.

# How to apply

In a pipefail script, count instead (`[[ "$(producer | grep -c pat)" -gt 0 ]]`), or capture the
producer's output first. Outside pipefail (an interactive shell, or a criterion's `ref` command)
`grep -q` is fine.
