---
id: majordomus.candidates-reviewed
version: 1
kind: rule
title: Candidates are reviewed
description: A review queue that grows without bound is not reviewed. More candidates than the policy cap, a candidate that has waited longer than the policy age, or a candidate the tree does not track, is reported, not stopped; both numbers are declared once in the policy.
statement: Candidates are reviewed, not accumulated. More records with status candidate than knowledge.candidates_max_files, a candidate whose review age exceeds knowledge.candidate_max_age_minutes, or a candidate under candidates/ that version control does not track, is a finding that names them; both numbers are declared once in the policy.
status: active
class: advisory
depends_on: [majordomus.knowledge-integrity@1]
tags: [knowledge, health, accumulation]

x-majordomus:
  validator: knowledge_accumulation
  category: knowledge
  enforced_by: [doctor]
  exit_code: 0
  claims: [candidates-reviewed-not-accumulated]
  tests: [test/cases/711_candidates_are_reviewed_not_accumulated.sh]
---

# Rationale

The deriver writes a candidate at every episode boundary that produced evidence, and nothing
removes one except a person promoting or rejecting it. Left alone, the directory grows, and
a queue that grows without bound is not a queue anybody reads. `project.accumulation-is-measured`
says that anything which grows on its own is measured and reported before it is a problem;
this rule is that measurement for the review queue.

Whether a candidate deserves promotion is a judgement, and a machine that emptied the queue
would be making it. So the rule is advisory: it names what has accumulated and stops nothing.
It is dispatched from `doctor` only, because under `watch` an advisory finding is drift and
the command exits 11, and a full review queue must not turn `watch` red in a hook.

Two of the three findings are about time and count, and the third is about visibility. A
candidate the tree does not track is discovered by nothing: the index does not see it, the
graph does not see it, no surface serves it, and a second checkout will never review it. It
is named so the person adds it beside the session record, which is what the deriver's own
output already asks for.

That remedy is a commit, and a trunk that changes only through pull requests cannot make
one. So on the trunk the deriver does not write into `candidates/` at all: it stages the
candidate, with the episode's session record, in the checkout-local outbox
`.ai/local/state/outbox/`, and the next episode boundary on another branch of the clone moves
it into `candidates/`, where that branch's pull request carries it to the trunk (ADR 0091 and
its amendment). This rule never asks the trunk for a commit it cannot make.

# Required behaviour

Candidates are reviewed, not accumulated. More records with `status: candidate` than
`knowledge.candidates_max_files`, a candidate whose review age exceeds
`knowledge.candidate_max_age_minutes`, or a candidate under `candidates/` that version control
does not track, is a finding that names them. Both numbers are declared once in the policy
and read by every surface; a superseded record stays under `candidates/` and counts toward
neither.

A candidate staged in the outbox is not under `candidates/`, so it is neither counted nor
reported as untracked here. It is not hidden: until a branch carries it, `context` prints a
line and `doctor` an `INFO` finding `outbox` under the category `clone`, each naming how many
session records and candidates wait in `.ai/local/state/outbox/`. A file there that is not a
record is left in place and named on standard error by the episode that would have carried
it, and is counted until a person moves or removes it.

Review age is the age of the queue entry, not of the evidence. It is measured from the
oldest `knowledge.derived` line whose paths name the file, falling back to the commit that
added the file, and only then to the record's `date`; the finding says which source it used.
A record's `date` is the day of the evidence and is deterministic, so a candidate derived
today from last month's decision has been waiting since today.

# Failure behaviour

A violation is a `WARN` finding under the category `knowledge`, naming the paths (up to a
few, then the count) and which threshold or condition they crossed; the command continues
and exits as it otherwise would. The reproduce for the queue findings is `majordomus
knowledge candidates`; for an untracked candidate it is `git status --porcelain
.ai/repo/knowledge/candidates`, and the remedy follows `fix:` in the message: `git add
.ai/repo/knowledge/candidates && majordomus derive`. A policy that declares neither
`knowledge.candidates_max_files` nor `knowledge.candidate_max_age_minutes` is reported at
this rule's own class, naming the missing key, the way the retention caps are reported.

# Verification

`mj_validate_knowledge_accumulation` decides it, dispatched from `doctor`. The behavioural
case `test/cases/711_candidates_are_reviewed_not_accumulated.sh` proves it — the cap, the
age, the untracked file and the missing key are each a finding of the right level, and
`watch` is unaffected — and CI runs that case.
