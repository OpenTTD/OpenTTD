---
id: majordomus.session-records
version: 1
kind: rule
title: Session records are shared objects with a closed field set
description: A closed execution episode is written once, into the layer's sessions section, against a schema that admits what the repository can prove and nothing else.
statement: A closed session is a shared object of the layer: written by the tool from git and the ledger, valid against the session record contract, carrying no conversation and no fact about the machine that ran it, and discovered like every other kind rather than registered anywhere.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1, majordomus.externalise-decisions@1]
tags: [session, records, schema]

x-majordomus:
  validator: session_records
  category: session
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [session-records]
  tests: [test/cases/63_session_records.sh]
---

# Rationale

A repository that cannot say what happened in it depends on whoever was there, and that
person leaves. The account has to be an artifact, and an artifact nobody can query is an
archive rather than a memory — which is why the record is a kind, discovered and projected
like every other, instead of a dated file in a directory somebody remembers to open.

Two things a shared record must not become. It must not become a transcript: a summary of a
conversation is unverifiable, ages badly, and makes the model that wrote it the authority on
what happened. And it must not become a description of a machine: an absolute path is true
of one disk, means nothing anywhere else, and is disclosure without purpose in a public
repository. The contract is what makes both refusals mechanical rather than remembered.

# Required behaviour

`majordomus session close` writes the record; nothing else writes one and nothing edits one
afterwards. It goes into the section the manifest names — `.ai/repo/sessions/` — and every
field of its front matter is derived: the times from the clock, the heads and the commits
from git, and the reference lists from the ledger's events for that episode. The body is
the only authored part, and it summarises the work rather than the conversation.

The front matter satisfies `session-record`: `schema: session/v1`, the kind, the identity,
the two times and the outcome are present, an unknown key is an error, no value is an
absolute path, and no two records claim one identity. The repository is named by its remote
and the working copy by a hash, so a record identifies where it came from without saying
where it lives.

Discovery is the source class `session`, and that declaration is the whole registration: the
index, the MCP resource, the object routes, the graph, the site and the cockpit follow it.

# Failure behaviour

A violation is a `FAIL` finding under the category `session`, naming the record and what is
wrong with it, and the command that found it exits 10; under `watch` it is drift and the
command exits 11. The Rust index refuses the same record when it builds, with the offending
key named, so a malformed record cannot reach a projection.

# Verification

`mj_validate_session_records` decides it, dispatched from `doctor, watch`. The behavioural
case `test/cases/63_session_records.sh` proves it by mutation: a record with an unknown key,
with an absolute path, with a duplicate identity and with an unknown schema, each refused by
name; and one closed episode that reaches the index, the graph and the listing without
anything being registered. ADR 0014 records the decision.
