---
schema: knowledge/v1
id: checks-can-test-an-old-panel-when-a-stack-moves-port
kind: knowledge
class: lesson
title: "A check run against :8800 can test an old panel: mcp-e2e moves a new stack to a free port when 8800 is held"
description: "When restarting the main stack and then running mcp-panel-check, mcp-route-check or mcp-view-check against the default address."
status: verified
epistemics: observed
date: 2026-11-16
tags: [panel, checks, mcp-e2e, ports, verification]
about: tools/research/mcp-e2e
route: platform
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-e2e
    - file:tools/research/mcp-view-check
---

# Lesson

`mcp-e2e` gives a new stack a free port when its preferred one is busy, and prints the address it
took ("control window: http://127.0.0.1:55005"). The checks default to `http://127.0.0.1:8800`.
If an old stack still holds 8800, the checks pass against the old panel's code, and the new code
goes untested.

On 2026-11-16 this happened after a restart that stopped one subshell of the old launcher instead
of the launcher itself (`mcp-e2e` runs as a parent with subshells; the parent, with parent PID 1
after `nohup`, owns the cleanup trap). The old panel kept 8800, the new stack went to 55005, and all
three checks passed on code from seven hours before.

# How to apply

- Stop a stack by its launcher: the `bash tools/research/mcp-e2e` process whose parent is 1 (or the
  shell that started it), not a child subshell. Check that the port is free before starting again:
  `lsof -nP -iTCP:8800 -sTCP:LISTEN`.
- After a restart, read the "control window:" line of the new stack's log and run the checks
  against that address, or confirm it is the address the checks use.
