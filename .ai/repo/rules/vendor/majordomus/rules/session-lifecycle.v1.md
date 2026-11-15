---
id: majordomus.session-lifecycle
version: 1
kind: rule
title: The episode boundary is drawn below the model, and its working context is local
description: Where a provider fires session events, the episode is opened and closed by that provider's hook rather than by the model, the start event hands the worker the bounded briefing the policy declares, and the working context each open freezes stays under the ignored half of the layer, carries the declared keys, and never carries a conversation.
statement: An episode boundary a provider can observe is drawn by that provider's hook, proven by driving a payload through it; the start event hands the worker a briefing bounded by the policy and the other events write nothing to standard output; every open episode has a working context, and the store of working contexts is ignored, untracked, contract-shaped and free of transcripts.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1, majordomus.session-records@1]
tags: [session, provider, evidence]

x-majordomus:
  validator: session_lifecycle
  category: session
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [session-lifecycle]
  tests: [test/cases/54_session_lifecycle_hooks.sh]
---

# Rationale

The same argument that puts prompt capture below the model puts the episode boundary there.
A worker asked to open its own session opens one when it remembers to, which is never the
episode that mattered — the one that ended in a crash, a compaction, or somebody closing the
window. An instruction in a bootstrap file is a request, and a request that is honoured
sometimes produces a record of sessions that went well and silence about the rest.

The provider knows. It fires an event when a sitting begins and another when it ends, and it
fires them whether or not a model is in a position to notice. So the boundary is drawn there
or it is fiction, and this rule is what keeps a repository from presenting the second as the
first.

Freezing the context at the open follows from the boundary existing. `.ai/local/session-contexts/`
was named by the layer from the beginning and had no producer: a directory the skeleton
creates and nothing fills is a promise the layer does not keep, and a reader cannot tell an
empty store from an unimplemented one. What belongs in it is the one fact neither neighbour
holds — the closed record says what the episode produced, the prompt archive holds the
person's half of the exchange, and neither says what the worker was actually told when it
began.

That document stays local for two reasons that are not the same one. It names this machine,
and a fact about a disk is not a fact about the repository. And it is a snapshot of a
projection: re-resolving it later gives a different document, so publishing it would publish
something no surface can reproduce.

# Required behaviour

A provider with lifecycle events has an adapter; one without is reported as unsupported and
never assumed to be silent. Where a repository declares the wiring — an `enforcement` entry
with `wired_by: provider-hook:<provider>:session` — the configuration names the shims this
tool wrote, both are executable, and a synthetic payload driven through the end shim reaches
the command. The dry run is what makes that safe to prove: everything on the path runs
except the mutation, because closing somebody's open episode is not a price a diagnostic may
charge.

Every event is idempotent, because the events themselves are: a start fires again on a
resume and keeps the open episode rather than opening a second, an end with nothing open
writes nothing and is not a failure, and a compaction with no active task records nothing.

Only the start event writes to standard output, and only the briefing the policy declares.
The provider adds that output to the context it is about to build, which is the one moment
at which a continuation record can reach a worker without the worker remembering to ask for
it — and a continuation record nothing loads is a record nobody reads. What it may carry is
bounded by `session.briefing_budget_lines` and limited to references, divergence labels, the
blockers that refuse acceptance, and the one section of a handover a resuming worker acts
on. It may never carry a conversation, and `session.briefing_on_start: false` restores the
older silence for a repository that wants it. The end and compaction events write nothing to
standard output: each fires inside a turn that is already under way, where output would
alter what somebody is doing rather than furnish it.

This is the one exception to the layer's rule that nothing under `local/` is loaded into a
context implicitly, and it is narrow on purpose. That rule protects three things — no
transcripts, no unbounded growth, and no fact about a disk becoming a fact about the
repository — and a bounded, declared, transcript-free briefing at the episode boundary
defeats none of them. What stays unconditional is the other half: nothing under `local/` is
published by a generator or served on a public surface, ever.

`session start` writes the working context of the episode and `session close` appends what
the close knows to the same document; nothing else writes one, and the document is appended
to rather than rewritten, so what a worker typed into it survives. Every open episode has
one. Under the store, nothing is tracked by git and the ignore boundary covers it, every
document opens with `schema: session-context/v1`, its front matter carries only the declared
keys, and a field naming a transcript, a message list or a model's reply is a violation —
the derived half is the builder's output and the authored half is a summary of the work, not
of a conversation.

A failure to write a working context never costs an episode: it is logged beside the store,
and a non-empty log is a failure of the repository, because otherwise the store stops
filling in silence.

# Failure behaviour

A violation is a `FAIL` finding under the category `session`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11.
An unwired provider is not a violation: a repository that declares nothing reports that it
draws no boundary.

# Verification

`mj_validate_session_lifecycle` decides the store's invariants and the wiring verifier
decides the provider state, both dispatched from `doctor, watch`. The behavioural case
`test/cases/54_session_lifecycle_hooks.sh` proves the wiring, the idempotence of every event,
the briefing the start event writes and the silence of the other two; `test/cases/55_session_context.sh` proves the document's
contract and the findings each mutation of it produces. ADR 0015 records the decision; ADR 0017 records the briefing and the compaction event.
