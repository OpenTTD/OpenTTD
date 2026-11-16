---
id: project.panel-charts-go-through-one-layer
version: 1
kind: rule
title: Every chart of the panel goes through one layer, is registered, maximizable and linked
description: The panel's charts (index.html overview, the plan DAG, the World page, the hub's viz page) are created only by components.js ecInit, listed in a registry with their address parameters and where a click leads, drawn on Flowbite Pro cards that maximize, and fed from data the panel derives (the server-side history, MCP reads). The stylesheet is rebuilt from its sources whenever markup gains a class.
statement: Create a panel chart only through components.js ecInit, register it with its parameters and drill target, keep its parameters in the address, make its card maximizable, and commit a static/app.css rebuilt from input.css and the current markup.
status: active
class: blocking
depends_on: [project.mcp-surfaces-are-derived@1, project.never-report-unrun-validation-as-pass@1]
tags: [panel, echarts, flowbite, ui]
---

# Rationale

The owner judged the overview's charts and its UX unacceptable (2026-11-16) against their own reference
work: one chart layer, interactive controls, URL state, maximize, mobile first, live, derived. Three
scripts had each called `echarts.init` with their own defaults. The tool pages shipped with Tailwind
classes the committed `app.css` did not contain, so they rendered half-styled, and no check noticed.

# Required behaviour

- **One door.** `tools/research/mcp-panel/static/components.js` `ecInit(box, echarts)` is the only
  `echarts.init` call. A page that needs the dark theme hands the door an echarts handle that only picks
  the theme (`world.js`); a classic script reaches the door through `globalThis.ecInit` (published by
  `overview.js`).
- **Registry.** The overview's charts are rows of `CHARTS` in `static/overview.js`: id, kind, the address
  keys it reads (`ov_metric`, `ov_range`, `ov_focus`, `max`) and where a click goes. Every
  `data-chart` card in `index.html` is in the registry, and every registry row has a box with
  `role="img"` and an `aria-label`.
- **Simulator, not picture.** Parameters live in the address (a configured view is a link). Ctrl+wheel
  zooms, a slider ranges, and `prefers-reduced-motion` stops the motion (in `ecInit`). Maximize goes
  through `components.js` `maximizable`, where Escape restores.
- **Linked.** A company mark focuses that company in every chart. The fleet chart leads to
  `/resource/vehicles`. The stat cards lead to their resource pages (`/resource/<path>`), each tool to
  `/tool/<name>`.
- **Derived data.** The history is the panel's (`GET /api/overview/history`, one sample per game day
  from `openttd://game/state` and `openttd://companies`, kept server-side), pushed by the event stream
  (`/api/events/stream`) and polled as a fallback. Nothing on the page is typed by hand.
- **Mobile first.** At 390 px the page does not scroll sideways: grid items may shrink (`input.css`) and
  wide tables scroll inside their cards.
- **Stylesheet.** `static/app.css` is `tailwindcss -i input.css -o static/app.css --minify`, rebuilt
  and committed with any markup that adds a class.

# Proof

- `node --test tools/research/mcp-panel/test/*.test.mjs` (with `PANEL_CSS_CHECK=1` for the stylesheet):
  the options rendered server-side by ECharts, the one-door scan of every static script, the registry
  against `index.html`, and `app.css` against a fresh build.
- `cargo test --bin mcp-panel`: the history keeps one sample a day, restarts with a new world, is
  bounded.
- `tools/research/mcp-overview-check <panel-url>` in a real browser: every registered chart drawn with
  data, the address round-trips a reload, maximize and Escape, a click focuses, the phone width does
  not scroll sideways, the World page draws through the door, a tool page builds its form, and the
  console has no errors.

# Failure behaviour

A second `echarts.init`, an unregistered chart, or a stale `app.css` fails the node tests, which blocks
the change. Majordomus has no validator for this rule; the tests, `mcp-overview-check` and review
enforce it.
