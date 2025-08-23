/**
 * @file tile_creation.cpp
 * Unit tests for tile creation functions Make{Clear,Field,RoadTunnel,...}.
 */
#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../clear_map.h"
#include "../object_map.h"
#include "../rail_map.h"
#include "../road_map.h"
#include "../station_map.h"
#include "../tile_map.h"
#include "../town_map.h"
#include "../tree_map.h"
#include "../tunnel_map.h"
#include "../water_map.h"
#include "../industry_map.h"
#include "../depot_map.h"

#include "../safeguards.h"


/**
 * Helper function to create a tile in a map for testing.
 * All fields are initialized to ones, so the tests can verify that the Make* functions cleanly initialize all fields.
 */
static Tile taintedMockTile()
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
	Tile t = taintedMockTile();

	MakeClear(t, CLEAR_GRASS, 2);

	CHECK(IsClearGround(t, CLEAR_GRASS));
	CHECK_FALSE(IsClearGround(t, CLEAR_FIELDS));
	CHECK(GetClearDensity(t) == 2);

	// Check that unused bits are cleared according to docs/landscape_grid.html
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
	Tile t = taintedMockTile();
	MakeField(t, 1, IndustryID(1));

	CHECK(IsClearGround(t, CLEAR_FIELDS));
	CHECK(GetIndustryIndexOfField(t) == IndustryID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(GB(t.m1(), 5, 3) == 0);
	REQUIRE(GB(t.m4(), 0, 2) == 0);
	//REQUIRE(GB(t.m6(), 0, 2) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m6(), 5, 3) == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	REQUIRE(t.m8() == 0);
}

TEST_CASE("MakeTile - MakeRailNormal")
{
	Tile t = taintedMockTile();
	MakeRailNormal(t, OWNER_NONE, TrackBits(0), RAILTYPE_MAGLEV);

	CHECK(IsPlainRailTile(t));
	CHECK(GetRailType(t) == RailType(RAILTYPE_MAGLEV));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	//REQUIRE(GB(t.m1(), 5, 2) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m2(), 0, 8) == 0);
	REQUIRE(GB(t.m2(), 12, 4) == 0);
	REQUIRE(t.m3() == 0);
	REQUIRE(GB(t.m4(), 4, 4) == 0);
	REQUIRE(GB(t.m5(), 6, 2) == 0); // not free, but hard-coded to 0
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	REQUIRE(GB(t.m8(), 6, 10) == 0);
}

TEST_CASE("MakeTile - MakeRailDepot")
{
	Tile t = taintedMockTile();
	MakeRailDepot(t, OWNER_NONE, DepotID(1), DIAGDIR_NE, RAILTYPE_RAIL);

	CHECK(IsRailDepot(t));
	CHECK(GetDepotIndex(t) == DepotID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	//REQUIRE(GB(t.m1(), 5, 2) == 0); FIXME: not cleared.
	REQUIRE(t.m3() == 0);
	REQUIRE(GB(t.m4(), 4, 4) == 0);
	REQUIRE(GB(t.m5(), 2, 2) == 0);
	REQUIRE(GB(t.m5(), 5, 1) == 0);
	REQUIRE(GB(t.m5(), 6, 2) == 3); // not free, but hard-coded to 1
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	REQUIRE(GB(t.m8(), 6, 10) == 0);
}

TEST_CASE("MakeTile - MakeRoadNormal")
{
	Tile t = taintedMockTile();
	MakeRoadNormal(t, RoadBits(0), INVALID_ROADTYPE, INVALID_ROADTYPE, TownID(0), OWNER_NONE, OWNER_NONE);
	SetRoadside(t, ROADSIDE_PAVED_ROAD_WORKS);

	CHECK(IsNormalRoadTile(t));
	CHECK(GetRoadTypeRoad(t) == INVALID_ROADTYPE);

	// Check that unused bits are cleared according to docs/landscape_grid.html
	//REQUIRE(GB(t.m1(), 5, 3) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m4(), 6, 2) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m5(), 6, 2) == 0); // not free, but hard-coded to 0
	//REQUIRE(GB(t.m6(), 0, 3) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m6(), 6, 2) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m7(), 4, 1) == 0);
	REQUIRE(GB(t.m7(), 6, 2) == 0);
	//REQUIRE(GB(t.m8(), 0, 6) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m8(), 12, 4) == 0); FIXME: not cleared.
}

TEST_CASE("MakeTile - MakeRoadDepot")
{
	Tile t = taintedMockTile();
	MakeRoadDepot(t, OWNER_NONE, DepotID(1), DIAGDIR_NE, INVALID_ROADTYPE);

	CHECK(IsRoadDepot(t));
	CHECK(GetDepotIndex(t) == DepotID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	//REQUIRE(GB(t.m1(), 5, 3) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m3(), 0, 4) == 0);
	REQUIRE(GB(t.m4(), 6, 2) == 0);
	REQUIRE(GB(t.m5(), 2, 4) == 0);
	REQUIRE(GB(t.m5(), 6, 2) == 2); // not free, but hard-coded to 10b
	//REQUIRE(GB(t.m6(), 0, 3) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m6(), 6, 2) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m7(), 6, 2) == 0);
	REQUIRE(GB(t.m8(), 0, 6) == 0);
	REQUIRE(GB(t.m8(), 12, 4) == 0);
}

TEST_CASE("MakeTile - MakeHouseTile")
{
	Tile t = taintedMockTile();
	MakeClear(t, CLEAR_GRASS, 2); // A house needs to be placed on a clear tile.
	MakeHouseTile(t, TownID(8), 1, 1, HouseID(42), 0, false);

	CHECK(IsTileType(t, MP_HOUSE));
	CHECK(GetTownIndex(t) == TownID(8));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(GB(t.m3(), 6, 1) == 0);
	REQUIRE(t.m4() == 0);
	REQUIRE(GB(t.m8(), 12, 4) == 0);
}

TEST_CASE("MakeTile - MakeTree")
{
	Tile t = taintedMockTile();
	MakeTree(t, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TREE_GROUND_GRASS, 1);

	CHECK(IsTileType(t, MP_TREES));
	CHECK(GetTreeType(t) == TREE_TEMPERATE);
	CHECK(GetTreeCount(t) == 3);
	CHECK(GetTreeGrowth(t) == TreeGrowthStage::Grown);

	// Check that unused bits are cleared according to docs/landscape_grid.html
	//REQUIRE(GB(t.m1(), 7, 1) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m2(), 9, 7) == 0);
	REQUIRE(GB(t.m4(), 0, 8) == 0);
	REQUIRE(GB(t.m5(), 3, 3) == 0);
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}


TEST_CASE("MakeTile - MakeStation")
{
	Tile t = taintedMockTile();
	MakeStation(t, OWNER_NONE, StationID(1), StationType::Rail, 0);

	CHECK(IsTileType(t, MP_STATION));
	CHECK(GetStationType(t) == StationType::Rail);
	CHECK(GetStationIndex(t) == StationID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(GB(t.m1(), 7, 1) == 0);
	REQUIRE(GB(t.m3(), 3, 1) == 0);
	//REQUIRE(GB(t.m6(), 7, 1) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m8(), 6, 6) == 0);
}

TEST_CASE("MakeTile - MakeAirport")
{
	Tile t = taintedMockTile();
	MakeAirport(t, OWNER_NONE, StationID(1), 0, WATER_CLASS_SEA);

	CHECK(IsTileType(t, MP_STATION));
	CHECK(GetStationType(t) == StationType::Airport);
	CHECK(GetStationIndex(t) == StationID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(GB(t.m1(), 7, 1) == 0);
	REQUIRE(GB(t.m3(), 0, 4) == 0);
	REQUIRE(t.m4() == 0);
	//REQUIRE(GB(t.m6(), 2, 1) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m6(), 7, 1) == 0); FIXME: not cleared.
	REQUIRE(t.m8() == 0);
}

TEST_CASE("MakeTile - MakeOilrig")
{
	Tile t = taintedMockTile();
	MakeOilrig(t, StationID(1), WATER_CLASS_SEA);

	CHECK(IsTileType(t, MP_STATION));
	CHECK(GetStationType(t) == StationType::Oilrig);
	CHECK(GetStationIndex(t) == StationID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(GB(t.m1(), 7, 1) == 0);
	REQUIRE(t.m3() == 0);
	REQUIRE(t.m4() == 0);
	//REQUIRE(GB(t.m6(), 2, 1) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m6(), 7, 1) == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	REQUIRE(t.m8() == 0);
}

TEST_CASE("MakeTile - MakeWater")
{
	Tile t = taintedMockTile();
	MakeWater(t, OWNER_NONE, WATER_CLASS_SEA, 0);

	CHECK(IsWaterTile(t));
	CHECK(GetWaterClass(t) == WATER_CLASS_SEA);

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 1, 7) == 0);
	REQUIRE(t.m4() == 0);
	REQUIRE(GB(t.m5(), 0, 4) == 0);
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}

TEST_CASE("MakeTile - MakeCanal")
{
	Tile t = taintedMockTile();
	MakeCanal(t, OWNER_NONE, 0);

	CHECK(IsCanal(t));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 1, 7) == 0);
	REQUIRE(t.m4() == 0);
	REQUIRE(GB(t.m5(), 0, 4) == 0);
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}

TEST_CASE("MakeTile - MakeShore")
{
	Tile t = taintedMockTile();
	MakeShore(t);

	CHECK(IsCoastTile(t));
	CHECK(GetWaterClass(t) == WATER_CLASS_SEA);

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 1, 7) == 0);
	REQUIRE(t.m4() == 0);
	REQUIRE(GB(t.m5(), 0, 4) == 0);
	REQUIRE(GB(t.m5(), 4, 1) == 1); // hard-coded to 1
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}

TEST_CASE("MakeTile - MakeLock")
{
	Tile t = taintedMockTile();
	MakeLock(t, OWNER_NONE, DIAGDIR_NE, WATER_CLASS_CANAL, WATER_CLASS_CANAL, WATER_CLASS_CANAL);

	CHECK(IsLock(t));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 1, 7) == 0);
	REQUIRE(t.m4() == 0);
	REQUIRE(GB(t.m5(), 0, 4) == 0);
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}

TEST_CASE("MakeTile - MakeShipDepot")
{
	Tile t = taintedMockTile();
	MakeShipDepot(t, OWNER_NONE, DepotID(1), DEPOT_PART_SOUTH, AXIS_X, WATER_CLASS_SEA);

	CHECK(IsShipDepot(t));
	CHECK(GetDepotIndex(t) == DepotID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(GB(t.m3(), 1, 7) == 0);
	REQUIRE(t.m4() == 0);
	REQUIRE(GB(t.m5(), 2, 2) == 0);
	REQUIRE(GB(t.m5(), 4, 2) == 3); // hard-coded to 1
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(t.m7() == 0);
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}

TEST_CASE("MakeTile - MakeIndustry")
{
	Tile t = taintedMockTile();
	MakeIndustry(t, IndustryID(1), IndustryGfx(0), 0, WATER_CLASS_SEA);

	CHECK(IsTileType(t, MP_INDUSTRY));
	CHECK(GetIndustryIndex(t) == IndustryID(1));

	// Check that unused bits are cleared according to docs/landscape_grid.html
	REQUIRE(GB(t.m1(), 0, 5) == 0);
	//REQUIRE(GB(t.m6(), 6, 2) == 0); FIXME: not cleared.
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}

TEST_CASE("MakeTile - MakeRailTunnel")
{
	Tile t = taintedMockTile();

	MakeRailTunnel(t, OWNER_NONE, DIAGDIR_NE, INVALID_RAILTYPE);

	CHECK(IsTunnel(t));

	// Check that unused bits according to docs/landscape_grid.html are cleared.
	//REQUIRE(GB(t.m1(), 5, 2) == 0); FIXME: not cleared.
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 0, 4) == 0);
	REQUIRE(GB(t.m4(), 6, 2) == 0);
	REQUIRE(GB(t.m5(), 5, 2) == 0);
	REQUIRE(GB(t.m5(), 7, 1) == 0); // hard-coded to 0
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(GB(t.m7(), 6, 2) == 0);
	REQUIRE(GB(t.m8(), 12, 4) == 0);
}


TEST_CASE("MakeTile - MakeRoadTunnel")
{
	Tile t = taintedMockTile();

	MakeRoadTunnel(t, OWNER_NONE, DIAGDIR_NE, INVALID_ROADTYPE, INVALID_ROADTYPE);

	CHECK(IsTunnel(t));

	// Check that unused bits according to docs/landscape_grid.html are cleared.
	//REQUIRE(GB(t.m1(), 5, 2) == 0); FIXME: not cleared.
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 0, 4) == 0);
	REQUIRE(GB(t.m4(), 6, 2) == 0);
	REQUIRE(GB(t.m5(), 5, 2) == 0);
	REQUIRE(GB(t.m5(), 7, 1) == 0); // hard-coded to 0
	//REQUIRE(t.m6() == 0); FIXME: not cleared.
	REQUIRE(GB(t.m7(), 6, 2) == 0);
	REQUIRE(GB(t.m8(), 12, 4) == 0);
}

TEST_CASE("MakeTile - MakeBridgeRamp")
{
	Tile t = taintedMockTile();

	MakeBridgeRamp(t, OWNER_NONE, BridgeType(9), DIAGDIR_NE, TRANSPORT_ROAD);

	CHECK(IsBridge(t));
	CHECK(GetBridgeType(t) == 9);

	// Check that unused bits according to docs/landscape_grid.html are cleared.
	//REQUIRE(GB(t.m1(), 5, 2) == 0); FIXME: not cleared.
	REQUIRE(t.m2() == 0);
	REQUIRE(GB(t.m3(), 0, 4) == 0);
	REQUIRE(GB(t.m4(), 6, 2) == 0);
	REQUIRE(GB(t.m5(), 5, 2) == 0);
	REQUIRE(GB(t.m5(), 7, 1) == 1); // hard-coded to 1
	//REQUIRE(GB(t.m6(), 0, 2) == 0); FIXME: not cleared.
	//REQUIRE(GB(t.m6(), 6, 2) == 0); FIXME: not cleared.
	REQUIRE(GB(t.m7(), 6, 2) == 0);
	REQUIRE(GB(t.m8(), 12, 4) == 0);
}

TEST_CASE("MakeTile - MakeObject")
{
	Tile t = taintedMockTile();

	MakeObject(t, OWNER_NONE, ObjectID(1), WATER_CLASS_SEA, 0);

	CHECK(IsTileType(t, MP_OBJECT)); // IsObjectTypeTile
	CHECK(GetObjectIndex(t) == ObjectID(1));
	CHECK(GetWaterClass(t) == WATER_CLASS_SEA);

	// Check that unused bits are cleared according to docs/landscape_grid.html
	//REQUIRE(GB(t.m1(), 7, 1) == 0); FIXME: not cleared.
	REQUIRE(t.m4() == 0);
	//REQUIRE(GB(t.m6(), 2, 6) == 0); FIXME: not cleared.
	//REQUIRE(t.m8() == 0); FIXME: not cleared.
}
