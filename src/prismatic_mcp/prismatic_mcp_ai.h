/*
 * Part of korczis's PRIVATE RESEARCH FORK of OpenTTD. RESEARCH-ONLY / UNSTABLE.
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 */

/**
 * @file prismatic_mcp_ai.h The company AIs over MCP: their settings, read live and changed live.
 *
 * Everything is read from the AI's own configuration (AIConfig and the AIInfo behind it):
 * each setting's name, description, range, step, flags and labels are what the script's
 * info.nut declared, and its value is what AIController.GetSetting returns to the running
 * script. Changing a setting writes the same configuration the script reads on its next
 * GetSetting. Starting, stopping and reloading an AI go through Commands::CompanyControl,
 * the path the start_ai, stop_ai and reload_ai console commands take.
 */

#ifndef PRISMATIC_MCP_AI_H
#define PRISMATIC_MCP_AI_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace PrismaticMCP::ai {

using json = nlohmann::json;

/** What executing an AI action started. */
struct Posted {
	bool ok = false;            ///< The action was carried out or queued.
	int callbacks = 0;          ///< Command results still to arrive through CcPrismaticMcp.
	std::function<bool()> observe; ///< Whether the intended effect is observed, asked when the last result arrived.
	std::string error;          ///< Why the action was refused.
};

/** @return Whether the action type is one of ai.set_setting, ai.start, ai.stop, ai.reload. */
bool IsAiAction(std::string_view type);
/** openttd://ai: every company's AI with its settings, and the AIs this game can start. */
json Resource();
/**
 * Validate an AI action without changing anything.
 * @param[out] errors Every reason the action is invalid.
 * @return Details for the caller (the setting's range, the AI found, ...).
 */
json Validate(std::string_view type, const json &args, json &errors);
/**
 * Carry out a validated, approved AI action. Main thread.
 * @param expect_callback Called once per command about to be posted, before it is posted.
 */
Posted Execute(std::string_view type, const json &args, const std::function<void()> &expect_callback);
/** @return The MCP tool entries: openttd.ai.list and the four gated actions. */
json Tools();

} // namespace PrismaticMCP::ai

#endif /* OTTD_PRISMATIC_MCP_SERVER */
#endif /* PRISMATIC_MCP_AI_H */
