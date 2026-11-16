/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 *
 * The MCP catalogue as console commands. Everything the console offers is read from the same
 * tools/list and resources/list the HTTP server answers: tool and resource names, descriptions,
 * parameter names, types, enums and required keys. The helpers in Console::detail are pure (no
 * game state) and unit-tested in src/tests/test_prismatic_mcp.cpp.
 */

/** @file prismatic_mcp_console.h The `mcp` console command family, derived from the MCP catalogue. */

#ifndef PRISMATIC_MCP_CONSOLE_H
#define PRISMATIC_MCP_CONSOLE_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PrismaticMCP {

/**
 * Answer one JSON-RPC request in process, through the dispatch the HTTP server uses (coverage
 * counters, approvals and input arming included). Main thread only.
 * @param request A JSON-RPC 2.0 request object.
 * @return The response object, or null for a notification.
 */
nlohmann::json Dispatch(const nlohmann::json &request);

namespace Console::detail {

/**
 * Resolve what a person typed to one catalogue name: the exact name, the name with the
 * `openttd.` prefix (or `openttd://` for resources) added, or the only name that ends with
 * `.<query>` or `/<query>`. Otherwise the names containing the query, as suggestions.
 * @param names The catalogue's names, in its order.
 * @param query What was typed.
 * @return One name when resolved; none or several (the candidates) otherwise.
 */
std::vector<std::string> Resolve(const std::vector<std::string> &names, std::string_view query);

/**
 * Type one console token by a JSON Schema property.
 * @param prop The property schema (may be empty: then JSON if it parses, else a string).
 * @param text The token text.
 * @param[out] error Why the token does not fit, when it does not.
 * @return The typed value; null JSON with a non-empty error when it does not fit.
 */
nlohmann::json ParseValue(const nlohmann::json &prop, std::string_view text, std::string &error);

/**
 * Build a tool's arguments from console tokens: `key=value` pairs typed by the schema, one
 * JSON object token merged as is, and bare tokens bound to the required keys in order.
 * @param schema The tool's inputSchema.
 * @param tokens The tokens after the tool name.
 * @param[out] errors Every unknown key, missing required key and mistyped value.
 * @return The arguments object; meaningful only when errors is empty.
 */
nlohmann::json ParseArgs(const nlohmann::json &schema, std::span<const std::string_view> tokens, std::vector<std::string> &errors);

/**
 * One line naming a tool's parameters as the console takes them, derived from its inputSchema.
 * @param name The tool's name.
 * @param schema The tool's inputSchema.
 * @return e.g. `mcp openttd.view.zoom level=<integer>`.
 */
std::string Usage(std::string_view name, const nlohmann::json &schema);

/**
 * The text a tools/call or resources/read answer carries, as display lines: an embedded JSON
 * text is pretty-printed, anything else is split on new lines.
 * @param result The JSON-RPC result object.
 * @param compact One line per JSON value instead of an indented dump.
 * @return The lines.
 */
std::vector<std::string> ResultLines(const nlohmann::json &result, bool compact);

} // namespace Console::detail

} // namespace PrismaticMCP

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_CONSOLE_H */
