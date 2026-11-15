---
schema: moment/v1
id: an-agent-desyncs-the-game
kind: moment
title: 'An agent changes game state through a side channel and the simulation stops being deterministic'
short_title: 'Side-channel mutation'
hook: 'saw an agent tool report success for a change the game never made'
summary: 'A convenient write path skips the command system, so clients diverge or a dispatched command is reported as an observed effect.'
status: stable
severity: high
frequency: occasional
weight: 50
audiences: [prismatic-integrator, coding-agent, research-owner]
areas: [agent-integration]
lifecycle: [implementation, operations]
tags: [determinism, mcp, bridge, approval]
signals:
  - id: direct-pool-write
    text: 'A handler writes a pool directly instead of posting a command.'
  - id: dispatch-as-success
    text: 'A tool answers success when the command was only queued.'
examples:
  - id: pause
    audience: prismatic-integrator
    title: 'Pausing the game, properly'
    before: 'An agent flips a pause flag from a network callback.'
    after: 'The DecisionEnvelope is approved by an operator for its exact argument digest, runs Command<Commands::Pause>::Post, and reports VERIFIED_SUCCESS only once the game is observed paused.'
  - id: replay
    audience: prismatic-integrator
    title: 'The same approval twice'
    before: 'An approved decision is resubmitted to pause the game again.'
    after: 'The one-time, digest-bound approval refuses the replay.'
  - id: changed-args
    audience: research-owner
    title: 'Approved one thing, executed another'
    before: 'Arguments change between approval and execution.'
    after: 'The argument digest no longer matches and the call is refused.'
related: []
---

# An agent changes game state through a side channel and the simulation stops being deterministic

## The moment

An agent changes game state outside the command system, or reports a dispatched command as an observed effect.

## Why it happens

A direct write is shorter than a command, and asynchronous command results tempt a tool to answer early.

## What answers it

ADR 0007 (rcon-driven Company AI), ADR 0008 (DecisionEnvelope), rule `project.game-state-mutates-only-through-commands`.

## What it does not do

It does not decide whether a decision is a good one; it only guarantees that the game executed what the operator approved.
