# 07 — Resource Catalog

Resources implemented in this build. All are read-only, wrapped in the common envelope
(`schema_version`, `source`, `source_revision`, `protocol_version`, `game_tick`, `game_date`,
`game_mode`, `availability`, `redactions`, `truncated`, `next_cursor`, `data`) built by
`MakeEnvelope()` in `prismatic_mcp_server.cpp`. Order in `resources/list` is fixed (deterministic).

| URI | `data` payload | Source |
|---|---|---|
| `openttd://mcp/status` | running, endpoint, bind, port, read_only, requests_total, requests_denied, active_connections | server state |
| `openttd://meta/build` | openttd_version, revision_hash, newgrf_version, research_instrumentation, mcp_server, dedicated | `rev.h` globals |
| `openttd://game/state` | paused, map_width, map_height, num_companies, num_vehicles, num_towns | `_pause_mode`, `Map`, pool counts |
| `openttd://companies` | array of {id, name, is_ai, money, money_unit, inaugurated_year} | `Company::Iterate()` |

Rules honored: unknown → `null`/explicit availability (never coerced to 0); invalid uri → JSON-RPC
`-32602`; lists sorted by stable id (pool ascending index); money states its unit; date states
calendar Y-M-D; no memory addresses; no secrets. See `exposure-coverage-matrix.md` for the domains
catalogued but not yet adapted (vehicles, towns, map regions, network-redacted, UI, debug, settings).
