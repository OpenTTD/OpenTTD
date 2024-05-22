/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file livery.h Functions/types related to livery colours. */

#ifndef LIVERY_H
#define LIVERY_H

#include "company_type.h"
#include "gfx_type.h"

static const uint8_t LIT_NONE    = 0; ///< Don't show the liveries at all
static const uint8_t LIT_COMPANY = 1; ///< Show the liveries of your own company
static const uint8_t LIT_ALL     = 2; ///< Show the liveries of all companies

/** List of different livery schemes. */
enum LiveryScheme : uint8_t {
	LS_BEGIN = 0,
	LS_DEFAULT = 0,

	/* Rail vehicles */
	LS_STEAM,
	LS_DIESEL,
	LS_ELECTRIC,
	LS_MONORAIL,
	LS_MAGLEV,
	LS_DMU,
	LS_EMU,
	LS_PASSENGER_WAGON_STEAM,
	LS_PASSENGER_WAGON_DIESEL,
	LS_PASSENGER_WAGON_ELECTRIC,
	LS_PASSENGER_WAGON_MONORAIL,
	LS_PASSENGER_WAGON_MAGLEV,
	LS_FREIGHT_WAGON,

	/* Road vehicles */
	LS_BUS,
	LS_TRUCK,

	/* Ships */
	LS_PASSENGER_SHIP,
	LS_FREIGHT_SHIP,

	/* Aircraft */
	LS_HELICOPTER,
	LS_SMALL_PLANE,
	LS_LARGE_PLANE,

	/* Trams (appear on Road Vehicles tab) */
	LS_PASSENGER_TRAM,
	LS_FREIGHT_TRAM,

	LS_END
};

DECLARE_POSTFIX_INCREMENT(LiveryScheme)

/** List of different livery classes, used only by the livery GUI. */
enum LiveryClass : uint8_t {
	LC_OTHER,
	LC_RAIL,
	LC_ROAD,
	LC_SHIP,
	LC_AIRCRAFT,
	LC_GROUP_RAIL,
	LC_GROUP_ROAD,
	LC_GROUP_SHIP,
	LC_GROUP_AIRCRAFT,
	LC_END
};
DECLARE_ENUM_AS_ADDABLE(LiveryClass)

/** Information about a particular livery. */
struct Livery {
	uint8_t in_use;  ///< Bit 0 set if this livery should override the default livery first colour, Bit 1 for the second colour.
	Colours colour1; ///< First colour, for all vehicles.
	Colours colour2; ///< Second colour, for vehicles with 2CC support.
};

void ResetCompanyLivery(Company *c);

#endif /* LIVERY_H */
