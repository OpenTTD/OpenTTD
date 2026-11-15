---
schema: adr/v1
id: adr-0004
kind: adr
title: Research access to internals is permitted, and research code is compile-gated, labelled and removable
status: proposed
date: 2026-07-24
tags:
  - research
  - instrumentation
  - internals
related:
  - rule:project.research-code-is-gated-and-labelled
  - rule:project.game-state-mutates-only-through-commands
  - file:AGENTS.md
  - file:cmake/Options.cmake
  - file:src/console_cmds.cpp
provenance:
  origin: extracted
  derived_from:
    - commit:00e4f6d53ba020ccfbef3876fed71b511b4560b1
    - commit:8c4cc4fc322f036c032aa6241073f5f0e20a3b5e
    - file:AGENTS.md
    - file:cmake/Options.cmake
    - file:src/console_cmds.cpp
---

# 4. Research access to internals is permitted, and research code is compile-gated, labelled and removable

## Context

Upstream's norms (public-API purity, no permanent debug output) explain why the code looks
the way it does, but research questions about the simulation need its internal state to be
observable: tick counters, pool sizes, pathfinder decisions. Agents that treated the norms as
rules either refused to look inside or built artificial abstractions to avoid it.

## Decision

Controlled access to internals is an expected part of research work in this fork, within an
already-scoped task: internal headers and symbols, debug builds, `OPTION_USE_ASSERTS`,
`-d <facility>=<level>` logging, sanitizer flags, state dumps and research-only hooks.

Research code is contained:

- compile-gated behind a default-OFF CMake option (`OPTION_RESEARCH_INSTRUMENTATION` defines
  `OTTD_RESEARCH_INSTRUMENTATION`), so default and release builds carry none of it;
- labelled at the point of change (`RESEARCH-ONLY`, `UNSTABLE`, `DEBUG-ONLY`);
- read-only unless the experiment needs otherwise, and then only through the command system;
- the smallest mechanism that makes the needed state observable.

The first instance is the `research_status` console command: a one-line, read-only snapshot
of tick, date, vehicle and company counts.

## Alternatives rejected

- **A public abstraction over every internal read.** It adds complexity without improving the
  experiment.
- **Runtime-only gating.** It leaves research symbols in every binary; the compile gate makes
  absence provable (`strings` finds `research_status` in research-debug and not in
  baseline-release).
- **Exposing a diagnostic interface on a public network interface.** Out of bounds without an
  explicit reason and protection.

## Consequences

- Every report that touched internals states the APIs used, the instrumentation added, the
  build and runtime flags and the observer effects.
- A debug-build or instrumented result is never presented as release-build behaviour.
- Upstream merges touch the gated code: the rebase onto upstream (ADR 0011) is verified by
  building with the option ON, since a default build does not compile it.
