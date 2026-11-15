---
schema: context/v1
id: ai.repo.adrs
kind: context
title: Architecture decisions
description: Accepted, durable decisions about how this repository is built.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Architecture decisions

Accepted, durable decisions about how this repository is built, one file each, with the
context, the decision, the alternatives rejected and the consequences. Nothing here is
generated and nothing is promoted here automatically: a decision recorded while working
(`majordomus decision add`) stays in the checkout's local state until a person judges it
durable enough to write down here.

## Index

Generated from each decision's front matter by `tools/surfaces/surfaces sync`; the decisions
themselves stay hand-written.

<!-- surfaces:begin ai-adrs -->
| Decision | Status | Title |
|---|---|---|
| [adr-0001](0001-this-fork-is-a-private-research-target-and-never-interacts-w.md) | proposed | This fork is a private research target and never interacts with upstream |
| [adr-0002](0002-validation-runs-in-three-cost-tiers-behind-one-gate-script-a.md) | proposed | Validation runs in three cost tiers behind one gate script, and a report never claims a step that did not run |
| [adr-0003](0003-principles-are-adapted-from-prismatic-into-this-fork-one-way.md) | proposed | Principles are adapted from Prismatic into this fork one way, and nothing flows back automatically |
| [adr-0004](0004-research-access-to-internals-is-permitted-and-research-code-.md) | proposed | Research access to internals is permitted, and research code is compile-gated, labelled and removable |
| [adr-0005](0005-an-experiment-is-driven-by-the-research-cli-and-its-build-pr.md) | proposed | An experiment is driven by the research CLI and its build profile, runtime flags and evidence are recorded in a versioned manifest |
| [adr-0006](0006-the-research-cli-keeps-the-system-compiler-when-it-supports-.md) | proposed | The research CLI keeps the system compiler when it supports source_location and falls back to Homebrew LLVM only when it does not |
| [adr-0007](0007-prismatic-steers-a-company-through-a-standalone-bridge-that-.md) | proposed | Prismatic steers a company through a standalone bridge that drives a Company AI over rcon, not through a Game Script relay |
| [adr-0008](0008-an-agent-reaches-the-game-through-an-embedded-loopback-mcp-s.md) | proposed | An agent reaches the game through an embedded loopback MCP server whose mutations pass a digest-bound human approval and the command system |
| [adr-0009](0009-the-fork-s-site-is-a-zola-project-published-to-github-pages-.md) | superseded | The fork's site is a Zola project published to GitHub Pages from a fork-only workflow |
| [adr-0010](0010-majordomus-supervises-this-fork-and-the-ai-layer-is-its-trac.md) | proposed | Majordomus supervises this fork and the .ai/ layer is its tracked governance record |
| [adr-0011](0011-the-fork-follows-upstream-by-rebasing-its-commits-onto-upstr.md) | proposed | The fork follows upstream by rebasing its commits onto upstream master, and upstream is a fetch-only remote |
| [adr-0012](0012-every-published-surface-is-derived-from-tracked-markdown-git.md) | proposed | Every published surface is derived from tracked Markdown, git and the .ai layer by one generic generator |
<!-- surfaces:end ai-adrs -->
