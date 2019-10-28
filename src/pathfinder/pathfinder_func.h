/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pathfinder_func.h General functions related to pathfinders. */

#ifndef PATHFINDER_FUNC_H
#define PATHFINDER_FUNC_H

#include "../waypoint_base.h"

/**
 * Calculates the tile of given station that is closest to a given tile
 * for this we assume the station is a rectangle,
 * as defined by its tile are (st->train_station)
 * @param station The station to calculate the distance to
 * @param tile The tile from where to calculate the distance
 * @param station_type the station type to get the closest tile of
 * @return The closest station tile to the given tile.
 */
static inline TileIndex CalcClosestStationTile(StationID station, TileIndex tile, StationType station_type)
{
	const BaseStation *st = BaseStation::Get(station);
	TileArea ta;
	st->GetTileArea(&ta, station_type);

	/* If the rail station is (temporarily) not present, use the station sign to drive near the station */
	if (ta.tile == INVALID_TILE) return st->xy;

	uint minx = TileX(ta.tile);  // topmost corner of station
	uint miny = TileY(ta.tile);
	uint maxx = minx + ta.w - 1; // lowermost corner of station
	uint maxy = miny + ta.h - 1;

	/* we are going the aim for the x coordinate of the closest corner
	 * but if we are between those coordinates, we will aim for our own x coordinate */
	uint x = ClampU(TileX(tile), minx, maxx);

	/* same for y coordinate, see above comment */
	uint y = ClampU(TileY(tile), miny, maxy);

	/* return the tile of our target coordinates */
	return TileXY(x, y);
}

#endif /* PATHFINDER_FUNC_H */
