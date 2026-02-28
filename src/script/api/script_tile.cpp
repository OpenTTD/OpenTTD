/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_tile.cpp Implementation of ScriptTile. */

#include "../../stdafx.h"
#include "script_tile.hpp"
#include "script_map.hpp"
#include "script_town.hpp"
#include "../../station_func.h"
#include "../../water_map.h"
#include "../../clear_map.h"
#include "../../tree_map.h"
#include "../../town.h"
#include "../../landscape.h"
#include "../../landscape_cmd.h"
#include "../../terraform_cmd.h"
#include "../../tree_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptTile::IsBuildable(TileIndex tile)
{
	EnforceDeityOrCompanyModeValid(false);
	if (!::IsValidTile(tile)) return false;

	switch (::GetTileType(tile)) {
		default: return false;
		case TileType::Clear: return true;
		case TileType::Trees: return true;
		case TileType::Water: return IsCoast(tile);
		case TileType::Road:
			/* Tram bits aren't considered buildable */
			if (::GetRoadTypeTram(tile) != INVALID_ROADTYPE) return false;
			/* Depots and crossings aren't considered buildable */
			if (::GetRoadTileType(tile) != RoadTileType::Normal) return false;
			if (!HasExactlyOneBit(::GetRoadBits(tile, RTT_ROAD))) return false;
			if (::IsRoadOwner(tile, RTT_ROAD, OWNER_TOWN)) return true;
			if (::IsRoadOwner(tile, RTT_ROAD, ScriptObject::GetCompany())) return true;
			return false;
	}
}

/* static */ bool ScriptTile::IsBuildableRectangle(TileIndex tile, SQInteger width, SQInteger height)
{
	/* Check whether we can extract valid X and Y */
	if (!::IsValidTile(tile) || width < 0 || height < 0) return false;

	uint tx = ScriptMap::GetTileX(tile);
	uint ty = ScriptMap::GetTileY(tile);

	for (uint x = tx; x < width + tx; x++) {
		for (uint y = ty; y < height + ty; y++) {
			if (!IsBuildable(ScriptMap::GetTileIndex(x, y))) return false;
		}
	}

	return true;
}

/* static */ bool ScriptTile::IsSeaTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::Water) && ::IsSea(tile);
}

/* static */ bool ScriptTile::IsRiverTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::Water) && ::IsRiver(tile);
}

/* static */ bool ScriptTile::IsWaterTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::Water) && !::IsCoast(tile);
}

/* static */ bool ScriptTile::IsCoastTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, TileType::Water) && ::IsCoast(tile)) ||
		(::IsTileType(tile, TileType::Trees) && ::GetTreeGround(tile) == TreeGround::Shore);
}

/* static */ bool ScriptTile::IsStationTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::Station);
}

/* static */ bool ScriptTile::IsSteepSlope(Slope slope)
{
	if ((slope & ~(SLOPE_ELEVATED | SLOPE_STEEP | SLOPE_HALFTILE_MASK)) != 0) return false;

	return ::IsSteepSlope((::Slope)slope);
}

/* static */ bool ScriptTile::IsHalftileSlope(Slope slope)
{
	if ((slope & ~(SLOPE_ELEVATED | SLOPE_STEEP | SLOPE_HALFTILE_MASK)) != 0) return false;

	return ::IsHalftileSlope((::Slope)slope);
}

/* static */ bool ScriptTile::HasTreeOnTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::Trees);
}

/* static */ bool ScriptTile::IsFarmTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, TileType::Clear) && ::IsClearGround(tile, ClearGround::Fields));
}

/* static */ bool ScriptTile::IsRockTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, TileType::Clear) && ::GetClearGround(tile) == ::ClearGround::Rocks);
}

/* static */ bool ScriptTile::IsRoughTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, TileType::Clear) && ::GetClearGround(tile) == ::ClearGround::Rough);
}

/* static */ bool ScriptTile::IsSnowTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, TileType::Clear) && ::IsSnowTile(tile));
}

/* static */ bool ScriptTile::IsDesertTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, TileType::Clear) && ::IsClearGround(tile, ClearGround::Desert));
}

/* static */ bool ScriptTile::IsHouseTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::House);
}

/* static */ ScriptTile::TerrainType ScriptTile::GetTerrainType(TileIndex tile)
{
	if (!::IsValidTile(tile)) return TERRAIN_NORMAL;

	switch (::GetTerrainType(tile)) {
		default:
		case 0: return TERRAIN_NORMAL;
		case 1: return TERRAIN_DESERT;
		case 2: return TERRAIN_RAINFOREST;
		case 4: return TERRAIN_SNOW;
	}
}

/* static */ ScriptTile::Slope ScriptTile::GetSlope(TileIndex tile)
{
	if (!::IsValidTile(tile)) return SLOPE_INVALID;

	return (Slope)::GetTileSlope(tile);
}

/* static */ ScriptTile::Slope ScriptTile::GetComplementSlope(Slope slope)
{
	if ((slope & ~SLOPE_ELEVATED) != 0) return SLOPE_INVALID;

	return (Slope)::ComplementSlope((::Slope)slope);
}

/* static */ SQInteger ScriptTile::GetMinHeight(TileIndex tile)
{
	if (!::IsValidTile(tile)) return -1;

	return ::GetTileZ(tile);
}

/* static */ SQInteger ScriptTile::GetMaxHeight(TileIndex tile)
{
	if (!::IsValidTile(tile)) return -1;

	return ::GetTileMaxZ(tile);
}

/* static */ SQInteger ScriptTile::GetCornerHeight(TileIndex tile, Corner corner)
{
	if (!::IsValidTile(tile) || !::IsValidCorner((::Corner)corner)) return -1;

	auto [slope, z] = ::GetTileSlopeZ(tile);
	return (z + ::GetSlopeZInCorner(slope, (::Corner)corner));
}

/* static */ ScriptCompany::CompanyID ScriptTile::GetOwner(TileIndex tile)
{
	if (!::IsValidTile(tile)) return ScriptCompany::COMPANY_INVALID;
	if (::IsTileType(tile, TileType::House)) return ScriptCompany::COMPANY_INVALID;
	if (::IsTileType(tile, TileType::Industry)) return ScriptCompany::COMPANY_INVALID;

	return ScriptCompany::ResolveCompanyID(ScriptCompany::ToScriptCompanyID(::GetTileOwner(tile)));
}

/* static */ bool ScriptTile::HasTransportType(TileIndex tile, TransportType transport_type)
{
	if (!::IsValidTile(tile)) return false;

	if (transport_type == TRANSPORT_ROAD) {
		return ::TrackStatusToTrackdirBits(::GetTileTrackStatus(tile, (::TransportType)transport_type, 0)) != TRACKDIR_BIT_NONE ||
				::TrackStatusToTrackdirBits(::GetTileTrackStatus(tile, (::TransportType)transport_type, 1)) != TRACKDIR_BIT_NONE;
	} else {
		return ::TrackStatusToTrackdirBits(::GetTileTrackStatus(tile, (::TransportType)transport_type, 0)) != TRACKDIR_BIT_NONE;
	}
}

/* static */ SQInteger ScriptTile::GetCargoAcceptance(TileIndex tile, CargoType cargo_type, SQInteger width, SQInteger height, SQInteger radius)
{
	if (!::IsValidTile(tile) || width <= 0 || height <= 0 || radius < 0 || !ScriptCargo::IsValidCargo(cargo_type)) return -1;

	CargoArray acceptance = ::GetAcceptanceAroundTiles(tile, width, height, _settings_game.station.modified_catchment ? radius : (int)CA_UNMODIFIED).first;
	return acceptance[cargo_type];
}

/* static */ SQInteger ScriptTile::GetCargoProduction(TileIndex tile, CargoType cargo_type, SQInteger width, SQInteger height, SQInteger radius)
{
	if (!::IsValidTile(tile) || width <= 0 || height <= 0 || radius < 0 || !ScriptCargo::IsValidCargo(cargo_type)) return -1;

	CargoArray produced = ::GetProductionAroundTiles(tile, width, height, _settings_game.station.modified_catchment ? radius : (int)CA_UNMODIFIED);
	return produced[cargo_type];
}

/* static */ SQInteger ScriptTile::GetDistanceManhattanToTile(TileIndex tile_from, TileIndex tile_to)
{
	return ScriptMap::DistanceManhattan(tile_from, tile_to);
}

/* static */ SQInteger ScriptTile::GetDistanceSquareToTile(TileIndex tile_from, TileIndex tile_to)
{
	return ScriptMap::DistanceSquare(tile_from, tile_to);
}

/* static */ bool ScriptTile::RaiseTile(TileIndex tile, Slope slope)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, tile < ScriptMap::GetMapSize());

	return ScriptObject::Command<Commands::TerraformLand>::Do(tile, (::Slope)slope, true);
}

/* static */ bool ScriptTile::LowerTile(TileIndex tile, Slope slope)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, tile < ScriptMap::GetMapSize());

	return ScriptObject::Command<Commands::TerraformLand>::Do(tile, (::Slope)slope, false);
}

/* static */ bool ScriptTile::LevelTiles(TileIndex start_tile, TileIndex end_tile)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, start_tile < ScriptMap::GetMapSize());
	EnforcePrecondition(false, end_tile < ScriptMap::GetMapSize());

	return ScriptObject::Command<Commands::LevelLand>::Do(end_tile, start_tile, false, LM_LEVEL);
}

/* static */ bool ScriptTile::DemolishTile(TileIndex tile)
{
	EnforceDeityOrCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));

	return ScriptObject::Command<Commands::LandscapeClear>::Do(tile);
}

/* static */ bool ScriptTile::PlantTree(TileIndex tile)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));

	return ScriptObject::Command<Commands::PlantTree>::Do(tile, tile, TREE_INVALID, false);
}

/* static */ bool ScriptTile::PlantTreeRectangle(TileIndex tile, SQInteger width, SQInteger height)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, width >= 1 && width <= 20);
	EnforcePrecondition(false, height >= 1 && height <= 20);
	TileIndex end_tile = TileAddWrap(tile, width - 1, height - 1);
	EnforcePrecondition(false, ::IsValidTile(end_tile));

	return ScriptObject::Command<Commands::PlantTree>::Do(tile, end_tile, TREE_INVALID, false);
}

/* static */ bool ScriptTile::IsWithinTownInfluence(TileIndex tile, TownID town_id)
{
	return ScriptTown::IsWithinTownInfluence(town_id, tile);
}

/* static */ TownID ScriptTile::GetTownAuthority(TileIndex tile)
{
	if (!::IsValidTile(tile)) return TownID::Invalid();

	Town *town = ::ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);
	if (town == nullptr) return TownID::Invalid();

	return town->index;
}

/* static */ TownID ScriptTile::GetClosestTown(TileIndex tile)
{
	if (!::IsValidTile(tile)) return TownID::Invalid();

	Town *town = ::ClosestTownFromTile(tile, UINT_MAX);
	if (town == nullptr) return TownID::Invalid();

	return town->index;
}

/* static */ Money ScriptTile::GetBuildCost(BuildType build_type)
{
	switch (build_type) {
		case BT_FOUNDATION:   return ::GetPrice(Price::BuildFoundation, 1, nullptr);
		case BT_TERRAFORM:    return ::GetPrice(Price::Terraform, 1, nullptr);
		case BT_BUILD_TREES:  return ::GetPrice(Price::BuildTrees, 1, nullptr);
		case BT_CLEAR_GRASS:  return ::GetPrice(Price::ClearGrass, 1, nullptr);
		case BT_CLEAR_ROUGH:  return ::GetPrice(Price::ClearRough, 1, nullptr);
		case BT_CLEAR_ROCKY:  return ::GetPrice(Price::ClearRocks, 1, nullptr);
		case BT_CLEAR_FIELDS: return ::GetPrice(Price::ClearFields, 1, nullptr);
		case BT_CLEAR_HOUSE:  return ::GetPrice(Price::ClearHouse, 1, nullptr);
		case BT_CLEAR_WATER:  return ::GetPrice(Price::ClearWater, 1, nullptr);
		default: return -1;
	}
}
