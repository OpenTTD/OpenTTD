---
id: majordomus.isolated-parallelism
version: 1
kind: rule
title: Parallel work requires isolation
description: Do not start a second task in a checkout that already has one; overlapping claims across worktrees are reported.
statement: Keep one active task per checkout, and treat a scope claimed by another worktree as a boundary to report, not to cross.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

Parallelism without isolation is how two patches for the same change came to exist dozens of times over. One task per checkout and visible overlap are the cheapest isolation there is.

# Required behaviour

Keep one active task per checkout, and treat a scope claimed by another worktree as a boundary to report, not to cross.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
