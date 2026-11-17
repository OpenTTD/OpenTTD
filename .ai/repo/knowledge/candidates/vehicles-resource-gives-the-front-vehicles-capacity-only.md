---
schema: knowledge/v1
id: vehicles-resource-gives-the-front-vehicles-capacity-only
kind: knowledge
class: fact
title: "openttd://vehicles reports the front vehicle's cargo capacity only, so a train led by an engine shows 0"
description: "Before computing a fleet's or a route's capacity from openttd://vehicles."
status: superseded
epistemics: observed
date: 2026-11-16
tags: [prismatic-mcp, vehicles, planning]
about: src/prismatic_mcp/prismatic_mcp_entities.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/prismatic_mcp/prismatic_mcp_entities.cpp
    - file:src/station_cmd.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/prismatic-mcp/README.md
---

# Statement

`src/prismatic_mcp/prismatic_mcp_entities.cpp:Vehicles` publishes `cargo_capacity` as `v->cargo_cap`
of the primary (front) vehicle. A road vehicle or a plane carries its whole capacity itself; a train's
front is usually an engine with no cargo, so its `cargo_capacity` is 0 while its wagons carry the load.
The game itself sums every vehicle of a consist (`src/station_cmd.cpp:IncreaseStats` walks
`v->Next()`).

# What to do

For a train, sum the consist (an MCP field for the consist's total capacity would be the clean fix),
or take the capacity from the link graph; never treat a train as empty.

# Rejected

Superseded by f7d13d3c71 (2026-11-16 07:55, after this record was added at 86b8ff0723 04:41): openttd://vehicles now also publishes parts, consist_capacity and consist_count summed over v->Next() (src/prismatic_mcp/prismatic_mcp_entities.cpp:70-86 at 5778b8909c), the fix this record asked for. cargo_capacity alone is still the front vehicle's; a train no longer reads as empty.
