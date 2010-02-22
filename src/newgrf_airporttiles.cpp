/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airporttiles.cpp NewGRF handling of airport tiles. */

#include "stdafx.h"
#include "airport.h"
#include "newgrf_airporttiles.h"
#include "table/airporttiles.h"


/**
 * Retrieve airport tile spec for the given airport tile
 * @param gfx index of airport tile
 * @return A pointer to the corresponding AirportTileSpec
 */
/* static */ const AirportTileSpec *AirportTileSpec::Get(StationGfx gfx)
{
	assert(gfx < NUM_AIRPORTTILES);
	extern const AirportTileSpec _origin_airporttile_specs[];
	return &_origin_airporttile_specs[gfx];
}

