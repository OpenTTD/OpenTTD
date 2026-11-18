---
id: project.web-ui-is-covered-end-to-end
version: 1
kind: rule
title: Every web UI page is driven end to end in a real browser, or it does not exist
description: Every page of the panel and the hub, as the one shell declares them, has a Playwright check that opens it in Chrome against a running game and proves its content, its shell, no page error and no sideways scroll at phone width. tools/research/e2e-coverage derives the pages from the shell and the coverage from the checks' declarations, and fails the gate on a page no check drives.
statement: Cover every web UI page with a Playwright end-to-end check that drives it in a real browser against a live game, declare it with an `e2e-covers:` line, and keep tools/research/e2e-coverage passing; a page, button or flow no such check drives is treated as not built.
status: active
class: blocking
depends_on: [project.panel-items-are-routable-pages@1, project.never-report-unrun-validation-as-pass@1]
tags: [panel, hub, ui, testing, playwright, e2e]
---

# Rationale

The owner's order (2026-11-18): "everything in the web UI must from now on always be covered by
Playwright end-to-end tests, and this must be a rule, always, enforced now; what is not tested end
to end through MCP, web, UI or app does not exist."

Unit tests of a page's pure parts (`node --test tools/research/mcp-panel/test/`) prove its
functions, not the page. On the day the rule was written, the first run of `mcp-pages-check` found
the Interface page showing empty cards with no word of why on a game without a GUI client. Every
unit test had passed.

# Required behaviour

- **Pages are derived.** The pages are every page of every app in `static/shell.js` `APPS`: the
  panel and the hub. A new page becomes part of the rule the moment it joins the registry, with no
  list to update.
- **A check drives a real browser.** It launches the installed Chrome through `playwright-core`
  (`chromium.launch`) against a running stack (`tools/research/mcp-e2e`). It proves for every page
  it covers:
  - the page answers;
  - its own content renders, not only the frame;
  - it sits in the one shell, with its sidebar entry current;
  - it throws no page error;
  - it does not scroll sideways at 390 px.

  A flow the page offers (save, run, start, a form) is driven through its controls, not only its
  API.
- **Coverage is declared where it is proven.** A check names what it covers in a comment line,
  `// e2e-covers: panel /scripts, hub /viz`. A sub-page (`/resource/<path>`, `/scripts/<name>`)
  counts for the page it belongs to.
- **Enforced.** `tools/research/e2e-coverage` fails on any of these:
  - a shell page no check declares;
  - a declaration in a file that drives no browser;
  - a declared page the shell does not have.

  `tools/gate.sh change` and CI (`surfaces.yml`) run it on every change, and the panel's node tests
  run it too (`test/e2e-coverage.test.mjs`). The pre-commit line is the owner's to add (rule
  `project.docs-are-derived-and-current`, "Enforced by").
- **Run, not only declared.** A change to a page reruns the checks that cover it against a live
  stack, and the report names each run and its result (rule
  `project.never-report-unrun-validation-as-pass`). A check that could not run is NOT RUN, never
  PASS.

# Checks today

| Check | Covers |
|---|---|
| `tools/research/mcp-overview-check` | panel `/`, `/world`, `/data`, `/results`, `/result/<name>`, `/tool/<name>` |
| `tools/research/mcp-pages-check` | panel `/ui`, `/tools`, `/resource/<path>`, `/docs`; hub `/docs` |
| `tools/research/mcp-scripts-check` | panel `/scripts`, `/scripts/<name>` (editor, save, run, service, hot reload) |
| `tools/research/mcp-hub-page-check` | hub `/` |
| `tools/research/mcp-viz-check` | hub `/viz` |

`tools/research/e2e-coverage` prints the live version of this table.

# Failure behaviour

A page without a declared, browser-driving check fails the gate and blocks the change. A report that
claims a page works without naming the check run against a live stack is a validation claim without
evidence.

# Related

Rules `project.panel-items-are-routable-pages` and `project.never-report-unrun-validation-as-pass`;
`research/mcp-control.md`, `research/rail-steward.md`.
