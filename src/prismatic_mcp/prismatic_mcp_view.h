/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_view.h View tools of the embedded MCP server: move the main viewport of a GUI process. */

#ifndef PRISMATIC_MCP_VIEW_H
#define PRISMATIC_MCP_VIEW_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

#include <optional>
#include <string>

namespace PrismaticMCP::View {

/**
 * The view tools' descriptors for tools/list.
 * @return An array of MCP tool descriptors (name, description, inputSchema).
 */
nlohmann::json Tools();

/**
 * Run a view tool. Main thread only (called from the MCP poll inside GameLoop()).
 * View tools change only the local main viewport, never game state, so they need no approval
 * and issue no command; in a dedicated server, which has no viewport, they answer an error.
 * @param name The tool name.
 * @param args The tool arguments.
 * @return The tool's JSON answer, or std::nullopt when \a name is not a view tool.
 */
std::optional<nlohmann::json> Call(const std::string &name, const nlohmann::json &args);

} // namespace PrismaticMCP::View

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_VIEW_H */
