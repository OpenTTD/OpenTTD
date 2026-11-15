---
id: majordomus.policy-completeness
version: 1
kind: rule
title: Policy completeness
description: Every policy value the code reads is declared in the skeleton policy, and no reader carries its own default for one.
statement: Every policy value the code reads is declared in the skeleton policy, and no reader carries a default of its own.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [policy]

x-majordomus:
  validator: policy_defaults
  category: policy
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [policy-parse]
  tests: [test/cases/28_no_hardcoded_values.sh]
---

# Rationale

Every policy value the code reads is declared in the skeleton policy, and no reader carries its own default for one.

# Required behaviour

Every policy value the code reads is declared in the skeleton policy, and no reader carries a default of its own.

# Failure behaviour

A violation is a `FAIL` finding under the category `policy`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_policy_defaults` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/28_no_hardcoded_values.sh` proves it, and CI runs that case.
