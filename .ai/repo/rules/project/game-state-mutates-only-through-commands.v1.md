---
id: project.game-state-mutates-only-through-commands
version: 1
kind: rule
title: Game state changes only through the command system, research hooks included
description: OpenTTD multiplayer is deterministic lockstep. Every mutation of persistent game state is a registered command (Command<>::Post), run in test and execute passes; a research hook, an MCP tool or a bridge never writes a pool directly.
statement: Change vehicles, companies, map tiles or any other persistent game state only through a registered command issued with Command<>::Post, never by writing a pool from a research hook, the MCP server or any other side channel.
status: active
class: blocking
depends_on: [project.research-code-is-gated-and-labelled@1]
tags: [determinism, command-system, research, mcp]
---

# Rationale

Every client must compute bit-identical results from the same command stream. A direct pool
write from an instrumentation point or an agent channel would desynchronise a network game
and would corrupt the substrate the experiments rely on.

# Required behaviour

- Research code reads pools on the main thread (the MCP server is serviced from `GameLoop()`
  for that reason) and mutates only by issuing existing commands.
- No new command is added "for MCP convenience" when an existing command expresses the change.
- Command handlers stay deterministic: no wall-clock time, no host-dependent iteration order,
  no uninitialised reads.
- A tool reports dispatch (EXECUTED_UNVERIFIED) separately from an observed effect
  (VERIFIED_SUCCESS).

# Failure behaviour

A mutation outside a command handler blocks the change.

# Related

ADR 0008, `src/command.cpp`, `src/command_type.h`, `CLAUDE.md` (The command system is the
core invariant).
