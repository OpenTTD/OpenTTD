/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_station.cpp Implementation of AIStation. */

#include "../../stdafx.h"
#include "ai_station.hpp"
#include "ai_cargo.hpp"
#include "ai_map.hpp"
#include "ai_town.hpp"
#include "../../debug.h"
#include "../../station_base.h"
#include "../../roadstop_base.h"
#include "../../company_func.h"
#include "../../town.h"

/* static */ bool AIStation::IsValidStation(StationID station_id)
{
	const Station *st = ::Station::GetIfValid(station_id);
	return st != NULL && (st->owner == _current_company || st->owner == OWNER_NONE);
}

/* static */ StationID AIStation::GetStationID(TileIndex tile)
{
	if (!::IsValidTile(tile) || !::IsTileType(tile, MP_STATION)) return INVALID_STATION;
	return ::GetStationIndex(tile);
}

/* static */ int32 AIStation::GetCargoWaiting(StationID station_id, CargoID cargo_id)
{
	if (!IsValidStation(station_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	return ::Station::Get(station_id)->goods[cargo_id].cargo.Count();
}

/* static */ int32 AIStation::GetCargoRating(StationID station_id, CargoID cargo_id)
{
	if (!IsValidStation(station_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	return ::ToPercent8(::Station::Get(station_id)->goods[cargo_id].rating);
}

/* static */ int32 AIStation::GetCoverageRadius(AIStation::StationType station_type)
{
	if (station_type == STATION_AIRPORT) {
		DEBUG(ai, 0, "GetCoverageRadius(): coverage radius of airports needs to be requested via AIAirport::GetAirportCoverageRadius(), as it requires AirportType");
		return -1;
	}
	if (!HasExactlyOneBit(station_type)) return -1;
	if (!_settings_game.station.modified_catchment) return CA_UNMODIFIED;

	switch (station_type) {
		case STATION_TRAIN:      return CA_TRAIN;
		case STATION_TRUCK_STOP: return CA_TRUCK;
		case STATION_BUS_STOP:   return CA_BUS;
		case STATION_DOCK:       return CA_DOCK;
		default:                 return CA_NONE;
	}
}

/* static */ int32 AIStation::GetDistanceManhattanToTile(StationID station_id, TileIndex tile)
{
	if (!IsValidStation(station_id)) return -1;

	return AIMap::DistanceManhattan(tile, GetLocation(station_id));
}

/* static */ int32 AIStation::GetDistanceSquareToTile(StationID station_id, TileIndex tile)
{
	if (!IsValidStation(station_id)) return -1;

	return AIMap::DistanceSquare(tile, GetLocation(station_id));
}

/* static */ bool AIStation::IsWithinTownInfluence(StationID station_id, TownID town_id)
{
	if (!IsValidStation(station_id)) return false;

	return AITown::IsWithinTownInfluence(town_id, GetLocation(station_id));
}

/* static */ bool AIStation::HasStationType(StationID station_id, StationType station_type)
{
	if (!IsValidStation(station_id)) return false;
	if (!HasExactlyOneBit(station_type)) return false;

	return (::Station::Get(station_id)->facilities & station_type) != 0;
}

/* static */ bool AIStation::HasRoadType(StationID station_id, AIRoad::RoadType road_type)
{
	if (!IsValidStation(station_id)) return false;
	if (!AIRoad::IsRoadTypeAvailable(road_type)) return false;

	::RoadTypes r = RoadTypeToRoadTypes((::RoadType)road_type);

	for (const RoadStop *rs = ::Station::Get(station_id)->GetPrimaryRoadStop(ROADSTOP_BUS); rs != NULL; rs = rs->next) {
		if ((::GetRoadTypes(rs->xy) & r) != 0) return true;
	}
	for (const RoadStop *rs = ::Station::Get(station_id)->GetPrimaryRoadStop(ROADSTOP_TRUCK); rs != NULL; rs = rs->next) {
		if ((::GetRoadTypes(rs->xy) & r) != 0) return true;
	}

	return false;
}

/* static */ TownID AIStation::GetNearestTown(StationID station_id)
{
	if (!IsValidStation(station_id)) return INVALID_TOWN;

	return ::Station::Get(station_id)->town->index;
}
