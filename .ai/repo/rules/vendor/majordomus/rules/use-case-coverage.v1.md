---
id: majordomus.use-case-coverage
version: 1
kind: rule
title: Use-case coverage
description: Every public command, and every guaranteed claim and MCP tool the policy asks for, is named and run by at least one active use case whose scenario executes against the real tool; a gap is a failure or a report according to the policy, never silence.
statement: A capability the policy requires coverage for is demonstrated by an executable use case, and a capability without one is a named gap.
status: active
class: blocking
depends_on: [majordomus.catalogue-integrity@1]
tags: [use-cases, evidence]

x-majordomus:
  validator: use_case_coverage
  category: use-case
  enforced_by: [doctor, check, finish]
  policy_key: use_cases_covered
  exit_code: 10
  claims: [use-case-coverage]
  tests: [test/cases/94_use_cases.sh]
---

# Rationale

A capability nobody has written a use case for is a capability nobody has shown a person
performing, and a use case with no scenario is prose. Coverage computed from the registry
and the use cases, with the policy saying which classes must be covered, turns "we should
document this" into a line `doctor` prints and `finish` refuses on.

# Required behaviour

For every public command of the command registry, every guaranteed claim with a
responsibility, and every MCP tool the executable projects, `majordomus usecase coverage`
counts the active use cases that name it and the ones whose scenario runs it. The policy's
`use_cases.coverage` says, per class, whether a gap is `required` (a failure), `advisory`
(reported) or `off`. A draft use case never counts.

# Failure behaviour

A violation is a `FAIL` finding under the category `use-case` naming the capability, the
counts and the scaffold command, and the command that found it exits 10; under `finish`
the policy key `use_cases_covered` refuses completion.

# Verification

`mj_validate_use_case_coverage` decides it, dispatched from `doctor, check, finish`. The
behavioural case `test/cases/94_use_cases.sh` proves it, and CI runs that case.
