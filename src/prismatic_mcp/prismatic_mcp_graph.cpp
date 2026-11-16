/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 *
 * The world as data a planner can trust, read from the engine's own map and pools:
 *  - openttd://map/ways: per tile, where a road vehicle can leave it (road bits, including drive-
 *    through and bay stops, depots, level crossings, road bridges and tunnels) and which rail
 *    tracks it has. Adjacency from these bits is real: two road tiles are joined only when each
 *    has the bit pointing at the other, which tile types alone cannot tell. Three more layers a
 *    planner needs: the foundation each tile stands on, both ends of every bridge and tunnel with
 *    its length, and which vehicles occupy which tile right now.
 *  - openttd://world/graph: one typed graph of towns, industries, stations and vehicles, with
 *    edges for stations serving towns, industries near towns, vehicle orders and the cargo links
 *    of the link graph (capacity and usage), schema world-graph/v1.
 * Read-only: no command, no game state written.
 */

/** @file prismatic_mcp_graph.cpp Road and rail ways per tile, and the world as one typed graph (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_graph.h"

#include "../cargotype.h"
#include "../industry.h"
#include "../industrytype.h"
#include "../linkgraph/linkgraph.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../rail_map.h"
#include "../road_func.h"
#include "../road_map.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../strings_func.h"
#include "../tile_cmd.h"
#include "../town.h"
#include "../track_func.h"
#include "../tunnel_map.h"
#include "../tunnelbridge.h"
#include "../tunnelbridge_map.h"
#include "../vehicle_base.h"

#include "../safeguards.h"

namespace PrismaticMCP::Graph {

using json = nlohmann::json;

namespace {

constexpr std::string_view HEX = "0123456789abcdef";

/** The one track along an axis, as track bits. */
uint AxisTrack(Axis a)
{
	return static_cast<uint>(TrackBits{AxisToTrack(a)}.base());
}

/** Where a road vehicle can leave a tile, as RoadBits (NW 1, SW 2, SE 4, NE 8); 0 when nowhere. */
uint RoadWays(TileIndex t)
{
	switch (GetTileType(t)) {
		case TileType::Road:
			if (IsRoadDepot(t)) return DiagDirToRoadBits(GetRoadDepotDirection(t)).base();
			if (IsLevelCrossing(t)) return GetCrossingRoadBits(t).base();
			return (GetRoadBits(t, RoadTramType::Road) | GetRoadBits(t, RoadTramType::Tram)).base();
		case TileType::Station:
			if (IsDriveThroughStopTile(t)) return AxisToRoadBits(GetDriveThroughStopAxis(t)).base();
			if (IsBayRoadStopTile(t)) return DiagDirToRoadBits(GetBayRoadStopDir(t)).base();
			return 0;
		case TileType::TunnelBridge:
			if (GetTunnelBridgeTransportType(t) != TransportType::Road) return 0;
			/* The head opens towards the map on one side and into the bridge or tunnel on the other. */
			return AxisToRoadBits(DiagDirToAxis(GetTunnelBridgeDirection(t))).base();
		default:
			return 0;
	}
}

/** The rail tracks of a tile as TrackBits (six bits); 0 when it has none. */
uint RailWays(TileIndex t)
{
	switch (GetTileType(t)) {
		case TileType::Railway:
			return IsPlainRail(t) ? static_cast<uint>(GetTrackBits(t).base()) : AxisTrack(DiagDirToAxis(GetRailDepotDirection(t)));
		case TileType::Road:
			return IsLevelCrossing(t) ? AxisTrack(GetCrossingRailAxis(t)) : 0;
		case TileType::Station:
			return HasStationRail(t) ? AxisTrack(GetRailStationAxis(t)) : 0;
		case TileType::TunnelBridge:
			return GetTunnelBridgeTransportType(t) == TransportType::Rail ? AxisTrack(DiagDirToAxis(GetTunnelBridgeDirection(t))) : 0;
		default:
			return 0;
	}
}

std::string CargoName(CargoType c)
{
	const CargoSpec *cs = CargoSpec::Get(c);
	return cs != nullptr && cs->IsValid() ? GetString(cs->name) : std::string{};
}

/** The foundation the tile's content stands on, as the engine's Foundation value (0 none). */
uint FoundationOf(TileIndex t)
{
	return static_cast<uint>(_tile_type_procs[GetTileType(t)]->get_foundation_proc(t, GetTileSlope(t)));
}

std::string_view TransportName(TransportType tt)
{
	switch (tt) {
		case TransportType::Rail: return "rail";
		case TransportType::Road: return "road";
		case TransportType::Water: return "water";
		default: return "other";
	}
}

std::string_view VehicleTypeName(VehicleType vt)
{
	switch (vt) {
		case VehicleType::Train: return "train";
		case VehicleType::Road: return "road";
		case VehicleType::Ship: return "ship";
		case VehicleType::Aircraft: return "aircraft";
		default: return "other";
	}
}

json Node(std::string id, std::string kind, TileIndex tile)
{
	return json{{"id", std::move(id)}, {"kind", std::move(kind)}, {"tile", tile.base()}, {"x", TileX(tile)}, {"y", TileY(tile)}};
}

} // namespace

json Ways()
{
	const uint w = Map::SizeX(), h = Map::SizeY();
	json roads = json::array(), rails = json::array();
	json foundations = json::array(), crossings = json::array();
	std::string rrow, trow, frow;
	for (uint y = 0; y < h; y++) {
		rrow.clear();
		trow.clear();
		frow.clear();
		for (uint x = 0; x < w; x++) {
			TileIndex t = TileXY(x, y);
			uint r = RoadWays(t), k = RailWays(t);
			rrow += HEX[r & 0xF];
			trow += HEX[(k >> 4) & 0xF];
			trow += HEX[k & 0xF];
			frow += HEX[FoundationOf(t) & 0xF];
			/* Each bridge or tunnel once, from the head nearer the map origin. */
			if (IsTileType(t, TileType::TunnelBridge)) {
				TileIndex other = GetOtherTunnelBridgeEnd(t);
				if (t < other) {
					crossings.push_back({
						{"kind", IsTunnel(t) ? "tunnel" : "bridge"}, {"transport", TransportName(GetTunnelBridgeTransportType(t))},
						{"from", {{"x", x}, {"y", y}}}, {"to", {{"x", TileX(other)}, {"y", TileY(other)}}},
						{"length", GetTunnelBridgeLength(t, other)}, {"direction", static_cast<int>(GetTunnelBridgeDirection(t))},
						{"owner", GetTileOwner(t).base()},
					});
				}
			}
		}
		roads.push_back(rrow);
		rails.push_back(trow);
		foundations.push_back(frow);
	}
	/* Occupancy: every tile with a ground vehicle on it now, naming the vehicles (their front part). */
	std::map<uint, std::pair<std::set<uint>, std::set<std::string_view>>> occupied;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type != VehicleType::Train && v->type != VehicleType::Road && v->type != VehicleType::Ship) continue;
		if (v->tile >= Map::Size()) continue;
		auto &o = occupied[v->tile.base()];
		o.first.insert(v->First()->index.base());
		o.second.insert(VehicleTypeName(v->type));
	}
	json occupancy = json::array();
	for (const auto &[tile, o] : occupied) {
		occupancy.push_back({{"x", TileX(TileIndex{tile})}, {"y", TileY(TileIndex{tile})}, {"vehicles", o.first}, {"types", o.second}});
	}
	return json{
		{"schema", "map-ways/v1"}, {"width", w}, {"height", h}, {"tile_index", "y * width + x"},
		{"encoding", {
			{"roads", "one string per row (y), one hex digit per tile (x): the directions a road vehicle can leave the tile, a sum of NW=1 (y-1), SW=2 (x+1), SE=4 (y+1), NE=8 (x-1). Covers road pieces, drive-through and bay stops, depots (their entrance), level crossings and road bridge or tunnel heads. Two tiles are joined when each points at the other."},
			{"rails", "one string per row, two hex digits per tile: the TrackBits (X=1, Y=2, upper=4, lower=8, left=16, right=32) of plain rail, rail stations, depots, crossings and rail bridge or tunnel heads"},
			{"foundations", "one string per row, one hex digit per tile: the foundation the tile's content stands on (0 none, 1 leveled, 2 inclined along x, 3 inclined along y, 4 steep lower, 5 steep both, 6-9 half tile W/S/E/N, a-d rail anti-zig-zag W/S/E/N)"},
			{"crossings", "every bridge and tunnel once: both heads, length in middle tiles, transport, the direction from the first head (0 NE, 1 SE, 2 SW, 3 NW) and the owner"},
			{"occupancy", "every tile with a train, road vehicle or ship on it now, with the vehicles' ids (the front part) and types"},
		}},
		{"roads", std::move(roads)}, {"rails", std::move(rails)}, {"foundations", std::move(foundations)},
		{"crossings", std::move(crossings)}, {"occupancy", std::move(occupancy)},
	};
}

json World()
{
	json nodes = json::array(), edges = json::array();
	for (const Town *t : Town::Iterate()) {
		json n = Node(fmt::format("town:{}", t->index.base()), "town", t->xy);
		n["name"] = t->GetCachedName();
		n["population"] = t->cache.population;
		nodes.push_back(std::move(n));
	}
	for (const Industry *i : Industry::Iterate()) {
		json n = Node(fmt::format("industry:{}", i->index.base()), "industry", i->location.tile);
		n["name"] = GetString(GetIndustrySpec(i->type)->name);
		json produced = json::array(), accepted = json::array();
		for (const auto &p : i->produced) if (IsValidCargoType(p.cargo)) produced.push_back({{"cargo", p.cargo}, {"name", CargoName(p.cargo)}, {"last_month", p.history[LAST_MONTH].production}});
		for (const auto &a : i->accepted) if (IsValidCargoType(a.cargo)) accepted.push_back({{"cargo", a.cargo}, {"name", CargoName(a.cargo)}});
		n["produces"] = std::move(produced);
		n["accepts"] = std::move(accepted);
		nodes.push_back(std::move(n));
		if (i->town != nullptr) edges.push_back({{"from", fmt::format("industry:{}", i->index.base())}, {"to", fmt::format("town:{}", i->town->index.base())}, {"kind", "near_town"}});
	}
	for (const Station *st : Station::Iterate()) {
		json n = Node(fmt::format("station:{}", st->index.base()), "station", st->xy);
		n["name"] = st->GetCachedName();
		n["owner"] = st->owner.base();
		json waiting = json::array();
		for (CargoType c : EnumRange(NUM_CARGO)) {
			const GoodsEntry &ge = st->goods[c];
			if (!ge.HasData() || ge.GetData().cargo.TotalCount() == 0) continue;
			waiting.push_back({{"cargo", c}, {"name", CargoName(c)}, {"count", ge.GetData().cargo.TotalCount()}, {"rating", ge.rating}});
		}
		n["waiting"] = std::move(waiting);
		nodes.push_back(std::move(n));
		if (st->town != nullptr) edges.push_back({{"from", fmt::format("station:{}", st->index.base())}, {"to", fmt::format("town:{}", st->town->index.base())}, {"kind", "serves_town"}});
	}
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!v->IsPrimaryVehicle()) continue;
		const std::string vid = fmt::format("vehicle:{}", v->index.base());
		json n = Node(vid, "vehicle", TileVirtXY(v->x_pos, v->y_pos));
		n["type"] = static_cast<int>(v->type);
		n["owner"] = v->owner.base();
		n["profit_this_year"] = static_cast<int64_t>(v->GetDisplayProfitThisYear());
		nodes.push_back(std::move(n));
		int seq = 0;
		for (const Order &o : v->Orders()) {
			if (o.IsType(OT_GOTO_STATION) || o.IsType(OT_IMPLICIT)) {
				edges.push_back({{"from", vid}, {"to", fmt::format("station:{}", o.GetDestination().ToStationID().base())}, {"kind", "order"}, {"seq", seq}});
			}
			seq++;
		}
	}
	for (const LinkGraph *lg : LinkGraph::Iterate()) {
		const CargoType cargo = lg->Cargo();
		for (NodeID from = 0; from < lg->Size(); from++) {
			const LinkGraph::BaseNode &node = (*lg)[from];
			for (const LinkGraph::BaseEdge &e : node.edges) {
				if (e.dest_node >= lg->Size()) continue;
				edges.push_back({{"from", fmt::format("station:{}", node.station.base())}, {"to", fmt::format("station:{}", (*lg)[e.dest_node].station.base())},
					{"kind", "cargo_link"}, {"cargo", cargo}, {"cargo_name", CargoName(cargo)}, {"capacity", e.capacity}, {"usage", e.usage}});
			}
		}
	}
	return json{{"schema", "world-graph/v1"}, {"nodes", std::move(nodes)}, {"edges", std::move(edges)}};
}

} // namespace PrismaticMCP::Graph

#endif /* OTTD_PRISMATIC_MCP_SERVER */
