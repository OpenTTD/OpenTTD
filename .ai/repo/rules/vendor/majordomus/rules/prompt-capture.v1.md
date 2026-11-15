---
id: majordomus.prompt-capture
version: 1
kind: rule
title: Prompt capture is proven, or reported unavailable
description: A repository that declares prompt capture has it wired below the model and proven by running it; the archive stays ignored and untracked, every prompt is present as both a record and a rendering, and neither carries anything the model said.
statement: Prompt capture is either wired below the model and proven by execution, or reported as unavailable; the archive is never committed, every prompt is present as both a JSON record and its Markdown rendering, and the schema every record names resolves to the file that describes it.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [prompts, provider, evidence]

x-majordomus:
  validator: prompt_capture
  category: capture
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [prompt-capture]
  tests: [test/cases/29_prompt_capture.sh]
---

# Rationale

A worker cannot be asked to record its own prompts. It never sees the bytes the person
typed, only what the provider assembled from them, and a record written by a model is
missing exactly the prompts that mattered: the first one of a session, which arrives before
any instruction has been read, and every one where the model was busy doing what it was
asked. An instruction in a bootstrap file is a request, not a mechanism, and there is no
behavioural test that can prove a request was honoured.

So capture belongs in the provider's own hook, below the model, where it is deterministic
and where running it is the proof that it works. That leaves two honest states for a
provider — capturing, or not observable — and this rule is what keeps the second from being
quietly presented as the first.

The archive is also the most sensitive file the tool writes. Raw prompts carry credentials,
customer names and paths as often as they carry intent, so the same rule that demands the
capture be real demands that it never leave the machine through Git.

An archive nobody opens is evidence of nothing, and a record shaped for a machine is not
one a person reads: the prompt arrives escaped, on one line, wrapped in the fields the
provider sent. So every prompt is kept twice under one stem — the JSON record, which is
what the provider sent and what everything else is derived from, and the Markdown
rendering, which is what a person opens. Writing only one of them is the failure this
covers, in either direction, because an archive where the format depends on when a prompt
happened to be captured cannot be read end to end and cannot be trusted to be complete.

# Required behaviour

An enforcement entry of kind `provider-hook` holds only when the provider's configuration
names the shim the tool wrote, the shim is executable, and a synthetic payload driven
through it produces a record. A repository that wires nothing reports that it captures
nothing.

A provider is reported `unsupported` only when the declaration says it has **no such event**,
with the citation that was verified from; a provider that documents one this distribution
ships no adapter for is reported `unadapted`. The distinction is not pedantry: `unsupported`
is a statement about somebody else's product, and equating it with "we have no adapter" made
this tool say something untrue about two of them the day they shipped hook systems it had not
been told about. Every capability cell carries its citation for that reason
(`project.a-provider-capability-cites-its-evidence`).

Under the archive directory, nothing is tracked by Git and the ignore boundary covers it.
Every record begins with the schema identifier and carries the closed field set the writer
emits; a field naming a model response, a completion or a transcript is a violation, because
the archive holds the person's half of the exchange and no more.

## The record

A record is a JSON object, one member per line, in the order the schema declares, opening
with `{`, then `  "schema": "<id>",`, and closing with `}`. It is pretty printed because a
record is read by people too and a single forty-kilobyte line is not something any editor
or diff shows usefully; the indentation lies outside the string spans, so the prompt's own
bytes cross the formatting untouched.

The fields a capture can fill are required and always present. Everything the schema
declares beyond them is optional in one specific sense: **optional means the writer could
not know it, never that it was dropped.** Capture runs before the model is invoked, so at
the moment a record is written the turn has not happened — the time it finished, how long
it took, which model answered, at what effort, what it spent, and what it produced are not
observable from that hook. Those fields are therefore absent from a record rather than
present as null, because an absent field says "not observed" and a null one would claim it
was observed to be nothing. A renderer omits the row or the section rather than printing a
placeholder.

A record may carry `meta.parent`: the file name of the record it derives from, within the
same archive. It is a name and never a path, because a record can only ever point inside
its own directory.

## The schema is a path

Every record names its schema, and that identifier **is** the location of what describes it:
`<namespace>.<name>/<version>` lives at `share/schemas/<namespace>/<name>/<version>`, so
`majordomus.capture/v1` resolves to `share/schemas/majordomus/capture/capture.v1` with nothing in
between — no registry, no lookup table, nothing that can fall out of step with the records it
claims to describe. A reader holding a record holds the way to read it.

There are two files at that stem, because there are two projections and they are not the
same kind of thing:

| projection | file | language |
|---|---|---|
| the record | `<stem>.schema.json` | JSON Schema |
| the document | `<stem>.proto` | protobuf |

The record is JSON, so JSON Schema describes it directly and can be validated against it.
The document is Markdown — a shape, an order, sections that are present or absent — and a
message with numbered fields states that without pretending a document is a bag of keys.
Neither is a wire format here: nothing serialises these, and they are the one place each
projection's parts are defined so that the two cannot disagree about what a prompt is.

A schema identifier naming a file that is not there is a violation, and both files must be
there: a record describes both its halves or it describes neither. Adding a version means
adding the two files at the path that version's own identifier already derives.

## The rendering

Every record has a Markdown rendering of the same stem beside it, and every rendering has
its record. A rendering has one shape and every rendering has that shape: the record's
fields as YAML front matter, the same fields as a table a person reads, then the optional
sections `## CONTEXT` — what was loaded when the prompt was read — and `## OUTPUT` — how the
turn ended — each present exactly when the record carries the field it renders, and finally
`## PROMPT`, always present and always last, decoded and fenced with a run of backticks
longer than the longest run the prompt contains, so that a prompt quoting code cannot end
the block early. The prompt is last because it is the only field with no bound on its size.

The fields appearing twice, as front matter and as a table, is not duplication that can
drift: one function writes both out of one record in one pass, and a rendering is never
edited by hand.

A record without a rendering is repaired from the record. A rendering without a record
cannot be repaired at all and is reported for removal, because nothing can reconstruct what
the provider sent from something that was derived from it. A record whose members the
reformatter cannot account for in full is left exactly as it is and reported, rather than
rewritten into something that silently drops what it could not read.

`## OUTPUT` renders the model's half of the exchange, which a prompt record may not carry.
A conforming writer puts it in a separate after-the-turn record whose `meta.parent` names
the prompt record, and the rendering joins the two. The document has the section; this rule
says where the bytes live.

# Failure behaviour

A violation is a `FAIL` finding under the category `capture`, and the command that found it
exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_prompt_capture` decides the archive invariants and the wiring verifier decides
the provider state, both dispatched from `doctor, watch`. The wiring verifier drives a
synthetic payload through the shim and reads back both halves, so `verified` means the pair
was produced and not merely that the hook ran. The behavioural case
`test/cases/29_prompt_capture.sh` proves them, and CI runs that case.
