/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_airport.cpp Implementation of ScriptAirport. */

#include "../../stdafx.h"
#include "script_airport.hpp"
#include "script_station.hpp"
#include "../../station_base.h"
#include "../../town.h"

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
	return _price[PR_BUILD_STATION_AIRPORT] * as->size_x * as->size_y;
}

/* static */ bool ScriptAirport::IsHangarTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION) && ::IsHangar(tile);
}

/* static */ bool ScriptAirport::IsAirportTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION) && ::IsAirport(tile);
}

/* static */ int32 ScriptAirport::GetAirportWidth(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return ::AirportSpec::Get(type)->size_x;
}

/* static */ int32 ScriptAirport::GetAirportHeight(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return ::AirportSpec::Get(type)->size_y;
}

/* static */ int32 ScriptAirport::GetAirportCoverageRadius(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return _settings_game.station.modified_catchment ? ::AirportSpec::Get(type)->catchment : (uint)CA_UNMODIFIED;
}

/* static */ bool ScriptAirport::BuildAirport(TileIndex tile, AirportType type, StationID station_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidAirportType(type));
	EnforcePrecondition(false, station_id == ScriptStation::STATION_NEW || station_id == ScriptStation::STATION_JOIN_ADJACENT || ScriptStation::IsValidStation(station_id));

	uint p2 = station_id == ScriptStation::STATION_JOIN_ADJACENT ? 0 : 1;
	p2 |= (ScriptStation::IsValidStation(station_id) ? station_id : INVALID_STATION) << 16;
	return ScriptObject::DoCommand(tile, type, p2, CMD_BUILD_AIRPORT);
}

/* static */ bool ScriptAirport::RemoveAirport(TileIndex tile)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, ::IsValidTile(tile))
	EnforcePrecondition(false, IsAirportTile(tile) || IsHangarTile(tile));

	return ScriptObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ int32 ScriptAirport::GetNumHangars(TileIndex tile)
{
	if (!::IsValidTile(tile)) return -1;
	if (!::IsTileType(tile, MP_STATION)) return -1;

	const Station *st = ::Station::GetByTile(tile);
	if (st->owner != ScriptObject::GetCompany() && ScriptObject::GetCompany() != OWNER_DEITY) return -1;
	if ((st->facilities & FACIL_AIRPORT) == 0) return -1;

	return st->airport.GetNumHangars();
}

/* static */ TileIndex ScriptAirport::GetHangarOfAirport(TileIndex tile)
{
	if (!::IsValidTile(tile)) return INVALID_TILE;
	if (!::IsTileType(tile, MP_STATION)) return INVALID_TILE;
	if (GetNumHangars(tile) < 1) return INVALID_TILE;

	const Station *st = ::Station::GetByTile(tile);
	if (st->owner != ScriptObject::GetCompany() && ScriptObject::GetCompany() != OWNER_DEITY) return INVALID_TILE;
	if ((st->facilities & FACIL_AIRPORT) == 0) return INVALID_TILE;

	return st->airport.GetHangarTile(0);
}

/* static */ ScriptAirport::AirportType ScriptAirport::GetAirportType(TileIndex tile)
{
	if (!ScriptTile::IsStationTile(tile)) return AT_INVALID;

	StationID station_id = ::GetStationIndex(tile);

	if (!ScriptStation::HasStationType(station_id, ScriptStation::STATION_AIRPORT)) return AT_INVALID;

	return (AirportType)::Station::Get(station_id)->airport.type;
}


/* static */ int ScriptAirport::GetNoiseLevelIncrease(TileIndex tile, AirportType type)
{
	extern Town *AirportGetNearestTown(const AirportSpec *as, const TileIterator &it);
	extern uint8 GetAirportNoiseLevelForTown(const AirportSpec *as, TileIterator &it, TileIndex town_tile);

	if (!::IsValidTile(tile)) return -1;
	if (!IsAirportInformationAvailable(type)) return -1;

	if (_settings_game.economy.station_noise_level) {
		const AirportSpec *as = ::AirportSpec::Get(type);
		AirportTileTableIterator it(as->table[0], tile);
		const Town *t = AirportGetNearestTown(as, it);
		return GetAirportNoiseLevelForTown(as, it, t->xy);
	}

	return 1;
}

/* static */ TownID ScriptAirport::GetNearestTown(TileIndex tile, AirportType type)
{
	extern Town *AirportGetNearestTown(const AirportSpec *as, const TileIterator &it);

	if (!::IsValidTile(tile)) return INVALID_TOWN;
	if (!IsAirportInformationAvailable(type)) return INVALID_TOWN;

	const AirportSpec *as = AirportSpec::Get(type);
	return AirportGetNearestTown(as, AirportTileTableIterator(as->table[0], tile))->index;
}

/* static */ uint16 ScriptAirport::GetMaintenanceCostFactor(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return INVALID_TOWN;

	return AirportSpec::Get(type)->maintenance_cost;
}

/* static */ Money ScriptAirport::GetMonthlyMaintenanceCost(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return (int64)GetMaintenanceCostFactor(type) * _price[PR_INFRASTRUCTURE_AIRPORT] >> 3;
}
