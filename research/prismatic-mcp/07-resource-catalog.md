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
| `openttd://map/terrain` | width, height, encoding; per row (y) strings, a character per tile (x): `slopes` (2 hex: Slope bits, steep 0x10), `ground` (g grass, b bare, r rough, k rocks, f fields, s snow, d desert; trees G R D C; water w n v c; `-` built), `gradients` (largest height step to a neighbour), `buildable` (`.` `t` `w` `x`), `vehicles` (`.` 1-9 `+`); `row_digests` (FNV-1a per row) | `GetTileSlope`, `GetClearGround`, `GetTreeGround`, `GetWaterClass`, `TileHeight`, `Vehicle::Iterate()` (I0073) |
| `openttd://economy/prices` | `prices` {name: money now}, `order` (enum order), `base` {name: start price}, `inflation`, `inflation_factor`, `construction_cost`, `unit` | `_price` (inflated, setting and NewGRF multipliers applied), `_price_base_specs`; names generated from `enum class Price` with a `static_assert` (I0073) |
| `openttd://bridges` | `types` [{type, name, available, avail_year, min_length, max_length, spec_max_length, price, speed}], `max_bridge_length`, `max_tunnel_length`, the cost formula | `GetBridgeSpec`, `CheckBridgeAvailability`'s rules (I0073) |

The table names some resources; the live list, with every description, is `resources/list` on a
running server (and the hub's catalogue across every instance).

Rules honored: unknown → `null`/explicit availability (never coerced to 0); invalid uri → JSON-RPC
`-32602`; lists sorted by stable id (pool ascending index); money states its unit; date states
calendar Y-M-D; no memory addresses; no secrets. See `exposure-coverage-matrix.md` for the domains
catalogued but not yet adapted (vehicles, towns, map regions, network-redacted, UI, debug, settings).
