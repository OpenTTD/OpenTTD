---
schema: moment/v1
id: an-experiment-nobody-can-rerun
kind: moment
title: 'An experiment result whose build and flags nobody recorded'
short_title: 'Unrepeatable run'
hook: 'had a result and could not say which build options produced it'
summary: 'Configuration lives in a terminal; the comparison between an instrumented and a plain build is remembered rather than recorded.'
status: stable
severity: high
frequency: common
weight: 30
audiences: [research-owner, coding-agent, prismatic-integrator]
areas: [observability]
lifecycle: [implementation, review]
tags: [reproducibility, manifest, evidence]
signals:
  - id: flags-in-scrollback
    text: 'The CMake options and runtime flags exist only in shell history.'
  - id: same-agent-different-result
    text: 'Two runs of the same agent cannot be compared because their configurations differ unknowingly.'
examples:
  - id: manifest
    audience: research-owner
    title: 'The run that wrote its own record'
    before: 'A research_status line is pasted into a note with no build type.'
    after: 'The experiment manifest holds profile, toolchain, commands and exit codes, and summary.md is generated from it.'
  - id: toolchain
    audience: coding-agent
    title: 'Which compiler built it'
    before: 'A run built with Homebrew LLVM on one day and with the system compiler on another, and nobody knows.'
    after: 'The manifest records the toolchain the probe selected.'
  - id: bridge-measure
    audience: prismatic-integrator
    title: 'Numbers without a build'
    before: 'company_value 1 to 11025 is quoted without the build that produced it.'
    after: 'The experiment bundle carries the build profile and the commands next to the number.'
related: [research-code-leaks-into-release]
---

# An experiment result whose build and flags nobody recorded

## The moment

A result exists, and the build options, flags and commands that produced it do not.

## Why it happens

Configuration lives in shell history and build directories are reused across experiments.

## What answers it

ADR 0005, `tools/research/research` and its manifest schema, ADR 0006 for the toolchain.

## What it does not do

It does not make the dedicated server's stdin start-up deterministic; that race is recorded as a limitation.
