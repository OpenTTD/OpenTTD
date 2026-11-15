---
schema: feature/v1
id: research-instrumentation
kind: feature
title: Compile-gated research instrumentation
short_title: Instrumentation
headline: Internal state becomes observable in research builds and provably absent from every other build.
summary: OPTION_RESEARCH_INSTRUMENTATION (default OFF) compiles RESEARCH-ONLY hooks such as the read-only research_status console command.
status: stable
weight: 20
featured: true
areas: [observability]
audiences: [coding-agent, research-owner]
rules: [project.research-code-is-gated-and-labelled, project.game-state-mutates-only-through-commands]
docs: [research/tooling.md]
adrs: [adr-0004]
related: [research-experiment-cli, embedded-mcp-server]
tags: [instrumentation, cmake, console]
---

## What it does

`research_status` prints one line: instrumentation state, tick, date, vehicle and company
counts. It mutates nothing.

## What it does not do

It is not a stable API, and an instrumented result says nothing about release behaviour without
separate evidence.
