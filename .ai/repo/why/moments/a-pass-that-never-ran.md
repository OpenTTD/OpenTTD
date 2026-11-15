---
schema: moment/v1
id: a-pass-that-never-ran
kind: moment
title: 'A report says PASS for a check that never ran'
short_title: 'Unrun PASS'
hook: 'read "all tests pass" and later found the suite was never executed'
summary: 'An agent rounds an unrun or partial validation up to success, and the owner learns it only when something breaks.'
status: stable
severity: high
frequency: common
weight: 10
audiences: [research-owner, coding-agent, prismatic-integrator]
areas: [validation]
lifecycle: [implementation, review]
tags: [reporting, gates, evidence]
signals:
  - id: should-be-fine
    text: 'A report says "should be fine" or "PASS" without the command that produced it.'
  - id: partial-as-pass
    text: 'Coverage was incomplete, and the result still reads as a pass.'
examples:
  - id: doc-fix-full-gate
    audience: coding-agent
    title: 'Ninety-eight tests that did not run'
    before: 'A change is reported done on the strength of an earlier 98/98 run on a different tree.'
    after: 'The report names tools/gate.sh change, its exit status and the tree it ran on, or says NOT RUN.'
  - id: stale-baseline
    audience: research-owner
    title: 'A baseline from another machine'
    before: 'README cites 98/98 in 7.40 s; a fresh machine hangs in the regressions.'
    after: 'The report says which machine and tree the count is from, and a rerun is labelled as one.'
  - id: bridge-effect
    audience: prismatic-integrator
    title: 'Dispatched is not done'
    before: 'A bridge call returns 200 and the report says the company built a route.'
    after: 'The report quotes the observed company_value and vehicle count from the live server.'
related: [the-test-hangs-instead-of-failing]
---

# A report says PASS for a check that never ran

## The moment

An agent's report claims a check passed when it did not run, ran on a different tree, or ran only in part.

## Why it happens

Running the full suite costs minutes and a report is cheap. Without a shared taxonomy, an agent rounds up to finish.

## What answers it

ADR 0002, `tools/gate.sh`, the PASS/FAIL/PARTIAL/NOT RUN/NOT APPLICABLE taxonomy of `research/README.md`, rule `project.never-report-unrun-validation-as-pass`.

## What it does not do

It does not make a gate cheaper, and it does not decide which tier a change needs. That remains the worker's judgement, which the report then exposes.
