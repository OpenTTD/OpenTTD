---
schema: adr/v1
id: adr-0001
kind: adr
title: This fork is a private research target and never interacts with upstream
status: proposed
date: 2026-07-23
tags:
  - governance
  - upstream
  - fork
related:
  - rule:project.no-upstream-interaction
  - rule:project.no-commit-without-current-instruction
  - file:AGENTS.md
  - file:CONTRIBUTING.md
provenance:
  origin: extracted
  derived_from:
    - commit:8d5f34027d6a59296b8b1bf33aa06a13fe05799c
    - commit:ad96e57c4ad014a28667773bfbcecd55a6669a8a
    - file:AGENTS.md
    - file:README.md
---

# 1. This fork is a private research target and never interacts with upstream

## Context

`korczis/OpenTTD` is a fork of a mature, volunteer-run upstream whose `CONTRIBUTING.md`
closes LLM-generated issues and pull requests and blocks repeat submitters. The fork exists
for a different purpose: it is the owner's private academic research target, used to
validate the Prismatic platform and to study agent-driven engineering on a real, unfamiliar
C++ codebase. Without an explicit boundary, an agent reading the tree would take upstream's
contribution process (commit grammar, PR templates, "ask before large changes") as the local
process, or worse, try to send work upstream.

## Decision

The fork is a private research target and is never staged for upstream contribution.

- No push, issue or pull request ever reaches `OpenTTD/OpenTTD`; everything stays in
  `korczis/OpenTTD`.
- Upstream's contribution process is background reference, not a blocking gate here.
- Upstream's *technical* constraints (the command system, savegame compatibility, code
  style) stay binding when OpenTTD's own code is touched: they are what makes the substrate
  meaningful to experiment against.
- `AGENTS.md` is the single vendor-neutral statement of the ground rules; `CLAUDE.md` and
  every other provider file point to it instead of restating it.

## Alternatives rejected

- **Contributing upstream.** The upstream policy forbids AI-generated contributions, and
  the research goal does not need it.
- **Following upstream's process locally.** It governs a many-maintainer project and adds
  no evidence to single-owner experiments.
- **One instruction file per provider.** It would fork the rules between agents.

## Consequences

- Every agent session starts from the same ground rules, whichever provider runs it.
- A commit or push needs an explicit, current instruction; an approval does not carry forward.
- The rule is enforced by review, not by a tool: `project.no-upstream-interaction` states it.
  The `upstream` remote's push URL is disabled (ADR 0011) as the one mechanical guard.
