---
schema: adr/v1
id: adr-0009
kind: adr
title: The fork's site is a Zola project published to GitHub Pages from a fork-only workflow
status: proposed
date: 2026-07-24
tags:
  - site
  - publication
  - zola
related:
  - file:zola/zola.toml
  - file:.github/workflows/gh-pages.yml
  - file:zola/content/_index.md
provenance:
  origin: extracted
  derived_from:
    - commit:49057dddd64e50e03673beefe9b6fcaeebff0619
    - file:zola/zola.toml
    - file:.github/workflows/gh-pages.yml
---

# 9. The fork's site is a Zola project published to GitHub Pages from a fork-only workflow

## Context

The fork's purpose, rules and research layer are spread over README, AGENTS.md and
`research/`. A landing page should present them without touching upstream's documentation
or CI.

## Decision

The site is a Zola project under `zola/` (config, base and index templates, a
dark-mode-aware stylesheet), built and deployed by a fork-only workflow,
`.github/workflows/gh-pages.yml`, on pushes that touch `zola/**`. The generated
`/zola/public` is ignored.

## Alternatives rejected

- **Publishing upstream's docs pipeline.** Upstream CI is not this fork's process.
- **A JavaScript site generator.** Zola is one static binary and needs no dependency tree.

## Consequences

- `zola build` and `zola check`, external links included, passed when it landed.
- Pages is not enabled yet. It needs Settings, Pages, Source: GitHub Actions, and a private
  repository needs a paid plan (issue I0019).
