---
id: majordomus.task-continuity
version: 1
kind: rule
title: Task continuity
description: A task finished as partial or blocked should leave a handover record, not only a note section.
statement: A task finished as partial or blocked leaves a handover record, not only a note section.
status: active
class: advisory
depends_on: [majordomus.handovers-carry-state@1]
tags: [continuity, finish]

x-majordomus:
  validator: continuity
  category: continuity
  enforced_by: [finish]
  exit_code: 0
  claims: [handover-record]
  tests: [test/cases/25_continuity_lifecycle.sh]
---

# Rationale

A task finished as partial or blocked should leave a handover record, not only a note section.

# Required behaviour

A task finished as partial or blocked leaves a handover record, not only a note section.

# Failure behaviour

A violation is a `WARN` finding under the category `continuity`; the command continues and exits as it otherwise would. Under `watch` it is reported as drift and the command exits 11.

# Verification

`mj_validate_continuity` decides it, dispatched from `finish`. The behavioural case `test/cases/25_continuity_lifecycle.sh` proves it, and CI runs that case.
