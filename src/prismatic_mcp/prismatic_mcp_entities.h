/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_entities.h Dynamic entities of the world as data for the embedded MCP server. */

#ifndef PRISMATIC_MCP_ENTITIES_H
#define PRISMATIC_MCP_ENTITIES_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

namespace PrismaticMCP::Entities {

/**
 * Every primary vehicle (the front of a train, a road vehicle, a ship, an aircraft) with its id,
 * unit number, type, owner, engine, tile, stopped/in-depot/crashed state, cargo, profits, current
 * order and order list (type and destination). Read-only. Main thread only.
 * @return {"vehicles": [...]}.
 */
nlohmann::json Vehicles();

} // namespace PrismaticMCP::Entities

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_ENTITIES_H */
