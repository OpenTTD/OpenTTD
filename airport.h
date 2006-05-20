/* $Id$ */

#ifndef AIRPORT_H
#define AIRPORT_H

enum {MAX_TERMINALS = 6};
enum {MAX_HELIPADS  = 2};
enum {MAX_ELEMENTS  = 255};
enum {MAX_HEADINGS  = 18};

// Airport types
enum {
	AT_SMALL         = 0,
	AT_LARGE         = 1,
	AT_HELIPORT      = 2,
	AT_METROPOLITAN  = 3,
	AT_INTERNATIONAL = 4,
	AT_OILRIG        = 15,
};

// do not change unless you change v->subtype too. This aligns perfectly with its current setting
enum {
	AIRCRAFT_ONLY    = 0,
	ALL              = 1,
	HELICOPTERS_ONLY = 2,
};

enum {
	AMED_NOSPDCLAMP = 1<<0,
	AMED_TAKEOFF    = 1<<1,
	AMED_SLOWTURN   = 1<<2,
	AMED_LAND       = 1<<3,
	AMED_EXACTPOS   = 1<<4,
	AMED_BRAKE      = 1<<5,
	AMED_HELI_RAISE = 1<<6,
	AMED_HELI_LOWER = 1<<7,
};

/* Movement States on Airports (headings target) */
enum {
	TO_ALL,
	HANGAR,
	TERM1,
	TERM2,
	TERM3,
	TERM4,
	TERM5,
	TERM6,
	HELIPAD1,
	HELIPAD2,
	TAKEOFF,
	STARTTAKEOFF,
	ENDTAKEOFF,
	HELITAKEOFF,
	FLYING,
	LANDING,
	ENDLANDING,
	HELILANDING,
	HELIENDLANDING,
};

/* Movement Blocks on Airports */
// blocks (eg_airport_flags)
enum {
	TERM1_block              = 1 << 0,
	TERM2_block              = 1 << 1,
	TERM3_block              = 1 << 2,
	TERM4_block              = 1 << 3,
	TERM5_block              = 1 << 4,
	TERM6_block              = 1 << 5,
	HELIPAD1_block           = 1 << 6,
	HELIPAD2_block           = 1 << 7,
	RUNWAY_IN_OUT_block      = 1 << 8,
	RUNWAY_IN_block          = 1 << 8,
	AIRPORT_BUSY_block       = 1 << 8,
	RUNWAY_OUT_block         = 1 << 9,
	TAXIWAY_BUSY_block       = 1 << 10,
	OUT_WAY_block            = 1 << 11,
	IN_WAY_block             = 1 << 12,
	AIRPORT_ENTRANCE_block   = 1 << 13,
	TERM_GROUP1_block        = 1 << 14,
	TERM_GROUP2_block        = 1 << 15,
	HANGAR2_AREA_block       = 1 << 16,
	TERM_GROUP2_ENTER1_block = 1 << 17,
	TERM_GROUP2_ENTER2_block = 1 << 18,
	TERM_GROUP2_EXIT1_block  = 1 << 19,
	TERM_GROUP2_EXIT2_block  = 1 << 20,
	PRE_HELIPAD_block        = 1 << 21,
	NOTHING_block            = 1 << 30,
};

typedef struct AirportMovingData {
	int x,y;
	byte flag;
	byte direction;
} AirportMovingData;

// Finite sTate mAchine --> FTA
typedef struct AirportFTAClass {
	byte nofelements;                     // number of positions the airport consists of
	const byte *terminals;
	const byte *helipads;
	byte entry_point;                     // when an airplane arrives at this airport, enter it at position entry_point
	byte acc_planes;                      // accept airplanes or helicopters or both
	const TileIndexDiffC *airport_depots; // gives the position of the depots on the airports
	byte nof_depots;                      // number of depots this airport has
	struct AirportFTA *layout;            // state machine for airport
	byte size_x;
	byte size_y;
} AirportFTAClass;

// internal structure used in openttd - Finite sTate mAchine --> FTA
typedef struct AirportFTA {
	byte position;                    // the position that an airplane is at
	byte next_position;               // next position from this position
	uint32 block;                     // 32 bit blocks (st->airport_flags), should be enough for the most complex airports
	byte heading;                     // heading (current orders), guiding an airplane to its target on an airport
	struct AirportFTA *next_in_chain; // possible extra movement choices from this position
} AirportFTA;

void InitializeAirports(void);
void UnInitializeAirports(void);
const AirportFTAClass* GetAirport(const byte airport_type);
const AirportMovingData *GetAirportMovingData(byte airport_type, byte position);

/** Get buildable airport bitmask.
 * @return get all buildable airports at this given time, bitmasked.
 * Bit 0 means the small airport is buildable, etc.
 * @todo set availability of airports by year, instead of airplane
 */
uint32 GetValidAirports(void);

#endif /* AIRPORT_H */
