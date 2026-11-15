---
id: majordomus.retention-caps
version: 1
kind: rule
title: Retention caps
description: The ledger, the handovers, the checkpoints and the session records stay under the caps the policy sets, so durable state does not grow without bound.
statement: The ledger, the handovers, the checkpoints and the session records stay under the caps the policy sets, and a cap is met by moving records aside, never by deleting one.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [retention]

x-majordomus:
  validator: retention
  category: retention
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [retention-caps]
  tests: [test/cases/02_doctor_basic.sh, test/cases/2985_session_records_are_capped_and_archived.sh]
---

# Rationale

The ledger, the handovers, the checkpoints and the session records stay under the caps the policy sets, so durable state does not grow without bound.

# Required behaviour

The ledger, the handovers, the checkpoints and the session records stay under the caps the policy sets, and a cap is met by moving records aside, never by deleting one.

# Failure behaviour

A violation is a `FAIL` finding under the category `retention`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_retention` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/02_doctor_basic.sh` proves it, and CI runs that case.
