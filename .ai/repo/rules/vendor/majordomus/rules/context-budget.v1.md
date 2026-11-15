---
id: majordomus.context-budget
version: 1
kind: rule
title: Context budget
description: The always-loaded projection stays within its line budget, every reference in it resolves, it states no count that will go stale, and the assembled context fits the builder's own budget.
statement: The always-loaded projection stays within its budget, every reference in it resolves, it states no count that will go stale, and the assembled context fits its own budget.
status: active
class: blocking
depends_on: [majordomus.minimum-sufficient-context@1, majordomus.projection-integrity@1]
tags: [context, budget]

x-majordomus:
  validator: budget
  category: budget
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [context-budget, pointer-integrity, no-counts-in-context]
  tests: [test/cases/02_doctor_basic.sh]
---

# Rationale

The always-loaded projection stays within its line budget, every reference in it resolves, it states no count that will go stale, and the assembled context fits the builder's own budget.

# Required behaviour

The always-loaded projection stays within its budget, every reference in it resolves, it states no count that will go stale, and the assembled context fits its own budget.

# Failure behaviour

A violation is a `FAIL` finding under the category `budget`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_budget` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/02_doctor_basic.sh` proves it, and CI runs that case.
