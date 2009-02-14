/* $Id$ */

/** @file ai_airport.cpp Implementation of AIAirport. */

#include "ai_airport.hpp"
#include "ai_station.hpp"
#include "../../station_map.h"
#include "../../company_func.h"
#include "../../command_type.h"
#include "../../town.h"

/* static */ bool AIAirport::IsValidAirportType(AirportType type)
{
	return type >= AT_SMALL && type <= AT_HELISTATION && HasBit(::GetValidAirports(), type);
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
	if (!IsValidAirportType(type)) return -1;

	return ::GetAirport(type)->size_x;
}

/* static */ int32 AIAirport::GetAirportHeight(AirportType type)
{
	if (!IsValidAirportType(type)) return -1;

	return ::GetAirport(type)->size_y;
}

/* static */ int32 AIAirport::GetAirportCoverageRadius(AirportType type)
{
	if (!IsValidAirportType(type)) return -1;

	return _settings_game.station.modified_catchment ? ::GetAirport(type)->catchment : (uint)CA_UNMODIFIED;
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

	const Station *st = ::GetStationByTile(tile);
	if (st->owner != _current_company) return -1;
	if ((st->facilities & FACIL_AIRPORT) == 0) return -1;

	return st->Airport()->nof_depots;
}

/* static */ TileIndex AIAirport::GetHangarOfAirport(TileIndex tile)
{
	if (!::IsValidTile(tile)) return INVALID_TILE;
	if (!::IsTileType(tile, MP_STATION)) return INVALID_TILE;
	if (GetNumHangars(tile) < 1) return INVALID_TILE;

	const Station *st = ::GetStationByTile(tile);
	if (st->owner != _current_company) return INVALID_TILE;
	if ((st->facilities & FACIL_AIRPORT) == 0) return INVALID_TILE;

	return ::ToTileIndexDiff(st->Airport()->airport_depots[0]) + st->airport_tile;
}

/* static */ AIAirport::AirportType AIAirport::GetAirportType(TileIndex tile)
{
	if (!AITile::IsStationTile(tile)) return AT_INVALID;

	StationID station_id = ::GetStationIndex(tile);

	if (!AIStation::HasStationType(station_id, AIStation::STATION_AIRPORT)) return AT_INVALID;

	return (AirportType)::GetStation(station_id)->airport_type;
}


/* static */ int AIAirport::GetNoiseLevelIncrease(TileIndex tile, AirportType type)
{
	extern Town *AirportGetNearestTown(const AirportFTAClass *afc, TileIndex airport_tile);
	extern uint8 GetAirportNoiseLevelForTown(const AirportFTAClass *afc, TileIndex town_tile, TileIndex tile);

	if (!::IsValidTile(tile)) return -1;
	if (!IsValidAirportType(type)) return -1;

	if (_settings_game.economy.station_noise_level) {
		const AirportFTAClass *afc = ::GetAirport(type);
		const Town *t = AirportGetNearestTown(afc, tile);
		return GetAirportNoiseLevelForTown(afc, t->xy, tile);
	}

	return 1;
}

/* static */ TownID AIAirport::GetNearestTown(TileIndex tile, AirportType type)
{
	extern Town *AirportGetNearestTown(const AirportFTAClass *afc, TileIndex airport_tile);

	if (!::IsValidTile(tile)) return INVALID_TOWN;
	if (!IsValidAirportType(type)) return INVALID_TOWN;

	return AirportGetNearestTown(GetAirport(type), tile)->index;
}
