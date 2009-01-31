/* $Id$ */

/** @file ai_tile.cpp Implementation of AITile. */

#include "ai_tile.hpp"
#include "ai_map.hpp"
#include "ai_town.hpp"
#include "../../station_func.h"
#include "../../company_func.h"
#include "../../road_map.h"
#include "../../water_map.h"
#include "../../clear_map.h"
#include "../../town.h"

/* static */ bool AITile::IsBuildable(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	switch (::GetTileType(tile)) {
		default: return false;
		case MP_CLEAR: return true;
		case MP_TREES: return true;
		case MP_WATER: return IsCoast(tile);
		case MP_ROAD:
			/* Tram bits aren't considered buildable */
			if (::GetRoadTypes(tile) != ROADTYPES_ROAD) return false;
			/* Depots and crossings aren't considered buildable */
			if (::GetRoadTileType(tile) != ROAD_TILE_NORMAL) return false;
			if (CountBits(::GetRoadBits(tile, ROADTYPE_ROAD)) != 1) return false;
			if (::IsRoadOwner(tile, ROADTYPE_ROAD, OWNER_TOWN)) return true;
			if (::IsRoadOwner(tile, ROADTYPE_ROAD, _current_company)) return true;
			return false;
	}
}

/* static */ bool AITile::IsBuildableRectangle(TileIndex tile, uint width, uint height)
{
	uint tx, ty;

	tx = AIMap::GetTileX(tile);
	ty = AIMap::GetTileY(tile);

	for (uint x = tx; x < width + tx; x++) {
		for (uint y = ty; y < height + ty; y++) {
			if (!IsBuildable(AIMap::GetTileIndex(x, y))) return false;
		}
	}

	return true;
}

/* static */ bool AITile::IsWaterTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_WATER) && !::IsCoast(tile);
}

/* static */ bool AITile::IsCoastTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_WATER) && ::IsCoast(tile);
}

/* static */ bool AITile::IsStationTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION);
}

/* static */ bool AITile::IsSteepSlope(Slope slope)
{
	if (slope == SLOPE_INVALID) return false;

	return ::IsSteepSlope((::Slope)slope);
}

/* static */ bool AITile::IsHalftileSlope(Slope slope)
{
	if (slope == SLOPE_INVALID) return false;

	return ::IsHalftileSlope((::Slope)slope);
}

/* static */ bool AITile::HasTreeOnTile(TileIndex tile)
{
	return ::IsTileType(tile, MP_TREES);
}

/* static */ bool AITile::IsFarmTile(TileIndex tile)
{
	return (::IsTileType(tile, MP_CLEAR) && ::IsClearGround(tile, CLEAR_FIELDS));
}

/* static */ bool AITile::IsRockTile(TileIndex tile)
{
	return (::IsTileType(tile, MP_CLEAR) && ::IsClearGround(tile, CLEAR_ROCKS));
}

/* static */ bool AITile::IsRoughTile(TileIndex tile)
{
	return (::IsTileType(tile, MP_CLEAR) && ::IsClearGround(tile, CLEAR_ROUGH));
}

/* static */ bool AITile::IsSnowTile(TileIndex tile)
{
	return (::IsTileType(tile, MP_CLEAR) && ::IsClearGround(tile, CLEAR_SNOW));
}

/* static */ bool AITile::IsDesertTile(TileIndex tile)
{
	return (::IsTileType(tile, MP_CLEAR) && ::IsClearGround(tile, CLEAR_DESERT));
}

/* static */ AITile::Slope AITile::GetSlope(TileIndex tile)
{
	if (!::IsValidTile(tile)) return SLOPE_INVALID;

	return (Slope)::GetTileSlope(tile, NULL);
}

/* static */ AITile::Slope AITile::GetComplementSlope(Slope slope)
{
	if (slope == SLOPE_INVALID) return SLOPE_INVALID;
	if (IsSteepSlope(slope)) return SLOPE_INVALID;
	if (IsHalftileSlope(slope)) return SLOPE_INVALID;

	return (Slope)::ComplementSlope((::Slope)slope);
}

/* static */ int32 AITile::GetHeight(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::TileHeight(tile);
}

/* static */ AICompany::CompanyID AITile::GetOwner(TileIndex tile)
{
	if (!::IsValidTile(tile)) return AICompany::COMPANY_INVALID;
	if (::IsTileType(tile, MP_HOUSE)) return AICompany::COMPANY_INVALID;
	if (::IsTileType(tile, MP_INDUSTRY)) return AICompany::COMPANY_INVALID;

	return AICompany::ResolveCompanyID((AICompany::CompanyID)(byte)::GetTileOwner(tile));
}

/* static */ bool AITile::HasTransportType(TileIndex tile, TransportType transport_type)
{
	if (!::IsValidTile(tile)) return false;

	return ::TrackStatusToTrackdirBits(::GetTileTrackStatus(tile, (::TransportType)transport_type, UINT32_MAX)) != TRACKDIR_BIT_NONE;
}

/* static */ int32 AITile::GetCargoAcceptance(TileIndex tile, CargoID cargo_type, uint width, uint height, uint radius)
{
	if (!::IsValidTile(tile)) return false;

	AcceptedCargo accepts;
	::GetAcceptanceAroundTiles(accepts, tile, width, height, _settings_game.station.modified_catchment ? radius : (uint)CA_UNMODIFIED);
	return accepts[cargo_type];
}

/* static */ int32 AITile::GetCargoProduction(TileIndex tile, CargoID cargo_type, uint width, uint height, uint radius)
{
	if (!::IsValidTile(tile)) return false;

	AcceptedCargo produced;
	::GetProductionAroundTiles(produced, tile, width, height, _settings_game.station.modified_catchment ? radius : (uint)CA_UNMODIFIED);
	return produced[cargo_type];
}

/* static */ int32 AITile::GetDistanceManhattanToTile(TileIndex tile_from, TileIndex tile_to)
{
	return AIMap::DistanceManhattan(tile_from, tile_to);
}

/* static */ int32 AITile::GetDistanceSquareToTile(TileIndex tile_from, TileIndex tile_to)
{
	return AIMap::DistanceSquare(tile_from, tile_to);
}

/* static */ bool AITile::RaiseTile(TileIndex tile, int32 slope)
{
	EnforcePrecondition(false, tile < ::MapSize());

	return AIObject::DoCommand(tile, slope, 1, CMD_TERRAFORM_LAND);
}

/* static */ bool AITile::LowerTile(TileIndex tile, int32 slope)
{
	EnforcePrecondition(false, tile < ::MapSize());

	return AIObject::DoCommand(tile, slope, 0, CMD_TERRAFORM_LAND);
}

/* static */ bool AITile::LevelTiles(TileIndex start_tile, TileIndex end_tile)
{
	EnforcePrecondition(false, start_tile < ::MapSize());
	EnforcePrecondition(false, end_tile < ::MapSize());

	return AIObject::DoCommand(end_tile, start_tile, 0, CMD_LEVEL_LAND);
}

/* static */ bool AITile::DemolishTile(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ bool AITile::PlantTree(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, UINT_MAX, tile, CMD_PLANT_TREE);
}

/* static */ bool AITile::PlantTreeRectangle(TileIndex tile, uint width, uint height)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, width >= 1 && width <= 20);
	EnforcePrecondition(false, height >= 1 && height <= 20);
	TileIndex end_tile = tile + ::TileDiffXY(width - 1, height - 1);

	return AIObject::DoCommand(tile, UINT_MAX, end_tile, CMD_PLANT_TREE);
}

/* static */ bool AITile::IsWithinTownInfluence(TileIndex tile, TownID town_id)
{
	return AITown::IsWithinTownInfluence(town_id, tile);
}

/* static */ TownID AITile::GetClosestTown(TileIndex tile)
{
	if (!::IsValidTile(tile)) return INVALID_TOWN;

	return ::ClosestTownFromTile(tile, UINT_MAX)->index;
}
