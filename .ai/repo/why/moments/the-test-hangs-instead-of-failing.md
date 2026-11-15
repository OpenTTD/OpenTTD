---
schema: moment/v1
id: the-test-hangs-instead-of-failing
kind: moment
title: 'A test suite times out instead of failing, and the cause is outside the repository'
short_title: 'Hanging suite'
hook: 'watched four regression tests time out at 120 seconds with no error message'
summary: 'An environmental prerequisite turns a fast failure into a silent hang that reads like a code defect.'
status: stable
severity: medium
frequency: occasional
weight: 20
audiences: [coding-agent, research-owner]
areas: [validation]
lifecycle: [onboarding, implementation]
tags: [macos, baseset, environment]
signals:
  - id: timeout-no-output
    text: 'A test ends at its timeout with no output at all.'
  - id: fresh-machine
    text: 'It passed before and fails on a machine whose personal data directory was cleared.'
examples:
  - id: baseset-dialog
    audience: coding-agent
    title: 'A modal dialog nobody can see'
    before: 'Regression tests hang under null drivers; the agent hunts for a code bug.'
    after: 'The knowledge record names the missing base set and the one-line check for it.'
  - id: research-run-124
    audience: coding-agent
    title: 'Exit 124 and an empty stdout'
    before: 'The headless research run times out and the agent suspects the rebase broke research_status.'
    after: 'The missing base set is checked first; after OpenGFX the line appears with tick=1392.'
  - id: ctest-gs
    audience: research-owner
    title: 'Did the compilation fail?'
    before: 'regression_gs reports that it did not output anything, and the owner suspects the GS compiler.'
    after: 'The same knowledge record explains the four cases together.'
related: [a-pass-that-never-ran]
---

# A test suite times out instead of failing, and the cause is outside the repository

## The moment

A suite that should fail fast hangs until its timeout because a prerequisite outside the repository is missing.

## Why it happens

On macOS the missing-base-set error is a modal NSAlert, even under null drivers, and the base set lives in `~/Documents/OpenTTD/baseset/`, which no gate checks.

## What answers it

Issue I0008, `research/baseset-prerequisite-macos.md`, knowledge `macos-missing-baseset-hangs-headless-runs`.

## What it does not do

It does not install the base set automatically or add a gate check for it. Both remain open choices.
