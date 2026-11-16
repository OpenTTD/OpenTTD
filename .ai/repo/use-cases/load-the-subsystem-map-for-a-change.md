---
id: load-the-subsystem-map-for-a-change
kind: use-case
title: 'Load the map of exactly the subsystem a change touches'
summary: 'Resolve a source path to its subsystem map, so a worker reads the entry points, invariants, traps and tests of what it edits and nothing else.'
category: policy
status: active
target: advisory
weight: 20
actors: [agent, contributor]
difficulty: basic
commands: [context]
applications: [govern-a-research-fork-of-a-large-codebase]
---

# Situation

A worker about to add an MCP tool that sends a vehicle to its depot needs the MCP server's
registration and approval boundary, the vehicle subsystem's command ownership, and, if the tool
stores anything, the savegame rules. It needs none of the other maps. Loading the whole `.ai/` tree
spends the context budget; guessing misses a constraint.

# What you run

- `context resolve src/prismatic_mcp/prismatic_mcp_server.cpp`: the fork's MCP map, not the network map
- `context resolve src/vehicle_cmd.cpp`: the vehicles map and the commands map, which both track it
- `context resolve src/saveload/vehicle_sl.cpp`: the save/load map

# Scenario

```yaml
# majordomus-skill: load-focused-context-for-a-change
mode: live
given:
  - 'the substrate maps under .ai/repo/workflows/substrate/ tracked'
steps:
  - id: mcp-server
    run: ['context', 'resolve', 'src/prismatic_mcp/prismatic_mcp_server.cpp']
    note: 'fork code resolves to the fork map, on top of the substrate document'
    expect:
      exit: 0
      stdout_contains: ['fork.substrate', 'fork.substrate.prismatic-mcp']
      stdout_not_contains: ['fork.substrate.network', 'fork.substrate.saveload']
  - id: vehicle-command
    run: ['context', 'resolve', 'src/vehicle_cmd.cpp']
    note: 'a command handler file is owned by its subsystem and by the command system'
    expect:
      exit: 0
      stdout_contains: ['fork.substrate.vehicles-and-pathfinding', 'fork.substrate.commands']
      stdout_not_contains: ['fork.substrate.prismatic-mcp']
  - id: savegame-chunk
    run: ['context', 'resolve', 'src/saveload/vehicle_sl.cpp']
    note: 'a chunk handler resolves to the save/load map'
    expect:
      exit: 0
      stdout_contains: ['fork.substrate.saveload']
      stdout_not_contains: ['fork.substrate.network']
then:
  - 'each path selects the maps of the subsystems that own it and no other'
  - 'the substrate document, with the rules that bind all of src/, is always in the chain'
```

# Outcome

The worker reads two or three maps of about a hundred lines each instead of the subsystem or the
whole layer, and every symbol those maps cite was checked against the tree by `tools/knowledge/refs check`.
