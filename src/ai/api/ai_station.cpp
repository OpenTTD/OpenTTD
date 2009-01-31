/* $Id$ */

/** @file ai_station.cpp Implementation of AIStation. */

#include "ai_station.hpp"
#include "ai_cargo.hpp"
#include "ai_map.hpp"
#include "ai_town.hpp"
#include "../../command_func.h"
#include "../../debug.h"
#include "../../station_map.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../company_func.h"
#include "../../town.h"
#include "table/strings.h"

/* static */ bool AIStation::IsValidStation(StationID station_id)
{
	return ::IsValidStationID(station_id) && ::GetStation(station_id)->owner == _current_company;
}

/* static */ StationID AIStation::GetStationID(TileIndex tile)
{
	if (!::IsValidTile(tile) || !::IsTileType(tile, MP_STATION)) return INVALID_STATION;
	return ::GetStationIndex(tile);
}

/* static */ char *AIStation::GetName(StationID station_id)
{
	if (!IsValidStation(station_id)) return NULL;

	static const int len = 64;
	char *station_name = MallocT<char>(len);

	::SetDParam(0, GetStation(station_id)->index);
	::GetString(station_name, STR_STATION, &station_name[len - 1]);
	return station_name;
}

/* static */ bool AIStation::SetName(StationID station_id, const char *name)
{
	EnforcePrecondition(false, IsValidStation(station_id));
	EnforcePrecondition(false, !::StrEmpty(name));
	EnforcePreconditionCustomError(false, ::strlen(name) < MAX_LENGTH_STATION_NAME_BYTES, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, station_id, 0, CMD_RENAME_STATION, name);
}

/* static */ TileIndex AIStation::GetLocation(StationID station_id)
{
	if (!IsValidStation(station_id)) return INVALID_TILE;

	return ::GetStation(station_id)->xy;
}

/* static */ int32 AIStation::GetCargoWaiting(StationID station_id, CargoID cargo_id)
{
	if (!IsValidStation(station_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	return ::GetStation(station_id)->goods[cargo_id].cargo.Count();
}

/* static */ int32 AIStation::GetCargoRating(StationID station_id, CargoID cargo_id)
{
	if (!IsValidStation(station_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	return ::GetStation(station_id)->goods[cargo_id].rating * 101 >> 8;
}

/* static */ int32 AIStation::GetCoverageRadius(AIStation::StationType station_type)
{
	if (station_type == STATION_AIRPORT) {
		DEBUG(ai, 0, "GetCoverageRadius(): coverage radius of airports needs to be requested via AIAirport::GetAirportCoverageRadius(), as it requires AirportType");
		return -1;
	}
	if (CountBits(station_type) != 1) return -1;
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
	if (CountBits(station_type) != 1) return false;

	return (::GetStation(station_id)->facilities & station_type) != 0;
}

/* static */ bool AIStation::HasRoadType(StationID station_id, AIRoad::RoadType road_type)
{
	if (!IsValidStation(station_id)) return false;
	if (!AIRoad::IsRoadTypeAvailable(road_type)) return false;

	::RoadTypes r = RoadTypeToRoadTypes((::RoadType)road_type);

	for (const RoadStop *rs = ::GetStation(station_id)->GetPrimaryRoadStop(ROADSTOP_BUS); rs != NULL; rs = rs->next) {
		if ((::GetRoadTypes(rs->xy) & r) != 0) return true;
	}
	for (const RoadStop *rs = ::GetStation(station_id)->GetPrimaryRoadStop(ROADSTOP_TRUCK); rs != NULL; rs = rs->next) {
		if ((::GetRoadTypes(rs->xy) & r) != 0) return true;
	}

	return false;
}

/* static */ TownID AIStation::GetNearestTown(StationID station_id)
{
	if (!IsValidStation(station_id)) return INVALID_TOWN;

	return ::GetStation(station_id)->town->index;
}
