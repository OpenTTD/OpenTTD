/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE.
 *
 * World resources: the whole map and every station, read-only, so an external planner (a Rhai
 * script, Prismatic, an algoglyph kernel) sees what a player sees and can derive graphs from it.
 */

/** @file prismatic_mcp_world.cpp Read-only world resources of the embedded MCP server (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_world.h"

#include "../company_type.h"
#include "../company_base.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../articulated_vehicles.h"
#include "../cargotype.h"
#include "../settings_type.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../timer/timer_game_calendar.h"
#include "../map_func.h"
#include "../station_base.h"
#include "../depot_base.h"
#include "../depot_map.h"
#include "../road_map.h"
#include "../rail_map.h"
#include "../pathfinder/pathfinder_type.h"
#include "../tile_map.h"
#include "../clear_map.h"
#include "../tree_map.h"
#include "../water_map.h"
#include "../vehicle_base.h"
#include "../economy_func.h"
#include "../newgrf.h"
#include "../settings_type.h"
#include "../table/pricebase.h"
#include "../bridge.h"
#include "../rail.h"
#include "../strings_func.h"
#include "../timer/timer_game_calendar.h"
#include "../town.h"

#include <optional>

#include "../safeguards.h"

namespace PrismaticMCP::World {

using json = nlohmann::json;

namespace {

/** One character per tile type, in TileType order. */
constexpr std::string_view TYPE_CODES = "CRrHTSWVIBO"; // Clear Railway road House Trees Station Water Void Industry Bridge/tunnel Object

char TypeCode(TileType t)
{
	auto i = static_cast<size_t>(t);
	return i < TYPE_CODES.size() ? TYPE_CODES[i] : '?';
}

/** One character per owner: a company as 0-9a-e, else T town, N none, W water, D deity, '.' not ownable. */
char OwnerCode(TileIndex tile, TileType t)
{
	if (t == TileType::House || t == TileType::Industry || t == TileType::Void) return '.';
	Owner o = GetTileOwner(tile);
	if (o == OWNER_TOWN) return 'T';
	if (o == OWNER_NONE) return 'N';
	if (o == OWNER_WATER) return 'W';
	if (o == OWNER_DEITY) return 'D';
	auto c = o.base();
	return c < 10 ? static_cast<char>('0' + c) : (c < MAX_COMPANIES ? static_cast<char>('a' + c - 10) : '?');
}

constexpr std::string_view HEX = "0123456789abcdef";

/** Ground under a tile, one character; see the encoding in Terrain(). */
char GroundCode(TileIndex tile, TileType t)
{
	switch (t) {
		case TileType::Clear:
			if (IsSnowTile(tile)) return 's';
			switch (GetClearGround(tile)) {
				case ClearGround::Grass: return GetClearDensity(tile) == 0 ? 'b' : 'g';
				case ClearGround::Rough: return 'r';
				case ClearGround::Rocks: return 'k';
				case ClearGround::Fields: return 'f';
				case ClearGround::Desert: return 'd';
				default: return '?';
			}
		case TileType::Trees:
			switch (GetTreeGround(tile)) {
				case TreeGround::Grass: return 'G';
				case TreeGround::Rough: case TreeGround::RoughSnow: return 'R';
				case TreeGround::SnowOrDesert: return 'D';
				case TreeGround::Shore: return 'C';
			}
			return '?';
		case TileType::Water:
			if (IsCoast(tile)) return 'c';
			switch (GetWaterClass(tile)) {
				case WaterClass::Canal: return 'n';
				case WaterClass::River: return 'v';
				default: return 'w';
			}
		default:
			return '-';
	}
}

/** What building on a tile needs, one character; see the encoding in Terrain(). */
char BuildCode(TileType t)
{
	switch (t) {
		case TileType::Clear: return '.';
		case TileType::Trees: return 't';
		case TileType::Water: return 'w';
		default: return 'x';
	}
}

/** FNV-1a, so a client can tell which rows changed without comparing them. */
uint64_t Fnv(std::string_view s, uint64_t h = 1469598103934665603ULL)
{
	for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
	return h;
}

/* Generated from enum class Price (economy_type.h), in its order; the static_assert keeps it in step. */
constexpr std::string_view PRICE_NAMES[] = {
	"station_value", "build_rail", "build_road", "build_signals", "build_bridge", "build_depot_train",
	"build_depot_road", "build_depot_ship", "build_tunnel", "build_station_rail", "build_station_rail_length",
	"build_station_airport", "build_station_bus", "build_station_truck", "build_station_dock",
	"build_vehicle_train", "build_vehicle_wagon", "build_vehicle_aircraft", "build_vehicle_road",
	"build_vehicle_ship", "build_trees", "terraform", "clear_grass", "clear_rough", "clear_rocks",
	"clear_fields", "clear_trees", "clear_rail", "clear_signals", "clear_bridge", "clear_depot_train",
	"clear_depot_road", "clear_depot_ship", "clear_tunnel", "clear_water", "clear_station_rail",
	"clear_station_airport", "clear_station_bus", "clear_station_truck", "clear_station_dock", "clear_house",
	"clear_road", "running_train_steam", "running_train_diesel", "running_train_electric", "running_aircraft",
	"running_roadveh", "running_ship", "build_industry", "clear_industry", "build_object", "clear_object",
	"build_waypoint_rail", "clear_waypoint_rail", "build_waypoint_buoy", "clear_waypoint_buoy", "town_action",
	"build_foundation", "build_industry_raw", "build_town", "build_canal", "clear_canal", "build_aqueduct",
	"clear_aqueduct", "build_lock", "clear_lock", "infrastructure_rail", "infrastructure_road",
	"infrastructure_water", "infrastructure_station", "infrastructure_airport",
};
static_assert(std::size(PRICE_NAMES) == to_underlying(Price::End), "PRICE_NAMES must name every Price");


} // namespace

json Map()
{
	const uint w = Map::SizeX(), h = Map::SizeY();
	json types = json::array(), heights = json::array(), owners = json::array();
	std::string trow, hrow, orow;
	for (uint y = 0; y < h; y++) {
		trow.clear(); hrow.clear(); orow.clear();
		for (uint x = 0; x < w; x++) {
			TileIndex tile = TileXY(x, y);
			TileType t = GetTileType(tile);
			uint z = TileHeight(tile);
			trow += TypeCode(t);
			hrow += HEX[(z >> 4) & 0xF];
			hrow += HEX[z & 0xF];
			orow += OwnerCode(tile, t);
		}
		types.push_back(trow);
		heights.push_back(hrow);
		owners.push_back(orow);
	}
	return json{
		{"width", w}, {"height", h}, {"tile_index", "y * width + x"},
		{"encoding", {
			{"types", "one string per row (y), one character per tile (x): C clear, R railway, r road, H house, T trees, S station, W water, V void, I industry, B bridge or tunnel, O object"},
			{"heights", "one string per row, two hex digits per tile: the tile's height level"},
			{"owners", "one string per row, one character per tile: company 0-9 then a-e, T town, N none, W water, D deity, '.' not ownable (house, industry, void)"},
		}},
		{"types", std::move(types)}, {"heights", std::move(heights)}, {"owners", std::move(owners)},
	};
}

json Terrain()
{
	const uint w = Map::SizeX(), h = Map::SizeY();
	/* Vehicles per tile: every vehicle part a company owns, wherever it stands (effects and disasters left out). */
	std::vector<uint8_t> vehicles(Map::Size(), 0);
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type == VehicleType::Effect || v->type == VehicleType::Disaster || v->tile >= Map::Size()) continue;
		uint8_t &n = vehicles[v->tile.base()];
		if (n < 255) n++;
	}
	json slopes = json::array(), ground = json::array(), gradients = json::array(), build = json::array(),
		vehs = json::array(), digests = json::array();
	std::string srow, grow, drow, brow, vrow, all;
	for (uint y = 0; y < h; y++) {
		srow.clear(); grow.clear(); drow.clear(); brow.clear(); vrow.clear(); all.clear();
		for (uint x = 0; x < w; x++) {
			TileIndex tile = TileXY(x, y);
			TileType t = GetTileType(tile);
			auto s = static_cast<uint>(GetTileSlope(tile));
			srow += HEX[(s >> 4) & 0xF];
			srow += HEX[s & 0xF];
			grow += GroundCode(tile, t);
			const int z = TileHeight(tile);
			uint g = 0;
			auto step = [&](uint nx, uint ny) { g = std::max<uint>(g, static_cast<uint>(std::abs(z - static_cast<int>(TileHeight(TileXY(nx, ny)))))); };
			if (x > 0) step(x - 1, y);
			if (y > 0) step(x, y - 1);
			if (x + 1 < w) step(x + 1, y);
			if (y + 1 < h) step(x, y + 1);
			drow += HEX[std::min<uint>(g, 15)];
			brow += BuildCode(t);
			uint8_t n = vehicles[tile.base()];
			vrow += n == 0 ? '.' : (n <= 9 ? static_cast<char>('0' + n) : '+');
			all += TypeCode(t);
			all += HEX[(z >> 4) & 0xF];
			all += HEX[z & 0xF];
		}
		all += srow; all += grow; all += vrow;
		digests.push_back(fmt::format("{:016x}", Fnv(all)));
		slopes.push_back(srow); ground.push_back(grow); gradients.push_back(drow); build.push_back(brow); vehs.push_back(vrow);
	}
	return json{
		{"width", w}, {"height", h}, {"tile_index", "y * width + x"},
		{"encoding", {
			{"slopes", "one string per row (y), two hex digits per tile (x): the tile's Slope bits (corners W=1 S=2 E=4 N=8, steep=0x10; 00 flat)"},
			{"ground", "one character per tile: clear land g grass, b bare, r rough, k rocks, f fields, s snow, d desert; trees on G grass, R rough, D snow or desert, C shore; water w sea, n canal, v river, c coast; - built on (the tile types in openttd://map say what)"},
			{"gradients", "one hex digit per tile: the largest height step to a neighbour (x+-1, y+-1), capped at f"},
			{"buildable", "one character per tile: . clear land, buildable as is; t trees, cleared first; w water; x built on (track, road, house, industry, station, object, void)"},
			{"vehicles", "one character per tile: . none, 1-9 the number of vehicle parts standing there, + more"},
			{"row_digests", "one 16-hex-digit FNV-1a per row over its tile types, heights, slopes, ground and vehicles: re-read only the rows whose digest changed"},
		}},
		{"slopes", std::move(slopes)}, {"ground", std::move(ground)}, {"gradients", std::move(gradients)},
		{"buildable", std::move(build)}, {"vehicles", std::move(vehs)}, {"row_digests", std::move(digests)},
	};
}

json Prices()
{
	json prices = json::object(), base = json::object(), order = json::array();
	for (size_t i = 0; i < std::size(PRICE_NAMES); i++) {
		const Price p = static_cast<Price>(i);
		std::string name{PRICE_NAMES[i]};
		prices[name] = static_cast<int64_t>(_price[p]);
		base[name] = static_cast<int64_t>(_price_base_specs[p].start_price);
		order.push_back(name);
	}
	return json{
		{"prices", std::move(prices)},
		{"order", std::move(order)},
		{"base", std::move(base)},
		{"unit", "the game's internal money (pounds); the GUI shows it in the chosen currency"},
		{"inflation", _settings_game.economy.inflation},
		{"inflation_factor", static_cast<double>(_economy.inflation_prices) / 65536.0},
		{"construction_cost", _settings_game.difficulty.construction_cost},
		{"note", "prices are what the game charges now: inflation, the construction cost setting and NewGRF price multipliers included; base is each price's start value before them (src/table/pricebase.h)"},
	};
}

json Bridges()
{
	json types = json::array();
	const uint setting_max = _settings_game.construction.max_bridge_length;
	for (BridgeType i = 0; i < MAX_BRIDGES; i++) {
		const BridgeSpec *b = GetBridgeSpec(i);
		types.push_back({{"type", i}, {"name", GetString(b->material)},
			{"available", b->avail_year <= TimerGameCalendar::year}, {"avail_year", b->avail_year.base()},
			{"min_length", b->min_length}, {"max_length", std::min<uint>(b->max_length, setting_max)}, {"spec_max_length", b->max_length},
			{"price", b->price}, {"speed", b->speed}});
	}
	return json{
		{"types", std::move(types)},
		{"max_bridge_length", setting_max},
		{"max_tunnel_length", _settings_game.construction.max_tunnel_length},
		{"lengths", "min_length and max_length count the tiles between the two heads; max_length is the type's limit capped by the game's max_bridge_length"},
		{"cost", "CmdBuildBridge, for a company: L = tiles between the heads + 2; L x the track's build cost + CalcBridgeLenCostFactor(L) x build_bridge (openttd://economy/prices) x price >> 8; heads on a slope may add build_foundation"},
		{"speed_unit", "1 unit = 1/1.6 mph, about 1 km/h"},
	};
}

json RailTypes()
{
	json types = json::array();
	for (int i = RAILTYPE_BEGIN; i < RAILTYPE_END; i++) {
		const RailType rt = static_cast<RailType>(i);
		const RailTypeInfo *rti = GetRailTypeInfo(rt);
		if (rti->label.Empty()) continue; // a slot no base set or NewGRF defines
		json powered = json::array(), compatible = json::array(), companies = json::array();
		for (int j = RAILTYPE_BEGIN; j < RAILTYPE_END; j++) {
			const RailType o = static_cast<RailType>(j);
			if (GetRailTypeInfo(o)->label.Empty()) continue;
			if (rti->powered_railtypes.Test(o)) powered.push_back(GetRailTypeInfo(o)->label.AsString());
			if (rti->compatible_railtypes.Test(o)) compatible.push_back(GetRailTypeInfo(o)->label.AsString());
		}
		for (const Company *c : Company::Iterate()) {
			if (c->avail_railtypes.Test(rt)) companies.push_back(c->index.base());
		}
		types.push_back({{"railtype", i}, {"label", rti->label.AsString()}, {"name", GetString(rti->strings.name)},
			{"build_cost", static_cast<int64_t>(RailBuildCost(rt))}, {"cost_multiplier", rti->cost_multiplier},
			{"maintenance_multiplier", rti->maintenance_multiplier},
			{"max_speed", rti->max_speed == 0 ? json(nullptr) : json(rti->max_speed)}, {"curve_speed", rti->curve_speed},
			{"introduction_date", rti->introduction_date.base() < 0 ? json(nullptr) : json(rti->introduction_date.base())},
			{"powered", std::move(powered)}, {"compatible", std::move(compatible)}, {"available_to", std::move(companies)}});
	}
	return json{
		{"types", std::move(types)},
		{"build_cost", "RailBuildCost: build_rail (openttd://economy/prices) x cost_multiplier >> 3, in the game's money now; plain rail's multiplier is 8"},
		{"max_speed", "km-ish/h (1 unit = 1/1.6 mph); null: no limit of its own"},
		{"available_to", "the company ids that may build it now"},
	};
}

json Stations()
{
	json list = json::array();
	for (const Station *st : Station::Iterate()) {
		json facilities = json::array();
		if (st->facilities.Test(StationFacility::Train)) facilities.push_back("train");
		if (st->facilities.Test(StationFacility::TruckStop)) facilities.push_back("truck");
		if (st->facilities.Test(StationFacility::BusStop)) facilities.push_back("bus");
		if (st->facilities.Test(StationFacility::Airport)) facilities.push_back("airport");
		if (st->facilities.Test(StationFacility::Dock)) facilities.push_back("dock");
		/* The catchment the game itself uses (Station::GetCatchmentRadius, catchment_tiles), so a reader
		 * judges coverage and duplicates from the game's rules rather than a copy of its constants. */
		json area = nullptr;
		uint min_x = UINT_MAX, min_y = UINT_MAX, max_x = 0, max_y = 0;
		for (TileIndex t : st->catchment_tiles) {
			min_x = std::min(min_x, TileX(t)); max_x = std::max(max_x, TileX(t));
			min_y = std::min(min_y, TileY(t)); max_y = std::max(max_y, TileY(t));
		}
		if (min_x != UINT_MAX) area = {{"x", min_x}, {"y", min_y}, {"w", max_x - min_x + 1}, {"h", max_y - min_y + 1}};
		list.push_back({{"id", st->index.base()}, {"name", st->GetCachedName()}, {"tile", st->xy.base()},
			{"x", TileX(st->xy)}, {"y", TileY(st->xy)}, {"owner", st->owner.base()}, {"facilities", std::move(facilities)},
			{"town", st->town != nullptr ? json(st->town->index.base()) : json(nullptr)},
			{"catchment_radius", st->GetCatchmentRadius()}, {"catchment_area", std::move(area)}});
	}
	return json{{"stations", std::move(list)}};
}

json Depots()
{
	json list = json::array();
	for (const Depot *d : Depot::Iterate()) {
		const TileIndex t = d->xy;
		json entry = {{"id", d->index.base()}, {"tile", t.base()}, {"x", TileX(t)}, {"y", TileY(t)},
			{"owner", GetTileOwner(t).base()},
			{"town", d->town != nullptr ? json(d->town->index.base()) : json(nullptr)}};
		std::optional<DiagDirection> dir;
		switch (GetDepotVehicleType(t)) {
			case VehicleType::Road: entry["type"] = "road"; dir = GetRoadDepotDirection(t); break;
			case VehicleType::Train: entry["type"] = "rail"; dir = GetRailDepotDirection(t); break;
			case VehicleType::Ship: entry["type"] = "ship"; break;
			default: entry["type"] = "other"; break;
		}
		/* The tile a vehicle enters from: where the depot joins its network. */
		if (dir.has_value()) {
			const TileIndex front = TileAddByDiagDir(t, *dir);
			entry["entrance"] = {{"tile", front.base()}, {"x", TileX(front)}, {"y", TileY(front)}};
		}
		list.push_back(std::move(entry));
	}
	/* How far a vehicle looks for a depot to be serviced at, in tiles: the game's own setting, so a
	 * reader judges which depots serve the same vehicles by the rule the pathfinder applies. */
	const uint reach = _settings_game.pf.yapf.maximum_go_to_depot_penalty / YAPF_TILE_LENGTH;
	return json{{"depots", std::move(list)}, {"service_reach_tiles", reach}};
}

json Engines()
{
	static constexpr std::string_view TYPES[] = {"rail", "road", "ship", "air"};
	json list = json::array();
	for (const Engine *e : Engine::Iterate()) {
		if (!e->IsEnabled() || e->company_avail.None()) continue;
		auto t = static_cast<size_t>(e->type);
		if (t >= std::size(TYPES)) continue;

		/* The cargo it carries most of, as the Script API's GetCargoType does. */
		CargoArray cap = GetCapacityOfArticulatedParts(e->index);
		auto it = std::max_element(std::cbegin(cap), std::cend(cap));
		json cargo = nullptr;
		uint capacity = 0;
		if (*it != 0) {
			capacity = *it;
			cargo = GetString(CargoSpec::Get(CargoType(std::distance(std::cbegin(cap), it)))->name);
		}

		uint speed = e->GetDisplayMaxSpeed(); // km-ish/h as the player reads it
		uint effective = (e->type == VehicleType::Aircraft) ? speed / _settings_game.vehicle.plane_speed : speed;
		json buildable_by = json::array();
		for (const Company *c : Company::Iterate()) {
			if (IsEngineBuildable(e->index, e->type, c->index)) buildable_by.push_back(c->index.base());
		}
		json item = {
			{"id", e->index.base()}, {"type", TYPES[t]}, {"name", GetString(STR_ENGINE_NAME, e->index)},
			{"intro_year", TimerGameCalendar::ConvertDateToYMD(e->intro_date).year.base()},
			{"max_speed_kmh", speed}, {"effective_speed_kmh", effective},
			{"capacity", capacity}, {"cargo", cargo},
			{"price", static_cast<int64_t>(e->GetCost())}, {"running_cost_per_year", static_cast<int64_t>(e->GetRunningCost())},
			{"reliability_pct", ToPercent16(e->reliability)}, {"buildable_by", std::move(buildable_by)},
		};
		if (e->type == VehicleType::Aircraft) {
			uint8_t sub = AircraftVehInfo(e->index)->subtype;
			item["plane_type"] = sub == 0 ? "helicopter" : ((sub & AIR_FAST) ? "big" : "small");
		}
		list.push_back(std::move(item));
	}
	/* The cargos those engines carry, with what the game pays for them (GetTransportedGoodsIncome:
	 * distance x time factor x current_payment / 2^21, the factor from transit_periods), so a planner
	 * prices passengers, mail and freight with the game's own numbers. */
	json cargos = json::array();
	for (const CargoSpec *cs : CargoSpec::Iterate()) {
		cargos.push_back({{"id", cs->Index()}, {"name", GetString(cs->name)},
			{"payment", static_cast<int64_t>(cs->current_payment)},
			{"transit_periods", json::array({cs->transit_periods[0], cs->transit_periods[1]})},
			{"freight", cs->is_freight}, {"passengers", cs->classes.Test(CargoClass::Passengers)},
			{"mail", cs->classes.Test(CargoClass::Mail)}});
	}
	return json{{"engines", std::move(list)}, {"cargos", std::move(cargos)},
		{"note", "every engine some company can buy now; effective_speed_kmh divides aircraft by the plane_speed setting; price and running cost in the game's base currency, after inflation; cargos: payment rate (after inflation) and transit periods of every cargo, the inputs of the game's payment formula"}};
}

} // namespace PrismaticMCP::World

#endif /* OTTD_PRISMATIC_MCP_SERVER */
