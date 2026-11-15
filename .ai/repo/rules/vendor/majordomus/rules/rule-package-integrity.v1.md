---
id: majordomus.rule-package-integrity
version: 1
kind: rule
title: Rule package integrity
description: The repository's effective rule set is real: the vendored baseline matches its manifest file for file, every rule resolves with its dependencies and no two claim one identity, and no project rule reuses the vendored namespace.
statement: The vendored baseline matches its manifest file for file, every effective rule resolves with its dependencies, no two rules claim one identity, and no project rule reuses the vendored namespace.
status: active
class: blocking
depends_on: [majordomus.define-done-first@1]
tags: [rules, doctrine]

x-majordomus:
  validator: rule_package
  category: rules
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [doctrine-registry, vendored-rule-package, rule-resolution]
  tests: [test/cases/67_rule_dag.sh]
---

# Rationale

The repository's effective rule set is real: the vendored baseline matches its manifest file for file, every rule resolves with its dependencies and no two claim one identity, and no project rule reuses the vendored namespace.

# Required behaviour

The vendored baseline matches its manifest file for file, every effective rule resolves with its dependencies, no two rules claim one identity, and no project rule reuses the vendored namespace.

# Failure behaviour

A violation is a `FAIL` finding under the category `rules`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_rule_package` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/67_rule_dag.sh` proves it, and CI runs that case.
