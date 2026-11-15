---
schema: adr/v1
id: adr-0011
kind: adr
title: The fork follows upstream by rebasing its commits onto upstream master, and upstream is a fetch-only remote
status: proposed
date: 2026-11-15
tags:
  - upstream
  - history
  - rebase
related:
  - rule:project.no-upstream-interaction
  - file:AGENTS.md
  - file:.ai/repo/adrs/0001-this-fork-is-a-private-research-target-and-never-interacts-w.md
  - file:.ai/repo/workflows/upstream-sync.md
provenance:
  origin: extracted
  derived_from:
    - commit:81b26a6497a8f547a5f4358fa74d4e667a06560b
    - file:AGENTS.md
---

# 11. The fork follows upstream by rebasing its commits onto upstream master, and upstream is a fetch-only remote

## Context

The fork's base fell 317 upstream commits behind. Upstream keeps changing internal APIs
(for example, `StringID` became a strong type), and the research code depends on them. A merge
history would interleave 24 fork commits with hundreds of upstream ones and hide which
commits are the fork's.

## Decision

The fork follows upstream by rebasing its commits onto `upstream/master`, keeping them
as one linear series on top of upstream.

- `upstream` is `https://github.com/OpenTTD/OpenTTD.git`, fetch-only: its push URL is set to
  `no-push-to-upstream`.
- Before a rebase, the current `master` is kept as `backup/master-pre-rebase-<date>`.
- `git range-diff` proves each fork patch survived unchanged, and the files touched by both
  sides are listed before the rebase.
- The rebased tree is verified with research instrumentation ON, because a default build does
  not compile the gated code.
- `origin/master` is updated with `--force-with-lease`, and only on an explicit instruction.

## Alternatives rejected

- **Merging upstream into master.** Hides the fork's series.
- **Staying on the old base.** Upstream API drift makes every later catch-up harder.

## Consequences

- Every rebase rewrites the published `master`. Other checkouts and the branch
  `research/prismatic-mcp-server` must be rebased onto it (issue I0017).
- The procedure is `.ai/repo/workflows/upstream-sync.md`.
- The 2026-11-15 rebase: 24/24 patches unchanged, one overlapping file
  (`src/console_cmds.cpp`), no conflict.
