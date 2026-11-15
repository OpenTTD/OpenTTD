---
id: majordomus.command-coverage
version: 1
kind: rule
title: Every public command is exercised and refuted
description: Every public command has a behavioural test and a negative test, computed from the registry and the coverage each case declares about itself rather than from a list someone maintains.
statement: Coverage is owed to every public command from the moment it is registered; a case declares which commands it exercises and which failure modes it asserts, and a public command that no case declares in both layers is a failure.
status: active
class: blocking
depends_on: [majordomus.verify-outcomes@1]
tags: [command, tests]

x-majordomus:
  validator: command_coverage
  category: command
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [command-coverage]
  tests: [test/cases/31_command_coverage.sh]
---

# Rationale

Test coverage that is remembered goes stale the day a command is added. Coverage that is
computed from the surface cannot: a command added to the registry is owed a behavioural
case and a negative case from that moment, and the maintainer who adds one without them is
told which layer is missing rather than discovering it later.

# Required behaviour

A case declares the commands it exercises with `# majordomus-covers:` and the commands
whose failure modes it asserts with `# majordomus-negative:`. Coverage is declared rather
than inferred, because assertions that pipe stdin or loop over a variable are invisible to
any scan of the source. Every public command must appear in at least one case of each
layer, and a header naming a command that is not public is a broken reference.

# Failure behaviour

A violation is a `FAIL` finding under the category `command`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11.
This rule applies only in the repository that carries the suite; an installation without
one reports the rule as skipped.

# Verification

`mj_validate_command_coverage` decides it, dispatched from `doctor, watch`. The behavioural
case `test/cases/31_command_coverage.sh` proves it, and CI runs that case.
