/* $Id$ */

/** @file station_map.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "station_map.h"

/**
 * Get the station type (rail, airport, truck etc) for the given tile.
 * @param t the tile to get the station type of.
 * @pre IsTileType(t, MP_STATION)
 * @return the station type of the given tile.
 */
StationType GetStationType(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	if (IsRailwayStation(t)) return STATION_RAIL;
	if (IsAirport(t)) return STATION_AIRPORT;
	if (IsTruckStop(t)) return STATION_TRUCK;
	if (IsBusStop(t)) return STATION_BUS;
	if (IsOilRig(t)) return STATION_OILRIG;
	if (IsDock(t)) return STATION_DOCK;
	assert(IsBuoy(t));
	return STATION_BUOY;
}
