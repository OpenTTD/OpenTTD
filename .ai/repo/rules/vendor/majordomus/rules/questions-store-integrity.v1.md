---
id: majordomus.questions-store-integrity
version: 1
kind: rule
title: Questions store integrity
description: Every entry in open-questions.md parses, because a gate that cannot read an entry can be bypassed by mistyping one.
statement: Every entry in the questions store parses, because a gate that cannot read an entry can be bypassed by mistyping one.
status: active
class: blocking
depends_on: [majordomus.externalise-decisions@1]
tags: [records, questions]

x-majordomus:
  validator: questions_store
  category: records
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [blocker-store]
  tests: [test/cases/21_decision_question.sh]
---

# Rationale

Every entry in open-questions.md parses, because a gate that cannot read an entry can be bypassed by mistyping one.

# Required behaviour

Every entry in the questions store parses, because a gate that cannot read an entry can be bypassed by mistyping one.

# Failure behaviour

A violation is a `FAIL` finding under the category `records`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_questions_store` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/21_decision_question.sh` proves it, and CI runs that case.
