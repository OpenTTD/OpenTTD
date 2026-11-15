---
id: majordomus.project-integrity
version: 1
kind: rule
title: Project model integrity
description: Every milestone and issue file parses, carries the id its filename claims, and contains no key nobody reads.
statement: Every milestone and issue file parses, carries the id its filename claims, and contains no key nobody reads.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [plan]

x-majordomus:
  validator: project
  category: project
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [project-schema, project-status-derived]
  tests: [test/cases/44_model_doctrine.sh]
---

# Rationale

Every milestone and issue file parses, carries the id its filename claims, and contains no key nobody reads.

# Required behaviour

Every milestone and issue file parses, carries the id its filename claims, and contains no key nobody reads.

# Failure behaviour

A violation is a `FAIL` finding under the category `project`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_project` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/44_model_doctrine.sh` proves it, and CI runs that case.
