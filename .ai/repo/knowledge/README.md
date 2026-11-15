---
schema: context/v1
id: ai.repo.knowledge
kind: context
title: Knowledge
description: How repository knowledge is declared and discovered, never collected.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Knowledge

Knowledge is declared, not collected. `sources.yaml` names the classes of repository
files that carry knowledge and the version-control pathspec each class is discovered
through. Discovery reads the Git index, never the filesystem: an untracked file, build
output or a vendored tree is not a source, and two machines discover the same list in the
same order.

`curated/` holds notes this repository chose to write about itself. It is not a copy of
the documentation, the source or the tests; those are referenced by the sources file and
read where they live. Anything compiled from the sources — an index, a graph — is a
rebuildable local product and belongs under `../../local/cache/`.
