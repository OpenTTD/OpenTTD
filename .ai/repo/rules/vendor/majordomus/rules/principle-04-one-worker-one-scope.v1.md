---
id: majordomus.one-worker-one-scope
version: 1
kind: rule
title: One worker, one clear scope
description: A task claims the paths it may touch, and touches only those.
statement: Claim the paths a task needs when it starts; if the fix is elsewhere, stop and record it rather than widening the scope silently.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

Two workers editing one file, and forty worktrees producing thousands of concurrently modified files, are the failures a declared scope prevents. The scope is a contract other workers can read and a boundary a command can check.

# Required behaviour

Claim the paths a task needs when it starts; if the fix is elsewhere, stop and record it rather than widening the scope silently.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
