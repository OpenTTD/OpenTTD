/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE.
 *
 * View tools of the embedded MCP server: an external controller moves the main viewport of the
 * GUI process the MCP server runs in (a player's client, or a GUI-hosted game), so the player
 * sees what their bot does. Only the local view changes: no command, no game state, nothing that
 * reaches the network, which is why these tools need no approval.
 */

/** @file prismatic_mcp_view.cpp View tools of the embedded MCP server (research-only). */

#include "../stdafx.h"
#include <deque>
#include <optional>

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_view.h"

#include "../map_func.h"
#include "../network/network.h"
#include "../settings_type.h"
#include "../town.h"
#include "../vehicle_base.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_func.h"
#include "../screenshot.h"
#include "../news_func.h"
#include "../company_base.h"
#include "../textbuf_gui.h"
#include "../news_gui.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../timer/timer_game_calendar.h"
#include "../fileio_func.h"

#include "../safeguards.h"

namespace PrismaticMCP::View {

using json = nlohmann::json;

namespace {

/** A question asked through openttd.view.ask, and the player's answer once given. */
struct Ask {
	uint32_t id;
	std::string question;
	std::optional<std::string> answer; ///< "yes", "no" (also a closed dialog), or "replaced" by a newer question.
};
std::deque<Ask> _asks; ///< Newest first; GUI state of this client only.
uint32_t _ask_next = 0; ///< Last id handed out.
uint32_t _ask_open = 0; ///< The question the dialog shows now, 0 if none.

/** The query dialog's callback: yes, no, or closed (no). */
void AskAnswered(Window *, bool confirmed)
{
	for (Ask &a : _asks) {
		if (a.id == _ask_open && !a.answer.has_value()) a.answer = confirmed ? "yes" : "no";
	}
	_ask_open = 0;
}

json Error(std::string message)
{
	return json{{"ok", false}, {"error", std::move(message)}};
}

/** The main window, or nullptr in a process without one (a dedicated server). */
Window *MainWindow()
{
	if (_network_dedicated) return nullptr;
	return FindWindowById(WindowClass::MainWindow, 0);
}

/** Resolve the target of scroll_to: a tile, x/y, a town or a vehicle. */
std::optional<TileIndex> Target(const json &args, std::string &error)
{
	if (args.contains("tile") && args["tile"].is_number_unsigned()) {
		TileIndex t{args["tile"].get<uint32_t>()};
		if (t.base() >= Map::Size()) { error = "tile is outside the map"; return std::nullopt; }
		return t;
	}
	if (args.contains("x") && args.contains("y") && args["x"].is_number_unsigned() && args["y"].is_number_unsigned()) {
		uint x = args["x"].get<uint>(), y = args["y"].get<uint>();
		if (x >= Map::SizeX() || y >= Map::SizeY()) { error = "x/y is outside the map"; return std::nullopt; }
		return TileXY(x, y);
	}
	if (args.contains("town") && args["town"].is_number_unsigned()) {
		const Town *t = Town::GetIfValid(args["town"].get<uint32_t>());
		if (t == nullptr) { error = "no such town"; return std::nullopt; }
		return t->xy;
	}
	if (args.contains("vehicle") && args["vehicle"].is_number_unsigned()) {
		const Vehicle *v = Vehicle::GetIfValid(args["vehicle"].get<uint32_t>());
		if (v == nullptr) { error = "no such vehicle"; return std::nullopt; }
		return v->tile;
	}
	error = "give one of tile, x and y, town or vehicle";
	return std::nullopt;
}

json State(Window *w)
{
	const ViewportData &vp = *w->viewport;
	return json{{"ok", true}, {"zoom", static_cast<int>(vp.zoom)},
		{"zoom_min", static_cast<int>(_settings_client.gui.zoom_min)}, {"zoom_max", static_cast<int>(_settings_client.gui.zoom_max)},
		{"following_vehicle", vp.follow_vehicle == VehicleID::Invalid() ? json(nullptr) : json(vp.follow_vehicle.base())}};
}

} // namespace

json Tools()
{
	json obj = {{"type", "object"}};
	json arr = json::array();
	/* A dedicated server has no window to view: it lists no view tool, so each instance's catalogue is
	 * what it can actually do. */
	if (_network_dedicated) return arr;
	arr.push_back({{"name", "openttd.view.scroll_to"},
		{"description", "GUI only, no approval: centre the main viewport on a tile, x/y, a town or a vehicle"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
			{"town", {{"type", "integer"}}}, {"vehicle", {{"type", "integer"}}}, {"instant", {{"type", "boolean"}}}}}}}});
	arr.push_back({{"name", "openttd.view.follow"},
		{"description", "GUI only, no approval: the main viewport follows a vehicle; vehicle null stops following"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"vehicle", {{"type", {"integer", "null"}}}}}}, {"required", json::array({"vehicle"})}}}});
	arr.push_back({{"name", "openttd.view.zoom"},
		{"description", "GUI only, no approval: set the main viewport's zoom level within the client's zoom limits"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"level", {{"type", "integer"}}}}}, {"required", json::array({"level"})}}}});
	arr.push_back({{"name", "openttd.view.mark"},
		{"description", "GUI only, no approval: the bot's cursor, a marked tile (the red tile square) at a tile, x/y, a town or a vehicle; clear removes it"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
			{"town", {{"type", "integer"}}}, {"vehicle", {{"type", "integer"}}}, {"clear", {{"type", "boolean"}}}}}}}});
	arr.push_back({{"name", "openttd.view.screenshot"},
		{"description", "GUI only, no approval: save what this window shows right now (the whole screen buffer, UI included) and answer the file path"},
		{"inputSchema", obj}});
	arr.push_back({{"name", "openttd.view.news.list"},
		{"description", "GUI only, no approval: the news this client has shown (newest first): text, type, style, date"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"limit", {{"type", "integer"}}}}}}},
		{"annotations", {{"readOnlyHint", true}}}});
	arr.push_back({{"name", "openttd.view.news.show"},
		{"description", "GUI only, no approval, local to this client: show a news message to the player (from the bot, a script or another AI). With a tile, town or vehicle it gets a viewport there (style thin); style: normal (newspaper), thin (headline + viewport), small (information window), company (the newspaper with a company's manager face: company id, optional title launch|trouble|merger|bankrupt)"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"text"})}, {"properties", {{"text", {{"type", "string"}}},
			{"style", {{"type", "string"}, {"enum", json::array({"normal", "thin", "small", "company"})}}},
			{"company", {{"type", "integer"}}}, {"title", {{"type", "string"}, {"enum", json::array({"none", "launch", "trouble", "merger", "bankrupt"})}}},
			{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
			{"town", {{"type", "integer"}}}, {"vehicle", {{"type", "integer"}}}}}}}});
	arr.push_back({{"name", "openttd.view.ask"},
		{"description", "GUI only, no approval: ask the player a yes/no question in the game window (the game's own query dialog). Answers {id}; the answer arrives in openttd.view.ask.answers: yes, no (also when the player closes the dialog), or replaced when a newer question takes its place"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"question"})}, {"properties", {{"question", {{"type", "string"}}}, {"caption", {{"type", "string"}}}}}}}});
	arr.push_back({{"name", "openttd.view.ask.answers"},
		{"description", "GUI only, no approval: every question asked through openttd.view.ask, newest first, with its answer (yes, no, replaced, or null while it is open)"},
		{"inputSchema", obj},
		{"annotations", {{"readOnlyHint", true}}}});
	arr.push_back({{"name", "openttd.view.news.close"},
		{"description", "GUI only, no approval: close the news window the player sees"},
		{"inputSchema", obj}});
	arr.push_back({{"name", "openttd.view.state"},
		{"description", "GUI only: the main viewport's zoom, zoom limits and followed vehicle"},
		{"inputSchema", obj},
		{"annotations", {{"readOnlyHint", true}}}});
	return arr;
}

std::optional<json> Call(const std::string &name, const json &args)
{
	if (name.rfind("openttd.view.", 0) != 0) return std::nullopt;

	Window *w = MainWindow();
	if (w == nullptr || w->viewport == nullptr) return Error("no main viewport in this process (a dedicated server has none)");

	if (name == "openttd.view.state") return State(w);

	if (name == "openttd.view.news.list") {
		static constexpr std::string_view TYPES[] = {"arrival_company", "arrival_other", "accident", "accident_other",
			"company_info", "industry_open", "industry_close_company", "industry_close_other", "industry_close_nobody",
			"economy", "industry_company", "industry_other", "industry_nobody", "advice", "new_vehicles",
			"acceptance", "subsidies", "general"};
		static constexpr std::string_view STYLES[] = {"thin", "small", "normal", "vehicle", "company"};
		size_t limit = std::min<size_t>(args.value("limit", 20), 200);
		json list = json::array();
		for (const NewsItem &ni : GetNews()) {
			if (list.size() >= limit) break;
			auto ymd = TimerGameCalendar::ConvertDateToYMD(ni.date);
			auto t = static_cast<size_t>(ni.type), st = static_cast<size_t>(ni.style);
			list.push_back({{"text", ni.headline.GetDecodedString()},
				{"type", t < std::size(TYPES) ? json(TYPES[t]) : json(t)},
				{"style", st < std::size(STYLES) ? json(STYLES[st]) : json(st)},
				{"date", fmt::format("{:04}-{:02}-{:02}", ymd.year.base(), ymd.month + 1, ymd.day)}});
		}
		return json{{"ok", true}, {"news", std::move(list)}};
	}

	if (name == "openttd.view.news.show") {
		/* Local to this client: a news item is GUI state, not game state, and is never sent to the server. */
		std::string text = args.value("text", "");
		if (text.empty()) return Error("text is required");
		std::string style = args.value("style", "");
		std::string error;
		NewsReference ref{};
		bool has_target = args.contains("tile") || args.contains("x") || args.contains("town") || args.contains("vehicle");
		if (has_target) {
			auto tile = Target(args, error);
			if (!tile.has_value()) return Error(error);
			ref = *tile;
		}
		if (style == "company") {
			const Company *c = Company::GetIfValid(CompanyID(static_cast<uint8_t>(args.value("company", -1))));
			if (c == nullptr) return Error("style company needs the id of an existing company");
			std::string title = args.value("title", "none");
			StringID tid = title == "launch" ? STR_NEWS_COMPANY_LAUNCH_TITLE : title == "trouble" ? STR_NEWS_COMPANY_IN_TROUBLE_TITLE :
				title == "merger" ? STR_NEWS_COMPANY_MERGER_TITLE : title == "bankrupt" ? STR_NEWS_COMPANY_BANKRUPT_TITLE : STR_EMPTY;
			AddCompanyNewsItem(GetEncodedString(STR_JUST_BIG_RAW_STRING, text), std::make_unique<CompanyNewsInformation>(tid, c));
			return json{{"ok", true}, {"shown", text}, {"style", style}, {"company", c->index.base()}};
		}
		if (style.empty()) style = has_target ? "thin" : "normal";
		NewsStyle ns = style == "thin" ? NewsStyle::Thin : (style == "small" ? NewsStyle::Small : NewsStyle::Normal);
		NewsFlags flags = ns == NewsStyle::Normal ? NewsFlags{} : NewsFlags{NewsFlag::NoTransparency, NewsFlag::Shaded};
		if (ns != NewsStyle::Normal && !has_target) return Error("styles thin and small need a tile, x/y, town or vehicle");
		AddNewsItem(GetEncodedString(STR_JUST_RAW_STRING, text), NewsType::General, ns, flags, ref);
		return json{{"ok", true}, {"shown", text}, {"style", style}};
	}

	if (name == "openttd.view.ask") {
		std::string question = args.value("question", "");
		if (question.empty()) return Error("question is required");
		std::string caption = args.value("caption", "Your bot asks");
		uint32_t id = ++_ask_next;
		/* A question still open is replaced, not answered: mark it before the dialog closes it. */
		for (Ask &a : _asks) {
			if (a.id == _ask_open && !a.answer.has_value()) a.answer = "replaced";
		}
		ShowQuery(GetEncodedString(STR_JUST_RAW_STRING, caption), GetEncodedString(STR_JUST_RAW_STRING, question), nullptr, AskAnswered, false); // no focus: a stray Enter must not answer for the player
		_ask_open = id;
		_asks.push_front({id, question, std::nullopt});
		if (_asks.size() > 100) _asks.pop_back();
		return json{{"ok", true}, {"id", id}};
	}

	if (name == "openttd.view.ask.answers") {
		json list = json::array();
		for (const Ask &a : _asks) {
			list.push_back({{"id", a.id}, {"question", a.question}, {"answer", a.answer.has_value() ? json(*a.answer) : json(nullptr)}});
		}
		return json{{"ok", true}, {"asks", std::move(list)}};
	}

	if (name == "openttd.view.news.close") {
		CloseWindowById(WindowClass::News, 0);
		return json{{"ok", true}};
	}

	if (name == "openttd.view.screenshot") {
		/* The screen buffer as the player sees it, captured on the drawing thread (never here, on
		 * the game thread, where the video buffer may be unlocked). The file appears within a frame. */
		std::string path = _personal_dir + "prismatic_mcp_view." + std::string(GetCurrentScreenshotExtension());
		QueueQuietScreenshot(path);
		json out = State(w);
		out["path"] = path;
		out["queued"] = true;
		return out;
	}

	if (name == "openttd.view.scroll_to") {
		std::string error;
		auto tile = Target(args, error);
		if (!tile.has_value()) return Error(error);
		w->viewport->follow_vehicle = VehicleID::Invalid();
		ScrollMainWindowToTile(*tile, args.value("instant", false));
		json out = State(w);
		out["tile"] = tile->base();
		out["x"] = TileX(*tile);
		out["y"] = TileY(*tile);
		return out;
	}

	if (name == "openttd.view.mark") {
		if (args.value("clear", false)) {
			SetRedErrorSquare(INVALID_TILE);
			return State(w);
		}
		std::string error;
		auto tile = Target(args, error);
		if (!tile.has_value()) return Error(error);
		SetRedErrorSquare(*tile);
		json out = State(w);
		out["marked"] = tile->base();
		out["x"] = TileX(*tile);
		out["y"] = TileY(*tile);
		return out;
	}

	if (name == "openttd.view.follow") {
		if (!args.contains("vehicle") || args["vehicle"].is_null()) {
			w->viewport->follow_vehicle = VehicleID::Invalid();
			return State(w);
		}
		if (!args["vehicle"].is_number_unsigned()) return Error("vehicle must be an id or null");
		const Vehicle *v = Vehicle::GetIfValid(args["vehicle"].get<uint32_t>());
		if (v == nullptr) return Error("no such vehicle");
		w->viewport->follow_vehicle = v->index;
		return State(w);
	}

	if (name == "openttd.view.zoom") {
		if (!args.contains("level") || !args["level"].is_number_integer()) return Error("level must be an integer");
		auto level = static_cast<ZoomLevel>(args["level"].get<int>());
		if (!IsInsideMM(level, _settings_client.gui.zoom_min, _settings_client.gui.zoom_max + 1)) {
			return Error(fmt::format("zoom level outside the client's limits {}..{}", _settings_client.gui.zoom_min, _settings_client.gui.zoom_max));
		}
		while (w->viewport->zoom > level && DoZoomInOutWindow(ZOOM_IN, w)) {}
		while (w->viewport->zoom < level && DoZoomInOutWindow(ZOOM_OUT, w)) {}
		return State(w);
	}

	return Error("unknown view tool");
}

} // namespace PrismaticMCP::View

#endif /* OTTD_PRISMATIC_MCP_SERVER */
