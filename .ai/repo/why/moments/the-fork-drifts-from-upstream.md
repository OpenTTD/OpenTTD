---
schema: moment/v1
id: the-fork-drifts-from-upstream
kind: moment
title: 'A research fork falls hundreds of commits behind and its results describe an OpenTTD that no longer exists'
short_title: 'Upstream drift'
hook: 'found the fork 317 commits behind upstream'
summary: 'Every week of drift makes the catch-up harder and the research less applicable to current code.'
status: stable
severity: medium
frequency: common
weight: 60
audiences: [research-owner, coding-agent]
areas: [upstream-currency]
lifecycle: [maintenance]
tags: [upstream, rebase, history]
signals:
  - id: behind-count
    text: 'git rev-list --count master..upstream/master is in the hundreds.'
examples:
  - id: rebase
    audience: research-owner
    title: 'One linear series on top of upstream'
    before: 'An old base; upstream API changes unknown to the research code.'
    after: 'A rebase with backup, range-diff, and a build of the gated code with the option ON.'
  - id: branch-left-behind
    audience: coding-agent
    title: 'A branch cut from the old base'
    before: 'research/prismatic-mcp-server still sits on the pre-rebase master.'
    after: 'Issue I0017 rebases it and re-verifies it with both options ON.'
  - id: hash-moved
    audience: research-owner
    title: 'A hash that no longer resolves'
    before: 'A note cites d63af1f505 after the rebase.'
    after: 'Knowledge upstream-rebase-2026-11-15 names the backup branches that keep the old history.'
related: [research-code-leaks-into-release]
---

# A research fork falls hundreds of commits behind and its results describe an OpenTTD that no longer exists

## The moment

The fork's base falls far behind upstream, and research results describe code that has since changed.

## Why it happens

A rebase rewrites published history, so it is put off, and the catch-up grows with every week.

## What answers it

ADR 0011, `.ai/repo/workflows/upstream-sync.md`, issue I0021.

## What it does not do

It does not contribute anything upstream (rule `project.no-upstream-interaction`).
