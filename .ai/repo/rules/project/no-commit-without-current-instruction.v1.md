---
id: project.no-commit-without-current-instruction
version: 1
kind: rule
title: A commit or a push needs an explicit instruction for this change, now
description: Approval in an earlier turn, or for a different change, does not carry forward. History rewrites and force pushes need the same instruction, naming the rewrite.
statement: Commit, push, rewrite history or force-push only on an explicit, current instruction from the owner that covers this change.
status: active
class: blocking
depends_on: []
tags: [governance, git, consent]
---

# Rationale

The fork's history is the evidence of the research. A commit nobody asked for mixes an
agent's guess into that record, and a push or a rewrite publishes it.

# Required behaviour

- Before any command that can discard uncommitted work (`checkout`, `restore`, `reset`,
  `clean`, `rm -rf` inside the repository), read `git status` and preserve what is there.
- Before a rebase or a rewrite, keep the current head as a `backup/<branch>-pre-<what>-<date>`
  branch.
- A force push uses `--force-with-lease`.

# Failure behaviour

Without the instruction, the worker stops and asks; it does not commit "to be safe".

# Related

ADR 0011, `AGENTS.md` (Ground rules).
