/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport.h Various declarations for airports */

#ifndef AIRPORT_H
#define AIRPORT_H

#include "direction_type.h"
#include "tile_type.h"

/** Some airport-related constants */
static const uint MAX_TERMINALS =   8;                       ///< maximum number of terminals per airport
static const uint MAX_HELIPADS  =   3;                       ///< maximum number of helipads per airport
static const uint MAX_ELEMENTS  = 255;                       ///< maximum number of aircraft positions at airport

static const uint NUM_AIRPORTTILES_PER_GRF = 255;            ///< Number of airport tiles per NewGRF; limited to 255 to allow extending Action3 with an extended byte later on.

static const uint NUM_AIRPORTTILES       = 256;              ///< Total number of airport tiles.
static const uint NEW_AIRPORTTILE_OFFSET = 74;               ///< offset of first newgrf airport tile
static const uint INVALID_AIRPORTTILE    = NUM_AIRPORTTILES; ///< id for an invalid airport tile

/** Airport types */
enum AirportTypes : uint8_t {
	AT_SMALL           =   0, ///< Small airport.
	AT_LARGE           =   1, ///< Large airport.
	AT_HELIPORT        =   2, ///< Heli port.
	AT_METROPOLITAN    =   3, ///< Metropolitan airport.
	AT_INTERNATIONAL   =   4, ///< International airport.
	AT_COMMUTER        =   5, ///< Commuter airport.
	AT_HELIDEPOT       =   6, ///< Heli depot.
	AT_INTERCON        =   7, ///< Intercontinental airport.
	AT_HELISTATION     =   8, ///< Heli station airport.
	AT_OILRIG          =   9, ///< Oilrig airport.
	NEW_AIRPORT_OFFSET =  10, ///< Number of the first newgrf airport.
	NUM_AIRPORTS_PER_GRF = 128, ///< Maximal number of airports per NewGRF.
	NUM_AIRPORTS       = 128, ///< Maximal number of airports in total.
	AT_INVALID         = 254, ///< Invalid airport.
	AT_DUMMY           = 255, ///< Dummy airport.
};

/** Flags for airport movement data. */
enum class AirportMovingDataFlag : uint8_t {
	NoSpeedClamp, ///< No speed restrictions.
	Takeoff, ///< Takeoff movement.
	SlowTurn, ///< Turn slowly (mostly used in the air).
	Land, ///< Landing onto landing strip.
	ExactPosition, ///< Go exactly to the destination coordinates.
	Brake, ///< Taxiing at the airport.
	HeliRaise, ///< Helicopter take-off.
	HeliLower, ///< Helicopter landing.
	Hold, ///< Holding pattern movement (above the airport).
};

using AirportMovingDataFlags = EnumBitSet<AirportMovingDataFlag, uint16_t>;

/** Movement States on Airports (headings target) */
enum AirportMovementStates : uint8_t {
	TO_ALL         =   0, ///< Go in this direction for every target.
	HANGAR         =   1, ///< Heading for hangar.
	TERM1          =   2, ///< Heading for terminal 1.
	TERM2          =   3, ///< Heading for terminal 2.
	TERM3          =   4, ///< Heading for terminal 3.
	TERM4          =   5, ///< Heading for terminal 4.
	TERM5          =   6, ///< Heading for terminal 5.
	TERM6          =   7, ///< Heading for terminal 6.
	HELIPAD1       =   8, ///< Heading for helipad 1.
	HELIPAD2       =   9, ///< Heading for helipad 2.
	TAKEOFF        =  10, ///< Airplane wants to leave the airport.
	STARTTAKEOFF   =  11, ///< Airplane has arrived at a runway for take-off.
	ENDTAKEOFF     =  12, ///< Airplane has reached end-point of the take-off runway.
	HELITAKEOFF    =  13, ///< Helicopter wants to leave the airport.
	FLYING         =  14, ///< %Vehicle is flying in the air.
	LANDING        =  15, ///< Airplane wants to land.
	ENDLANDING     =  16, ///< Airplane wants to finish landing.
	HELILANDING    =  17, ///< Helicopter wants to land.
	HELIENDLANDING =  18, ///< Helicopter wants to finish landing.
	TERM7          =  19, ///< Heading for terminal 7.
	TERM8          =  20, ///< Heading for terminal 8.
	HELIPAD3       =  21, ///< Heading for helipad 3.
	MAX_HEADINGS   =  21, ///< Last valid target to head for.
	TERMGROUP      = 255, ///< Aircraft is looking for a free terminal in a terminalgroup.
};

/** Movement Blocks on Airports blocks (eg_airport_flags). */
enum class AirportBlock : uint8_t {
	Term1            =  0, ///< Block belonging to terminal 1.
	Term2            =  1, ///< Block belonging to terminal 2.
	Term3            =  2, ///< Block belonging to terminal 3.
	Term4            =  3, ///< Block belonging to terminal 4.
	Term5            =  4, ///< Block belonging to terminal 5.
	Term6            =  5, ///< Block belonging to terminal 6.
	Helipad1         =  6, ///< Block belonging to helipad 1.
	Helipad2         =  7, ///< Block belonging to helipad 2.
	RunwayInOut      =  8,
	RunwayIn         =  8,
	AirportBusy      =  8,
	RunwayOut        =  9,
	TaxiwayBusy      = 10,
	OutWay           = 11,
	InWay            = 12,
	AirportEntrance  = 13,
	TermGroup1       = 14,
	TermGroup2       = 15,
	Hangar2Area      = 16,
	TermGroup2Enter1 = 17,
	TermGroup2Enter2 = 18,
	TermGroup2Exit1  = 19,
	TermGroup2Exit2  = 20,
	PreHelipad       = 21,

	/* blocks for new airports */
	Term7            = 22, ///< Block belonging to terminal 7.
	Term8            = 23, ///< Block belonging to terminal 8.
	Helipad3         = 24, ///< Block belonging to helipad 3.
	Hangar1Area      = 26,
	OutWay2          = 27,
	InWay2           = 28,
	RunwayIn2        = 29,
	RunwayOut2       = 10, ///< @note re-uses #AirportBlock::TaxiwayBusy
	HelipadGroup     = 13, ///< @note re-uses #AirportBlock::AirportEntrance
	OutWay3          = 31,
	/* end of new blocks */

	Nothing          = 30,
	Zeppeliner       = 62, ///< Block for the zeppeliner disaster vehicle.
	AirportClosed    = 63, ///< Dummy block for indicating a closed airport.
};
using AirportBlocks = EnumBitSet<AirportBlock, uint64_t>;

/** A single location on an airport where aircraft can move to. */
struct AirportMovingData {
	int16_t x;             ///< x-coordinate of the destination.
	int16_t y;             ///< y-coordinate of the destination.
	AirportMovingDataFlags flags; ///< special flags when moving towards the destination.
	Direction direction; ///< Direction to turn the aircraft after reaching the destination.
};

AirportMovingData RotateAirportMovingData(const AirportMovingData *orig, Direction rotation, uint num_tiles_x, uint num_tiles_y);

struct AirportFTAbuildup;

/** Internal structure used in openttd - Finite sTate mAchine --> FTA */
struct AirportFTA {
	AirportFTA(const AirportFTAbuildup&);

	std::unique_ptr<AirportFTA> next; ///< possible extra movement choices from this position
	AirportBlocks blocks; ///< bitmap of blocks that could be reserved
	uint8_t position; ///< the position that an airplane is at
	uint8_t next_position; ///< next position from this position
	uint8_t heading; ///< heading (current orders), guiding an airplane to its target on an airport
};

/** Finite sTate mAchine (FTA) of an airport. */
struct AirportFTAClass {
public:
	/** Bitmask of airport flags. */
	enum class Flag : uint8_t {
		Airplanes   = 0, ///< Can planes land on this airport type?
		Helicopters = 1, ///< Can helicopters land on this airport type?
		ShortStrip  = 2, ///< This airport has a short landing strip, dangerous for fast aircraft.
	};
	using Flags = EnumBitSet<Flag, uint8_t>;

	AirportFTAClass(
		const AirportMovingData *moving_data,
		const uint8_t *terminals,
		const uint8_t num_helipads,
		const uint8_t *entry_points,
		Flags flags,
		const AirportFTAbuildup *apFA,
		uint8_t delta_z
	);

	/**
	 * Get movement data at a position.
	 * @param position Element number to get movement data about.
	 * @return Pointer to the movement data.
	 */
	const AirportMovingData *MovingData(uint8_t position) const
	{
		assert(position < nofelements);
		return &moving_data[position];
	}

	const AirportMovingData *moving_data; ///< Movement data.
	std::vector<AirportFTA> layout; ///< state machine for airport
	const uint8_t *terminals;                ///< %Array with the number of terminal groups, followed by the number of terminals in each group.
	const uint8_t num_helipads;              ///< Number of helipads on this airport. When 0 helicopters will go to normal terminals.
	Flags flags;                          ///< Flags for this airport type.
	uint8_t nofelements;                     ///< number of positions the airport consists of
	const uint8_t *entry_points;             ///< when an airplane arrives at this airport, enter it at position entry_point, index depends on direction
	uint8_t delta_z;                         ///< Z adjustment for helicopter pads
};


const AirportFTAClass *GetAirport(const uint8_t airport_type);
uint8_t GetVehiclePosOnBuild(TileIndex hangar_tile);

#endif /* AIRPORT_H */
