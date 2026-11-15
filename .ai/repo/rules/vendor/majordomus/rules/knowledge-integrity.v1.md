---
id: majordomus.knowledge-integrity
version: 1
kind: rule
title: Knowledge integrity
description: Every knowledge record is one evidenced, well-formed assertion. It is valid against its schema with no unknown key, its identity is unique and equals its file name, a verified record carries provenance, every reference it names resolves, a superseded record says what replaced it or why it was rejected, a candidate never claims to be verified, and no record carries a conversation.
statement: Every knowledge record under candidates/ and curated/ is valid against knowledge/v1 with no unknown key; ids are unique across both directories and equal the file name; verified carries provenance and an extracted origin names evidence; every derived_from and relations target resolves; superseded names superseded_by or records its rejection; no record under candidates/ claims verified; no title, description or body carries a conversation.
status: active
class: blocking
depends_on: []
tags: [knowledge, integrity]

x-majordomus:
  validator: knowledge_integrity
  category: knowledge
  enforced_by: [check, doctor]
  exit_code: 10
  claims: [knowledge-integrity, knowledge-promotion-is-an-act]
  tests: [test/cases/707_knowledge_check_refuses_malformed_records.sh, test/cases/709_knowledge_promotion_is_an_act.sh]
---

# Rationale

A knowledge record is an assertion about the repository with the evidence it rests on. What
makes it worth more than a sentence in a document is that the evidence is a typed reference
something can resolve: a session the layer holds, a task the ledger saw, a commit git has, a
file the tree contains. A record whose reference resolves to nothing is an assertion wearing
a citation, and a reader months later cannot tell it from one that was true.

The store has two directories and one status field, and the field is what separates them.
A candidate is what the deriver wrote from the ledger and git; `verified` is what a person
wrote after reading the evidence. A candidate that claims `verified` has skipped the act
that gives the word its meaning, and a verified record with no provenance has kept the word
and dropped the evidence. Both are refused, because the promotion act is the whole point of
having two statuses.

The record is never a conversation. The layer's oldest rule is that a transcript is not
state, and a knowledge record is the place where that rule is most tempting to break: a
summary of what was said is easy to write and reads as knowledge. The check that keeps
`project.never-store-transcripts` applies to a record's title, description and body, so the
rule is kept mechanically and not by memory.

# Required behaviour

Every knowledge record under `candidates/` and `curated/` is valid against `knowledge/v1`
with no unknown key; ids are unique across both directories and equal the file name;
`verified` carries provenance and an `extracted` origin names evidence; every `derived_from`
and `relations` target resolves; `superseded` names `superseded_by` or records its rejection
under `# Rejected`; no record under `candidates/` claims `verified`; no title, description or
body carries a conversation.

A reference resolves where its type says the target lives: a file or test in the tree, a
session in the sessions store or the ledger, a task or decision in the ledger or the state
directory, a commit in git, an issue in the project section, a knowledge record in either
directory, a rule or a decision record in the layer. `decision:none` and `task:none` resolve
to nothing and are refused. The validator reads both directories once and resolves every
reference in one pass over the ledger and one batched query of git; its cost grows with the
store, not with the product of records and references. A directory that does not exist has
no records and passes.

`majordomus knowledge check` is the same check as a command, so a person can run it before
`check` or `doctor` does.

# Failure behaviour

A violation is a `FAIL` finding under the category `knowledge`, one per failing record naming
its path and what is wrong, and the command that found it exits 10. The reproduce is
`majordomus knowledge check`.

# Verification

`mj_validate_knowledge_integrity` decides it, dispatched from `check, doctor`. The
behavioural cases `test/cases/707_knowledge_check_refuses_malformed_records.sh` and
`test/cases/709_knowledge_promotion_is_an_act.sh` prove it — the first that a candidate
claiming `verified`, a dangling reference, a duplicate id, an unknown key, a transcript line
and a superseded record with no reason are each refused, the second that `verified` is
written only by the promotion act with evidence and that rejection supersedes with a reason
— and CI runs both.
