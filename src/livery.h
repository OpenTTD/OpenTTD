/* $Id$ */

#ifndef LIVERY_H
#define LIVERY_H


/* List of different livery schemes. */
typedef enum LiverySchemes {
	LS_DEFAULT,

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

	LS_END
} LiveryScheme;


/* List of different livery classes, used only by the livery GUI. */
typedef enum LiveryClasses {
	LC_OTHER,
	LC_RAIL,
	LC_ROAD,
	LC_SHIP,
	LC_AIRCRAFT,
	LC_END
} LiveryClass;


typedef struct Livery {
	bool in_use;  ///< Set if this livery should be used instead of the default livery.
	byte colour1; ///< First colour, for all vehicles.
	byte colour2; ///< Second colour, for vehicles with 2CC support.
} Livery;

#endif /* LIVERY_H */
