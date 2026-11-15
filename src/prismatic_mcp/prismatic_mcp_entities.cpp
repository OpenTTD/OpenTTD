/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 *
 * Dynamic entities of the world as data for the embedded MCP server: every primary vehicle with
 * its owner, type, place, state, profit and orders. Read-only; no command, no game state written.
 */

/** @file prismatic_mcp_entities.cpp Dynamic entities (vehicles and their orders) as data (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_entities.h"

#include "../engine_base.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../strings_func.h"
#include "../vehicle_base.h"
#include "../table/strings.h"

#include "../safeguards.h"

namespace PrismaticMCP::Entities {

using json = nlohmann::json;

namespace {

std::string_view TypeName(VehicleType t)
{
	switch (t) {
		case VehicleType::Train: return "train";
		case VehicleType::Road: return "road";
		case VehicleType::Ship: return "ship";
		case VehicleType::Aircraft: return "aircraft";
		default: return "other";
	}
}

std::string_view OrderName(OrderType t)
{
	switch (t) {
		case OT_GOTO_STATION: return "goto_station";
		case OT_GOTO_DEPOT: return "goto_depot";
		case OT_GOTO_WAYPOINT: return "goto_waypoint";
		case OT_CONDITIONAL: return "conditional";
		case OT_IMPLICIT: return "implicit";
		default: return "other";
	}
}

} // namespace

json Vehicles()
{
	json list = json::array();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!v->IsPrimaryVehicle()) continue;
		json orders = json::array();
		for (const Order &o : v->Orders()) {
			json jo = {{"type", OrderName(o.GetType())}};
			if (o.IsType(OT_GOTO_STATION) || o.IsType(OT_GOTO_DEPOT) || o.IsType(OT_GOTO_WAYPOINT) || o.IsType(OT_IMPLICIT)) {
				jo["destination"] = o.GetDestination().ToStationID().base();
			}
			orders.push_back(std::move(jo));
		}
		list.push_back({
			{"id", v->index.base()}, {"unit", v->unitnumber}, {"type", TypeName(v->type)}, {"owner", v->owner.base()},
			{"engine", v->GetEngine() != nullptr ? GetString(STR_ENGINE_NAME, v->engine_type) : std::string{}},
			{"tile", v->tile.base()}, {"x", TileX(v->tile)}, {"y", TileY(v->tile)},
			{"stopped", v->vehstatus.Test(VehState::Stopped)}, {"in_depot", v->IsStoppedInDepot()},
			{"crashed", v->vehstatus.Test(VehState::Crashed)},
			{"cargo_capacity", v->cargo_cap}, {"cargo_count", v->cargo.StoredCount()},
			{"profit_this_year", static_cast<int64_t>(v->GetDisplayProfitThisYear())},
			{"profit_last_year", static_cast<int64_t>(v->GetDisplayProfitLastYear())},
			{"current_order", v->cur_real_order_index}, {"orders", std::move(orders)},
		});
	}
	return json{{"vehicles", std::move(list)}};
}

} // namespace PrismaticMCP::Entities

#endif /* OTTD_PRISMATIC_MCP_SERVER */
