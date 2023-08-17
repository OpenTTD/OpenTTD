/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_kdtree.h Declarations for accessing the k-d tree of stations */

#ifndef STATION_KDTREE_H
#define STATION_KDTREE_H

#include "core/kdtree.hpp"
#include "core/math_func.hpp"
#include "station_base.h"
#include "map_func.h"

inline uint16_t Kdtree_StationXYFunc(StationID stid, int dim) { return (dim == 0) ? TileX(BaseStation::Get(stid)->xy) : TileY(BaseStation::Get(stid)->xy); }
typedef Kdtree<StationID, decltype(&Kdtree_StationXYFunc), uint16_t, int> StationKdtree;
extern StationKdtree _station_kdtree;

/**
 * Call a function on all stations whose sign is within a radius of a center tile.
 * @param center  Central tile to search around.
 * @param radius  Distance in both X and Y to search within.
 * @param func    The function to call, must take a single parameter which is Station*.
 */
template <typename Func>
void ForAllStationsRadius(TileIndex center, uint radius, Func func)
{
	uint16_t x1, y1, x2, y2;
	x1 = (uint16_t)std::max<int>(0, TileX(center) - radius);
	x2 = (uint16_t)std::min<int>(TileX(center) + radius + 1, Map::SizeX());
	y1 = (uint16_t)std::max<int>(0, TileY(center) - radius);
	y2 = (uint16_t)std::min<int>(TileY(center) + radius + 1, Map::SizeY());

	_station_kdtree.FindContained(x1, y1, x2, y2, [&](StationID id) {
		func(Station::Get(id));
	});
}

#endif
