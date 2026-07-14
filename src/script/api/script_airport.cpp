/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_airport.cpp Implementation of ScriptAirport. */

#include "../../stdafx.h"
#include "script_airport.hpp"
#include "script_station.hpp"
#include "../../station_base.h"
#include "../../town.h"
#include "../../landscape_cmd.h"
#include "../../station_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptAirport::IsValidAirportType(AirportType type)
{
	return IsAirportInformationAvailable(type) && ::AirportSpec::Get(type)->IsAvailable();
}

/* static */ bool ScriptAirport::IsAirportInformationAvailable(AirportType type)
{
	return type >= 0 && type < (AirportType)NUM_AIRPORTS && AirportSpec::Get(type)->enabled;
}

/* static */ Money ScriptAirport::GetPrice(AirportType type)
{
	if (!IsValidAirportType(type)) return -1;

	const AirportSpec *as = ::AirportSpec::Get(type);
	return _price[Price::BuildStationAirport] * as->size_x * as->size_y;
}

/* static */ bool ScriptAirport::IsHangarTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::Station) && ::IsHangar(tile);
}

/* static */ bool ScriptAirport::IsAirportTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, TileType::Station) && ::IsAirport(tile);
}

/* static */ SQInteger ScriptAirport::GetAirportWidth(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return ::AirportSpec::Get(type)->size_x;
}

/* static */ SQInteger ScriptAirport::GetAirportHeight(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return ::AirportSpec::Get(type)->size_y;
}

/* static */ SQInteger ScriptAirport::GetAirportCoverageRadius(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return _settings_game.station.modified_catchment ? ::AirportSpec::Get(type)->catchment : (uint)CA_UNMODIFIED;
}

/* static */ bool ScriptAirport::BuildAirport(TileIndex tile, AirportType type, StationID station_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidAirportType(type));
	EnforcePrecondition(false, station_id == ScriptStation::STATION_NEW || station_id == ScriptStation::STATION_JOIN_ADJACENT || ScriptStation::IsValidStation(station_id));

	return ScriptObject::Command<Commands::BuildAirport>::Do(tile, type, 0, (ScriptStation::IsValidStation(station_id) ? station_id : StationID::Invalid()), station_id != ScriptStation::STATION_JOIN_ADJACENT);
}

/* static */ bool ScriptAirport::RemoveAirport(TileIndex tile)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(tile))
	EnforcePrecondition(false, IsAirportTile(tile) || IsHangarTile(tile));

	return ScriptObject::Command<Commands::LandscapeClear>::Do(tile);
}

/* static */ SQInteger ScriptAirport::GetNumHangars(TileIndex tile)
{
	EnforceDeityOrCompanyModeValid(-1);
	if (!::IsValidTile(tile)) return -1;
	if (!::IsTileType(tile, TileType::Station)) return -1;

	const Station *st = ::Station::GetByTile(tile);
	if (st->owner != ScriptObject::GetCompany() && ScriptCompanyMode::IsValid()) return -1;
	if (!st->facilities.Test(StationFacility::Airport)) return -1;

	return st->airport.GetNumHangars();
}

/* static */ TileIndex ScriptAirport::GetHangarOfAirport(TileIndex tile)
{
	EnforceDeityOrCompanyModeValid(INVALID_TILE);
	if (!::IsValidTile(tile)) return INVALID_TILE;
	if (!::IsTileType(tile, TileType::Station)) return INVALID_TILE;
	if (GetNumHangars(tile) < 1) return INVALID_TILE;

	const Station *st = ::Station::GetByTile(tile);
	if (st->owner != ScriptObject::GetCompany() && ScriptCompanyMode::IsValid()) return INVALID_TILE;
	if (!st->facilities.Test(StationFacility::Airport)) return INVALID_TILE;

	return st->airport.GetHangarTile(0);
}

/* static */ ScriptAirport::AirportType ScriptAirport::GetAirportType(TileIndex tile)
{
	if (!ScriptTile::IsStationTile(tile)) return AT_INVALID;

	StationID station_id = ::GetStationIndex(tile);

	if (!ScriptStation::HasStationType(station_id, ScriptStation::STATION_AIRPORT)) return AT_INVALID;

	return (AirportType)::Station::Get(station_id)->airport.type;
}


/* static */ SQInteger ScriptAirport::GetNoiseLevelIncrease(TileIndex tile, AirportType type)
{
	if (!::IsValidTile(tile)) return -1;
	if (!IsAirportInformationAvailable(type)) return -1;

	const AirportSpec *as = ::AirportSpec::Get(type);
	if (!as->IsWithinMapBounds(0, tile)) return -1;

	if (_settings_game.economy.station_noise_level) {
		uint dist;
		const auto &layout = as->layouts[0];
		AirportGetNearestTown(as, layout.rotation, tile, AirportTileTableIterator(layout.tiles, tile), dist);
		return GetAirportNoiseLevelForDistance(as, dist);
	}

	return 1;
}

/* static */ TownID ScriptAirport::GetNearestTown(TileIndex tile, AirportType type)
{
	if (!::IsValidTile(tile)) return TownID::Invalid();
	if (!IsAirportInformationAvailable(type)) return TownID::Invalid();

	const AirportSpec *as = AirportSpec::Get(type);
	if (!as->IsWithinMapBounds(0, tile)) return TownID::Invalid();

	uint dist;
	const auto &layout = as->layouts[0];
	return AirportGetNearestTown(as, layout.rotation, tile, AirportTileTableIterator(layout.tiles, tile), dist)->index;
}

/* static */ SQInteger ScriptAirport::GetMaintenanceCostFactor(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return 0;

	return AirportSpec::Get(type)->maintenance_cost;
}

/* static */ Money ScriptAirport::GetMonthlyMaintenanceCost(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return (int64_t)GetMaintenanceCostFactor(type) * _price[Price::InfrastructureAirport] >> 3;
}

/* static */ SQInteger ScriptAirport::GetAirportNumHelipads(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return ::AirportSpec::Get(type)->fsm->num_helipads;
}
