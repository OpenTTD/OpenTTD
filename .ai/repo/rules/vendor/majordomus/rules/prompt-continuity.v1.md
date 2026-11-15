---
id: majordomus.prompt-continuity
version: 1
kind: rule
title: A prompt record carries its own identity, is bounded, and is private
description: Every captured prompt names the episode it belongs to or says why it cannot; the archive is readable by its owner alone, bounded by a declared retention policy, and never holds credential material verbatim.
statement: A prompt record carries the canonical episode, provider session, repository and worktree it was captured in, or records explicitly that no episode could be resolved; the archive is mode 0600, bounded by prompts.retention_max_days and prompts.retention_max_bytes, and obvious credential material is replaced before it is persisted.
status: active
class: blocking
depends_on: [majordomus.prompt-capture@1]
tags: [prompts, continuity, privacy, retention]

x-majordomus:
  validator: prompt_continuity
  category: prompts
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [prompt-continuity]
  tests: [test/cases/140_prompt_continuity.sh]
---

# Rationale

## The join died at close

A prompt record carried the provider's own session identifier — a UUID that means nothing
outside that provider — and nothing else that could place it. Turning it into the canonical
episode id needed `state/sessions-open/<provider session>.yaml`, and closing the episode
deletes that file. So from the moment an episode ended, nothing in the repository could say
which episode its prompts belonged to.

This is not a gap that widens slowly. It is total and it is immediate: the mapping exists while
the episode is open and is gone the instant it closes, and every prompt of that episode becomes
unattributable at once. Measured in the Majordomus repository, where this rule was written, on
2026-09-11: 974 records, 80 distinct provider sessions, 20 session contexts, and 713 prompts
with no surviving path to an episode. The archive was a heap of text with timestamps.

An identity that lives in a file somebody else will delete is not an identity. So the record
carries it: the episode, the provider session it came from, and the repository and worktree
it was captured in, written at the moment they are still true, using the identity the rest of
the tool already computes — `mj_repository_id` and `mj_worktree_id` — and not a second notion
invented here.

## Attribution is evidence or it is nothing

A record says how it came to name its episode. `open` means the episode was open under that
provider session when the prompt arrived: an observation. `session-context` and `ledger` mean
it was linked afterwards from evidence that survived the close: an inference, and the reader
is told which. `orphan` and `unlinked-legacy` carry no episode at all, and they are two words
rather than one because they are two different facts — a capture that looked and found no
episode, and a record written before the field existed.

Nothing attaches a prompt to the episode nearest it in time. A prompt attributed to the wrong
episode is worse than a prompt attributed to none: the second is visibly missing, and the
first is quietly false — it will be read as evidence, and it will be wrong. The reconciliation
this rule names refuses the moment two episodes could both be the answer, and a validator may
not report an archive as fully linked when the evidence for that claim does not exist.

## The archive is private, and it was not

Raw prompts are the most private thing this tool writes. People paste credentials into them —
to ask why a request returns 401, to have a configuration read back — along with customer
names, internal hostnames and paths. The session state beside them has always been 0600. The
prompt records were 0644 for the entire life of the archive: readable by every account on the
machine, including their file names, which are the openings of the prompts themselves.

Two things follow. The archive and everything in it is readable by its owner alone, directory
included. And obvious credential material is replaced on its way to the record, before the
bytes are anywhere but a variable — not afterwards, because a secret that was on disk has been
on disk. What was replaced is recorded, so that "nothing was found" and "the secret is gone"
are different statements a reader can tell apart.

## Something bounds it

The ledger, the handovers and the checkpoints each rotate under a declared cap. Prompts had
none, on the argument that a prompt restates nothing and so nothing may delete one. That
argument is about the *record* — that it happened, when, in which episode, under which head —
and not about the bytes of the text, which is the part that carries the credentials and whose
risk does not decay with its value.

So retention takes the body and keeps the record. Every field survives, including the
provenance of the episode link, and a tombstone says when the body went, how long it was and
the digest of what it was. A record is still never deleted. The archive stays a complete index
of what was asked and when.

# Required behaviour

Every record under the archive carries `episode_link`, whose value is one of `open`,
`session-context`, `ledger`, `orphan` or `unlinked-legacy`. A record with `open`,
`session-context` or `ledger` carries a non-null `episode`; the other two carry none. A record
carrying no `episode_link` at all is a violation, because it can no longer be told apart from
one whose episode was lost.

A record written by the capture hook carries `repository_id` and `worktree_id` as the tool
computes them elsewhere. A payload that names no provider session, or one whose provider
session has no episode open in this worktree, produces `orphan` — never the episode of
whatever opened one most recently in this checkout.

The archive directory is mode 0700 and every file under it is mode 0600.

The policy declares `prompts.retention_max_days` and `prompts.retention_max_bytes`. A body
older than the first, or the oldest bodies past the second, are pruned by an explicit command
and never as a side effect of capture. Pruning is atomic per record, and leaves the record,
its identity, its provenance and a tombstone of what was taken.

The credential shapes the writer knows are replaced before the record is written, so that they
appear neither in the record, nor in either rendering, nor in the file name, nor in any
diagnostic. No diagnostic prints the value of an environment variable.

# Failure behaviour

A violation is a `FAIL` finding under the category `prompts`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11. An
archive that is not present at all is a skip, not a pass: nothing has been captured here.

# Verification

`mj_validate_prompt_continuity` decides it, dispatched from `doctor, watch`. The behavioural
case `test/cases/140_prompt_continuity.sh` drives the provider's own hook shims — not the
library functions — through a live episode, an orphan, a reconciliation and a prune, and puts
a fake credential through the real capture path to assert the persisted representation does
not carry it. CI runs that case.
