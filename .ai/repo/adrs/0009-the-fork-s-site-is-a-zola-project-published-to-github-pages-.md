---
schema: adr/v1
id: adr-0009
kind: adr
title: The fork's site is a Zola project published to GitHub Pages from a fork-only workflow
status: superseded
superseded_by: adr-0012
date: 2026-07-24
tags:
  - site
  - publication
  - zola
related:
  - file:.github/workflows/surfaces.yml
  - file:tools/surfaces/README.md
provenance:
  origin: extracted
  derived_from:
    - commit:49057dddd64e50e03673beefe9b6fcaeebff0619
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

## Superseded

Superseded by ADR 0012 on 2026-11-15: the Zola site never deployed, restated the README
until it disagreed with it, and could not publish the repository's Markdown unchanged.
`zola/` and `gh-pages.yml` are removed; `tools/surfaces` derives the site, the wiki and the
Markdown regions instead.
