---
schema: context/v1
id: ai.repo.knowledge.candidates
kind: context
title: Candidate records
description: What the tool derived from the ledger and git at an episode boundary and nobody has reviewed yet, discovered as a knowledge source class.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Candidate records

Records the tool derived, each one a Markdown file discovered through the `candidates`
source class in `../sources.yaml`. A candidate is the same kind as a note under
`../curated/`, one status apart: it carries `status: candidate` and
`provenance.origin: extracted`, and its `derived_from` names the ledger objects it came
from — the episode, the task or decision, the commits of the episode that touched a rule or
a decision record. Nothing here was written by a person and nothing here was read from a
conversation: the evidence is the ledger and git, and the same evidence yields the same
bytes.

A candidate is written at every episode boundary — the provider's end event and its
compaction event, or `majordomus knowledge derive` by hand — and it waits here for an act.
`majordomus knowledge promote <id>` with evidence on standard input moves it to
`../curated/` as `verified`; `majordomus knowledge reject <id> --reason "<why>"` marks it
`superseded` in place with the reason. The deriver never writes `verified` and never writes
into `../curated/`; a record here that claims to be verified is refused by `check` and
`doctor`. The file name is the record id, and the id is derived from the episode and a
digest of the evidence, so a second derivation over the same evidence rewrites the same
file and changes nothing.

A candidate derived from an observation (`majordomus knowledge observe`) also says what it is
`about` — the subject the worker named — and its `route`, which owner should look first,
derived from that subject's structure (ADR 0118). Each stays one episode's own file;
`majordomus knowledge candidates` groups the records about one subject, so the same thing
observed by several sessions reads as one group, a rejected record listed beside what came
after it.

The directory is tracked for the reason every closed session record is: a record only one
machine can read is a record nobody reads. Add a candidate beside the session record it
names; `doctor` names the ones version control does not hold, and the queue is measured
against the policy's cap and age so that it is reviewed rather than accumulated.
