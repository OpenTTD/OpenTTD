/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_coverage.h What an MCP server's clients have actually exercised, counted by the dispatch itself. */

#ifndef PRISMATIC_MCP_COVERAGE_H
#define PRISMATIC_MCP_COVERAGE_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

#include <string>

namespace PrismaticMCP::Coverage {

/**
 * Count one call of a tool or one read of a resource, as the dispatch receives it.
 * @param kind "tool" or "resource".
 * @param name The tool name or the resource URI.
 */
void Count(const char *kind, const std::string &name);

/**
 * Every tool and resource this server publishes (tools/list, resources/list), each with how often
 * it was called since the server started, and the lists of those never called. The catalogue is
 * passed in, so coverage is measured against what the server publishes right now, not a list.
 * @param tools The tools/list answer ({"tools": [...]}).
 * @param resources The resources/list answer ({"resources": [...]}).
 * @return {"tools": {name: calls}, "resources": {uri: calls}, "uncovered": {"tools": [...], "resources": [...]}, "covered": n, "total": n}.
 */
nlohmann::json Report(const nlohmann::json &tools, const nlohmann::json &resources);

} // namespace PrismaticMCP::Coverage

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_COVERAGE_H */
