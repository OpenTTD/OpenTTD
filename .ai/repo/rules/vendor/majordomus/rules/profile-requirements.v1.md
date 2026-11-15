---
id: majordomus.profile-requirements
version: 1
kind: rule
title: Profile requirements
description: A profile may demand more than the shared contract — a regression test, a decision record — and finish refuses without it.
statement: A profile may require more than the shared contract, and finish refuses a completed outcome that lacks it.
status: active
class: blocking
depends_on: [majordomus.justified-escalation@1]
tags: [profiles, finish]

x-majordomus:
  validator: profile_requirements
  category: regression
  enforced_by: [finish]
  exit_code: 10
  claims: [profile-axes, profile-validate]
  tests: [test/cases/16_profiles.sh]
---

# Rationale

A profile may demand more than the shared contract — a regression test, a decision record — and finish refuses without it.

# Required behaviour

A profile may require more than the shared contract, and finish refuses a completed outcome that lacks it.

# Failure behaviour

A violation is a `FAIL` finding under the category `regression`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_profile_requirements` decides it, dispatched from `finish`. The behavioural case `test/cases/16_profiles.sh` proves it, and CI runs that case.
