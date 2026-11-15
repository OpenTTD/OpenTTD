---
id: majordomus.policy-integrity
version: 1
kind: rule
title: Policy integrity
description: The policy and every profile parse, declare version 1, and carry no key the schema does not define.
statement: The policy and every profile parse, declare a supported version, and carry no key the schema does not define.
status: active
class: blocking
depends_on: [majordomus.minimum-sufficient-context@1]
tags: [policy, profiles]

x-majordomus:
  validator: policy
  category: policy
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [policy-parse, profile-validate]
  tests: [test/cases/02_doctor_basic.sh]
---

# Rationale

The policy and every profile parse, declare version 1, and carry no key the schema does not define.

# Required behaviour

The policy and every profile parse, declare a supported version, and carry no key the schema does not define.

# Failure behaviour

A violation is a `FAIL` finding under the category `policy`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_policy` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/02_doctor_basic.sh` proves it, and CI runs that case.
