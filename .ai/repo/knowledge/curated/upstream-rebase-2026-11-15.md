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
- Two rewrites the same day: the rebase and the `Refs:` trailers. The MCP branch was rebased
  separately (`git rebase --onto master 0822f6b762`). The three pre-rewrite heads are kept as
  annotated tags on origin, `archive/master-pre-rebase-2026-11-15`,
  `archive/master-pre-trailers-2026-11-15` and
  `archive/research-prismatic-mcp-server-pre-rebase-2026-11-15`. Hashes such as `247fd19c7e`,
  `d63af1f505` or `5129fc214a` name that old history; `git log archive/<name>` reaches it.
- The tags replaced the local `backup/*` branches on the owner's decision. Question (session
  cd8d20a8, 2026-11-15): "The three backup branches ... carry no content that master lacks:
  every change is already in master, only the old commit identities differ. How should they be
  integrated?" Answer: "Archive tags on origin (Recommended)". Rejected: a `-s ours` merge into
  master (it would break the linear history of ADR 0011), and pushing them as branches.
- Verified: `tools/gate.sh change` 98/98 and the research-debug experiment
  `rebase-verify-2026-11-15b` PASS.
