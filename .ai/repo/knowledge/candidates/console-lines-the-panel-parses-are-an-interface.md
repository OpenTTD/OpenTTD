---
schema: knowledge/v1
id: console-lines-the-panel-parses-are-an-interface
kind: knowledge
class: lesson
title: "The panel parses console output, so a reworded console line in src/ can stop every stack; run mcp-e2e after changing one"
description: "Before changing the text of a console line that tools/research parses (the bearer token line, the MCP endpoint, approvals)."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [prismatic-mcp, console, panel, verification]
about: src/console_cmds.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/console_cmds.cpp
    - file:tools/research/mcp-panel/server.py
    - commit:5a26a6189ea7d3e7616b946283ee0e77dd849dbf
    - commit:812defb0ce76d1af188bbc5a2a052a82c94f90a8
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/prismatic-mcp/README.md
---

# Statement

`tools/research/mcp-panel/server.py` reads the dedicated server's console and takes the MCP bearer
token from the `Bearer token (...): <hex>` line (`TOKEN_RE`). Commit 5a26a6189e (I0061) reworded that
line in `src/console_cmds.cpp:ConPrismaticMcp`. Its research gate passed, because the gate builds and
unit-tests the server but never starts the panel. Every stack started from that master then reported
"MCP did not come up", until 812defb0ce made the panel match the line by its shape.

# What to do

A console line that a tool parses is an interface. Change its text only with the parser in the same
commit, and verify the change with a running stack (`tools/research/mcp-e2e --no-gui --no-browser`
with explicit free ports until "MCP up"), not with `tools/gate.sh research` alone.
