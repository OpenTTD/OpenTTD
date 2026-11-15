---
id: majordomus.scope-integrity
version: 1
kind: rule
title: Scope integrity
description: A task touches only the paths it claimed; work found elsewhere is not accepted as done.
statement: Work outside the paths a task claimed is not accepted as that task's work.
status: active
class: blocking
depends_on: [majordomus.one-worker-one-scope@1]
tags: [scope, verification]

x-majordomus:
  validator: scope
  category: scope
  enforced_by: [check, finish, watch]
  policy_key: scope_respected
  exit_code: 10
  claims: [scope-enforcement, scoped-task]
  tests: [test/cases/04_start_check.sh]
---

# Rationale

A task touches only the paths it claimed; work found elsewhere is not accepted as done.

# Required behaviour

Work outside the paths a task claimed is not accepted as that task's work.

# Failure behaviour

A violation is a `FAIL` finding under the category `scope`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_scope` decides it, dispatched from `check, finish, watch`. The behavioural case `test/cases/04_start_check.sh` proves it, and CI runs that case.
