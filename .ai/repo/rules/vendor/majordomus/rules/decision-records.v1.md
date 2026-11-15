---
id: majordomus.decision-records
version: 1
kind: rule
title: Decision records
description: Every entry in decisions.md carries the task, the head and the reason, so a decision can be found by the worker who needs it.
statement: Every recorded decision carries the task, the head and the reason, so the worker who needs it can find it.
status: active
class: advisory
depends_on: [majordomus.externalise-decisions@1]
tags: [records, decisions]

x-majordomus:
  validator: decisions
  category: records
  enforced_by: [check, doctor, watch]
  exit_code: 0
  claims: [decision-attribution]
  tests: [test/cases/21_decision_question.sh]
---

# Rationale

Every entry in decisions.md carries the task, the head and the reason, so a decision can be found by the worker who needs it.

# Required behaviour

Every recorded decision carries the task, the head and the reason, so the worker who needs it can find it.

# Failure behaviour

A violation is a `WARN` finding under the category `records`; the command continues and exits as it otherwise would. Under `watch` it is reported as drift and the command exits 11.

# Verification

`mj_validate_decisions` decides it, dispatched from `check, doctor, watch`. The behavioural case `test/cases/21_decision_question.sh` proves it, and CI runs that case.
