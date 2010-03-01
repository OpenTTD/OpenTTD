/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airport.h NewGRF handling of airports. */

#include "stdafx.h"
#include "airport.h"
#include "newgrf_airport.h"
#include "date_func.h"
#include "settings_type.h"

AirportSpec AirportSpec::dummy = {NULL, NULL, 0, 0, 0, 0, 0, MIN_YEAR, MIN_YEAR, ATP_TTDP_LARGE};
AirportSpec AirportSpec::oilrig = {NULL, NULL, 0, 1, 1, 0, 4, MIN_YEAR, MIN_YEAR, ATP_TTDP_OILRIG};

/**
 * Retrieve airport spec for the given airport
 * @param type index of airport
 * @return A pointer to the corresponding AirportSpec
 */
/* static */ const AirportSpec *AirportSpec::Get(byte type)
{
	if (type == AT_OILRIG) return &oilrig;
	assert(type < NUM_AIRPORTS);
	extern const AirportSpec _origin_airport_specs[];
	return &_origin_airport_specs[type];
}

bool AirportSpec::IsAvailable() const
{
	if (_cur_year < this->min_year) return false;
	if (_settings_game.station.never_expire_airports) return true;
	return _cur_year <= this->max_year;
}
