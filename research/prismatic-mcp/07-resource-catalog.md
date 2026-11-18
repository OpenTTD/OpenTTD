# 07 — Resource Catalog

Resources implemented in this build. All are read-only, wrapped in the common envelope
(`schema_version`, `source`, `source_revision`, `protocol_version`, `game_tick`, `game_date`,
`game_mode`, `availability`, `redactions`, `truncated`, `next_cursor`, `data`) built by
`MakeEnvelope()` in `prismatic_mcp_server.cpp`. Order in `resources/list` is fixed (deterministic).

| URI | `data` payload | Source |
|---|---|---|
| `openttd://mcp/status` | running, endpoint, bind, port, read_only, requests_total, requests_denied, active_connections | server state |
| `openttd://meta/build` | openttd_version, revision_hash, newgrf_version, research_instrumentation, mcp_server, dedicated | `rev.h` globals |
| `openttd://game/state` | paused, map_width, map_height, num_companies, num_vehicles (pool items: wagons, parts, effects), fleet (primary vehicles: train, road, ship, aircraft, total), num_towns | `_pause_mode`, `Map`, pool counts, `IsPrimaryVehicle()` |
| `openttd://companies` | array of {id, name, custom_name, is_ai, money, loan, max_loan, loan_step, company_value, money_unit, inaugurated_year, fleet {train, road, ship, aircraft, total}} | `Company::Iterate()`, primary vehicles by owner |
| `openttd://map/terrain` | width, height, encoding; per row (y) strings, a character per tile (x): `slopes` (2 hex: Slope bits, steep 0x10), `ground` (g grass, b bare, r rough, k rocks, f fields, s snow, d desert; trees G R D C; water w n v c; `-` built), `gradients` (largest height step to a neighbour), `buildable` (`.` `t` `w` `x`), `vehicles` (`.` 1-9 `+`); `row_digests` (FNV-1a per row) | `GetTileSlope`, `GetClearGround`, `GetTreeGround`, `GetWaterClass`, `TileHeight`, `Vehicle::Iterate()` (I0073) |
| `openttd://vehicles` | array of primary vehicles {id, unit, type, owner, engine, engine_id, tile, x, y, z, speed, stopped, in_depot, crashed, lost (the pathfinder found no way: `VehicleFlag::PathfinderLost`), broken_down (stopped by a breakdown now: `breakdown_ctr == 1`), cargo_capacity, cargo_count, parts, consist_capacity, consist_count, profit_this_year, profit_last_year, age, max_age, reliability, breakdowns, value (the front vehicle's), consist_value (the whole consist's), current_order, orders [{type, destination}]} | `Vehicle::Iterate()`, `IsPrimaryVehicle()`, the whole consist summed (`prismatic_mcp_entities.cpp`) |
| `openttd://engines` | array of enabled engines {id, type, name, intro_year, max_speed_kmh, effective_speed_kmh, capacity, cargo, price, running_cost_per_year, reliability_pct, buildable_by}; trains add wagon, power_hp and railtypes (labels); aircraft add plane_type | `Engine::Iterate()`, `IsEngineBuildable`, `RailVehicleInfo` (`prismatic_mcp_world.cpp`) |
| `openttd://economy/prices` | `prices` {name: money now}, `order` (enum order), `base` {name: start price}, `inflation`, `inflation_factor`, `construction_cost`, `unit` | `_price` (inflated, setting and NewGRF multipliers applied), `_price_base_specs`; names generated from `enum class Price` with a `static_assert` (I0073) |
| `openttd://railtypes` | `types` [{railtype, label, name, build_cost (RailBuildCost now), cost_multiplier, maintenance_multiplier, max_speed, curve_speed, introduction_date, powered, compatible, available_to}] | `GetRailTypeInfo`, `RailBuildCost`, `Company::avail_railtypes`; types with an empty label skipped (I0074) |
| `openttd://bridges` | `types` [{type, name, available, avail_year, min_length, max_length, spec_max_length, price, speed}], `max_bridge_length`, `max_tunnel_length`, the cost formula | `GetBridgeSpec`, `CheckBridgeAvailability`'s rules (I0073) |

The table names some resources; the live list, with every description, is `resources/list` on a
running server (and the hub's catalogue across every instance).

Rules honored: unknown → `null`/explicit availability (never coerced to 0); invalid uri → JSON-RPC
`-32602`; lists sorted by stable id (pool ascending index); money states its unit; date states
calendar Y-M-D; no memory addresses; no secrets. See `exposure-coverage-matrix.md` for the domains
catalogued but not yet adapted (network-redacted, debug, game settings).

Status update (2026-11-17): this paragraph used to list vehicles, towns, map regions and UI as not
yet adapted. They are resources now (`openttd://vehicles`, `openttd://towns`, `openttd://stations`,
`openttd://map`, `openttd://map/ways`, `openttd://ui/menu`, ...: the rows of `RESOURCES` in
`src/prismatic_mcp/prismatic_mcp_server.cpp`).
