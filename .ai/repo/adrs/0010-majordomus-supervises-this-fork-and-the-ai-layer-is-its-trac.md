---
schema: adr/v1
id: adr-0010
kind: adr
title: Majordomus supervises this fork and the .ai/ layer is its tracked governance record
status: proposed
date: 2026-11-15
tags:
  - majordomus
  - governance
  - supervision
related:
  - file:.ai/repo/policy.yaml
  - file:.ai/manifest.yaml
  - file:AGENTS.md
  - file:CLAUDE.md
  - file:GEMINI.md
provenance:
  origin: extracted
  derived_from:
    - commit:81b26a6497a8f547a5f4358fa74d4e667a06560b
    - file:.ai/repo/policy.yaml
    - file:.ai/manifest.yaml
---

# 10. Majordomus supervises this fork and the .ai/ layer is its tracked governance record

## Context

Work on the fork has been done by several agents and providers (Claude Sonnet, Opus and
Fable sessions, Codex and Gemini skills in the local tooling) across months. Its decisions
lived in commit bodies and README prose, its plan in nobody's file, and its continuity in
conversations that were discarded.

## Decision

Majordomus supervises the checkout. The `.ai/` layer is tracked (a plain `majordomus init`,
not a personal one): policy, profiles, the vendored rule baseline plus project rules, ADRs,
curated knowledge, the plan (milestones, issues, intents), the product features and the
catalogue of why it exists. Provider files (`AGENTS.md`, `CLAUDE.md`, `GEMINI.md`) carry a
generated Majordomus region below the hand-written content. `.ai/local/` stays ignored.

The pre-commit hook runs `majordomus doctor`, and the pre-push hook runs
`majordomus finish --check`.

## Alternatives rejected

- **A personal layer covered by `info/exclude`.** Discovery reads the git index, so an
  untracked layer falls back to a walk of the whole work tree and its records reach no
  surface.
- **Replacing AGENTS.md with a generated file.** It would discard the hand-written ground
  rules.

## Consequences

- Every commit pays for a doctor run (24 to 32 s here, over the 20 s budget).
- Commits name the issue they serve with a `Refs: I00NN` trailer. The fork's history was
  rewritten once to add them (issue I0020).
- The personal tooling (`.claude/`, `.aiad/`, `.agents/`, `.codex/`) stays excluded and
  uncommitted.
