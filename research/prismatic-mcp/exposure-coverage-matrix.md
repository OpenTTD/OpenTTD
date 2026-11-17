# Exposure Coverage Matrix

Honest coverage of internal domains using the mission's status vocabulary. What is not adapted
is catalogued as designed-but-not-yet-adapted, not pretended-covered.

| Domain | Status | Notes |
|---|---|---|
| MCP runtime status (`openttd://mcp/status`) | `EXPOSED_READ_ONLY` | running/endpoint/port/counters. |
| Build & runtime meta (`openttd://meta/build`) | `EXPOSED_READ_ONLY` | version, revision hash, NewGRF version, dedicated flag, gates. |
| Game state (`openttd://game/state`) | `EXPOSED_READ_ONLY` | mode, pause, map size, object counts. |
| Companies (`openttd://companies`) | `EXPOSED_READ_ONLY` | id/name/is_ai/money/inaugurated. Server-visible; see privacy note. |
| Vehicles / towns / industries / stations | `EXPOSED_READ_ONLY` | `openttd://vehicles`, `openttd://towns`, `openttd://stations`, `openttd://depots`, `openttd://engines`; industries only as nodes of `openttd://world/graph`. Unpaginated. |
| Map tiles / regions | `EXPOSED_READ_ONLY` | `openttd://map`, `openttd://map/terrain`, `openttd://map/ways`: the whole map, one string per row, encoding included. No area cap. |
| AI / GameScript state & logs | `EXPOSED_READ_ONLY` (state) / `NOT_YET_ADAPTED` (logs) | `openttd://ai`: every company's AI, its declared settings and live values. Script logs are not exposed. |
| Native UI windows / viewport | `EXPOSED_READ_ONLY` / local GUI tools | `openttd.ui.*`, `openttd.input.*`, `openttd.view.*` tools and `openttd://ui/menu`, `openttd://ui/requests`; they change only the local window and viewport, never game state. |
| Debug / performance / pools | `NOT_YET_ADAPTED` | expensive diagnostics must be opt-in/bounded (Phase E). |
| Settings schema | `NOT_YET_ADAPTED` | game settings are not exposed; AI settings are, through `openttd://ai`. Secrets omitted/redacted when added. |
| Network clients (IP, keys, names) | `WITHHELD_PRIVACY` | redacted-by-default resource; not exposed in this build. |
| Admin password / client_secret_key / X25519 keys | `WITHHELD_SECRET` | never serialized. |
| Bearer token | `WITHHELD_SECRET` | only via explicit `prismatic_mcp token`; never in cfg/logs/snapshots. |
| Raw pointers / process memory / filesystem | `NOT_APPLICABLE` | never serialized by design (forbidden-shortcuts list). |
| Game-state mutations (pause/AI/vehicle/commands) | `EXPOSED_APPROVAL_REQUIRED` | `openttd.game.set_pause`, `openttd.cmd.<Name>` (one per exposable game command), `openttd.ai.set_setting/start/stop/reload`, and `openttd.api.*` with `execute: true`; all through the approval gate (`16-decision-contract.md`). |
| Server-admin ops | `EXPOSED_SERVER_ONLY` (target) | staged; server-authority + audit required. |
| Determinism-sensitive internals (RNG, tick order) | `WITHHELD_DETERMINISM` | never mutated by reads; token uses OS CSPRNG, not game RNG. |

Invariant honored: *everything discoverable; everything safe observable; only adapted operations
executable; nothing sensitive leaks implicitly*. The live catalogue is the measure, not this
table: `resources/list` and `tools/list` on a running server, or `openttd://mcp/coverage`, which
lists every published tool and resource with its call count.

Status update (2026-11-17): when this matrix was written only four read-only domains were
observable and no operation was executable. The rows above now reflect the resources in
`RESOURCES` (`src/prismatic_mcp/prismatic_mcp_server.cpp`) and the mutations in `ToolsList()`.
