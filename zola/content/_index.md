+++
title = "korczis/OpenTTD — private research fork"
template = "index.html"
+++

## What this is

`korczis/OpenTTD` is a private academic research fork of [OpenTTD](https://www.openttd.org/), used as a real-world validation target for the Prismatic platform — evaluating coding-agent workflows, gating, and reproducibility against a large, long-lived C++ codebase.

It is **not** the official OpenTTD project and is **not** staged for upstream contribution. See the repository's [`README.md`](https://github.com/korczis/OpenTTD/blob/master/README.md) for the full picture: purpose and goals, the relationship to the Prismatic platform, the layered validation model (`tools/gate.sh`, `tools/research/research`), and the research-mode instrumentation policy.

## Where to look

- [Repository](https://github.com/korczis/OpenTTD)
- [`AGENTS.md`](https://github.com/korczis/OpenTTD/blob/master/AGENTS.md) — ground rules for any coding agent working in this fork
- [`research/`](https://github.com/korczis/OpenTTD/tree/master/research) — gating model, experiment-report taxonomy, and the OpenTTD↔Prismatic bridge design
