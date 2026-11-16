---
schema: knowledge/v1
id: link-graph-capacity-counts-trips-not-vehicles
kind: knowledge
class: fact
title: "A link's capacity in the game's link graph is the capacity of the trips run over it, not of the vehicles assigned to it"
description: "Before judging a route's fleet from cargo_link capacity and usage (openttd://world/graph), or sizing a fleet from it."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [linkgraph, economy, prismatic-mcp, planning]
about: src/linkgraph/linkgraph.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/linkgraph/linkgraph.cpp
    - file:src/station_cmd.cpp
    - file:src/prismatic_mcp/prismatic_mcp_graph.cpp
    - commit:4a24cf81a8f056ad5a304e034401322405fe1266
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/economy-and-companies/README.md
  - type: relates_to
    target: file:research/mcp-hub.md
---

# Statement

`src/station_cmd.cpp:IncreaseStats` adds, every time a vehicle leaves a station for the next one,
the refit capacity of every vehicle in the consist (wagons included) to the link, and
`src/linkgraph/linkgraph.cpp:LinkGraph::BaseEdge::Update` sums it in its Increase mode. A link's
`capacity` in `openttd://world/graph` (`src/prismatic_mcp/prismatic_mcp_graph.cpp:World`) therefore
counts trips actually run in the measuring period.

# Why it matters

A new route with four buses, only one of which has run the link, shows the capacity of one bus.
Dividing that by the four assigned buses invents three idle buses; the first live run of the hub's
capacity audit did exactly that (commit 4a24cf81a8 replaced it). Judge a link only once its capacity
covers every assigned vehicle (mature), and compute the vehicles needed as
max(1, ceil(usage / one vehicle's capacity)).
