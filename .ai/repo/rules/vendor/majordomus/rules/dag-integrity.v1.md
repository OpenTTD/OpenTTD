---
id: majordomus.dag-integrity
version: 1
kind: rule
title: Dependency graph integrity
description: The issue dependency graph is acyclic, every edge names an issue that exists, and no issue is executing ahead of a dependency that is not done.
statement: The issue dependency graph is acyclic, every edge names an issue that exists, and no issue executes ahead of a dependency that is not done.
status: active
class: blocking
depends_on: [majordomus.one-worker-one-scope@1]
tags: [plan, dag]

x-majordomus:
  validator: dag
  category: dag
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [dag-validation, execution-waves]
  tests: [test/cases/44_model_doctrine.sh]
---

# Rationale

The issue dependency graph is acyclic, every edge names an issue that exists, and no issue is executing ahead of a dependency that is not done.

# Required behaviour

The issue dependency graph is acyclic, every edge names an issue that exists, and no issue executes ahead of a dependency that is not done.

# Failure behaviour

A violation is a `FAIL` finding under the category `dag`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_dag` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/44_model_doctrine.sh` proves it, and CI runs that case.
