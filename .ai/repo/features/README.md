---
schema: context/v1
id: ai.repo.features
kind: context
title: Product features
description: What this repository's product does for a person, one feature per file, each made of things the other registries already own; every surface that presents a feature is derived from these files.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Product features

A **feature** is one thing this repository's product does for a person, stated once, in one
Markdown file here (`schema: feature/v1`). The front matter names what the feature is made
of — capability modules, commands, object kinds, rules, documents, decisions, claims, use
cases — each a typed reference the tool validates, and the presentation decisions nothing
can infer: the headline, the order, whether it is featured. Everything else a page or an
API says about a feature is derived from those references and never written here.

The contract is `share/schemas/majordomus/feature/feature.v1.schema.json` of the tool
distribution; a key it does not declare is an error, and a `schema:` version the tool does
not know is refused rather than guessed. The file name is the `id` is the route.

## Discovery

Nothing registers a feature. Declare the source class in `../knowledge/sources.yaml` and
every file here is discovered through the version-control index:

```yaml
  - id: feature
    kind: feature
    discovery: vcs
    pathspec: ':(glob).ai/repo/features/*.md'
    required: false
```

From then on, adding a file is the whole act: `majordomus product list` shows it,
`/api/v1/product/features` returns it, `majordomus://feature/<id>` serves it.

## Checking it

```bash
majordomus product validate     # schema, references with the nearest candidate, floors
```

An empty section is a valid one. This section starts empty on purpose.
