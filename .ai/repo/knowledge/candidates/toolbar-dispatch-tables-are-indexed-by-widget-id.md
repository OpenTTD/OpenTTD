---
schema: knowledge/v1
id: toolbar-dispatch-tables-are-indexed-by-widget-id
kind: knowledge
class: constraint
title: "The main toolbar's click and menu tables and its tooltips are indexed by widget id, so a new WID_TN_* value must be added at the same position everywhere"
description: "When adding, removing or reordering a main or scenario-editor toolbar button."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [game-windows, gui-and-console]
about: src/toolbar_gui.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/toolbar_gui.cpp
    - file:src/widgets/toolbar_widget.h
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/game-windows/README.md
---

# Statement

`src/toolbar_gui.cpp:MainToolbarWindow` calls `_toolbar_button_procs[widget]` on a click and
`_menu_clicked_procs[widget]` on a dropdown result, and `src/toolbar_gui.cpp:MakeMainToolbar` gives
each button the tooltip `STR_TOOLBAR_TOOLTIP_PAUSE_GAME + widget`. The tables are plain arrays in
the order of `src/widgets/toolbar_widget.h:ToolbarNormalWidgets` with no `static_assert`, so a value
inserted out of step compiles and silently runs another button's handler and shows another tooltip.
The scenario-editor tables (`_scen_toolbar_button_procs`, `_scen_toolbar_dropdown_procs`) work the
same way.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote toolbar-dispatch-tables-are-indexed-by-widget-id` with evidence, or `majordomus knowledge reject toolbar-dispatch-tables-are-indexed-by-widget-id --reason ...`.
