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

#include "../safeguards.h"

namespace PrismaticMCP::View {

using json = nlohmann::json;

namespace {

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
	arr.push_back({{"name", "openttd.view.state"},
		{"description", "GUI only: the main viewport's zoom, zoom limits and followed vehicle"},
		{"inputSchema", obj}});
	return arr;
}

std::optional<json> Call(const std::string &name, const json &args)
{
	if (name.rfind("openttd.view.", 0) != 0) return std::nullopt;

	Window *w = MainWindow();
	if (w == nullptr || w->viewport == nullptr) return Error("no main viewport in this process (a dedicated server has none)");

	if (name == "openttd.view.state") return State(w);

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
