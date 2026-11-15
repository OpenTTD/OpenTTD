---
id: project.published-surfaces-are-derived
version: 1
kind: rule
title: Every published surface is derived, and a stale region or a dangling link blocks the change
description: The site, the wiki and the generated Markdown regions are produced by tools/surfaces from tracked Markdown, git and the .ai/ layer. Nothing published is hand-written, and surfaces check passes before a change lands.
statement: Publish documentation only through tools/surfaces, edit the source document and never a generated region, page or wiki entry, and keep tools/surfaces/surfaces check passing (no stale region, no relative link or anchor to nothing).
status: active
class: blocking
depends_on: []
tags: [documentation, publication, derivation]
---

# Rationale

A published copy that a person keeps in step drifts. The Zola landing page this rule
replaces (ADR 0009, superseded by ADR 0012) called the repository private while GitHub
reported it public. Derivation removes the second copy, and the check turns any remaining
drift into a failure with a file and a line.

# Required behaviour

- A new fact about the repository goes into the document or the `.ai/` record that owns
  it. The site, the wiki and every `surfaces:begin`/`surfaces:end` region pick it up on
  the next build or `tools/surfaces/surfaces sync`.
- The text between region markers is never edited by hand. Change the generator, its
  arguments, or the source it reads.
- A relative link points at a tracked file or directory, and an anchor names a heading or
  an explicit anchor in the target. Documents carried unmodified from upstream are
  exempted by path in `surfaces.toml` (`[check] exclude`), each with its reason.
- The generator stays generic. It names no repository, and every repository-specific
  choice lives in `surfaces.toml`.

# Failure behaviour

`tools/surfaces/surfaces check` exits 10 and names each finding as `FAIL <kind>
<path>:<line> — <message>`. It runs, and blocks, in three places:

- the pre-commit hook (policy `enforcement`, entry `surfaces-on-commit`, reconciled by
  `majordomus doctor`);
- `tools/gate.sh change`;
- `.github/workflows/surfaces.yml`, which publishes nothing from a failing run.

# Related

ADR 0012, `tools/surfaces/README.md`, `.github/workflows/surfaces.yml`, `surfaces.toml`.
