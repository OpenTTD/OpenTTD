/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_graph.h Road and rail ways per tile, and the world as one typed graph, for the embedded MCP server. */

#ifndef PRISMATIC_MCP_GRAPH_H
#define PRISMATIC_MCP_GRAPH_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

namespace PrismaticMCP::Graph {

/**
 * Per tile, the directions a road vehicle can leave it (road bits, stops, depots, crossings,
 * road bridge and tunnel heads) and the rail tracks it has, one string per row with its encoding.
 * Read-only. Main thread only.
 * @return {"width", "height", "encoding", "roads": [...], "rails": [...]}.
 */
nlohmann::json Ways();

/**
 * The world as one typed graph (schema world-graph/v1): town, industry, station and vehicle nodes;
 * near_town, serves_town, order (with seq) and cargo_link (capacity, usage per cargo) edges.
 * Read-only. Main thread only.
 * @return {"schema", "nodes": [...], "edges": [...]}.
 */
nlohmann::json World();

} // namespace PrismaticMCP::Graph

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_GRAPH_H */
