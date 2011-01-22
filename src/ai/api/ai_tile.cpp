/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_tile.cpp Implementation of AITile. */

#include "../../stdafx.h"
#include "ai_tile.hpp"
#include "ai_map.hpp"
#include "ai_town.hpp"
#include "../../station_func.h"
#include "../../company_func.h"
#include "../../water_map.h"
#include "../../clear_map.h"
#include "../../tree_map.h"
#include "../../town.h"
#include "../../landscape.h"
#include "../../economy_func.h"

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
			if (!HasExactlyOneBit(::GetRoadBits(tile, ROADTYPE_ROAD))) return false;
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

	return (::IsTileType(tile, MP_WATER) && ::IsCoast(tile)) ||
		(::IsTileType(tile, MP_TREES) && ::GetTreeGround(tile) == TREE_GROUND_SHORE);
}

/* static */ bool AITile::IsStationTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION);
}

/* static */ bool AITile::IsSteepSlope(Slope slope)
{
	if ((slope & ~(SLOPE_ELEVATED | SLOPE_STEEP | SLOPE_HALFTILE_MASK)) != 0) return false;

	return ::IsSteepSlope((::Slope)slope);
}

/* static */ bool AITile::IsHalftileSlope(Slope slope)
{
	if ((slope & ~(SLOPE_ELEVATED | SLOPE_STEEP | SLOPE_HALFTILE_MASK)) != 0) return false;

	return ::IsHalftileSlope((::Slope)slope);
}

/* static */ bool AITile::HasTreeOnTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_TREES);
}

/* static */ bool AITile::IsFarmTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, MP_CLEAR) && ::IsClearGround(tile, CLEAR_FIELDS));
}

/* static */ bool AITile::IsRockTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, MP_CLEAR) && ::GetRawClearGround(tile) == ::CLEAR_ROCKS);
}

/* static */ bool AITile::IsRoughTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, MP_CLEAR) && ::GetRawClearGround(tile) == ::CLEAR_ROUGH);
}

/* static */ bool AITile::IsSnowTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, MP_CLEAR) && ::IsSnowTile(tile));
}

/* static */ bool AITile::IsDesertTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, MP_CLEAR) && ::IsClearGround(tile, CLEAR_DESERT));
}

/* static */ AITile::Slope AITile::GetSlope(TileIndex tile)
{
	if (!::IsValidTile(tile)) return SLOPE_INVALID;

	return (Slope)::GetTileSlope(tile, NULL);
}

/* static */ AITile::Slope AITile::GetComplementSlope(Slope slope)
{
	if ((slope & ~SLOPE_ELEVATED) != 0) return SLOPE_INVALID;

	return (Slope)::ComplementSlope((::Slope)slope);
}

/* static */ int32 AITile::GetMinHeight(TileIndex tile)
{
	if (!::IsValidTile(tile)) return -1;

	return ::GetTileZ(tile) / ::TILE_HEIGHT;
}

/* static */ int32 AITile::GetMaxHeight(TileIndex tile)
{
	if (!::IsValidTile(tile)) return -1;

	return ::GetTileMaxZ(tile) / ::TILE_HEIGHT;
}

/* static */ int32 AITile::GetCornerHeight(TileIndex tile, Corner corner)
{
	if (!::IsValidTile(tile) || !::IsValidCorner((::Corner)corner)) return -1;

	uint z;
	::Slope slope = ::GetTileSlope(tile, &z);
	return (z + ::GetSlopeZInCorner(slope, (::Corner)corner)) / ::TILE_HEIGHT;
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

/* static */ int32 AITile::GetCargoAcceptance(TileIndex tile, CargoID cargo_type, int width, int height, int radius)
{
	if (!::IsValidTile(tile) || width <= 0 || height <= 0 || radius < 0) return -1;

	CargoArray acceptance = ::GetAcceptanceAroundTiles(tile, width, height, _settings_game.station.modified_catchment ? radius : (int)CA_UNMODIFIED);
	return acceptance[cargo_type];
}

/* static */ int32 AITile::GetCargoProduction(TileIndex tile, CargoID cargo_type, int width, int height, int radius)
{
	if (!::IsValidTile(tile) || width <= 0 || height <= 0 || radius < 0) return -1;

	CargoArray produced = ::GetProductionAroundTiles(tile, width, height, _settings_game.station.modified_catchment ? radius : (int)CA_UNMODIFIED);
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

	return AIObject::DoCommand(end_tile, start_tile, LM_LEVEL << 1, CMD_LEVEL_LAND);
}

/* static */ bool AITile::DemolishTile(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ bool AITile::PlantTree(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, TREE_INVALID, tile, CMD_PLANT_TREE);
}

/* static */ bool AITile::PlantTreeRectangle(TileIndex tile, uint width, uint height)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, width >= 1 && width <= 20);
	EnforcePrecondition(false, height >= 1 && height <= 20);
	TileIndex end_tile = tile + ::TileDiffXY(width - 1, height - 1);

	return AIObject::DoCommand(tile, TREE_INVALID, end_tile, CMD_PLANT_TREE);
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

/* static */ Money AITile::GetBuildCost(BuildType build_type)
{
	switch (build_type) {
		case BT_FOUNDATION:   return ::GetPrice(PR_BUILD_FOUNDATION, 1, NULL);
		case BT_TERRAFORM:    return ::GetPrice(PR_TERRAFORM, 1, NULL);
		case BT_BUILD_TREES:  return ::GetPrice(PR_BUILD_TREES, 1, NULL);
		case BT_CLEAR_GRASS:  return ::GetPrice(PR_CLEAR_GRASS, 1, NULL);
		case BT_CLEAR_ROUGH:  return ::GetPrice(PR_CLEAR_ROUGH, 1, NULL);
		case BT_CLEAR_ROCKY:  return ::GetPrice(PR_CLEAR_ROCKS, 1, NULL);
		case BT_CLEAR_FIELDS: return ::GetPrice(PR_CLEAR_FIELDS, 1, NULL);
		case BT_CLEAR_HOUSE:  return ::GetPrice(PR_CLEAR_HOUSE, 1, NULL);
		default: return -1;
	}
}
