---
schema: adr/v1
id: adr-0008
kind: adr
title: An agent reaches the game through an embedded loopback MCP server whose mutations pass a digest-bound human approval and the command system
status: proposed
date: 2026-07-24
tags:
  - mcp
  - research
  - security
  - determinism
related:
  - rule:project.game-state-mutates-only-through-commands
  - rule:project.research-code-is-gated-and-labelled
  - file:src/command.cpp
  - file:src/network/core/address.cpp
  - file:.ai/repo/adrs/0004-research-access-to-internals-is-permitted-and-research-code-.md
provenance:
  origin: extracted
  derived_from:
    - commit:0a49a3dbb85e7b87daf4cd16097040e433de623a
    - commit:3bfd9a096ca7a8638b6cddc70909f8c3bef69f76
    - commit:64d95df5ae0b02c7e89ecf2bf60d4ddcdabe592f
    - commit:b9a92468ce38fecb489aed83fe84f2f113f5eb8c
    - commit:5129fc214aa96c0732a51070e0bf731d66e82529
---

# 8. An agent reaches the game through an embedded loopback MCP server whose mutations pass a digest-bound human approval and the command system

## Context

An agent should be able to read live game state and, with approval, change it, without
breaking deterministic lockstep, savegame compatibility, frame time or local security. OpenTTD
has no inbound HTTP server, but it has a non-blocking listener, a main-thread network pump, a
vendored JSON library and a CSPRNG. All mutation must pass `Command<>::Post`; pools are
unsynchronised main-thread state.

## Decision

Embed a Model Context Protocol server (stable spec 2025-11-25) in the research fork, on the
branch `research/prismatic-mcp-server`:

1. **Compile gate.** `OPTION_PRISMATIC_MCP_SERVER`, default OFF, requires
   `OPTION_RESEARCH_INSTRUMENTATION=ON`; when OFF, nothing is compiled.
2. **Transport and threading.** Streamable HTTP bound to loopback only, serviced from
   `GameLoop()` with a zero-timeout `select`, so handlers read pools on the main thread with
   no locking, the same property the admin network relies on.
3. **Security.** A 256-bit bearer token per start, compared in constant time and never
   written to config or logs. Any `Origin` header is rejected, and request sizes are bounded.
4. **Read first, then approved mutation.** Read-only resources shipped first. Mutation
   (`openttd.game.set_pause`) goes through a typed DecisionEnvelope: `submit` returns a pending
   approval bound to a digest of the arguments, an operator approves in the console, and the
   resubmission executes through `Command<Commands::Pause>::Post`. Replay and changed
   arguments are refused. The answer separates EXECUTED_UNVERIFIED from VERIFIED_SUCCESS.
5. **No savegame change and no new command** for MCP convenience.

## Alternatives rejected

- **A sidecar process over the admin network.** Coarser, and cannot read pools directly.
- **A worker thread for the server.** Would need locking around every pool read.
- **Mutation before the approval guarantees exist.** No unsafe path ahead of its guarantees.

## Consequences

- The security-critical helpers (digest, Content-Length parse, bearer extraction,
  constant-time compare) are unit-tested through the production code path
  (`src/tests/test_prismatic_mcp.cpp`: 25 assertions in 5 cases).
- A generated contributor catalogue keeps README, AGENTS.md and the site in step, and
  `tools/check-prismatic-docs.sh` is wired into `tools/gate.sh change`.
- The branch has not landed on `master`. It was cut before the rebase onto upstream and must be
  rebased and re-verified with both options ON (issue I0017).
- The branch carries its own decision record,
  `research/prismatic-mcp/04-architecture-decision-record.md`. This ADR states the decision
  for the layer and points there for the 20-question design review.
