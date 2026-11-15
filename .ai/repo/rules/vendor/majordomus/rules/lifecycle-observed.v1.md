---
id: majordomus.lifecycle-observed
version: 1
kind: rule
title: Lifecycle observation
description: A store that can be read is not a store that is being written. While episodes go on opening, the records a future worker resumes from must go on advancing, every lifecycle event received must leave evidence it arrived, and an episode that outlives the staleness threshold must be named rather than left open.
statement: While episodes keep opening, the newest checkpoint and the newest handover for this worktree and branch must be within the policy's freshness thresholds; every provider lifecycle event received must be recorded before any guard runs; and an open episode older than the stale threshold is a finding.
status: active
class: blocking
depends_on: [majordomus.handover-integrity@1]
tags: [sessions, continuity, health]

x-majordomus:
  validator: lifecycle
  category: lifecycle
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [session-lifecycle]
  tests: [test/cases/130_lifecycle_survives_a_stale_task.sh, test/cases/132_health_sees_a_stopped_writer.sh]
---

# Rationale

Between 2026-09-05 and 2026-09-11 the Majordomus repository, where this rule was written, wrote
no checkpoint and no handover. Every health check passed throughout. They passed honestly:
`retention` counted the files and found them under their caps, `layout` found the directories
present, and `resolver` found a record and reported its divergence label as `advanced`. A store
that nothing writes to is perfectly reachable, perfectly well-formed and perfectly under its
retention cap.

Reachability is the wrong question. The question is whether the thing is still running, and
it can only be answered by comparing two facts that no single check owned: that episodes were
opening — eighteen of them that week — and that the records those episodes are supposed to
produce had not moved since the first day. Either fact alone is unremarkable. A repository
where nothing is happening is not failing, and a repository with an old handover and no
activity is simply idle. Together they are a writer that has stopped, and nothing was looking
at both.

The same reasoning applies to the events themselves. An adapter that receives an event and
declines to act on it writes a line to stderr, which nobody keeps, and returns 0, which is
what a provider hook must do. Afterwards, an event that never fired and an event that fired
and did nothing are the same observation. A receipt written before any guard runs is what
makes them different, and a receipt with no resulting record and no failure beside it is the
shape of this outage in the ledger.

# Required behaviour

While episodes keep opening, the newest checkpoint and the newest handover for this worktree
and branch must be within the policy's freshness thresholds; every provider lifecycle event
received must be recorded before any guard runs; and an open episode older than the stale
threshold is a finding.

The thresholds are `session.freshness` in the policy and are read, never restated. A
repository with fewer than two recorded episodes is not judged: there is not yet enough
history to distinguish a stopped writer from a new checkout.

The validator names stranded episodes; it does not close them. A diagnostic that silently
mutates state is the watchdog this design refuses, and recovery is a decision somebody makes.

# Failure behaviour

A violation is a `FAIL` finding under the category `lifecycle`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_lifecycle` decides it, dispatched from `doctor, watch`. The behavioural cases
`test/cases/130_lifecycle_survives_a_stale_task.sh` and
`test/cases/132_health_sees_a_stopped_writer.sh` prove it — the first that the lifecycle now
runs against the exact state that stopped it, the second that health goes red when the writer
stops — and CI runs both.
