---
schema: context/v1
id: ai.repo.knowledge.curated
kind: context
title: Curated notes
description: What this repository chose to write about itself, discovered as a knowledge source class.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Curated notes

Notes this repository wrote about itself, each one a Markdown file discovered through the
`curated` source class in `../sources.yaml`.

A note here earns its place by being something no other file in the repository already
says. It is not a summary of the documentation, not a copy of a rule, and not a place to
restate a decision record: those are sources of their own, discovered where they live, and
a copy here would be the second source of truth that goes stale first. Anything compiled
from the sources — an index, a graph — is a rebuildable product and belongs under the
checkout-local cache.
