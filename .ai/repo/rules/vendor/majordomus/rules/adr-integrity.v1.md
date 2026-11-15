---
id: majordomus.adr-integrity
version: 1
kind: rule
title: Architecture decision integrity
description: Every architecture decision the repository holds parses against the decision contract, claims an identity nothing else claims, and every relation and reference it makes resolves.
statement: A decision is canonical data under the adrs section; it is valid against the decision schema, its identity is unique and fixes its file name, supersession is reciprocal, every reference it makes resolves in both directions of the graph, and a record the tool derived is proposed rather than accepted.
status: active
class: blocking
depends_on: [majordomus.externalise-decisions@1]
tags: [adrs, records]

x-majordomus:
  validator: adr
  category: adr
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [adr-catalogue, adr-propose, adr-traceability]
  tests: [test/cases/99_adr.sh]
---

# Rationale

A decision record is read months after it was written, by someone deciding whether a constraint still holds. Two records claiming one identity make every reference to that identity ambiguous, and nothing notices until a reader follows one. This is not hypothetical: in the Majordomus repository, where this rule was written, two worktrees allocated `0005` within three hours of each other and two allocated `0007` within one, and the collision survived a merge because no command had an opinion about the section. Validating the set on every `doctor` run puts the failure in the tree, with the files named, instead of in the head of whoever reads the second record.

The second half of the rule is about what a machine may assert. `majordomus adr propose` writes a record from a local decision, and a record it wrote may be `proposed` or `rejected` and nothing else. A tool that can write `accepted` can turn its own inference into repository truth by the act of writing it down, and the reader who later finds that record has no way to tell which happened.

# Required behaviour

Every file the source class `adr` discovers has front matter that satisfies `share/schemas/majordomus/adr/adr.v1.schema.json` (no unknown key, `schema: adr/v1`, `kind: adr`, a `status` from the closed set, a `date` as `YYYY-MM-DD`), an `id` of the form `adr-NNNN` whose number is the file name's prefix, and a body with non-empty `## Context`, `## Decision` and `## Consequences` sections. No two records claim one identity and no two file names claim one number. `superseded_by` is present exactly when the status is `superseded`, and every `supersedes` target both exists and names the superseding record back. A record whose `provenance.origin` is `extracted` names at least one `derived_from` reference and carries the status `proposed` or `rejected`. Every reference is `<type>:<value>` with a known type, and a `file:` or `test:` reference resolves to a path in the repository.

# Failure behaviour

A violation is a `FAIL` finding under the category `adr`, naming the file and every reason, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11. A repository with no decisions is reported as such, never as a pass over nothing.

# Verification

`mj_validate_adr` decides it, dispatched from `doctor, watch`; `majordomus adr check` runs the same examination on demand and prints its counts. The behavioural case `test/cases/99_adr.sh` proves it, and CI runs that case.
