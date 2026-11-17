/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_api.h The Script API as MCP tools: every static function of the Game Script API, derived from its registration. */

#ifndef PRISMATIC_MCP_API_H
#define PRISMATIC_MCP_API_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

#include <optional>
#include <string>

namespace PrismaticMCP::Api {

/**
 * One tool per static function of the Game Script API, `openttd.api.Script<Class>.<Function>`,
 * read from a private Game Script engine with the whole API registered: the classes and functions
 * are the engine's own, the parameter types its parameter checks. Nothing here names a function.
 * @return An array of MCP tool descriptors (name, description, inputSchema).
 */
nlohmann::json Tools();

/**
 * Run one Script API function, main thread only. Without `execute` it runs in GSTestMode: a
 * reading function answers as in a script, a function that issues a command answers its estimate
 * (whether it would succeed, the cost, the error) and changes nothing, so no approval is needed.
 * @param name The tool name, openttd.api.Script<Class>.<Function>.
 * @param args {args: [positional arguments], company?: id to act as (GSCompanyMode), execute?: bool}.
 * @return The answer, or std::nullopt when the name is not a Script API tool.
 */
std::optional<nlohmann::json> Call(const std::string &name, const nlohmann::json &args);

/**
 * Whether a call asks to execute (`execute: true`); such a call goes through the approval gate as
 * action `api.Script<Class>.<Function>` and runs through Execute() once approved.
 */
bool WantsExecution(const std::string &name, const nlohmann::json &args);

/** Validate an `api.*` action: the function exists and its estimate succeeds. */
nlohmann::json Validate(const std::string &type, const nlohmann::json &args, nlohmann::json &errors);

/** Execute an approved `api.*` action in GSAsyncMode: the command is posted, its answer returned. */
nlohmann::json Execute(const std::string &type, const nlohmann::json &args);

} // namespace PrismaticMCP::Api

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_API_H */
