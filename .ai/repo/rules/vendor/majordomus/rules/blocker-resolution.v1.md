---
id: majordomus.blocker-resolution
version: 1
kind: rule
title: Blocker resolution
description: No task can be completed while any question on this branch is unresolved; it can still be finished as blocked, partial, no_match or failed.
statement: No task is accepted as completed while any question on the branch is unresolved.
status: active
class: blocking
depends_on: [majordomus.externalise-decisions@1]
tags: [blockers, finish]

x-majordomus:
  validator: blockers
  category: blockers
  enforced_by: [check, finish]
  policy_key: no_open_blockers
  exit_code: 10
  claims: [finish-contract]
  tests: [test/cases/17_doctrine_enforcement.sh]
---

# Rationale

No task can be completed while any question on this branch is unresolved; it can still be finished as blocked, partial, no_match or failed.

# Required behaviour

No task is accepted as completed while any question on the branch is unresolved.

# Failure behaviour

A violation is a `FAIL` finding under the category `blockers`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_blockers` decides it, dispatched from `check, finish`. The behavioural case `test/cases/17_doctrine_enforcement.sh` proves it, and CI runs that case.
