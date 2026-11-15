---
schema: feature/v1
id: supervised-ai-layer
kind: feature
title: Supervised AI layer derived from the fork's history
short_title: AI layer
headline: Decisions, rules, plan, intents and knowledge live in tracked records that every agent and provider reads the same way.
summary: Majordomus supervises the checkout; .ai/ holds 11 ADRs, project rules, curated knowledge, 6 milestones, 21 issues and 3 intents, all cross-linked and traced to commits by Refs trailers.
status: stable
weight: 70
areas: [validation, upstream-currency]
audiences: [research-owner, coding-agent]
rules: [project.no-upstream-interaction, project.no-commit-without-current-instruction, project.personal-tooling-stays-uncommitted]
docs: [AGENTS.md]
adrs: [adr-0010, adr-0011]
related: [layered-validation-gate]
tags: [majordomus, governance, traceability]
---

## What it does

Every commit runs `majordomus doctor`, every push runs `majordomus finish --check`, and the
plan's state is derived from the recorded stamps and evidence.

## What it does not do

It does not accept a proposed ADR. That is the owner's act.
