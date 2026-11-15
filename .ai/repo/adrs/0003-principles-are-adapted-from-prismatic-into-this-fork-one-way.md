---
schema: adr/v1
id: adr-0003
kind: adr
title: Principles are adapted from Prismatic into this fork one way, and nothing flows back automatically
status: proposed
date: 2026-07-23
tags:
  - prismatic
  - governance
  - independence
related:
  - file:README.md
  - file:AGENTS.md
  - file:tools/gate.sh
provenance:
  origin: extracted
  derived_from:
    - commit:6b102e510a9d5ffabc0dcfe0a03473d6ebec8054
    - commit:5f9db99f0e1d738ffdc164f5e6755dae5f13cdaf
    - commit:fed1e29f56876512cac1aff85a6d147ab5feb59f
    - commit:765b9c144e9d319c85bac112265e7d08f363c77d
    - file:README.md
---

# 3. Principles are adapted from Prismatic into this fork one way, and nothing flows back automatically

## Context

The fork exists to validate the Prismatic platform (an Elixir/Phoenix umbrella for
decision support, OSINT, due diligence and modelling) and the agentic tooling built around
it. A validation target is only useful if it is independent of what it validates: a codebase
shaped by the tooling under test would confirm the tooling's own assumptions.

## Decision

The relationship is one-directional and at arm's length:

1. read a principle in Prismatic's own workflow (its `just check/precommit/ci` tiers, its
   fail-closed completion taxonomy),
2. classify it as transferable or not,
3. adapt it to OpenTTD's toolchain instead of copying code,
4. write it once, in `AGENTS.md`, `tools/gate.sh` or `research/`,
5. validate the adaptation here.

Nothing flows back into `prismatic-platform` automatically. A principle that does not fit is
rejected explicitly: the migration-rollback ban, for one, is backwards for OpenTTD's savegame
model, where old versions must stay loadable.

## Alternatives rejected

- **Vendoring Prismatic's tooling.** It would import a stack (Elixir, mix) the substrate does
  not have and make the target depend on the system under test.
- **A synchronised two-way link.** It would remove the independence that makes the fork
  worth testing against.

## Consequences

- The transferability table in `README.md` (section 0.2.4) is the record of what was adapted
  and what was rejected.
- Agent-workflow claims made on Prismatic can be tested on a codebase none of those agents
  shaped.
