---
id: majordomus.layout-integrity
version: 1
kind: rule
title: Layout integrity
description: The directories the durable commands write into are installed rather than created on first use.
statement: The directories the durable commands write into are installed rather than created on first use.
status: active
class: advisory
depends_on: [majordomus.sessions-are-workers@1]
tags: [layout]

x-majordomus:
  validator: layout
  category: layout
  enforced_by: [doctor]
  exit_code: 0
  claims: [retention-caps]
  tests: [test/cases/25_continuity_lifecycle.sh]
---

# Rationale

The directories the durable commands write into are installed rather than created on first use.

# Required behaviour

The directories the durable commands write into are installed rather than created on first use.

# Failure behaviour

A violation is a `WARN` finding under the category `layout`; the command continues and exits as it otherwise would. Under `watch` it is reported as drift and the command exits 11.

# Verification

`mj_validate_layout` decides it, dispatched from `doctor`. The behavioural case `test/cases/25_continuity_lifecycle.sh` proves it, and CI runs that case.
