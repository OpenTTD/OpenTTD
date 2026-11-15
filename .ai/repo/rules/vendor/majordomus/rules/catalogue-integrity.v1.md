---
id: majordomus.catalogue-integrity
version: 1
kind: rule
title: Catalogue integrity
description: Every use case and application describes the tool in terms the tool has — each command, doctrine and claim it names exists, and the two catalogues reference each other in both directions.
statement: Every use case and application names only commands, rules and claims that exist, and the two catalogues reference each other in both directions.
status: active
class: blocking
depends_on: [majordomus.minimum-sufficient-context@1]
tags: [catalogue]

x-majordomus:
  validator: catalogue
  category: catalogue
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [catalogue-resolves]
  tests: [test/cases/28_catalogue.sh]
---

# Rationale

Every use case and application describes the tool in terms the tool has — each command, doctrine and claim it names exists, and the two catalogues reference each other in both directions.

# Required behaviour

Every use case and application names only commands, rules and claims that exist, and the two catalogues reference each other in both directions.

# Failure behaviour

A violation is a `FAIL` finding under the category `catalogue`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_catalogue` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/28_catalogue.sh` proves it, and CI runs that case.
