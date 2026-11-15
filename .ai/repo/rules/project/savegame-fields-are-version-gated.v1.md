---
id: project.savegame-fields-are-version-gated
version: 1
kind: rule
title: Saved fields are never removed, reordered or renumbered; new ones are version-gated
description: Every revision loads every older savegame. A field that goes away keeps a version-gated load-and-discard path; a new field is added behind a SaveLoadVersion check.
statement: Never remove, reorder or renumber an existing saved field or chunk; give an obsolete field a version-gated load-and-discard path and add a new field behind a SaveLoadVersion check.
status: active
class: blocking
depends_on: []
tags: [savegame, compatibility, substrate]
---

# Rationale

Savegame compatibility is one of the real upstream constraints that make OpenTTD a meaningful
substrate (ADR 0001). It is also why Prismatic's migration-rollback ban was rejected when
principles were adapted (ADR 0003): here, old data must keep loading forward.

# Required behaviour

Research state (the MCP server's sessions and approvals, for one) is process-local and never
saved. A change that has to be saved follows `src/saveload/` conventions and
`docs/savegame_format.md`.

# Failure behaviour

A change that breaks loading an older savegame blocks.

# Related

ADR 0003, ADR 0008, `docs/savegame_format.md`.
