---
schema: adr/v1
id: adr-0014
kind: adr
title: Interface input from an agent is armed per process by the owner and yields to the owner's mouse, and every action it causes is an ordinary client command
status: proposed
date: 2026-11-16
tags:
  - mcp
  - security
related:
  - file:.ai/repo/adrs/0008-an-agent-reaches-the-game-through-an-embedded-loopback-mcp-s.md
  - file:.ai/repo/adrs/0013-every-openttd-process-may-run-its-own-mcp-server-and-its-vie.md
  - rule:project.game-state-mutates-only-through-commands
  - file:src/prismatic_mcp/prismatic_mcp_ui.cpp
  - file:research/mcp-human.md
provenance:
  origin: extracted
  derived_from:
    - commit:f308b13c03b1778b7d4cf4a31f83749ffe35e024
    - issue:I0044
---

# 14. Interface input from an agent is armed per process by the owner and yields to the owner's mouse, and every action it causes is an ordinary client command

## Context

The owner asked for a bot that plays "like a person through the client's own interface": it
reads the windows as data and moves the visible cursor, so every build step is seen (issue
I0044, commit f308b13c03). Moving the cursor and pressing keys is a stronger capability than the
read-only resources and the single approval-gated tool of ADR 0008: every button of the client
becomes reachable, and the actions it triggers are whatever the interface does.

## Decision

1. **Input is fed where a video driver feeds it.** `openttd.input.pointer`, `button`, `wheel`
   and `key` (`src/prismatic_mcp/prismatic_mcp_ui.cpp`) call `HandleMouseEvents()` and
   `HandleKeypress()` with `_current_company` set to `_local_company`
   (`src/prismatic_mcp/prismatic_mcp_ui.cpp:AsDriver`). An action that changes the game is the
   command the interface posts, from that client, for its own company.
2. **Armed per process, by the owner.** The input tools refuse unless the process was started
   with `PRISMATIC_MCP_INPUT=1` (`src/prismatic_mcp/prismatic_mcp_ui.cpp:Armed`); the read tools
   (`openttd.ui.*`, `openttd.input.state`) are always served.
3. **The person wins.** Input refuses while the real mouse has moved within the last three
   seconds (`src/prismatic_mcp/prismatic_mcp_ui.cpp:OwnerHasMouse`, `OWNER_GRACE`).
4. **No interface in a dedicated server.** Every UI tool refuses there.

## Alternatives rejected

None is recorded in the history. In particular, the history does not record a decision to route
each interface action through the digest-bound approval of ADR 0008; this record states that the
input path does not, and leaves to the owner whether that is the intended boundary.

## Consequences

- The approval boundary for interface play is the start-time switch and the owner's hand, not a
  per-action approval. `--auto-approve` does not govern it.
- Arming also subsumes the approval of ADR 0008 in that process: key input reaches the console
  window, which executes `prismatic_mcp approve <id>` (`src/console_gui.cpp`,
  `src/console_cmds.cpp:ConPrismaticMcp`). An armed controller can approve its own decisions
  (traced in the source, not run; knowledge candidate `armed-input-can-approve-its-own-decisions`).
  If the owner wants approval to bind an armed controller, the console must refuse injected input.
- Multiplayer safety rests on the command system, not on this module: the client acts only for
  its own company and the server validates every command as it would a person's
  (`research/mcp-human.md`, "Network rules unchanged"; rule
  `project.game-state-mutates-only-through-commands`).
- A gated read-only hook in `src/dropdown.cpp` (`src/prismatic_mcp/prismatic_mcp_hooks.h`) exposes
  an open dropdown's items; it is an upstream file the fork modifies and a rebase must carry.
- Tests: `tools/research/mcp-human-e2e --test` asserts on the server's map; the C++ side's input
  path has no unit test (`src/tests/test_prismatic_mcp.cpp` covers the server's helpers).
