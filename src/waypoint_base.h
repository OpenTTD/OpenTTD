/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file waypoint_base.h Base of waypoints. */

#ifndef WAYPOINT_H
#define WAYPOINT_H

#include "base_station_base.h"

struct Waypoint : SpecializedStation<Waypoint, true> {
	uint16 town_cn;    ///< The Nth waypoint for this town (consecutive number)

	Waypoint(TileIndex tile = INVALID_TILE) : SpecializedStation<Waypoint, true>(tile) { }
	~Waypoint();

	void UpdateVirtCoord();

	/* virtual */ FORCEINLINE bool TileBelongsToRailStation(TileIndex tile) const
	{
		return IsRailWaypointTile(tile) && GetStationIndex(tile) == this->index;
	}

	/* virtual */ uint32 GetNewGRFVariable(const struct ResolverObject *object, byte variable, byte parameter, bool *available) const;

	/* virtual */ void GetTileArea(TileArea *ta, StationType type) const;

	/* virtual */ uint GetPlatformLength(TileIndex tile, DiagDirection dir) const
	{
		return 1;
	}

	/* virtual */ uint GetPlatformLength(TileIndex tile) const
	{
		return 1;
	}
};

#define FOR_ALL_WAYPOINTS(var) FOR_ALL_BASE_STATIONS_OF_TYPE(Waypoint, var)

#endif /* WAYPOINT_H */
