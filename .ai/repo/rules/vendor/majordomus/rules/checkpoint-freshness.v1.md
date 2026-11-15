---
id: majordomus.checkpoint-freshness
version: 1
kind: rule
title: Checkpoint freshness
description: A task whose last checkpoint is older than its profile's interval is reported, not stopped.
statement: A task whose newest checkpoint is older than its profile's interval is reported as stale, and the report does not stop the work.
status: active
class: advisory
depends_on: [majordomus.sessions-are-workers@1]
tags: [checkpoint, continuity]

x-majordomus:
  validator: checkpoint
  category: checkpoint
  enforced_by: [check, watch]
  exit_code: 0
  claims: [checkpoint-interval]
  tests: [test/cases/04_start_check.sh]
---

# Rationale

A task whose last checkpoint is older than its profile's interval is reported, not stopped.

# Required behaviour

A task whose newest checkpoint is older than its profile's interval is reported as stale, and the report does not stop the work.

# Failure behaviour

A violation is a `WARN` finding under the category `checkpoint`; the command continues and exits as it otherwise would. Under `watch` it is reported as drift and the command exits 11.

# Verification

`mj_validate_checkpoint` decides it, dispatched from `check, watch`. The behavioural case `test/cases/04_start_check.sh` proves it, and CI runs that case.
