---
id: majordomus.enforcement-wiring
version: 1
kind: rule
title: Enforcement wiring
description: Every enforcement the policy declares is invoked by the hook it names, from a file the dispatcher will actually execute, without swallowing the exit code.
statement: Every enforcement the policy declares is invoked by the hook it names, from a file the dispatcher executes, without a swallowed exit code.
status: active
class: blocking
depends_on: [majordomus.define-done-first@1]
tags: [wiring]

x-majordomus:
  validator: wiring
  category: wiring
  enforced_by: [doctor]
  exit_code: 10
  claims: [wiring-reconciliation, dispatcher-wiring]
  tests: [test/cases/14_wiring_dispatcher.sh]
---

# Rationale

Every enforcement the policy declares is invoked by the hook it names, from a file the dispatcher will actually execute, without swallowing the exit code.

# Required behaviour

Every enforcement the policy declares is invoked by the hook it names, from a file the dispatcher executes, without a swallowed exit code.

# Failure behaviour

A violation is a `FAIL` finding under the category `wiring`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_wiring` decides it, dispatched from `doctor`. The behavioural case `test/cases/14_wiring_dispatcher.sh` proves it, and CI runs that case.
