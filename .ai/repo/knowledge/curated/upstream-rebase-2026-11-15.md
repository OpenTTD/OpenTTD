---
schema: knowledge/v1
id: upstream-rebase-2026-11-15
kind: knowledge
class: memory
title: "On 2026-11-15 the fork moved from 317 commits behind upstream to a linear series on top of it"
description: "When the next upstream sync needs a baseline, or when a hash from before 2026-11-15 does not resolve."
status: verified
epistemics: observed
date: 2026-11-15
tags: [upstream, rebase, history]
provenance:
  origin: authored
  derived_from:
    - issue:I0021
    - file:.ai/repo/workflows/upstream-sync.md
relations:
  - type: documents
    target: file:.ai/repo/adrs/0011-the-fork-follows-upstream-by-rebasing-its-commits-onto-upstr.md
---

# What happened

- Old base `9130c591dc`, 317 upstream commits behind, 24 fork commits, no merges.
- Overlap: `src/console_cmds.cpp` only. The rebase had no conflict, and `git range-diff` showed
  24/24 patches unchanged.
- Two rewrites the same day: the rebase (backup `backup/master-pre-rebase-2026-11-15`) and
  the `Refs:` trailers (backup `backup/master-pre-trailers-2026-11-15`). Hashes such as
  `247fd19c7e` or `d63af1f505` name the old history.
- Verified: `tools/gate.sh change` 98/98 and the research-debug experiment
  `rebase-verify-2026-11-15b` PASS.
