/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_ui.h The user interface as data and as input: windows, widgets and tiles a GUI process shows, and the pointer and keys that drive it. */

#ifndef PRISMATIC_MCP_UI_H
#define PRISMATIC_MCP_UI_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"

#include <optional>
#include <string>

namespace PrismaticMCP::UI {

/**
 * The UI tools' descriptors for tools/list: openttd.ui.* (read the interface) and
 * openttd.input.* (drive it).
 * @return An array of MCP tool descriptors (name, description, inputSchema).
 */
nlohmann::json Tools();

/**
 * Run a UI tool. Main thread only (called from the MCP poll inside GameLoop()).
 *
 * openttd.ui.* reads what the process shows: every window with its widgets, their screen
 * rectangles and their resolved text, derived from the window system itself rather than from a
 * hand-written list, and the projection between screen pixels and map tiles.
 *
 * openttd.input.* feeds the pointer, the mouse buttons, the wheel and keys into the same entry
 * points a video driver calls (HandleMouseEvents(), HandleKeypress()), so the game reacts exactly
 * as to a person: the cursor sprite moves, tools highlight tiles, a click or a drag issues the
 * command a person's would, through the network like any client. Input is refused unless the
 * process was started with PRISMATIC_MCP_INPUT=1 (the owner's switch for their own client), and
 * it pauses for a few seconds whenever the real mouse moves, so the owner can take over at once.
 *
 * In a dedicated server, which has no interface, every UI tool answers an error.
 * @param name The tool name.
 * @param args The tool arguments.
 * @return The tool's JSON answer, or std::nullopt when \a name is not a UI tool.
 */
std::optional<nlohmann::json> Call(const std::string &name, const nlohmann::json &args);

} // namespace PrismaticMCP::UI

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_UI_H */
