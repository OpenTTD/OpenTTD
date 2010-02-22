/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airporttiles.h NewGRF handling of airport tiles. */

#ifndef NEWGRF_AIRPORTTILES_H
#define NEWGRF_AIRPORTTILES_H

#include "station_map.h"

/**
 * Defines the data structure of each indivudual tile of an airport.
 */
struct AirportTileSpec {
	uint16 animation_info;                ///< Information about the animation (is it looping, how many loops etc)
	uint8 animation_speed;                ///< The speed of the animation

	static const AirportTileSpec *Get(StationGfx gfx);
};

#endif /* NEWGRF_AIRPORTTILES_H */
