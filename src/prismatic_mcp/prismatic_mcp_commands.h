/*
 * Part of korczis's PRIVATE RESEARCH FORK of OpenTTD. RESEARCH-ONLY / UNSTABLE.
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 */

/**
 * @file prismatic_mcp_commands.h The command catalogue the MCP server derives from the command table.
 *
 * Every entry of the Commands enum is described from its CommandTraits: its name, category,
 * flags and the JSON Schema of each parameter. A command whose parameters all convert from
 * JSON is exposable: it can be test-run without side effects and posted through the normal,
 * network-safe command path on behalf of a company. Its real result arrives later through
 * CcPrismaticMcp, which is registered in the network callback table.
 */

#ifndef PRISMATIC_MCP_COMMANDS_H
#define PRISMATIC_MCP_COMMANDS_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"
#include "../command_type.h"
#include "../company_type.h"
#include "../tile_type.h"

#include <optional>
#include <string>
#include <string_view>

namespace PrismaticMCP::cmds {

using json = nlohmann::json;

/** Who a command is posted as. */
struct Actor {
	bool keep_current = true;   ///< Post as the server's own current company (server-only commands).
	CompanyID company = COMPANY_SPECTATOR;
};

/** Result of a side-effect-free test run. */
struct TestResult {
	bool params_ok = false;     ///< The JSON parameters converted.
	bool would_succeed = false; ///< The command's test phase succeeded.
	int64_t cost = 0;           ///< Estimated cost in internal currency.
	std::string error;          ///< Conversion or command error text.
};

/** @return Number of entries in the Commands enum. */
size_t Count();
/** @return The command's short name (its Cmd* function without the prefix). */
std::string_view ShortName(Commands cmd);
/** @return The command with that short name, if there is one. */
std::optional<Commands> Find(std::string_view short_name);
/** @return Whether every parameter of the command converts from JSON. */
bool IsExposable(Commands cmd);
/** @return Whether the command needs an explicit "location" (CommandFlag::Location). */
bool NeedsLocation(Commands cmd);
/** @return One catalogue entry: name, category, flags, params (schemas), exposable, unsupported types. */
json Describe(Commands cmd);
/** @return The JSON Schema of the MCP tool arguments for the command. */
json ToolInputSchema(Commands cmd);
/** Test-run the command without executing it. Main thread only. */
TestResult Test(Commands cmd, const Actor &actor, const json &params);
/**
 * Post the command through the network-safe path with CcPrismaticMcp as callback.
 * @return true when the command was accepted for execution; its result arrives in the callback.
 */
bool Post(Commands cmd, const Actor &actor, const json &params, TileIndex location, std::string &error);
/** @return The text of a command's error, or an empty string. */
std::string ErrorText(const CommandCost &cost);

} // namespace PrismaticMCP::cmds

namespace PrismaticMCP {
/** The executed result of a command this server posted, in posting order. Defined by the server. */
void OnCommandResult(Commands cmd, const CommandCost &result);
} // namespace PrismaticMCP

/** Network-registered callback: the executed result of a command the MCP server posted. */
void CcPrismaticMcp(Commands cmd, const CommandCost &result, const CommandDataBuffer &data, CommandDataBuffer result_data);

#endif /* OTTD_PRISMATIC_MCP_SERVER */
#endif /* PRISMATIC_MCP_COMMANDS_H */
