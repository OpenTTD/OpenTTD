# 08 — Tool Catalog

**This build registers no tools.** `tools/list` returns an empty array and `tools/call` is not
implemented. This is deliberate (ADR decision 6): no tool — especially no mutating tool — is exposed
until the privilege/arming/approval machinery exists to gate it. Exposing a read-only analysis tool
first would be safe, but the mission's mutation tools must never precede the approval workflow, so the
whole tool surface is staged together behind Phases F/G/H.

## Planned tools (designed, not implemented)

**Read-only (Phase F):** `openttd.search_entities`, `openttd.get_entity`, `openttd.analyze_company`,
`openttd.analyze_vehicle`, `openttd.analyze_town`, `openttd.find_idle_vehicles`,
`openttd.export_snapshot`. Return observations + exact source entity URIs + explicit unknowns; never a
fabricated causal claim.

**Client-local UI (Phase F, privilege UI_CONTROL):** `openttd.ui.center_viewport`,
`openttd.ui.focus_entity`, `openttd.ui.open_company/vehicle/station/town`, `openttd.ui.highlight_*`,
`openttd.ui.capture_screenshot`. Main-thread, local presentation only; never touch synchronized state.

**Game mutation (Phase H, privilege GAME_MUTATION, approval-required):** `openttd.game.set_pause`,
`openttd.game.save`, `openttd.ai.start/reload/stop`, `openttd.vehicle.start_stop`,
`openttd.vehicle.send_to_depot`, `openttd.company.rename`. Each: validate mode → authorize → check
arming → validate ownership/server-authority → immutable request digest → approval → execute on main
thread via `Command<>::Post` → capture actual result → prevent duplicate execution → audit.

The read-only command *inventory* (which internal `Commands` exist, their sync/server class, and
adapter status) is the mapping in `01-openttd-system-map.md §5.4`; the future
`openttd://commands/catalog` resource surfaces it without making any raw command remotely executable.
