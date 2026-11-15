---
id: majordomus.knowledge-graph
version: 1
kind: rule
title: Knowledge graph integrity
description: The graph the knowledge compiler builds from what the repository states has one node per identity, no edge with a missing end or without provenance, and no declared path or tracked source that no longer exists; a broken owned reference is a failure, a broken link an author wrote in prose is not.
statement: No two sources claim one node id; every edge Majordomus derives from a structured field names an existing node at both ends and the line it was observed on; every declared path and every tracked source exists in the working tree. A link written in prose that resolves to nothing is reported and never fails.
status: active
class: advisory
depends_on: []
tags: [knowledge, integrity]

x-majordomus:
  validator: knowledge_graph
  category: knowledge
  enforced_by: [doctor, watch]
  exit_code: 10
  tests: [test/cases/3075_knowledge_validate_judges_the_graph.sh, test/cases/74_knowledge_edges.sh]
---

# Rationale

The knowledge compiler turns what the repository already states into nodes and edges: an
issue's milestone, a claim's specification, implementation and case, a rule's dependency, a
link in a document. Every one of those statements can break without anyone touching it — a
file is deleted, an issue renumbered, a second file claims an identity — and a graph that
breaks in silence answers a lookup with an absence that reads as a fact.

Validation that exists twice diverges, and the second copy is the one that stops being run.
So there is one judge. The compiler emits every finding while it extracts; `majordomus
knowledge validate` classifies them, and doctor and watch dispatch this rule, which asks
the same compile and the same classification.

# Required behaviour

No two sources claim one node id. Every edge names both ends and the file, and where it has
one the field or line, it was observed in. Every edge Majordomus derives from a structured
field resolves to a node, every path a structured field declares exists in the repository,
and every source git tracks exists in the working tree.

Which break is owned is decided by where the relationship was stated. A relationship
Majordomus defines from a structured field — `milestone`, `depends_on`, a claim's `source`,
`implementation` and `test`, a rule's dependency — is owned, and a broken one is a failure.
A link an author wrote in prose, and the `references` edge taken from it, is the author's:
it may point deliberately outside the repository, so a target that resolves to nothing is
reported and never fails. A ledger or git target named by a knowledge record is outside the
graph and is judged by `majordomus.knowledge-integrity`.

No generated index is stored: every read compiles the checkout from the version-control
index, so there is no index to go stale and no stored hash to mismatch, and the command says
so on every run. Nothing here rebuilds or repairs anything.

# Failure behaviour

`majordomus knowledge validate` prints one finding per break with its code, whether it is
owned, a tracked source, an author's link or a gap in the model, and the reproduce
`majordomus knowledge validate`; it exits 10 when an owned reference is broken and writes
nothing. Under doctor the rule is advisory, so an owned break is a `WARN knowledge` finding,
and doctor also reports whether the graph can be built in this checkout at all, compiling it
into a temporary file it removes. Under watch every owned break and every missing tracked
source is `DRIFT`. Links in prose are summarised in one `INFO` line under both.

# Verification

`mj_validate_knowledge_graph` decides it, dispatched from `doctor, watch`.
`test/cases/3075_knowledge_validate_judges_the_graph.sh` introduces each fault on its own —
a duplicate id, a dangling dependency, a declared path that does not exist, a tracked source
deleted from the working tree, an edge without provenance, a broken link in prose — and
proves the command's verdict, the doctor and watch levels, that doctor names the graph
buildable, and that nothing is written. `test/cases/74_knowledge_edges.sh` proves the
compiler's own refusals.
