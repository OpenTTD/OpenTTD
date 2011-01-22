/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_airport.cpp Implementation of AIAirport. */

#include "../../stdafx.h"
#include "ai_airport.hpp"
#include "ai_station.hpp"
#include "../../station_base.h"
#include "../../company_func.h"
#include "../../town.h"

/* static */ bool AIAirport::IsValidAirportType(AirportType type)
{
	return IsAirportInformationAvailable(type) && ::AirportSpec::Get(type)->IsAvailable();
}

/* static */ bool AIAirport::IsAirportInformationAvailable(AirportType type)
{
	return type >= 0 && type < (AirportType)NUM_AIRPORTS && AirportSpec::Get(type)->enabled;
}

/* static */ Money AIAirport::GetPrice(AirportType type)
{
	if (!IsValidAirportType(type)) return -1;

	const AirportSpec *as = ::AirportSpec::Get(type);
	return _price[PR_BUILD_STATION_AIRPORT] * as->size_x * as->size_y;
}

/* static */ bool AIAirport::IsHangarTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION) && ::IsHangar(tile);
}

/* static */ bool AIAirport::IsAirportTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION) && ::IsAirport(tile);
}

/* static */ int32 AIAirport::GetAirportWidth(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return ::AirportSpec::Get(type)->size_x;
}

/* static */ int32 AIAirport::GetAirportHeight(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return ::AirportSpec::Get(type)->size_y;
}

/* static */ int32 AIAirport::GetAirportCoverageRadius(AirportType type)
{
	if (!IsAirportInformationAvailable(type)) return -1;

	return _settings_game.station.modified_catchment ? ::AirportSpec::Get(type)->catchment : (uint)CA_UNMODIFIED;
}

/* static */ bool AIAirport::BuildAirport(TileIndex tile, AirportType type, StationID station_id)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsValidAirportType(type));
	EnforcePrecondition(false, station_id == AIStation::STATION_NEW || station_id == AIStation::STATION_JOIN_ADJACENT || AIStation::IsValidStation(station_id));

	uint p2 = station_id == AIStation::STATION_JOIN_ADJACENT ? 0 : 1;
	p2 |= (AIStation::IsValidStation(station_id) ? station_id : INVALID_STATION) << 16;
	return AIObject::DoCommand(tile, type, p2, CMD_BUILD_AIRPORT);
}

/* static */ bool AIAirport::RemoveAirport(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile))
	EnforcePrecondition(false, IsAirportTile(tile) || IsHangarTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ int32 AIAirport::GetNumHangars(TileIndex tile)
{
	if (!::IsValidTile(tile)) return -1;
	if (!::IsTileType(tile, MP_STATION)) return -1;

	const Station *st = ::Station::GetByTile(tile);
	if (st->owner != _current_company) return -1;
	if ((st->facilities & FACIL_AIRPORT) == 0) return -1;

	return st->airport.GetNumHangars();
}

/* static */ TileIndex AIAirport::GetHangarOfAirport(TileIndex tile)
{
	if (!::IsValidTile(tile)) return INVALID_TILE;
	if (!::IsTileType(tile, MP_STATION)) return INVALID_TILE;
	if (GetNumHangars(tile) < 1) return INVALID_TILE;

	const Station *st = ::Station::GetByTile(tile);
	if (st->owner != _current_company) return INVALID_TILE;
	if ((st->facilities & FACIL_AIRPORT) == 0) return INVALID_TILE;

	return st->airport.GetHangarTile(0);
}

/* static */ AIAirport::AirportType AIAirport::GetAirportType(TileIndex tile)
{
	if (!AITile::IsStationTile(tile)) return AT_INVALID;

	StationID station_id = ::GetStationIndex(tile);

	if (!AIStation::HasStationType(station_id, AIStation::STATION_AIRPORT)) return AT_INVALID;

	return (AirportType)::Station::Get(station_id)->airport.type;
}


/* static */ int AIAirport::GetNoiseLevelIncrease(TileIndex tile, AirportType type)
{
	extern Town *AirportGetNearestTown(const AirportSpec *as, TileIndex airport_tile);
	extern uint8 GetAirportNoiseLevelForTown(const AirportSpec *as, TileIndex town_tile, TileIndex tile);

	if (!::IsValidTile(tile)) return -1;
	if (!IsValidAirportType(type)) return -1;

	if (_settings_game.economy.station_noise_level) {
		const AirportSpec *as = ::AirportSpec::Get(type);
		const Town *t = AirportGetNearestTown(as, tile);
		return GetAirportNoiseLevelForTown(as, t->xy, tile);
	}

	return 1;
}

/* static */ TownID AIAirport::GetNearestTown(TileIndex tile, AirportType type)
{
	extern Town *AirportGetNearestTown(const AirportSpec *as, TileIndex airport_tile);

	if (!::IsValidTile(tile)) return INVALID_TOWN;
	if (!IsAirportInformationAvailable(type)) return INVALID_TOWN;

	return AirportGetNearestTown(AirportSpec::Get(type), tile)->index;
}
