---
schema: feature/v1
id: fork-site
kind: feature
title: Fork landing site
short_title: Site
headline: The fork's purpose, rules and research layer on one static page.
summary: A Zola project under zola/, deployed by a fork-only GitHub Pages workflow.
status: draft
weight: 60
areas: [validation]
audiences: [research-owner]
docs: [README.md]
adrs: [adr-0009]
related: [supervised-ai-layer]
tags: [site, zola, pages]
---

## What it does

`zola build` and `zola check` pass, and the workflow deploys on pushes that touch `zola/**`.

## What it does not do

It is not served yet: GitHub Pages is not enabled (issue I0019).
