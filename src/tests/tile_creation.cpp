/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tile_creation.cpp Test low-level Map Tile creation functions. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../clear_map.h"
#include "../rail_map.h"
#include "../town_map.h"
#include "../tree_map.h"
#include "../station_map.h"
#include "../industry_map.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../object_map.h"

#include "../safeguards.h"

/**
 * Helper function to create a tile in a map for testing.
 * All fields are initialized to ones, so the tests can verify that individual bits were set as documented.
 */
static Tile TaintedMockTile()
{
	Map::Allocate(64, 64);
	Tile t(TileXY(32, 32));
	t.type() = 0xFF;
	t.height() = 0xFF;
	t.m1() = 0xFF;
	t.m2() = 0xFFFF;
	t.m3() = 0xFF;
	t.m4() = 0xFF;
	t.m5() = 0xFF;
	t.m6() = 0xFF;
	t.m7() = 0xFF;
	t.m8() = 0xFFFF;
	return t;
}

TEST_CASE("MakeTile - MakeClear")
{
	Tile t = TaintedMockTile();

	const uint density = 3;
	MakeClear(t, CLEAR_GRASS, density);

	CHECK(IsClearGround(t, CLEAR_GRASS));
	CHECK_FALSE(IsSnowTile(t));

	// Check that unused bits according to docs/landscape_grid.html are cleared.
	// The vast majority of calls to other MakeXXX are preceded by MakeClear,
	// so we check that MakeClear creates a clean tile.
	REQUIRE(GB(t.m1(), 5, 3) == 0);
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 0, 4) == 0);
	REQUIRE(GB(t.m4(), 0, 2) == 0);
	REQUIRE(GB(t.m6(), 0, 2) == 0);
	REQUIRE(GB(t.m6(), 5, 3) == 0);
	REQUIRE(t.m7() == 0);
	REQUIRE(t.m8() == 0);
}


TEST_CASE("MakeTile - MakeField")
{
	Tile t = TaintedMockTile();

	const uint field_type = 9; // maximum legal value, according to docs/landscape.html
	const IndustryID industry_id = IndustryID(42); // randomly chosen. Industry pool is not initialized in this unit test.
	MakeField(t, field_type, industry_id);

	CHECK(IsClearGround(t, CLEAR_FIELDS));
	CHECK(GetFieldType(t) == field_type);
	CHECK(GetIndustryIndexOfField(t) == industry_id);
}

TEST_CASE("MakeTile - MakeRailNormal")
{
	Tile t = TaintedMockTile();

	MakeRailNormal(t, OWNER_NONE, TRACK_BIT_ALL, RAILTYPE_MAGLEV);

	CHECK(IsPlainRailTile(t));

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 6, 2) == 0); // hard-coded
}

TEST_CASE("MakeTile - MakeRailDepot")
{
	Tile t = TaintedMockTile();

	const DepotID depot_id = DepotID(42); // randomly chosen.
	MakeRailDepot(t, OWNER_NONE, depot_id, DIAGDIR_NW, RAILTYPE_MAGLEV);

	CHECK(IsRailDepotTile(t));
	CHECK(GetRailDepotDirection(t) == DIAGDIR_NW);

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 6, 2) == 0b11); // hard-coded
}

TEST_CASE("MakeTile - MakeRoadNormal")
{
	Tile t = TaintedMockTile();

	const TownID town_id = TownID(42); // randomly chosen.
	MakeRoadNormal(t, ROAD_ALL, INVALID_ROADTYPE, INVALID_ROADTYPE, town_id, OWNER_NONE, OWNER_NONE);

	CHECK(IsNormalRoadTile(t));

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 6, 2) == 0); // hard-coded
}

TEST_CASE("MakeTile - MakeRoadDepot")
{
	Tile t = TaintedMockTile();

	const DepotID depot_id = DepotID(42); // randomly chosen.
	MakeRoadDepot(t, OWNER_NONE, depot_id, DIAGDIR_NW, INVALID_ROADTYPE);

	CHECK(IsRoadDepotTile(t));
	CHECK(GetRoadDepotDirection(t) == DIAGDIR_NW);

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 6, 2) == 0b10); // hard-coded
}

TEST_CASE("MakeTile - MakeHouseTile")
{
	Tile t = TaintedMockTile();

	const TownID town_id = TownID(42); // randomly chosen.
	const HouseID house_id = 4095; // large number, NUM_HOUSES - 1
	const uint8_t random_bits = 0xFF;
	MakeClear(t, CLEAR_GRASS, 3); // A HouseTile needs to be placed on MP_CLEAR.
	MakeHouseTile(t, town_id, 0, TOWN_HOUSE_COMPLETED, house_id, random_bits, true);

	CHECK(IsTileType(t, MP_HOUSE));
	CHECK(IsHouseCompleted(t));
}

TEST_CASE("MakeTile - MakeTree")
{
	Tile t = TaintedMockTile();

	const uint tree_count = 3;
	MakeTree(t, TREE_INVALID, tree_count, TreeGrowthStage::Growing1, TreeGround::TREE_GROUND_GRASS, 3);

	CHECK(GetTreeCount(t) == tree_count + 1);
	CHECK(GetTreeGrowth(t) == TreeGrowthStage::Growing1);
	CHECK(GetTreeGround(t) == TreeGround::TREE_GROUND_GRASS);
}

TEST_CASE("MakeTile - MakeRailStation")
{
	Tile t = TaintedMockTile();

	const StationID sid = StationID(42); // randomly chosen.
	const uint8_t section = 0;
	MakeRailStation(t, OWNER_NONE, sid, AXIS_X, section, RAILTYPE_MAGLEV);

	CHECK(IsRailStationTile(t));
	CHECK(GetStationIndex(t) == sid);

	REQUIRE(GB(t.m6(), 0, 2) == 0b11); // Animated tile state is preserved.
}

TEST_CASE("MakeTile - MakeAirport")
{
	Tile t = TaintedMockTile();

	const StationID sid = StationID(42); // randomly chosen.
	const uint8_t section = 0;
	MakeAirport(t, OWNER_NONE, sid, section, WATER_CLASS_INVALID);

	CHECK(IsAirport(t));
	CHECK(GetStationIndex(t) == sid);

	REQUIRE(GB(t.m6(), 0, 2) == 0b11); // Animated tile state is preserved.
}

TEST_CASE("MakeTile - MakeOilrig")
{
	Tile t = TaintedMockTile();

	const StationID sid = StationID(42); // randomly chosen.
	MakeOilrig(t, sid, WATER_CLASS_SEA);

	CHECK(IsOilRig(t));
	CHECK(GetStationIndex(t) == sid);
	CHECK(GetWaterClass(t) == WATER_CLASS_SEA);

	REQUIRE(GB(t.m6(), 0, 2) == 0b11); // Animated tile state is preserved.
}

TEST_CASE("MakeTile - MakeSea")
{
	Tile t = TaintedMockTile();

	MakeSea(t);

	CHECK(IsSea(t));
	CHECK(IsWaterTile(t));
	CHECK(GetWaterClass(t) == WATER_CLASS_SEA);
}

TEST_CASE("MakeTile - MakeCanal")
{
	Tile t = TaintedMockTile();

	const uint8_t random_bits = 0xFF;
	MakeCanal(t, OWNER_NONE, random_bits);

	CHECK(IsCanal(t));
	CHECK(IsWaterTile(t));
	CHECK(GetWaterClass(t) == WATER_CLASS_CANAL);
}

TEST_CASE("MakeTile - MakeShore")
{
	Tile t = TaintedMockTile();

	MakeShore(t);

	CHECK(IsCoast(t));
	CHECK_FALSE(IsWaterTile(t));
	CHECK(GetWaterClass(t) == WATER_CLASS_SEA);

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 4, 4) == 0b0001); // hard-coded
}

TEST_CASE("MakeTile - MakeShipDepot")
{
	Tile t = TaintedMockTile();

	const DepotID depot_id = DepotID(42); // randomly chosen.
	MakeShipDepot(t, OWNER_NONE, depot_id, DEPOT_PART_SOUTH, AXIS_Y, WATER_CLASS_SEA);

	CHECK(IsShipDepotTile(t));
	CHECK(GetShipDepotAxis(t) == AXIS_Y);
	CHECK(GetShipDepotPart(t) == DEPOT_PART_SOUTH);
	CHECK(GetWaterClass(t) == WATER_CLASS_SEA);

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 4, 4) == 0b0011); // hard-coded
}

TEST_CASE("MakeTile - MakeIndustry")
{
	Tile t = TaintedMockTile();

	const IndustryID industry_id = IndustryID(42); // randomly chosen.
	const IndustryGfx gfx = 0xFFFF; // randomly chosen.
	const uint8_t random_bits = 0xFF;
	MakeIndustry(t, industry_id, gfx, random_bits, WATER_CLASS_INVALID);

	CHECK(IsTileType(t, MP_INDUSTRY));
	CHECK(GetIndustryIndex(t) == industry_id);
	CHECK(GetIndustryRandomBits(t) == random_bits);

	REQUIRE(GB(t.m6(), 0, 2) == 0b11); // Animated tile state is preserved.
}

TEST_CASE("MakeTile - MakeRailTunnel")
{
	Tile t = TaintedMockTile();

	MakeRailTunnel(t, OWNER_NONE, DIAGDIR_NW, RAILTYPE_MAGLEV);

	CHECK(IsTunnelTile(t));
	CHECK(GetTunnelBridgeDirection(t) == DIAGDIR_NW);
	CHECK(GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL);

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 7, 1) == 0); // hard-coded
}

TEST_CASE("MakeTile - MakeRoadTunnel")
{
	Tile t = TaintedMockTile();

	MakeRoadTunnel(t, OWNER_NONE, DIAGDIR_NW, INVALID_ROADTYPE, INVALID_ROADTYPE);

	CHECK(IsTunnelTile(t));
	CHECK(GetTunnelBridgeDirection(t) == DIAGDIR_NW);
	CHECK(GetTunnelBridgeTransportType(t) == TRANSPORT_ROAD);

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 7, 1) == 0); // hard-coded
}

TEST_CASE("MakeTile - MakeRailBridgeRamp")
{
	Tile t = TaintedMockTile();

	const BridgeType tubular_silicon = 0xC; // see docs/landscape.html
	MakeRailBridgeRamp(t, OWNER_NONE, tubular_silicon, DIAGDIR_NW, RAILTYPE_MAGLEV);

	CHECK(IsBridgeTile(t));
	CHECK(GetBridgeType(t) == tubular_silicon);
	CHECK(GetTunnelBridgeDirection(t) == DIAGDIR_NW);
	CHECK(GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL);

	// Check specific bits according to docs/landscape_grid.html
	REQUIRE(GB(t.m5(), 7, 1) == 1); // hard-coded
}

TEST_CASE("MakeTile - MakeObject")
{
	Tile t = TaintedMockTile();

	const ObjectID object_id = ObjectID(42); // randomly chosen.
	const uint8_t random_bits = 0xFF;
	MakeObject(t, OWNER_NONE, object_id, WATER_CLASS_INVALID, random_bits);

	CHECK(IsTileType(t, MP_OBJECT));
	CHECK(GetObjectIndex(t) == object_id);
	CHECK(GetObjectRandomBits(t) == random_bits);

	REQUIRE(GB(t.m6(), 0, 2) == 0b11); // Animated tile state is preserved.
}
