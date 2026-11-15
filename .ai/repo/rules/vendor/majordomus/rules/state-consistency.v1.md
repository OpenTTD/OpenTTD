---
id: majordomus.state-consistency
version: 1
kind: rule
title: State consistency
description: The task record still describes this checkout — same branch, and HEAD at or ahead of the recorded commit.
statement: A task record is trusted only while it still describes this checkout: same branch, HEAD at or ahead of the recorded commit.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [state, git]

x-majordomus:
  validator: state
  category: state
  enforced_by: [check, finish, watch]
  policy_key: state_updated
  exit_code: 10
  claims: [git-identity, divergence-label, consistency-check]
  tests: [test/cases/04_start_check.sh]
---

# Rationale

The task record still describes this checkout — same branch, and HEAD at or ahead of the recorded commit.

# Required behaviour

A task record is trusted only while it still describes this checkout: same branch, HEAD at or ahead of the recorded commit.

# Failure behaviour

A violation is a `FAIL` finding under the category `state`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_state` decides it, dispatched from `check, finish, watch`. The behavioural case `test/cases/04_start_check.sh` proves it, and CI runs that case.
