---
id: majordomus.decision-threshold
version: 1
kind: rule
title: The decision threshold
description: A change that is hard to reverse records the decision behind it as a decision record, and names what it put in force; an ordinary change does not.
statement: A change that alters an architecture, a durable contract, a data or storage semantic, a trust boundary, or the repository's own governance records a decision before it lands, and names in that record the rule, claim, document, implementation or case it put in force; nothing else needs one.
status: active
class: blocking
depends_on: [majordomus.adr-integrity@1, majordomus.externalise-decisions@1]
tags: [adrs, records, architecture]

x-majordomus:
  tests: [test/cases/99_adr.sh]
---

# Rationale

A repository without decision records answers "why is this like this?" from the memory of
whoever was there, and that memory leaves. A repository that records every change answers
it from a pile nobody reads: a hundred records about renamed variables bury the four that
say why the storage layer looks the way it does. The threshold is the whole value of the
practice, and it is a judgement, so it is written down here rather than inferred.

Recording is also not a separate act of paperwork. The record is written while the decision
is being made, by whoever is making it, because a record reconstructed afterwards states
the outcome and loses the alternatives — and the alternatives are what a later reader needs
in order to know whether the decision still holds.

# Required behaviour

A change records a decision when it alters an architecture or a subsystem boundary, a
public or protocol contract, a plugin or extension surface, a persistence, storage or
durable-data semantic, a trust or security boundary, a dependency the repository would
find hard to leave, or the repository's own governance — its rules, its kinds, its schemas
or the layer's structure — and whenever it accepts a trade-off between correctness,
performance and complexity that a later reader could reasonably want to reopen.

A change does not record one when it is a typo, a formatting pass, a mechanical
refactoring, a defect fixed without a new design, the straightforward implementation of a
decision already recorded, a routine dependency bump, or a regeneration of derived output.

The record is written before the change lands, not after it is merged, and it names what
it put in force: the rule, the claim, the document, the implementation and the case, so
that the reverse question — what decided this file? — is answerable from the graph.

A worker does not ask a person whether to record a decision that clears this threshold. It
says which threshold the change crosses, records it, and continues; the person is asked
about the decision itself, never about the bookkeeping.

# Failure behaviour

No command decides this rule, and none can: whether a diff embodies an architectural
decision is a judgement about intent, and a tool that claimed to make it would be guessing
in a way nobody could audit. A reviewer decides it, and a change that crosses the threshold
with no record is not merged.

What the tool does decide is everything downstream of the judgement: `majordomus adr check`
and the doctrine `majordomus.adr-integrity` refuse a malformed record, a duplicate
identity, one-sided supersession and a reference that resolves to nothing, and
`majordomus adr affected` names the decisions a change set touches, so a reviewer reads the
records the change reaches instead of remembering them.

# Verification

Review, with `majordomus adr affected --base <ref>` as the aid: it lists the decisions whose
own file or whose named paths the change set touches. `test/cases/99_adr.sh` proves that
listing, and the format checks it depends on, by mutation.
