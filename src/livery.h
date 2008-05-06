/* $Id$ */

/** @file livery.h Functions/types related to livery colours. */

#ifndef LIVERY_H
#define LIVERY_H

#include "player_type.h"

/* List of different livery schemes. */
enum LiveryScheme {
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

DECLARE_POSTFIX_INCREMENT(LiveryScheme);

/* List of different livery classes, used only by the livery GUI. */
enum LiveryClass {
	LC_OTHER,
	LC_RAIL,
	LC_ROAD,
	LC_SHIP,
	LC_AIRCRAFT,
	LC_END
};


struct Livery {
	bool in_use;  ///< Set if this livery should be used instead of the default livery.
	byte colour1; ///< First colour, for all vehicles.
	byte colour2; ///< Second colour, for vehicles with 2CC support.
};

/**
 * Reset the livery schemes to the player's primary colour.
 * This is used on loading games without livery information and on new player start up.
 * @param p Player to reset.
 */
void ResetPlayerLivery(Player *p);

#endif /* LIVERY_H */
