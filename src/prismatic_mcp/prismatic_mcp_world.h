/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_world.h Read-only world resources of the embedded MCP server: the whole map and the stations. */

#ifndef PRISMATIC_MCP_WORLD_H
#define PRISMATIC_MCP_WORLD_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

namespace PrismaticMCP::World {

/**
 * The whole map, compactly: one string per row for tile types, heights and owners, with the
 * encoding stated in the answer so a reader needs no other document. Read-only, main thread.
 * @return The map payload (the caller adds the resource envelope).
 */
nlohmann::json Map();

/**
 * Every station: id, name, tile, owner and facilities. Read-only, main thread.
 * @return The stations payload (the caller adds the resource envelope).
 */
nlohmann::json Stations();

/**
 * Every depot: id, tile, x/y, owner, type (road, rail, ship), town and, for road and rail, the
 * entrance tile; plus `service_reach_tiles`, how far a vehicle looks for a depot to be serviced at
 * (pf.yapf.maximum_go_to_depot_penalty in tiles). Read-only. Main thread only.
 * @return The depots payload (the caller adds the resource envelope).
 */
nlohmann::json Depots();

/** Every engine some company can buy now: speed, capacity, cargo, price, running cost, plane type, who may buy it. */
nlohmann::json Engines();

} // namespace PrismaticMCP::World

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_WORLD_H */
