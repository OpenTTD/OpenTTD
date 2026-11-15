---
id: majordomus.schema-integrity
version: 1
kind: rule
title: Every object has a schema, and the rules resolve in one stated order
description: The order the rules apply in is written down rather than left in the code; every kind the tool reads declares a schema, every schema is named by a kind, and every path under the repository's AI layer is claimed by a source so that nothing is carried through unvalidated.
statement: The effective rule set resolves in one deterministic order — the vendored baseline in manifest order, then the project's own, as a dependency graph — and every object kind declares a schema, every schema is named by a kind, and every path under .ai/repo/ is claimed by a source mapping.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [schema, rules, order, contract]

x-majordomus:
  validator: schema_integrity
  category: schema
  enforced_by: [doctor, watch]
  exit_code: 10
  tests: [test/cases/32_schema_integrity.sh]
---

# Rationale

Two things the Majordomus repository, where this rule was written, relied on were true only
because the code happened to do them, and neither was written anywhere a person could read.

The first is the order the rules apply in. A rule set where the order is an accident of
loading is a rule set nobody can reason about: which of two rules wins is answerable only by
running it, and a change to the loader silently changes what the repository means. The order
is a contract, so it belongs in a rule and not only in `lib/rules.sh`.

The second is that a schema is optional. A kind may be declared with no `schema:`, and its
metadata is then carried through unvalidated — quietly, with no finding, looking exactly like a
kind that validates. Every unschema'd kind is a place where a typo in front matter becomes
data. The same hole exists in the other direction: a schema file that no kind names describes
nothing, is never applied, and drifts away from the shape it was written for without anything
noticing. The Majordomus repository had four such files when this rule was written, which is
how the hole was found.

And a path under the AI layer that no source claims is read by nothing and validated by
nothing, while looking from the outside like part of the layer.

# Required behaviour

**The order.** The effective set is the vendored baseline in the order its manifest lists,
followed by the repository's own rules, resolved as a dependency graph: a rule comes after
every rule it declares in `depends_on`. The resolution is deterministic — the same inputs
give the same order on every run — and it is total: a missing dependency, a cycle, or two
rules claiming one identity is an error, and nothing is applied partially. A project rule may
not reuse the vendored namespace, so the baseline can be replaced without silently dropping
an override nobody declared.

**The schemas.** Every kind the tool reads declares a schema, except a kind whose format
carries no metadata at all — a plain text file has nothing to validate, and that exemption is
the format's, not the author's. Every schema file under the distribution is named by some
kind: a schema nothing applies is deleted or wired, never left. Every schema parses as JSON
Schema draft 2020-12.

A schema that describes state under the ignored half of the layer is not a kind, because
nothing indexes that half; it is applied by the command that writes the file, and the rule
holds it to being applied by something rather than to being a kind.

**The coverage.** Every path under `.ai/repo/` is claimed by a source mapping, so that a file
in the layer is either an object of a declared kind or a deliberate exclusion, and never
merely unnoticed.

# Failure behaviour

A violation is a `FAIL` finding under the category `schema`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_schema_integrity` decides all three, dispatched from `doctor, watch`. The
behavioural case `test/cases/32_schema_integrity.sh` proves them by removing a kind's schema,
adding a schema no kind names, and adding a path no source claims, and CI runs that case.
