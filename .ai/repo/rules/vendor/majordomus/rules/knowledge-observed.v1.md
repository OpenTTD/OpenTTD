---
id: majordomus.knowledge-observed
version: 1
kind: rule
title: Knowledge observation
description: While episodes keep closing, the knowledge they produced must keep being derived. A closed episode no derivation followed, once it is older than the freshness threshold, is a stopped writer; a lifecycle whose close path never calls the deriver while the policy switch is on is a writer that was never started.
statement: While episodes keep closing, every closed episode must be followed by a knowledge derivation. The newest session.closed line must name an episode a knowledge.derived line names, or be younger than session.freshness.stale_minutes; and while session.knowledge_on_end is on, the lifecycle source that closes an episode must call the deriver.
status: active
class: blocking
depends_on: [majordomus.lifecycle-observed@1]
tags: [knowledge, sessions, continuity, health]

x-majordomus:
  validator: knowledge_lifecycle
  category: knowledge
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [knowledge-writer-observed, knowledge-derived-at-the-boundary, knowledge-failure-is-recorded]
  tests: [test/cases/563_knowledge_is_derived_at_the_boundary.sh, test/cases/565_knowledge_switch_and_failure_leave_evidence.sh, test/cases/708_health_sees_a_stopped_knowledge_writer.sh]
---

# Rationale

An episode records decisions, resolves questions and finishes tasks, and each of those is a
typed line in the ledger of one machine. The ledger is retention-capped and local; the
decision store is local; the note a blocked task leaves is local. Every one of them is
evidence of something true about the repository, and every one of them is unreachable from
the tracked tree, from every other checkout and from every surface the registry feeds. The
knowledge kind exists to hold exactly that, and until this rule nothing wrote to it at the
moment the evidence was about to stop being reachable.

A writer that exists is not a writer that runs. `majordomus.lifecycle-observed` was written
after a week in which every health check passed while no checkpoint and no handover were
written, because reachability was the question every check asked and running was the
question none did. The same shape applies here with one more way to fail: a derivation is a
step inside the close, and a close path that skips it, or a policy switch nobody noticed was
off, produces closed episodes and no knowledge, forever, with every record well-formed.

The judgement is made from two facts no single check owned: that episodes are closing, and
that derivations are following them. A `session.closed` line whose episode no
`knowledge.derived` line names is the shape of this outage in the ledger, and it is
distinguishable from a repository that is merely idle only once the close is older than the
freshness threshold the handover is already judged by.

# Required behaviour

While episodes keep closing, every closed episode must be followed by a knowledge
derivation. The newest `session.closed` line must name an episode a `knowledge.derived` line
names, or be younger than `session.freshness.stale_minutes`; and while
`session.knowledge_on_end` is on, the lifecycle source that closes an episode must call the
deriver, which the validator reads from the source the way the doctrine wiring is read.

The threshold is `session.freshness.stale_minutes` in the policy and is read, never restated.
A checkout in whose ledger no `knowledge.derived` line exists at all is not judged and the
validator says so as a skip: on the day the deriver arrives every checkout has closed
episodes and none has derived, and a finding that is red everywhere at once is a finding
nobody reads. The first derivation on a checkout ends the skip. A repository with no
`session.closed` line has no episodes to judge and passes. `session.knowledge_on_end: false`
is a deliberate choice and is reported as a skip, not as a failure.

The validator names the episode; it does not derive. The remedy follows `fix:` in the
finding and is a command the person runs: `majordomus knowledge derive --episode <id>`.

# Failure behaviour

A violation is a `FAIL` finding under the category `knowledge`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11. The
finding's reproduce is read-only: `majordomus history --event session.closed` and
`majordomus history --event knowledge.derived` show the two facts the judgement compared.

# Verification

`mj_validate_knowledge_lifecycle` decides it, dispatched from `doctor, watch`. The
behavioural cases `test/cases/563_knowledge_is_derived_at_the_boundary.sh`,
`test/cases/565_knowledge_switch_and_failure_leave_evidence.sh` and
`test/cases/708_health_sees_a_stopped_knowledge_writer.sh` prove it — the first that the
provider's end and compaction events derive whether or not a task is active, the second
that a switch that is off and a derivation that fails both leave evidence and never block
the provider, the third that health goes red when the writer stops and clears when a
derivation is written, and that a lifecycle source with the call removed is named — and CI
runs all three.
