---
schema: adr/v1
id: adr-0002
kind: adr
title: Validation runs in three cost tiers behind one gate script, and a report never claims a step that did not run
status: proposed
date: 2026-07-23
tags:
  - validation
  - gating
  - reporting
related:
  - rule:project.never-report-unrun-validation-as-pass
  - file:tools/gate.sh
  - file:research/README.md
  - file:research/experiment-template.md
provenance:
  origin: extracted
  derived_from:
    - commit:ad96e57c4ad014a28667773bfbcecd55a6669a8a
    - commit:83133b7cef6be3c638c94e2226dde363881fdc7f
    - file:tools/gate.sh
    - file:research/README.md
    - file:research/experiment-template.md
---

# 2. Validation runs in three cost tiers behind one gate script, and a report never claims a step that did not run

## Context

A change in a codebase this size can be checked at very different cost: a diff check
and an incremental build in seconds, the full ctest suite in minutes, a clean-room configure
and build far longer. Without a shared vocabulary, agents either ran nothing and reported
success, or ran everything for a one-line doc fix. Experiments needed a reporting taxonomy
that could not round a partial result up to a pass.

## Decision

Validation has one entry point, `tools/gate.sh`, with three tiers over the existing
CMake/CTest toolchain, plus a reporting layer:

| Tier | Runs | When |
|---|---|---|
| `smoke` | `git diff --check`, incremental build, optional `ctest -R` | while iterating |
| `change` | configure if missing, incremental build, full ctest, diff hygiene, untracked review | before a task is done |
| `full` | wipe `build-fullgate/`, configure, build, test from scratch | significant changes, baselines |

A report uses the fail-closed taxonomy of `research/README.md`: PASS, FAIL, PARTIAL,
NOT RUN, NOT APPLICABLE. A result that is ambiguous is not a pass, and a gate that did not
run is reported as NOT RUN.

## Alternatives rejected

- **A separate lint or format gate.** The codebase has no `.clang-format` or linter; inventing
  one would fake a check.
- **A pass/fail binary.** It hides incomplete coverage, which PARTIAL makes visible.
- **Bypassing a failing gate** (`--no-verify`, "fix it in a follow-up"). A failed gate means
  nothing was committed; the human decides on an override and its reason.

## Consequences

- The baseline measured when the tiers landed: `tools/gate.sh change`, 98/98 ctest PASS.
- The tier names are reused by `tools/research/research validate --gate` (ADR 0005).
- The gate never commits or pushes and never touches files outside the repository.
