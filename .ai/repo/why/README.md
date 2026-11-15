---
schema: context/v1
id: ai.repo.why
kind: context
title: Why this exists
description: The operational failure modes this repository's work is a response to, the audiences that recognise them and the areas they fall under, each one a file every projection is derived from.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
children:
  require_contract: false
---

# Why this exists

The catalogue of operational failure modes this repository's work answers. A **moment** is
one such failure mode — something a reader recognises from their own week. An **audience**
is who recognises it. An **area** is the part of operations it falls under.

```text
moments/<id>.md      one operational failure mode      schema: moment/v1
audiences/<id>.md    who recognises it                 schema: audience/v1
areas/<id>.md        the operational area              schema: area/v1
```

The three directories hold instances of a kind, not sections of the layer, so they owe no
contract of their own (`children.require_contract: false`): the format is stated here.

## The contract

Front matter is the machine side and the body is the human side. Discovery, validation,
relations, filtering, ordering, search, the command line, the HTTP API, MCP and the
diagnosis are all read from the front matter; nothing derives structure from the prose.
The contracts are `share/schemas/majordomus/moment/moment.v1.schema.json`, `majordomus/audience/audience.v1.schema.json` and
`majordomus/area/area.v1.schema.json`. A key they do not declare is an error, and a `schema:` version the tool
does not know is refused rather than guessed.

The file name is the `id` is the slug. There is no mapping table anywhere.

## Discovery

Nothing registers a moment. Declare the three source classes in
`../knowledge/sources.yaml` and every file here is discovered through the version-control
index:

```yaml
  - id: moment
    kind: moment
    discovery: vcs
    pathspec: ':(glob).ai/repo/why/moments/*.md'
    required: false
```

From then on, adding a file is the whole act: `majordomus why list` shows it,
`/api/v1/why` returns it, `majordomus://moment/<id>` serves it, and the audience and area
it names gain it without either file being edited.

## Membership and relations are declared once

A moment declares the audiences and areas it belongs to. An audience never lists its
moments — that list is derived where it is shown, and so is every reverse direction. A
backlink is never authored, and the schema refuses a derived key such as `route:`.

## Checking it

```bash
majordomus why validate     # schema, references with the nearest candidate, quality floors
```

An empty catalogue is a valid one. This section starts empty on purpose.
