---
id: project.panel-items-are-routable-pages
version: 1
kind: rule
title: Every tool, resource, duel and proof of the panel is a page of its own, reached by a click
description: The panel's catalogue (every MCP tool, GUI client tool and resource), its duels and its derived proofs (the README scoreboard, transfer yards) each have a stable address served with its own title and description, are reached from every list or mark that names them, keep their filters in the address, and are backed by a panel route that OpenAPI, Swagger UI and Rhai derive. Facets and groups are read from the catalogue, never typed.
statement: Give every panel item its own stable page with a server-side title and description, link every mention of it, keep filters and views in the address, back every button by a declared panel route, and derive every grouping from the catalogue itself.
status: active
class: blocking
depends_on: [project.mcp-surfaces-are-derived@1, project.panel-charts-go-through-one-layer@1, project.never-report-unrun-validation-as-pass@1]
tags: [panel, catalogue, ui, routing, flowbite]
---

# Rationale

The owner's order (2026-11-17): "everything clickable, routable, interactive, working, leading to its
own page for each tool, point and functionality, with a stable SEO-friendly UI, connected to the
backend, useful, MCP integrated", and "expose it over MCP, API, Swagger, UI and app so I can click
it". The panel's home listed 321 catalogue items as two plain columns of links, and the README
scoreboard could only be regenerated from a shell.

# Required behaviour

- **A page per item.** `/tool/<name>`, `/client/<name>`, `/resource/<path>`, `/result/<duel>` are served
  by the panel from one template each, with the item's name and description put into `<title>` and
  `<meta name="description">` before any script runs (`panel.rs`, `panel_results.rs` `page_meta`); an
  unknown item answers 404.
- **Linked.** Every card, row, chart mark and search result naming an item links to its page: the
  catalogue component's cards and sunburst leaves (`static/catalogue.js`), the duel table and bars,
  the scoreboard's best run, the yard rows (to the owner's camera at the shared end).
- **One catalogue component.** `static/catalogue.js` serves `/tools` and the home's `#catalogue`. Its
  facets (kind, namespace, command category, safety) are read from the OpenAPI document the panel
  derives from the live MCP catalogue; no tool, namespace or category is listed by hand. The drill of
  its map is the facet filter, so chart and cards cannot disagree.
- **Address state.** Search, facets and maximized views live in the address (`c_` prefix on the home),
  so every view is a link.
- **Every button is a route.** A click calls a row of `routes()` (`panel.rs`), so the same action is in
  OpenAPI, Swagger UI and Rhai: `POST /api/scoreboard`, `POST /api/duel`, `GET /api/plan?track&yards`.
  The page shows the route it calls (an "API" link to its Swagger operation).
- **Flowbite Pro and mobile first.** Pages use the panel's Flowbite Pro shell and cards; at 390 px they
  do not scroll sideways and the navbar does not overflow.

# Proof

- `node --test tools/research/mcp-panel/test/*.test.mjs`: `catalogue.test.mjs` derives items, facets,
  counts, the hierarchy and the drill from an OpenAPI fixture and round-trips the address.
- `cargo test --offline --release` in `tools/research/mcp-rhai`: the route table is unique, every POST
  declares its body, the scoreboard routes are present, `page_meta` titles each page.
- `tools/research/mcp-overview-check <panel-url>` in a real browser: a tool page builds its form, a
  duel bar leads to its run, a comparison survives a reload of its address, no console errors.

# Failure behaviour

A hand-listed facet, an item without its page, or a POST without a body schema fails the tests above,
which blocks the change. Majordomus has no validator for this rule; the tests, `mcp-overview-check`
and review enforce it.
