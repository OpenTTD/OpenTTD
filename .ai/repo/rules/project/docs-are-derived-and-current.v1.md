---
id: project.docs-are-derived-and-current
version: 1
kind: rule
title: Everything published is derived, inferred, dynamic and generic, kept current by the gates, and every diagram is Mermaid that renders
description: Documentation, tables, counts, charts and diagrams are generated from the sources or proven against them on every commit. Nothing a reader sees is typed where data can give it, nothing names an instance where a rule can find it, every diagram is a Mermaid block that renders on GitHub, the wiki and the site, and a gate fails the change that lets any of it fall behind.
statement: Derive every number, table, list and chart from the sources; infer what can be inferred instead of declaring it; keep code generic (no instance named where data can give it); draw every Markdown diagram in Mermaid; and keep the gates that prove this, surfaces check, e2e-coverage, readme-scoreboard and diagrams-check, passing on every change.
status: active
class: blocking
depends_on: [project.published-surfaces-are-derived@1, project.mcp-surfaces-are-derived@1, project.web-ui-is-covered-end-to-end@1, project.never-report-unrun-validation-as-pass@1]
tags: [docs, surfaces, derived, mermaid, enforcement]
---

# Rationale

The owner's order (2026-11-18): "everything must be derived, inferred, dynamic, generic, and this
must always be enforced through .ai / Majordomus"; "update all graphs, all docs, all Markdown, all
illustrations, so nothing is obsolete: that must not happen"; "replace the ugly in-place ASCII charts
with Mermaid".

A restated fact is a second source of truth, wrong the day the first one moves. A diagram drawn in
characters cannot be maximized, does not follow the reader's theme and looks broken on a phone.
Advice does not keep either from coming back. A failing check does.

# Required behaviour

- **Derived.** A count, list, table or chart a document shows is generated from the sources:
  - a region between `surfaces:begin`/`surfaces:end` markers (`tools/surfaces/surfaces sync`; the
    `facts` region counts what `surfaces.toml` declares);
  - a tool's own output (`tools/research/readme-scoreboard`);
  - a page rendered from data at build time (the site's charts).

  Prose explains; it never carries a number the data gives.
- **Inferred, not declared.** Code reads what the game, git or the registry already states. Pages
  come from the shell's registry, routes from `routes()`, settings from the AI's `info.nut`, line
  geometry from the bot's records. No second list is kept beside the first.
- **Dynamic and generic.** Nothing names an instance, a line, a station, a port or an engine where
  a rule can find it. A tool takes its target as an argument or discovers it (the registry,
  `majordomus serve ensure`).
- **Mermaid diagrams.** A diagram in Markdown is a ```mermaid block. GitHub and the wiki render it
  natively. The site renders it in its own palette, and a reader can maximize it and press Escape
  to restore it (`tools/surfaces/assets/app.js`). A directory listing (`├──`, `└──`) is a list of
  paths and stays text.
- **Current.** A change that moves what a document describes updates the document in the same
  change. `tools/knowledge/refs impact` names the records that cite what a change touches; read and
  update them.

# Enforced by

| Gate | What fails | Wired |
|---|---|---|
| `tools/surfaces/surfaces check` | a stale region, a link or anchor to nothing, a tool flag the docs and the usage disagree on, front matter that does not parse, **a diagram drawn in characters** (`check_diagrams`) | pre-commit (`.ai/repo/policy.yaml` `surfaces-on-commit`), `tools/gate.sh change`, CI `surfaces.yml` |
| `tools/research/readme-scoreboard --check` | the README scoreboard behind `research/results` | pre-commit (`scoreboard-on-commit`), `tools/gate.sh change` |
| `tools/research/e2e-coverage` | a web UI page no browser check drives | `tools/gate.sh change`, CI `surfaces.yml`; the pre-commit hook line is the owner's to add (below) |
| `tools/surfaces/diagrams-check` | a Mermaid block that does not render to an SVG in Chrome | CI `surfaces.yml`, after the site build |
| `tools/knowledge/refs check` | a knowledge record citing a path that is gone | `tools/gate.sh change` |
| `python3 -m unittest tools/research/test_surfaces_diagrams.py` | the diagram detector missing a drawing or flagging code | `tools/gate.sh change` |

`majordomus doctor` reconciles every `enforcement` entry of the policy: the tool resolves, is
executable and its exit code reaches the hook.

The pre-commit hook lives in `.git/hooks/`, outside the repository, and agents do not edit it. To
add `e2e-coverage` to every commit, the owner appends this line to `.git/hooks/pre-commit`. The
guard keeps a checkout without the script unaffected. Then add a policy entry
`e2e-coverage-on-commit` (path `tools/research/e2e-coverage`, `wired_by: git-hook:pre-commit`) so
that `majordomus doctor` proves the wiring:

```sh
if [ -x tools/research/e2e-coverage ]; then tools/research/e2e-coverage || exit $?; fi
```

# Failure behaviour

Any gate above failing blocks the commit or the change. A document that restates what a region
could derive is a review finding, and fixing it means generating it.

# Related

Rules `project.published-surfaces-are-derived`, `project.mcp-surfaces-are-derived` and
`project.web-ui-is-covered-end-to-end`; `tools/surfaces/README.md`;
[`research/concepts/derived-not-restated.md`](../../../../research/concepts/derived-not-restated.md).
