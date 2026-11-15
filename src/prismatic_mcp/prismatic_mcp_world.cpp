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
#include "../map_func.h"
#include "../station_base.h"
#include "../tile_map.h"
#include "../town.h"

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
		list.push_back({{"id", st->index.base()}, {"name", st->GetCachedName()}, {"tile", st->xy.base()},
			{"x", TileX(st->xy)}, {"y", TileY(st->xy)}, {"owner", st->owner.base()}, {"facilities", std::move(facilities)},
			{"town", st->town != nullptr ? json(st->town->index.base()) : json(nullptr)}});
	}
	return json{{"stations", std::move(list)}};
}

} // namespace PrismaticMCP::World

#endif /* OTTD_PRISMATIC_MCP_SERVER */
