---
id: majordomus.doctrine-wiring-integrity
version: 1
kind: rule
title: Doctrine wiring integrity
description: Every doctrine in this registry resolves to a validator that exists, is reached from every command it names, propagates failure, is proved by a test, and is run by CI — and every validator in the source is declared here.
statement: Every enforced rule resolves to a validator that exists, is reached from every command it names, propagates failure, is proved by a test that CI runs; and every validator is declared by a rule.
status: active
class: blocking
depends_on: [majordomus.define-done-first@1]
tags: [doctrine, wiring]

x-majordomus:
  validator: doctrine_wiring
  category: doctrine
  enforced_by: [doctor]
  exit_code: 10
  claims: [wiring-reconciliation, doctrine-registry, doctrine-class-decides]
  tests: [test/cases/18_doctrine_wiring.sh]
---

# Rationale

Every doctrine in this registry resolves to a validator that exists, is reached from every command it names, propagates failure, is proved by a test, and is run by CI — and every validator in the source is declared here.

# Required behaviour

Every enforced rule resolves to a validator that exists, is reached from every command it names, propagates failure, is proved by a test that CI runs; and every validator is declared by a rule.

# Failure behaviour

A violation is a `FAIL` finding under the category `doctrine`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_doctrine_wiring` decides it, dispatched from `doctor`. The behavioural case `test/cases/18_doctrine_wiring.sh` proves it, and CI runs that case.
