/* $Id$ */

/** @file airport.h Various declarations for airports */

#ifndef AIRPORT_H
#define AIRPORT_H

#include "direction.h"

enum {MAX_TERMINALS =  10};
enum {MAX_HELIPADS  =   4};
enum {MAX_ELEMENTS  = 255};
enum {MAX_HEADINGS  =  22};

// Airport types
enum {
	AT_SMALL         =   0,
	AT_LARGE         =   1,
	AT_HELIPORT      =   2,
	AT_METROPOLITAN  =   3,
	AT_INTERNATIONAL =   4,
	AT_COMMUTER      =   5,
	AT_HELIDEPOT     =   6,
	AT_INTERCON      =   7,
	AT_HELISTATION   =   8,
	AT_OILRIG        =  15,
	AT_DUMMY         = 255
};


enum {
	AMED_NOSPDCLAMP = 1 << 0,
	AMED_TAKEOFF    = 1 << 1,
	AMED_SLOWTURN   = 1 << 2,
	AMED_LAND       = 1 << 3,
	AMED_EXACTPOS   = 1 << 4,
	AMED_BRAKE      = 1 << 5,
	AMED_HELI_RAISE = 1 << 6,
	AMED_HELI_LOWER = 1 << 7,
	AMED_HOLD       = 1 << 8
};

/* Movement States on Airports (headings target) */
enum {
	TO_ALL         =  0,
	HANGAR         =  1,
	TERM1          =  2,
	TERM2          =  3,
	TERM3          =  4,
	TERM4          =  5,
	TERM5          =  6,
	TERM6          =  7,
	HELIPAD1       =  8,
	HELIPAD2       =  9,
	TAKEOFF        = 10,
	STARTTAKEOFF   = 11,
	ENDTAKEOFF     = 12,
	HELITAKEOFF    = 13,
	FLYING         = 14,
	LANDING        = 15,
	ENDLANDING     = 16,
	HELILANDING    = 17,
	HELIENDLANDING = 18,
	TERM7          = 19,
	TERM8          = 20,
	HELIPAD3       = 21,
	HELIPAD4       = 22
};

/* Movement Blocks on Airports */
// blocks (eg_airport_flags)
static const uint64
	TERM1_block              = 1ULL <<  0,
	TERM2_block              = 1ULL <<  1,
	TERM3_block              = 1ULL <<  2,
	TERM4_block              = 1ULL <<  3,
	TERM5_block              = 1ULL <<  4,
	TERM6_block              = 1ULL <<  5,
	HELIPAD1_block           = 1ULL <<  6,
	HELIPAD2_block           = 1ULL <<  7,
	RUNWAY_IN_OUT_block      = 1ULL <<  8,
	RUNWAY_IN_block          = 1ULL <<  8,
	AIRPORT_BUSY_block       = 1ULL <<  8,
	RUNWAY_OUT_block         = 1ULL <<  9,
	TAXIWAY_BUSY_block       = 1ULL << 10,
	OUT_WAY_block            = 1ULL << 11,
	IN_WAY_block             = 1ULL << 12,
	AIRPORT_ENTRANCE_block   = 1ULL << 13,
	TERM_GROUP1_block        = 1ULL << 14,
	TERM_GROUP2_block        = 1ULL << 15,
	HANGAR2_AREA_block       = 1ULL << 16,
	TERM_GROUP2_ENTER1_block = 1ULL << 17,
	TERM_GROUP2_ENTER2_block = 1ULL << 18,
	TERM_GROUP2_EXIT1_block  = 1ULL << 19,
	TERM_GROUP2_EXIT2_block  = 1ULL << 20,
	PRE_HELIPAD_block        = 1ULL << 21,

// blocks for new airports
	TERM7_block              = 1ULL << 22,
	TERM8_block              = 1ULL << 23,
	TERM9_block              = 1ULL << 24,
	HELIPAD3_block           = 1ULL << 24,
	TERM10_block             = 1ULL << 25,
	HELIPAD4_block           = 1ULL << 25,
	HANGAR1_AREA_block       = 1ULL << 26,
	OUT_WAY2_block           = 1ULL << 27,
	IN_WAY2_block            = 1ULL << 28,
	RUNWAY_IN2_block         = 1ULL << 29,
	RUNWAY_OUT2_block        = 1ULL << 10,   // note re-uses TAXIWAY_BUSY
	HELIPAD_GROUP_block      = 1ULL << 13,   // note re-uses AIRPORT_ENTRANCE
	OUT_WAY_block2           = 1ULL << 31,
// end of new blocks

	NOTHING_block            = 1ULL << 30;

struct AirportMovingData {
	int16 x;
	int16 y;
	uint16 flag;
	DirectionByte direction;
};

struct AirportFTAbuildup;

// Finite sTate mAchine --> FTA
struct AirportFTAClass {
	public:
		enum Flags {
			AIRPLANES   = 0x1,
			HELICOPTERS = 0x2,
			ALL         = AIRPLANES | HELICOPTERS,
			SHORT_STRIP = 0x4
		};

		AirportFTAClass(
			const AirportMovingData *moving_data,
			const byte *terminals,
			const byte *helipads,
			const byte *entry_points,
			Flags flags,
			const AirportFTAbuildup *apFA,
			const TileIndexDiffC *depots,
			byte nof_depots,
			uint size_x,
			uint size_y,
			byte delta_z,
			byte catchment
		);

		~AirportFTAClass();

		const AirportMovingData *MovingData(byte position) const
		{
			assert(position < nofelements);
			return &moving_data[position];
		}

	const AirportMovingData *moving_data;
	struct AirportFTA *layout;            // state machine for airport
	const byte *terminals;
	const byte *helipads;
	const TileIndexDiffC *airport_depots; // gives the position of the depots on the airports
	Flags flags;
	byte nof_depots;                      // number of depots this airport has
	byte nofelements;                     // number of positions the airport consists of
	const byte *entry_points;             ///< when an airplane arrives at this airport, enter it at position entry_point, index depends on direction
	byte size_x;
	byte size_y;
	byte delta_z;                         // Z adjustment for helicopter pads
	byte catchment;
};

DECLARE_ENUM_AS_BIT_SET(AirportFTAClass::Flags)


// internal structure used in openttd - Finite sTate mAchine --> FTA
struct AirportFTA {
	AirportFTA *next;        // possible extra movement choices from this position
	uint64 block;            // 64 bit blocks (st->airport_flags), should be enough for the most complex airports
	byte position;           // the position that an airplane is at
	byte next_position;      // next position from this position
	byte heading;            // heading (current orders), guiding an airplane to its target on an airport
};

void InitializeAirports();
void UnInitializeAirports();
const AirportFTAClass *GetAirport(const byte airport_type);

/** Get buildable airport bitmask.
 * @return get all buildable airports at this given time, bitmasked.
 * Bit 0 means the small airport is buildable, etc.
 * @todo set availability of airports by year, instead of airplane
 */
uint32 GetValidAirports();

#endif /* AIRPORT_H */
