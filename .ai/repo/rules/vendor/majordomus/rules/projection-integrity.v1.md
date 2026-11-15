---
id: majordomus.projection-integrity
version: 1
kind: rule
title: Projection integrity
description: Every generated instruction file exists, matches the stamp it carries, and is never silently overwritten after a hand edit.
statement: Every generated instruction file exists, matches the content it declares, and is never silently overwritten after a hand edit.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [projection]

x-majordomus:
  validator: projection
  category: projection
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [projection-fingerprint, no-silent-overwrite, region-projection, projection-generation]
  tests: [test/cases/03_update.sh]
---

# Rationale

Every generated instruction file exists, matches the stamp it carries, and is never silently overwritten after a hand edit.

# Required behaviour

Every generated instruction file exists, matches the content it declares, and is never silently overwritten after a hand edit.

# Failure behaviour

A violation is a `FAIL` finding under the category `projection`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_projection` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/03_update.sh` proves it, and CI runs that case.
