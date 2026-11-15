---
schema: moment/v1
id: research-code-leaks-into-release
kind: moment
title: 'Instrumentation added for one experiment ends up in every build'
short_title: 'Leaking instrumentation'
hook: 'found a debug hook in a release binary months after the experiment ended'
summary: 'Research access to internals without containment turns into permanent debug output and unclear release behaviour.'
status: stable
severity: medium
frequency: occasional
weight: 40
audiences: [coding-agent, research-owner]
areas: [observability]
lifecycle: [implementation, maintenance]
tags: [instrumentation, containment, gating]
signals:
  - id: runtime-flag-only
    text: 'A research hook is guarded by a runtime flag and still compiled into every binary.'
examples:
  - id: strings-check
    audience: research-owner
    title: 'Absent, provably'
    before: 'Nobody can say whether research_status ships in the release build.'
    after: 'strings finds it in research-debug and not in baseline-release, because the option is compile-time and default OFF.'
  - id: rebase
    audience: coding-agent
    title: 'Upstream changed the API under the gate'
    before: 'A rebase passes the default build and breaks research_status unseen.'
    after: 'The workflow builds the research-debug profile on the rebased tree.'
  - id: mcp-off
    audience: research-owner
    title: 'Zero footprint'
    before: 'Nobody can say what the MCP server adds to a normal build.'
    after: 'With OPTION_PRISMATIC_MCP_SERVER OFF nothing of it is compiled.'
related: [an-experiment-nobody-can-rerun]
---

# Instrumentation added for one experiment ends up in every build

## The moment

Instrumentation added for one experiment stays in every build, or silently breaks because no build compiles it.

## Why it happens

Runtime flags are easier than compile gates, and a default build cannot see gated code.

## What answers it

ADR 0004, rule `project.research-code-is-gated-and-labelled`, knowledge `gated-code-is-invisible-to-a-default-build`.

## What it does not do

It does not review the research code itself; it only keeps it contained and verified.
