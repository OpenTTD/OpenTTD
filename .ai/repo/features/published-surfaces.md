---
schema: feature/v1
id: published-surfaces
kind: feature
title: Published surfaces derived from the repository
short_title: Surfaces
headline: The site, the wiki and the Markdown index tables, all generated from what git tracks and the .ai/ layer, and checked on every commit.
summary: tools/surfaces renders every tracked Markdown document to a GitHub Pages site and the GitHub wiki, keeps generated regions inside the documents current, and fails the commit on a stale region or a dangling link.
status: stable
weight: 60
areas: [validation]
audiences: [research-owner]
docs: [tools/surfaces/README.md, README.md]
adrs: [adr-0012]
related: [supervised-ai-layer]
tags: [site, wiki, pages, documentation, derivation]
---

## What it does

- **Site.** `tools/surfaces/surfaces site` renders every tracked Markdown document as a
  page. It adds an overview with interactive ECharts views of commits, documentation and
  plan progress, an `.ai/` layer page with features, decisions, rules, the plan and a
  Mermaid dependency graph, and client-side search. `.github/workflows/surfaces.yml`
  deploys it to GitHub Pages from `master`.
- **Wiki.** `tools/surfaces/surfaces wiki` turns the same documents into wiki pages, and
  the workflow pushes them to the GitHub wiki.
- **Markdown regions.** `tools/surfaces/surfaces sync` regenerates the tables between
  `surfaces:begin` and `surfaces:end` markers inside the documents.
- **Gate.** `tools/surfaces/surfaces check` fails on a stale region or on a relative link
  or anchor to nothing. It runs in the pre-commit hook, in `tools/gate.sh change` and in CI.

## What it does not do

It publishes nothing a person writes for the site alone, and it does not edit upstream's
documents: those are exempted from the link check by path in `surfaces.toml`. The wiki is
overwritten on every sync, so an edit made there is lost.
