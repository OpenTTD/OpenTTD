/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/*
 * RESEARCH-ONLY / UNSTABLE. Compiled only with OPTION_PRISMATIC_MCP_SERVER (ADR 0008, ADR 0030).
 *
 * The fork's own main-toolbar menu. One table (Items) is the menu: the toolbar draws it as its
 * dropdown, openttd://ui/menu describes it, openttd.ui.menu.run runs an item as a click does. A
 * question typed through "Ask the planner...", and every Request item (a fixed kind such as
 * serve_here, with the tile under the main view's centre), becomes a request in openttd://ui/requests;
 * any MCP client (the hub, a Rhai script, mcp-human, another AI) answers it with
 * openttd.ui.requests.answer, and the answer is shown in this game as a news message. A Toggle item
 * calls an MCP tool with a boolean it flips. Nothing here changes the game: a console command
 * runs as typed, a URL opens in the browser, a request and its answer are local to this process.
 */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_menu.h"
#include "prismatic_mcp_view.h"

#include "../console_func.h"
#include "../console_gui.h"
#include "../dropdown_func.h"
#include "../dropdown_common_type.h"
#include "../map_func.h"
#include "../network/network.h"
#include "../openttd.h"
#include "../timer/timer_game_calendar.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../window_gui.h"

#include <array>
#include <span>
#include <cstdlib>
#include <deque>

#include "../safeguards.h"

using json = nlohmann::json;

namespace PrismaticMCP::Menu {

static constexpr std::array<Item, 13> ITEMS = {{
	{"MCP server: status", Action::Console, "prismatic_mcp status"},
	{"MCP server: start", Action::Console, "prismatic_mcp start"},
	{"MCP approvals waiting", Action::Console, "prismatic_mcp approvals"},
	{"MCP decisions", Action::Console, "prismatic_mcp decisions"},
	{"Ask the planner...", Action::Ask, "ask"},
	/* Rows of the human-like player (tools/research/mcp-human, openttd-07): it answers these kinds. */
	{"Human player: paused", Action::Toggle, "openttd.input.yield", "paused"},
	{"Serve the towns here", Action::Request, "serve_here"},
	{"Plan a rail line here", Action::Request, "plan_rail"},
	{"Watch my buses", Action::Request, "watch"},
	{"Open the hub", Action::Browse, "/"},
	{"World view", Action::Browse, "/viz"},
	{"API (Swagger)", Action::Browse, "/docs"},
	{"GraphQL schema", Action::Browse, "/graphql/schema"},
}};

std::span<const Item> Items()
{
	return ITEMS;
}

std::string HubUrl()
{
	const char *env = std::getenv("PRISMATIC_HUB_URL");
	std::string url = (env != nullptr && *env != '\0') ? env : "http://127.0.0.1:8790";
	while (!url.empty() && url.back() == '/') url.pop_back();
	return url;
}

/** The switches of the Toggle items, by index; off until flipped. */
static std::array<bool, ITEMS.size()> _toggled{};

DropDownList DropDown()
{
	DropDownList list;
	for (size_t i = 0; i < ITEMS.size(); i++) {
		if (ITEMS[i].action == Action::Toggle) {
			list.push_back(std::make_unique<DropDownListCheckedItem>(0, _toggled[i], std::string{ITEMS[i].label}, static_cast<int>(i)));
		} else {
			list.push_back(MakeDropDownListStringItem(std::string{ITEMS[i].label}, static_cast<int>(i)));
		}
	}
	return list;
}

/** Flip the switch of Toggle item `index` through its tool; the switch moves only when the tool says ok. */
static json Flip(size_t index)
{
	const Item &item = ITEMS[index];
	json result = CallTool(item.arg, {{std::string{item.key}, !_toggled[index]}});
	bool ok = !result.value("isError", true);
	if (ok) _toggled[index] = !_toggled[index];
	IConsolePrint(ok ? CC_INFO : CC_ERROR, "{}: {}", item.label, ok ? (_toggled[index] ? "on" : "off") : result.dump());
	return json{{"ok", ok}, {"item", item.label}, {"on", _toggled[index]}, {"result", result}};
}

static std::string_view ActionName(Action a)
{
	switch (a) {
		case Action::Console: return "console";
		case Action::Browse: return "browse";
		case Action::Ask: return "ask";
		case Action::Request: return "request";
		case Action::Toggle: return "toggle";
	}
	return "unknown";
}

bool Run(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= ITEMS.size()) return false;
	const Item &item = ITEMS[index];
	switch (item.action) {
		case Action::Console:
			IConsoleCmdExec(item.arg);
			if (_iconsole_mode == IConsoleMode::Closed) IConsoleSwitch();
			return false;
		case Action::Browse:
			OpenBrowser(HubUrl() + std::string{item.arg});
			return false;
		case Action::Ask:
			return true;
		case Action::Request:
			Queue(item.arg, "");
			return false;
		case Action::Toggle:
			Flip(index);
			return false;
	}
	return false;
}

/** One question typed in the game, and its answer once an MCP client gave one. */
struct Request {
	uint64_t id = 0;
	std::string kind;
	std::string text;
	std::optional<TileIndex> tile;
	std::string date;
	std::optional<std::string> answer;
};

static std::deque<Request> _requests;
static uint64_t _next_request = 1;
static constexpr size_t MAX_REQUESTS = 64;

/** The tile under the main view's centre, if there is a main window. */
static std::optional<TileIndex> ViewCentreTile()
{
	const Window *w = FindWindowById(WindowClass::MainWindow, 0); // not GetMainWindow: that asserts one exists
	if (w == nullptr || w->viewport == nullptr) return std::nullopt;
	const Viewport &vp = *w->viewport;
	Point pt = TranslateXYToTileCoord(vp, vp.left + vp.width / 2, vp.top + vp.height / 2);
	if (pt.x < 0 || pt.y < 0) return std::nullopt;
	return TileVirtXY(pt.x, pt.y);
}

uint64_t Queue(std::string_view kind, std::string_view text)
{
	if (_requests.size() >= MAX_REQUESTS) _requests.pop_front();
	_requests.push_back({_next_request++, std::string{kind}, std::string{text}, ViewCentreTile(),
		fmt::format("{}", TimerGameCalendar::date.base()), std::nullopt});
	const Request &r = _requests.back();
	IConsolePrint(CC_INFO, "Request {} ({}{}{}): an MCP client answers it through openttd.ui.requests.answer", r.id, r.kind,
		text.empty() ? "" : ": ", text);
	return r.id;
}

void Ask(std::string_view text)
{
	if (!text.empty()) Queue("ask", text);
}

json Describe()
{
	/* A dedicated server has no toolbar: nothing of the menu exists there. */
	if (_network_dedicated) return {{"items", json::array()}, {"hub", HubUrl()}, {"dedicated", true}};
	json items = json::array();
	for (size_t i = 0; i < ITEMS.size(); i++) {
		json target = ITEMS[i].action == Action::Browse ? json(HubUrl() + std::string{ITEMS[i].arg}) : json(std::string{ITEMS[i].arg});
		json item = {{"index", i}, {"label", ITEMS[i].label}, {"action", ActionName(ITEMS[i].action)}, {"target", target}};
		if (ITEMS[i].action == Action::Toggle) { item["key"] = ITEMS[i].key; item["on"] = _toggled[i]; }
		items.push_back(std::move(item));
	}
	return {{"items", std::move(items)}, {"hub", HubUrl()}};
}

json Requests()
{
	if (_network_dedicated) return {{"requests", json::array()}};
	json list = json::array();
	for (const Request &r : _requests) {
		json tile = nullptr, x = nullptr, y = nullptr;
		if (r.tile.has_value()) { tile = r.tile->base(); x = TileX(*r.tile); y = TileY(*r.tile); }
		list.push_back({{"id", r.id}, {"kind", r.kind}, {"text", r.text}, {"tile", tile}, {"x", x}, {"y", y}, {"date", r.date}, {"answered", r.answer.has_value()},
			{"answer", r.answer.has_value() ? json(*r.answer) : json(nullptr)}});
	}
	return {{"requests", std::move(list)}};
}

json Tools()
{
	json arr = json::array();
	/* Each instance's catalogue is what it can do: a dedicated server has no toolbar to run. */
	if (_network_dedicated) return arr;
	arr.push_back({{"name", "openttd.ui.menu.run"},
		{"description", "Run an item of the fork's toolbar menu (openttd://ui/menu) as a click on it does: a console command, a hub page in the browser, or with text for the Ask item, a request. Local to this process"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"index"})},
			{"properties", {{"index", {{"type", "integer"}}}, {"text", {{"type", "string"}}}}}}},
		/* Not read-only: a row runs a console command, opens a browser, queues a request or flips a switch. */
		{"annotations", {{"readOnlyHint", false}, {"idempotentHint", false}}}});
	arr.push_back({{"name", "openttd.ui.requests.answer"},
		{"description", "Answer a question the player asked in the game (openttd://ui/requests): the answer is recorded and shown in this game as a news message. Local to this process"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"id", "answer"})},
			{"properties", {{"id", {{"type", "integer"}}}, {"answer", {{"type", "string"}}}}}}},
		{"annotations", {{"readOnlyHint", false}, {"idempotentHint", false}}}});
	return arr;
}

std::optional<json> Call(std::string_view name, const json &args)
{
	if (_network_dedicated && (name == "openttd.ui.menu.run" || name == "openttd.ui.requests.answer")) {
		return json{{"ok", false}, {"error", "no toolbar on a dedicated server"}};
	}
	if (name == "openttd.ui.menu.run") {
		int index = args.value("index", -1);
		if (index < 0 || static_cast<size_t>(index) >= ITEMS.size()) return json{{"ok", false}, {"error", "no such menu item; see openttd://ui/menu"}};
		switch (ITEMS[index].action) {
			case Action::Ask: {
				std::string text = args.value("text", "");
				if (text.empty()) return json{{"ok", false}, {"error", "the Ask item needs text"}};
				return json{{"ok", true}, {"request", Queue("ask", text)}};
			}
			case Action::Request:
				return json{{"ok", true}, {"request", Queue(ITEMS[index].arg, args.value("text", ""))}};
			case Action::Toggle:
				return Flip(index);
			default:
				break;
		}
		Run(index);
		return json{{"ok", true}, {"ran", ITEMS[index].label}};
	}
	if (name == "openttd.ui.requests.answer") {
		uint64_t id = args.value("id", uint64_t{0});
		std::string answer = args.value("answer", "");
		if (answer.empty()) return json{{"ok", false}, {"error", "answer is required"}};
		for (Request &r : _requests) {
			if (r.id != id) continue;
			r.answer = answer;
			/* Shown the way every message from the fork reaches the player: an in-game news item. */
			auto shown = View::Call("openttd.view.news.show", {{"text", fmt::format("{}\n{}", r.text, answer)}, {"style", "small"}});
			return json{{"ok", true}, {"id", id}, {"shown", shown.has_value() && shown->value("ok", false)}};
		}
		return json{{"ok", false}, {"error", "no such request; see openttd://ui/requests"}};
	}
	return std::nullopt;
}

} // namespace PrismaticMCP::Menu

#endif /* OTTD_PRISMATIC_MCP_SERVER */
