/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/*
 * RESEARCH-ONLY / UNSTABLE. Compiled only with OPTION_PRISMATIC_MCP_SERVER (ADR 0008, ADR 0030).
 */

/** @file prismatic_mcp_menu.h The fork's own main-toolbar menu: one table drawn as the dropdown, described over MCP, run by both. */

#ifndef PRISMATIC_MCP_MENU_H
#define PRISMATIC_MCP_MENU_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"
#include "../dropdown_type.h"

#include <optional>
#include <string>
#include <string_view>

namespace PrismaticMCP::Menu {

/** What a menu item does. */
enum class Action : uint8_t {
	Console, ///< Run a console command and show the console.
	Browse,  ///< Open a URL of the hub in the browser.
	Ask,     ///< Ask a question in plain language (queued as a request of kind "ask" for any MCP client to answer).
	Request, ///< Queue a request of a fixed kind (arg) for the tile under the main view's centre, no text typed.
	Toggle,  ///< Call an MCP tool (arg) with {key: on|off}, flipping a switch the item shows as checked.
};

/** One item of the menu. The table of these is the only place the menu is written down. */
struct Item {
	std::string_view label; ///< What the dropdown shows.
	Action action;          ///< What it does.
	std::string_view arg;   ///< The console command, the hub path to open, the request kind, or the tool to call.
	std::string_view key = {}; ///< Toggle only: the boolean argument the tool takes.
};

/** The items, in menu order. */
std::span<const Item> Items();

/** The hub's base URL: $PRISMATIC_HUB_URL, else http://127.0.0.1:8790. */
std::string HubUrl();

/** The dropdown of the toolbar button, built from Items(). */
DropDownList DropDown();

/**
 * Run item `index`, as a click on it does.
 * @return true when the item asks for text, so the caller shows the query window.
 */
bool Run(int index);

/** A question typed in the game: queued as a request until an MCP client answers it. */
void Ask(std::string_view text);

/**
 * Queue a request: {id, kind, text, date, tile, x, y}; the tile is the one under the main view's
 * centre (null without a main window).
 * @return The request's id.
 */
uint64_t Queue(std::string_view kind, std::string_view text);

/** openttd://ui/menu: every item with its action and target, and the hub URL. */
nlohmann::json Describe();

/** openttd://ui/requests: every request typed in the game, answered or not. */
nlohmann::json Requests();

/** The tools of this module: openttd.ui.menu.run and openttd.ui.requests.answer. */
nlohmann::json Tools();

/**
 * Call any published MCP tool in this process, the way tools/call does: the menu's Toggle items and
 * nothing else use it. Defined by the server (prismatic_mcp_server.cpp), which owns the dispatch.
 */
nlohmann::json CallTool(std::string_view name, const nlohmann::json &args);

/** Run one of Tools() by name; nullopt when the name is not this module's. */
std::optional<nlohmann::json> Call(std::string_view name, const nlohmann::json &args);

} // namespace PrismaticMCP::Menu

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_MENU_H */
