---
schema: adr/v1
id: adr-0012
kind: adr
title: Every published surface is derived from tracked Markdown, git and the .ai layer by one generic generator
status: proposed
date: 2026-11-15
tags:
  - site
  - publication
  - surfaces
supersedes:
  - adr-0009
provenance:
  origin: extracted
  derived_from:
    - file:tools/surfaces/surfaces.py
    - file:.github/workflows/surfaces.yml
    - decision:adr-0009
---

# 12. Every published surface is derived from tracked Markdown, git and the .ai layer by one generic generator

## Context

ADR 0009 put a hand-written Zola landing page in `zola/` with a workflow that deployed it
on changes under `zola/**`. Three things went wrong with it:

- It restated the README. Within weeks it called the repository private when GitHub
  reported it public, and it named a paid-plan blocker that no longer applied.
- It never deployed, because Pages was off.
- It could not grow. Zola treats `{{` and `{%` in content as template syntax, so the
  repository's own Markdown (workflow snippets, Rhai, C++ templates) cannot be published
  through it unchanged.

The owner also asked for a wiki, for Markdown that stays in step with the records it
summarises, and for all of it to be enforced from the `.ai/` layer and to stay generic.

## Decision

One repository-agnostic generator, `tools/surfaces/surfaces`, derives every published
surface from what git tracks:

- **Site.** Each tracked Markdown document becomes a page. An overview carries interactive
  ECharts views of the history, the documentation and the plan. The `.ai/` layer
  (features, ADRs, rules, milestones and issues, with a Mermaid dependency graph derived
  from `depends_on`) gets its own page. The shell is Flowbite on Tailwind. The site is
  deployed to GitHub Pages.
- **Wiki.** The same documents, with links rewritten to wiki pages, are pushed to the
  GitHub wiki.
- **Markdown regions.** Tables generated in place, between `surfaces:begin`/`surfaces:end`
  markers, from the documents and the `.ai/` records.

`surfaces check` fails on a stale region or on a relative link or anchor that resolves to
nothing. It runs in the pre-commit hook (a policy `enforcement` entry reconciled by
`majordomus doctor`), in `tools/gate.sh change`, and in `.github/workflows/surfaces.yml`.
That workflow replaces `gh-pages.yml` and publishes only from the default branch.

The generator names no repository. Its facts come from git, the `.ai/manifest.yaml`
sections and front matter. `surfaces.toml` declares only what data cannot decide. Each
output writes a `web-surface/v1` declaration under `target/web/<id>/`, so Majordomus
discovers it.

## Alternatives rejected

- **Keep Zola and generate its content.** Zola's shortcode syntax rejects ordinary Markdown
  (measured: `${{ secrets.X }}` fails `zola build`), so every document would need
  escaping, which is a second copy by another name.
- **Client-side rendering of raw Markdown.** No page would exist without JavaScript, links
  and anchors could not be checked at build time, and search engines would see nothing.
- **A CDN for Mermaid and ECharts.** That is a runtime dependency on a third party, and it
  does not pin what a reader sees. The libraries are pinned in `package-lock.json` and
  published with the site.

## Consequences

- Python 3.11+ (with `markdown-it-py` and `PyYAML`) and Node (for the asset build) are
  needed to build the site. `check`, `sync` and `wiki` need only Python.
- Pages and the wiki are enabled on the repository. The wiki needs its first page saved
  once in the web interface before the workflow can push to it.
- A document's first paragraph and front matter now feed every index that lists it, so
  they carry more weight than before.
- Any edit made in the wiki is overwritten by the next sync.
- The generator is written to be lifted into Majordomus as a shipped surface producer;
  nothing in it is specific to this fork.
