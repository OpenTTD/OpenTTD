/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file airport_movement.h Heart of the airports and their finite state machines */

#ifndef AIRPORT_MOVEMENT_H
#define AIRPORT_MOVEMENT_H

#include "../airport.h"
#include "../newgrf_airport.h"

/**
 * State machine input struct (from external file, etc.)
 * Finite sTate mAchine --> FTA
 */
struct AirportFTAbuildup {
	uint8_t position; ///< The position that an airplane is at.
	uint8_t heading;  ///< The current orders (eg. TAKEOFF, HANGAR, ENDLANDING, etc.).
	AirportBlocks blocks;  ///< The block this position is on on the airport (st->airport.flags).
	uint8_t next;     ///< Next position from this position.
};

///////////////////////////////////////////////////////////////////////
/////*********Movement Positions on Airports********************///////

/**
 * Airport movement data creation macro.
 * @param x     X position.
 * @param y     Y position.
 * @param flags Movement flags.
 * @param dir   Direction.
 */
/** Dummy airport. */
static const AirportMovingData _airport_moving_data_dummy[] = {
	{    0,    0, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N },
	{    0,   96, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N },
	{   96,   96, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N },
	{   96,    0, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N },
};

/** Country Airfield (small) 4x3. */
static const AirportMovingData _airport_moving_data_country[22] = {
	{   53,    3, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 00 In Hangar
	{   53,   27, {},                                                                     DIR_N }, // 01 Taxi to right outside depot
	{   32,   23, {AirportMovingDataFlag::ExactPosition},                                 DIR_NW}, // 02 Terminal 1
	{   10,   23, {AirportMovingDataFlag::ExactPosition},                                 DIR_NW}, // 03 Terminal 2
	{   43,   37, {},                                                                     DIR_N }, // 04 Going towards terminal 2
	{   24,   37, {},                                                                     DIR_N }, // 05 Going towards terminal 2
	{   53,   37, {},                                                                     DIR_N }, // 06 Going for takeoff
	{   61,   40, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 07 Taxi to start of runway (takeoff)
	{    3,   40, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 08 Accelerate to end of runway
	{  -79,   40, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Takeoff},  DIR_N }, // 09 Take off
	{  177,   40, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 10 Fly to landing position in air
	{   56,   40, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Land},     DIR_N }, // 11 Going down for land
	{    3,   40, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Brake},    DIR_N }, // 12 Just landed, brake until end of runway
	{    7,   40, {},                                                                     DIR_N }, // 13 Just landed, turn around and taxi 1 square
	{   53,   40, {},                                                                     DIR_N }, // 14 Taxi from runway to crossing
	{    1,  193, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 15 Fly around waiting for a landing spot (north-east)
	{    1,    1, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 16 Fly around waiting for a landing spot (north-west)
	{  257,    1, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 17 Fly around waiting for a landing spot (south-west)
	{  273,   47, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 18 Fly around waiting for a landing spot (south)
	{   44,   37, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 19 Helicopter takeoff
	{   44,   40, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 20 In position above landing spot helicopter
	{   44,   40, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 21 Helicopter landing
};

/** Commuter Airfield (small) 5x4. */
static const AirportMovingData _airport_moving_data_commuter[38] = {
	{   69,    3, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 00 In Hangar
	{   72,   22, {},                                                                     DIR_N }, // 01 Taxi to right outside depot
	{    8,   22, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 02 Taxi to right outside depot
	{   24,   36, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 03 Terminal 1
	{   40,   36, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 04 Terminal 2
	{   56,   36, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 05 Terminal 3
	{   40,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 06 Helipad 1
	{   56,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 07 Helipad 2
	{   24,   22, {},                                                                     DIR_SW}, // 08 Taxiing
	{   40,   22, {},                                                                     DIR_SW}, // 09 Taxiing
	{   56,   22, {},                                                                     DIR_SW}, // 10 Taxiing
	{   72,   40, {},                                                                     DIR_SE}, // 11 Airport OUTWAY
	{   72,   54, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 12 Accelerate to end of runway
	{    7,   54, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 13 Release control of runway, for smoother movement
	{    5,   54, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 14 End of runway
	{  -79,   54, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Takeoff},  DIR_N }, // 15 Take off
	{  145,   54, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 16 Fly to landing position in air
	{   73,   54, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Land},     DIR_N }, // 17 Going down for land
	{    3,   54, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Brake},    DIR_N }, // 18 Just landed, brake until end of runway
	{   12,   54, {AirportMovingDataFlag::SlowTurn},                                      DIR_NW}, // 19 Just landed, turn around and taxi
	{    8,   32, {},                                                                     DIR_NW}, // 20 Taxi from runway to crossing
	{    1,  149, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 21 Fly around waiting for a landing spot (north-east)
	{    1,    6, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 22 Fly around waiting for a landing spot (north-west)
	{  193,    6, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 23 Fly around waiting for a landing spot (south-west)
	{  225,   62, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 24 Fly around waiting for a landing spot (south)
	/* Helicopter */
	{   80,    0, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 25 Bufferspace before helipad
	{   80,    0, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 26 Bufferspace before helipad
	{   32,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 27 Get in position for Helipad1
	{   48,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 28 Get in position for Helipad2
	{   32,    8, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 29 Land at Helipad1
	{   48,    8, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 30 Land at Helipad2
	{   32,    8, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 31 Takeoff Helipad1
	{   48,    8, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 32 Takeoff Helipad2
	{   64,   22, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 33 Go to position for Hangarentrance in air
	{   64,   22, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 34 Land in front of hangar
	{   40,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_N }, // 35 pre-helitakeoff helipad 1
	{   56,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_N }, // 36 pre-helitakeoff helipad 2
	{   64,   25, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 37 Take off in front of hangar
};

/** City Airport (large) 6x6. */
static const AirportMovingData _airport_moving_data_city[] = {
	{   85,    3, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 00 In Hangar
	{   85,   22, {},                                                                     DIR_N }, // 01 Taxi to right outside depot
	{   26,   41, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 02 Terminal 1
	{   56,   22, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 03 Terminal 2
	{   38,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 04 Terminal 3
	{   65,    6, {},                                                                     DIR_N }, // 05 Taxi to right in infront of terminal 2/3
	{   80,   27, {},                                                                     DIR_N }, // 06 Taxiway terminals 2-3
	{   44,   63, {},                                                                     DIR_N }, // 07 Taxi to Airport center
	{   58,   71, {},                                                                     DIR_N }, // 08 Towards takeoff
	{   72,   85, {},                                                                     DIR_N }, // 09 Taxi to runway (takeoff)
	{   89,   85, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 10 Taxi to start of runway (takeoff)
	{    3,   85, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 11 Accelerate to end of runway
	{  -79,   85, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Takeoff},  DIR_N }, // 12 Take off
	{  177,   87, {AirportMovingDataFlag::Hold,         AirportMovingDataFlag::SlowTurn}, DIR_N }, // 13 Fly to landing position in air
	{   89,   87, {AirportMovingDataFlag::Hold,         AirportMovingDataFlag::Land},     DIR_N }, // 14 Going down for land
	{   20,   87, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Brake},    DIR_N }, // 15 Just landed, brake until end of runway
	{   20,   87, {},                                                                     DIR_N }, // 16 Just landed, turn around and taxi 1 square // NOT USED
	{   36,   71, {},                                                                     DIR_N }, // 17 Taxi from runway to crossing
	{  160,   87, {AirportMovingDataFlag::Hold,         AirportMovingDataFlag::SlowTurn}, DIR_N }, // 18 Fly around waiting for a landing spot (north-east)
	{  140,    1, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 19 Final approach fix
	{  257,    1, {AirportMovingDataFlag::Hold,         AirportMovingDataFlag::SlowTurn}, DIR_N }, // 20 Fly around waiting for a landing spot (south-west)
	{  273,   49, {AirportMovingDataFlag::Hold,         AirportMovingDataFlag::SlowTurn}, DIR_N }, // 21 Fly around waiting for a landing spot (south)
	{   44,   63, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 22 Helicopter takeoff
	{   28,   74, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 23 In position above landing spot helicopter
	{   28,   74, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 24 Helicopter landing
	{  145,    1, {AirportMovingDataFlag::Hold,         AirportMovingDataFlag::SlowTurn}, DIR_N }, // 25 Fly around waiting for a landing spot (north-west)
	{  -32,    1, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 26 Initial approach fix (north)
	{  300,  -48, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 27 Initial approach fix (south)
	{  140,  -48, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 28 Intermediate Approach fix (south), IAF (west)
	{  -32,  120, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 29 Initial approach fix (east)
};

/** Metropolitan Airport (metropolitan) - 2 runways. */
static const AirportMovingData _airport_moving_data_metropolitan[28] = {
	{   85,    3, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 00 In Hangar
	{   85,   22, {},                                                                     DIR_N }, // 01 Taxi to right outside depot
	{   26,   41, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 02 Terminal 1
	{   56,   22, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 03 Terminal 2
	{   38,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 04 Terminal 3
	{   65,    6, {},                                                                     DIR_N }, // 05 Taxi to right in infront of terminal 2/3
	{   80,   27, {},                                                                     DIR_N }, // 06 Taxiway terminals 2-3
	{   49,   58, {},                                                                     DIR_N }, // 07 Taxi to Airport center
	{   72,   58, {},                                                                     DIR_N }, // 08 Towards takeoff
	{   72,   69, {},                                                                     DIR_N }, // 09 Taxi to runway (takeoff)
	{   89,   69, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 10 Taxi to start of runway (takeoff)
	{    3,   69, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 11 Accelerate to end of runway
	{  -79,   69, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Takeoff},  DIR_N }, // 12 Take off
	{  177,   85, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 13 Fly to landing position in air
	{   89,   85, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Land},     DIR_N }, // 14 Going down for land
	{    3,   85, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Brake},    DIR_N }, // 15 Just landed, brake until end of runway
	{   21,   85, {},                                                                     DIR_N }, // 16 Just landed, turn around and taxi 1 square
	{   21,   69, {},                                                                     DIR_N }, // 17 On Runway-out taxiing to In-Way
	{   21,   58, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 18 Taxi from runway to crossing
	{    1,  193, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 19 Fly around waiting for a landing spot (north-east)
	{    1,    1, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 20 Fly around waiting for a landing spot (north-west)
	{  257,    1, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 21 Fly around waiting for a landing spot (south-west)
	{  273,   49, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 22 Fly around waiting for a landing spot (south)
	{   44,   58, {},                                                                     DIR_N }, // 23 Helicopter takeoff spot on ground (to clear airport sooner)
	{   44,   63, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 24 Helicopter takeoff
	{   15,   54, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 25 Get in position above landing spot helicopter
	{   15,   54, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 26 Helicopter landing
	{   21,   58, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 27 Transitions after landing to on-ground movement
};

/** International Airport (international) - 2 runways, 6 terminals, dedicated helipad. */
static const AirportMovingData _airport_moving_data_international[53] = {
	{    7,   55, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 00 In Hangar 1
	{  100,   21, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 01 In Hangar 2
	{    7,   70, {},                                                                     DIR_N }, // 02 Taxi to right outside depot (Hangar 1)
	{  100,   36, {},                                                                     DIR_N }, // 03 Taxi to right outside depot (Hangar 2)
	{   38,   70, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 04 Terminal 1
	{   38,   54, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 05 Terminal 2
	{   38,   38, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 06 Terminal 3
	{   70,   70, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 07 Terminal 4
	{   70,   54, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 08 Terminal 5
	{   70,   38, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 09 Terminal 6
	{  104,   71, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 10 Helipad 1
	{  104,   55, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 11 Helipad 2
	{   22,   87, {},                                                                     DIR_N }, // 12 Towards Terminals 4/5/6, Helipad 1/2
	{   60,   87, {},                                                                     DIR_N }, // 13 Towards Terminals 4/5/6, Helipad 1/2
	{   66,   87, {},                                                                     DIR_N }, // 14 Towards Terminals 4/5/6, Helipad 1/2
	{   86,   87, {AirportMovingDataFlag::ExactPosition},                                 DIR_NW}, // 15 Towards Terminals 4/5/6, Helipad 1/2
	{   86,   70, {},                                                                     DIR_N }, // 16 In Front of Terminal 4 / Helipad 1
	{   86,   54, {},                                                                     DIR_N }, // 17 In Front of Terminal 5 / Helipad 2
	{   86,   38, {},                                                                     DIR_N }, // 18 In Front of Terminal 6
	{   86,   22, {},                                                                     DIR_N }, // 19 Towards Terminals Takeoff (Taxiway)
	{   66,   22, {},                                                                     DIR_N }, // 20 Towards Terminals Takeoff (Taxiway)
	{   60,   22, {},                                                                     DIR_N }, // 21 Towards Terminals Takeoff (Taxiway)
	{   38,   22, {},                                                                     DIR_N }, // 22 Towards Terminals Takeoff (Taxiway)
	{   22,   70, {},                                                                     DIR_N }, // 23 In Front of Terminal 1
	{   22,   58, {},                                                                     DIR_N }, // 24 In Front of Terminal 2
	{   22,   38, {},                                                                     DIR_N }, // 25 In Front of Terminal 3
	{   22,   22, {AirportMovingDataFlag::ExactPosition},                                 DIR_NW}, // 26 Going for Takeoff
	{   22,    6, {},                                                                     DIR_N }, // 27 On Runway-out, prepare for takeoff
	{    3,    6, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 28 Accelerate to end of runway
	{   60,    6, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 29 Release control of runway, for smoother movement
	{  105,    6, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 30 End of runway
	{  190,    6, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Takeoff},  DIR_N }, // 31 Take off
	{  193,  104, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 32 Fly to landing position in air
	{  105,  104, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Land},     DIR_N }, // 33 Going down for land
	{    3,  104, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Brake},    DIR_N }, // 34 Just landed, brake until end of runway
	{   12,  104, {AirportMovingDataFlag::SlowTurn},                                      DIR_N }, // 35 Just landed, turn around and taxi 1 square
	{    7,   84, {},                                                                     DIR_N }, // 36 Taxi from runway to crossing
	{    1,  209, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 37 Fly around waiting for a landing spot (north-east)
	{    1,    6, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 38 Fly around waiting for a landing spot (north-west)
	{  273,    6, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 39 Fly around waiting for a landing spot (south-west)
	{  305,   81, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 40 Fly around waiting for a landing spot (south)
	/* Helicopter */
	{  128,   80, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 41 Bufferspace before helipad
	{  128,   80, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 42 Bufferspace before helipad
	{   96,   71, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 43 Get in position for Helipad1
	{   96,   55, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 44 Get in position for Helipad2
	{   96,   71, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 45 Land at Helipad1
	{   96,   55, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 46 Land at Helipad2
	{  104,   71, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 47 Takeoff Helipad1
	{  104,   55, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 48 Takeoff Helipad2
	{  104,   32, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 49 Go to position for Hangarentrance in air
	{  104,   32, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 50 Land in HANGAR2_AREA to go to hangar
	{    7,   70, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 51 Takeoff from HANGAR1_AREA
	{  100,   36, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 52 Takeoff from HANGAR2_AREA
};

/** Intercontinental Airport - 4 runways, 8 terminals, 2 dedicated helipads. */
static const AirportMovingData _airport_moving_data_intercontinental[77] = {
	{    8,   87, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 00 In Hangar 1
	{  136,   72, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 01 In Hangar 2
	{    8,  104, {},                                                                     DIR_N }, // 02 Taxi to right outside depot 1
	{  136,   88, {},                                                                     DIR_N }, // 03 Taxi to right outside depot 2
	{   56,  120, {AirportMovingDataFlag::ExactPosition},                                 DIR_W }, // 04 Terminal 1
	{   56,  104, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 05 Terminal 2
	{   56,   88, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 06 Terminal 3
	{   56,   72, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 07 Terminal 4
	{   88,  120, {AirportMovingDataFlag::ExactPosition},                                 DIR_N }, // 08 Terminal 5
	{   88,  104, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 09 Terminal 6
	{   88,   88, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 10 Terminal 7
	{   88,   72, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 11 Terminal 8
	{   88,   56, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 12 Helipad 1
	{   72,   56, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 13 Helipad 2
	{   40,  136, {},                                                                     DIR_N }, // 14 Term group 2 enter 1 a
	{   56,  136, {},                                                                     DIR_N }, // 15 Term group 2 enter 1 b
	{   88,  136, {},                                                                     DIR_N }, // 16 Term group 2 enter 2 a
	{  104,  136, {},                                                                     DIR_N }, // 17 Term group 2 enter 2 b
	{  104,  120, {},                                                                     DIR_N }, // 18 Term group 2 - opp term 5
	{  104,  104, {},                                                                     DIR_N }, // 19 Term group 2 - opp term 6 & exit2
	{  104,   88, {},                                                                     DIR_N }, // 20 Term group 2 - opp term 7 & hangar area 2
	{  104,   72, {},                                                                     DIR_N }, // 21 Term group 2 - opp term 8
	{  104,   56, {},                                                                     DIR_N }, // 22 Taxi Term group 2 exit a
	{  104,   40, {},                                                                     DIR_N }, // 23 Taxi Term group 2 exit b
	{   56,   40, {},                                                                     DIR_N }, // 24 Term group 2 exit 2a
	{   40,   40, {},                                                                     DIR_N }, // 25 Term group 2 exit 2b
	{   40,  120, {},                                                                     DIR_N }, // 26 Term group 1 - opp term 1
	{   40,  104, {},                                                                     DIR_N }, // 27 Term group 1 - opp term 2 & hangar area 1
	{   40,   88, {},                                                                     DIR_N }, // 28 Term group 1 - opp term 3
	{   40,   72, {},                                                                     DIR_N }, // 29 Term group 1 - opp term 4
	{   18,   72, {},                                                                     DIR_NW}, // 30 Outway 1
	{    8,   40, {},                                                                     DIR_NW}, // 31 Airport OUTWAY
	{    8,   24, {AirportMovingDataFlag::ExactPosition},                                 DIR_SW}, // 32 Accelerate to end of runway
	{  119,   24, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 33 Release control of runway, for smoother movement
	{  117,   24, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 34 End of runway
	{  197,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Takeoff},  DIR_N }, // 35 Take off
	{  254,   84, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 36 Flying to landing position in air
	{  117,  168, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Land},     DIR_N }, // 37 Going down for land
	{    8,  168, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Brake},    DIR_N }, // 38 Just landed, brake until end of runway
	{    8,  168, {},                                                                     DIR_N }, // 39 Just landed, turn around and taxi
	{    8,  144, {},                                                                     DIR_NW}, // 40 Taxi from runway
	{    8,  128, {},                                                                     DIR_NW}, // 41 Taxi from runway
	{    8,  120, {AirportMovingDataFlag::ExactPosition},                                 DIR_NW}, // 42 Airport entrance
	{   56,  344, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 43 Fly around waiting for a landing spot (north-east)
	{ -200,   88, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 44 Fly around waiting for a landing spot (north-west)
	{   56, -168, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 45 Fly around waiting for a landing spot (south-west)
	{  312,   88, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 46 Fly around waiting for a landing spot (south)
	/* Helicopter */
	{   96,   40, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 47 Bufferspace before helipad
	{   96,   40, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 48 Bufferspace before helipad
	{   82,   54, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 49 Get in position for Helipad1
	{   64,   56, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 50 Get in position for Helipad2
	{   81,   55, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 51 Land at Helipad1
	{   64,   56, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 52 Land at Helipad2
	{   80,   56, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 53 Takeoff Helipad1
	{   64,   56, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 54 Takeoff Helipad2
	{  136,   96, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 55 Go to position for Hangarentrance in air
	{  136,   96, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 56 Land in front of hangar2
	{  126,  104, {},                                                                     DIR_SE}, // 57 Outway 2
	{  136,  136, {},                                                                     DIR_NE}, // 58 Airport OUTWAY 2
	{  136,  152, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 59 Accelerate to end of runway2
	{   16,  152, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 60 Release control of runway2, for smoother movement
	{   20,  152, {AirportMovingDataFlag::NoSpeedClamp},                                  DIR_N }, // 61 End of runway2
	{  -56,  152, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Takeoff},  DIR_N }, // 62 Take off2
	{   24,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Land},     DIR_N }, // 63 Going down for land2
	{  136,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::Brake},    DIR_N }, // 64 Just landed, brake until end of runway2in
	{  136,    8, {},                                                                     DIR_N }, // 65 Just landed, turn around and taxi
	{  136,   24, {},                                                                     DIR_SE}, // 66 Taxi from runway 2in
	{  136,   40, {},                                                                     DIR_SE}, // 67 Taxi from runway 2in
	{  136,   56, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 68 Airport entrance2
	{  -56,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 69 Fly to landing position in air2
	{   88,   40, {},                                                                     DIR_N }, // 70 Taxi Term group 2 exit - opp heli1
	{   72,   40, {},                                                                     DIR_N }, // 71 Taxi Term group 2 exit - opp heli2
	{   88,   57, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 72 pre-helitakeoff helipad 1
	{   71,   56, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 73 pre-helitakeoff helipad 2
	{    8,  120, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 74 Helitakeoff outside depot 1
	{  136,  104, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 75 Helitakeoff outside depot 2
	{  197,  168, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 76 Fly to landing position in air1
};


/** Heliport (heliport). */
static const AirportMovingData _airport_moving_data_heliport[9] = {
	{    5,    9, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 0 - At heliport terminal
	{    2,    9, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 1 - Take off (play sound)
	{   -3,    9, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 2 - In position above landing spot helicopter
	{   -3,    9, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 3 - Land
	{    2,    9, {},                                                                     DIR_N }, // 4 - Goto terminal on ground
	{  -31,   59, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 5 - Circle #1 (north-east)
	{  -31,  -49, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 6 - Circle #2 (north-west)
	{   49,  -49, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 7 - Circle #3 (south-west)
	{   70,    9, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 8 - Circle #4 (south)
};

/** HeliDepot 2x2 (heliport). */
static const AirportMovingData _airport_moving_data_helidepot[18] = {
	{   24,    4, {AirportMovingDataFlag::ExactPosition},                                  DIR_NE}, // 0 - At depot
	{   24,   28, {},                                                                      DIR_N }, // 1 Taxi to right outside depot
	{    5,   38, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_N }, // 2 Flying
	{  -15,  -15, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_N }, // 3 - Circle #1 (north-east)
	{  -15,  -49, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_N }, // 4 - Circle #2 (north-west)
	{   49,  -49, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_N }, // 5 - Circle #3 (south-west)
	{   49,  -15, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_N }, // 6 - Circle #4 (south-east)
	{    8,   32, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_NW}, // 7 - PreHelipad
	{    8,   32, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_NW}, // 8 - Helipad
	{    8,   16, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_NW}, // 9 - Land
	{    8,   16, {AirportMovingDataFlag::HeliLower},                                      DIR_NW}, // 10 - Land
	{    8,   24, {AirportMovingDataFlag::HeliRaise},                                      DIR_N }, // 11 - Take off (play sound)
	{   32,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn},  DIR_NW}, // 12 Air to above hangar area
	{   32,   24, {AirportMovingDataFlag::HeliLower},                                      DIR_NW}, // 13 Taxi to right outside depot
	{    8,   24, {AirportMovingDataFlag::ExactPosition},                                  DIR_NW}, // 14 - on helipad1
	{   24,   28, {AirportMovingDataFlag::HeliRaise},                                      DIR_N }, // 15 Takeoff right outside depot
	{    8,   24, {AirportMovingDataFlag::HeliRaise},                                      DIR_SW}, // 16 - Take off (play sound)
	{    8,   24, {AirportMovingDataFlag::SlowTurn, AirportMovingDataFlag::ExactPosition}, DIR_E }, // 17 - turn on helipad1 for takeoff
};

/** HeliDepot 2x2 (heliport). */
static const AirportMovingData _airport_moving_data_helistation[33] = {
	{    8,    3, {AirportMovingDataFlag::ExactPosition},                                 DIR_SE}, // 00 In Hangar2
	{    8,   22, {},                                                                     DIR_N }, // 01 outside hangar 2
	{  116,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 02 Fly to landing position in air
	{   14,   22, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 03 Helitakeoff outside hangar1(play sound)
	{   24,   22, {},                                                                     DIR_N }, // 04 taxiing
	{   40,   22, {},                                                                     DIR_N }, // 05 taxiing
	{   40,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 06 Helipad 1
	{   56,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 07 Helipad 2
	{   56,   24, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 08 Helipad 3
	{   40,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_N }, // 09 pre-helitakeoff helipad 1
	{   56,    8, {AirportMovingDataFlag::ExactPosition},                                 DIR_N }, // 10 pre-helitakeoff helipad 2
	{   56,   24, {AirportMovingDataFlag::ExactPosition},                                 DIR_N }, // 11 pre-helitakeoff helipad 3
	{   32,    8, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 12 Takeoff Helipad1
	{   48,    8, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 13 Takeoff Helipad2
	{   48,   24, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 14 Takeoff Helipad3
	{   84,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 15 Bufferspace before helipad
	{   68,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 16 Bufferspace before helipad
	{   32,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 17 Get in position for Helipad1
	{   48,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 18 Get in position for Helipad2
	{   48,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_NE}, // 19 Get in position for Helipad3
	{   40,    8, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 20 Land at Helipad1
	{   48,    8, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 21 Land at Helipad2
	{   48,   24, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 22 Land at Helipad3
	{    0,   22, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 23 Go to position for Hangarentrance in air
	{    0,   22, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 24 Land in front of hangar
	{  148,   -8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 25 Fly around waiting for a landing spot (south-east)
	{  148,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 26 Fly around waiting for a landing spot (south-west)
	{  132,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 27 Fly around waiting for a landing spot (south-west)
	{  100,   24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 28 Fly around waiting for a landing spot (north-east)
	{   84,    8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 29 Fly around waiting for a landing spot (south-east)
	{   84,   -8, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 30 Fly around waiting for a landing spot (south-west)
	{  100,  -24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 31 Fly around waiting for a landing spot (north-west)
	{  132,  -24, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 32 Fly around waiting for a landing spot (north-east)
};

/** Oilrig. */
static const AirportMovingData _airport_moving_data_oilrig[9] = {
	{   31,    9, {AirportMovingDataFlag::ExactPosition},                                 DIR_NE}, // 0 - At oilrig terminal
	{   28,    9, {AirportMovingDataFlag::HeliRaise},                                     DIR_N }, // 1 - Take off (play sound)
	{   23,    9, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 2 - In position above landing spot helicopter
	{   23,    9, {AirportMovingDataFlag::HeliLower},                                     DIR_N }, // 3 - Land
	{   28,    9, {},                                                                     DIR_N }, // 4 - Goto terminal on ground
	{  -31,   69, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 5 - circle #1 (north-east)
	{  -31,  -49, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 6 - circle #2 (north-west)
	{   69,  -49, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 7 - circle #3 (south-west)
	{   69,    9, {AirportMovingDataFlag::NoSpeedClamp, AirportMovingDataFlag::SlowTurn}, DIR_N }, // 8 - circle #4 (south)
};

#undef AMD

///////////////////////////////////////////////////////////////////////
/////**********Movement Machine on Airports*********************///////
static const uint8_t _airport_entries_dummy[] = {0, 1, 2, 3};
static const AirportFTAbuildup _airport_fta_dummy[] = {
	{ 0, TO_ALL, {}, 3},
	{ 1, TO_ALL, {}, 0},
	{ 2, TO_ALL, {}, 1},
	{ 3, TO_ALL, {}, 2},
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

/* First element of terminals array tells us how many depots there are (to know size of array)
 * this may be changed later when airports are moved to external file  */
static const HangarTileTable _airport_depots_country[] = { {{3, 0}, DIR_SE, 0} };
static const uint8_t _airport_terminal_country[] = {1, 2};
static const uint8_t _airport_entries_country[] = {16, 15, 18, 17};
static const AirportFTAbuildup _airport_fta_country[] = {
	{  0, HANGAR, AirportBlock::Nothing, 1 },
	{  1, TERMGROUP, AirportBlock::AirportBusy, 0 }, { 1, HANGAR, {}, 0 }, { 1, TERM1, AirportBlock::Term1, 2 }, { 1, TERM2, {}, 4 }, { 1, HELITAKEOFF, {}, 19 }, { 1, TO_ALL, {}, 6 },
	{  2, TERM1, AirportBlock::Term1, 1 },
	{  3, TERM2, AirportBlock::Term2, 5 },
	{  4, TERMGROUP, AirportBlock::AirportBusy, 0 }, { 4, TERM2, {}, 5 }, { 4, HANGAR, {}, 1 }, { 4, TAKEOFF, {}, 6 }, { 4, HELITAKEOFF, {}, 1 },
	{  5, TERMGROUP, AirportBlock::AirportBusy, 0 }, { 5, TERM2, AirportBlock::Term2, 3 }, { 5, TO_ALL, {}, 4 },
	{  6, TO_ALL, AirportBlock::AirportBusy, 7 },
	/* takeoff */
	{  7, TAKEOFF, AirportBlock::AirportBusy, 8 },
	{  8, STARTTAKEOFF, AirportBlock::Nothing, 9 },
	{  9, ENDTAKEOFF, AirportBlock::Nothing, 0 },
	/* landing */
	{ 10, FLYING, AirportBlock::Nothing, 15 }, { 10, LANDING, {}, 11 }, { 10, HELILANDING, {}, 20 },
	{ 11, LANDING, AirportBlock::AirportBusy, 12 },
	{ 12, TO_ALL, AirportBlock::AirportBusy, 13 },
	{ 13, ENDLANDING, AirportBlock::AirportBusy, 14 }, { 13, TERM2, {}, 5 }, { 13, TO_ALL, {}, 14 },
	{ 14, TO_ALL, AirportBlock::AirportBusy, 1 },
	/* In air */
	{ 15, TO_ALL, AirportBlock::Nothing, 16 },
	{ 16, TO_ALL, AirportBlock::Nothing, 17 },
	{ 17, TO_ALL, AirportBlock::Nothing, 18 },
	{ 18, TO_ALL, AirportBlock::Nothing, 10 },
	{ 19, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 20, HELILANDING, AirportBlock::AirportBusy, 21 },
	{ 21, HELIENDLANDING, AirportBlock::AirportBusy, 1 },
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

static const HangarTileTable _airport_depots_commuter[] = { {{4, 0}, DIR_SE, 0} };
static const uint8_t _airport_terminal_commuter[] = { 1, 3 };
static const uint8_t _airport_entries_commuter[] = {22, 21, 24, 23};
static const AirportFTAbuildup _airport_fta_commuter[] = {
	{  0, HANGAR, AirportBlock::Nothing, 1 }, { 0, HELITAKEOFF, AirportBlock::TaxiwayBusy, 1 }, { 0, TO_ALL, {}, 1 },
	{  1, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 1, HANGAR, {}, 0 }, { 1, TAKEOFF, {}, 11 }, { 1, TERM1, AirportBlock::TaxiwayBusy, 10 }, { 1, TERM2, AirportBlock::TaxiwayBusy, 10 }, { 1, TERM3, AirportBlock::TaxiwayBusy, 10 }, { 1, HELIPAD1, AirportBlock::TaxiwayBusy, 10 }, { 1, HELIPAD2, AirportBlock::TaxiwayBusy, 10 }, { 1, HELITAKEOFF, AirportBlock::TaxiwayBusy, 37 }, { 1, TO_ALL, {}, 0 },
	{  2, TERMGROUP, AirportBlock::AirportEntrance, 2 }, { 2, HANGAR, {}, 8 }, { 2, TERM1, {}, 8 }, { 2, TERM2, {}, 8 }, { 2, TERM3, {}, 8 }, { 2, HELIPAD1, {}, 8 }, { 2, HELIPAD2, {}, 8 }, { 2, HELITAKEOFF, {}, 8 }, { 2, TO_ALL, {}, 2 },
	{  3, TERM1, AirportBlock::Term1, 8 }, { 3, HANGAR, {}, 8 }, { 3, TAKEOFF, {}, 8 }, { 3, TO_ALL, {}, 3 },
	{  4, TERM2, AirportBlock::Term2, 9 }, { 4, HANGAR, {}, 9 }, { 4, TAKEOFF, {}, 9 }, { 4, TO_ALL, {}, 4 },
	{  5, TERM3, AirportBlock::Term3, 10 }, { 5, HANGAR, {}, 10 }, { 5, TAKEOFF, {}, 10 }, { 5, TO_ALL, {}, 5 },
	{  6, HELIPAD1, AirportBlock::Helipad1, 6 }, { 6, HANGAR, AirportBlock::TaxiwayBusy, 9 }, { 6, HELITAKEOFF, {}, 35 },
	{  7, HELIPAD2, AirportBlock::Helipad2, 7 }, { 7, HANGAR, AirportBlock::TaxiwayBusy, 10 }, { 7, HELITAKEOFF, {}, 36 },
	{  8, TERMGROUP, AirportBlock::TaxiwayBusy, 8 }, { 8, TAKEOFF, AirportBlock::TaxiwayBusy, 9 }, { 8, HANGAR, AirportBlock::TaxiwayBusy, 9 }, { 8, TERM1, AirportBlock::Term1, 3 }, { 8, TO_ALL, AirportBlock::TaxiwayBusy, 9 },
	{  9, TERMGROUP, AirportBlock::TaxiwayBusy, 9 }, { 9, TAKEOFF, AirportBlock::TaxiwayBusy, 10 }, { 9, HANGAR, AirportBlock::TaxiwayBusy, 10 }, { 9, TERM2, AirportBlock::Term2, 4 }, { 9, HELIPAD1, AirportBlock::Helipad1, 6 }, { 9, HELITAKEOFF, AirportBlock::Helipad1, 6 }, { 9, TERM1, AirportBlock::TaxiwayBusy, 8 }, { 9, TO_ALL, AirportBlock::TaxiwayBusy, 10 },
	{ 10, TERMGROUP, AirportBlock::TaxiwayBusy, 10 }, { 10, TERM3, AirportBlock::Term3, 5 }, { 10, HELIPAD1, {}, 9 }, { 10, HELIPAD2, AirportBlock::Helipad2, 7 }, { 10, HELITAKEOFF, {}, 1 }, { 10, TAKEOFF, AirportBlock::TaxiwayBusy, 1 }, { 10, HANGAR, AirportBlock::TaxiwayBusy, 1 }, { 10, TO_ALL, AirportBlock::TaxiwayBusy, 9 },
	{ 11, TO_ALL, AirportBlock::OutWay, 12 },
	/* takeoff */
	{ 12, TAKEOFF, AirportBlock::RunwayInOut, 13 },
	{ 13, TO_ALL, AirportBlock::RunwayInOut, 14 },
	{ 14, STARTTAKEOFF, AirportBlock::RunwayInOut, 15 },
	{ 15, ENDTAKEOFF, AirportBlock::Nothing, 0 },
	/* landing */
	{ 16, FLYING, AirportBlock::Nothing, 21 }, { 16, LANDING, AirportBlock::InWay, 17 }, { 16, HELILANDING, {}, 25 },
	{ 17, LANDING, AirportBlock::RunwayInOut, 18 },
	{ 18, TO_ALL, AirportBlock::RunwayInOut, 19 },
	{ 19, TO_ALL, AirportBlock::RunwayInOut, 20 },
	{ 20, ENDLANDING, AirportBlock::InWay, 2 },
	/* In Air */
	{ 21, TO_ALL, AirportBlock::Nothing, 22 },
	{ 22, TO_ALL, AirportBlock::Nothing, 23 },
	{ 23, TO_ALL, AirportBlock::Nothing, 24 },
	{ 24, TO_ALL, AirportBlock::Nothing, 16 },
	/* Helicopter -- stay in air in special place as a buffer to choose from helipads */
	{ 25, HELILANDING, AirportBlock::PreHelipad, 26 },
	{ 26, HELIENDLANDING, AirportBlock::PreHelipad, 26 }, { 26, HELIPAD1, {}, 27 }, { 26, HELIPAD2, {}, 28 }, { 26, HANGAR, {}, 33 },
	{ 27, TO_ALL, AirportBlock::Nothing, 29 }, // helipad1 approach
	{ 28, TO_ALL, AirportBlock::Nothing, 30 },
	/* landing */
	{ 29, TERMGROUP, AirportBlock::Nothing, 0 }, { 29, HELIPAD1, AirportBlock::Helipad1, 6 },
	{ 30, TERMGROUP, AirportBlock::Nothing, 0 }, { 30, HELIPAD2, AirportBlock::Helipad2, 7 },
	/* Helicopter -- takeoff */
	{ 31, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 32, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 33, TO_ALL, AirportBlock::TaxiwayBusy, 34 }, // need to go to hangar when waiting in air
	{ 34, TO_ALL, AirportBlock::TaxiwayBusy, 1 },
	{ 35, TO_ALL, AirportBlock::Helipad1, 31 },
	{ 36, TO_ALL, AirportBlock::Helipad2, 32 },
	{ 37, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

static const HangarTileTable _airport_depots_city[] = { {{5, 0}, DIR_SE, 0} };
static const uint8_t _airport_terminal_city[] = { 1, 3 };
static const uint8_t _airport_entries_city[] = {26, 29, 27, 28};
static const AirportFTAbuildup _airport_fta_city[] = {
	{  0, HANGAR, AirportBlock::Nothing, 1 }, { 0, TAKEOFF, AirportBlock::OutWay, 1 }, { 0, TO_ALL, {}, 1 },
	{  1, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 1, HANGAR, {}, 0 }, { 1, TERM2, {}, 6 }, { 1, TERM3, {}, 6 }, { 1, TO_ALL, {}, 7 }, // for all else, go to 7
	{  2, TERM1, AirportBlock::Term1, 7 }, { 2, TAKEOFF, AirportBlock::OutWay, 7 }, { 2, TO_ALL, {}, 7 },
	{  3, TERM2, AirportBlock::Term2, 5 }, { 3, TAKEOFF, AirportBlock::OutWay, 6 }, { 3, TO_ALL, {}, 6 },
	{  4, TERM3, AirportBlock::Term3, 5 }, { 4, TAKEOFF, AirportBlock::OutWay, 5 }, { 4, TO_ALL, {}, 5 },
	{  5, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 5, TERM2, AirportBlock::Term2, 3 }, { 5, TERM3, AirportBlock::Term3, 4 }, { 5, TO_ALL, {}, 6 },
	{  6, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 6, TERM2, AirportBlock::Term2, 3 }, { 6, TERM3, {}, 5 }, { 6, HANGAR, {}, 1 }, { 6, TO_ALL, {}, 7 },
	{  7, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 7, TERM1, AirportBlock::Term1, 2 }, { 7, TAKEOFF, AirportBlock::OutWay, 8 }, { 7, HELITAKEOFF, {}, 22 }, { 7, HANGAR, {}, 1 }, { 7, TO_ALL, {}, 6 },
	{  8, TO_ALL, AirportBlock::OutWay, 9 },
	{  9, TO_ALL, AirportBlock::RunwayInOut, 10 },
	/* takeoff */
	{ 10, TAKEOFF, AirportBlock::RunwayInOut, 11 },
	{ 11, STARTTAKEOFF, AirportBlock::Nothing, 12 },
	{ 12, ENDTAKEOFF, AirportBlock::Nothing, 0 },
	/* landing */
	{ 13, FLYING, AirportBlock::Nothing, 18 }, { 13, LANDING, {}, 14 }, { 13, HELILANDING, {}, 23 },
	{ 14, LANDING, AirportBlock::RunwayInOut, 15 },
	{ 15, TO_ALL, AirportBlock::RunwayInOut, 17 },
	{ 16, TO_ALL, AirportBlock::RunwayInOut, 17 }, // not used, left for compatibility
	{ 17, ENDLANDING, AirportBlock::InWay, 7 },
	/* In Air */
	{ 18, TO_ALL, AirportBlock::Nothing, 25 },
	{ 19, TO_ALL, AirportBlock::Nothing, 20 },
	{ 20, TO_ALL, AirportBlock::Nothing, 21 },
	{ 21, TO_ALL, AirportBlock::Nothing, 13 },
	/* helicopter */
	{ 22, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 23, HELILANDING, AirportBlock::InWay, 24 },
	{ 24, HELIENDLANDING, AirportBlock::InWay, 17 },
	{ 25, TO_ALL, AirportBlock::Nothing, 20},
	{ 26, TO_ALL, AirportBlock::Nothing, 19},
	{ 27, TO_ALL, AirportBlock::Nothing, 28},
	{ 28, TO_ALL, AirportBlock::Nothing, 19},
	{ 29, TO_ALL, AirportBlock::Nothing, 26},
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

static const HangarTileTable _airport_depots_metropolitan[] = { {{5, 0}, DIR_SE, 0} };
static const uint8_t _airport_terminal_metropolitan[] = { 1, 3 };
static const uint8_t _airport_entries_metropolitan[] = {20, 19, 22, 21};
static const AirportFTAbuildup _airport_fta_metropolitan[] = {
	{  0, HANGAR, AirportBlock::Nothing, 1 },
	{  1, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 1, HANGAR, {}, 0 }, { 1, TERM2, {}, 6 }, { 1, TERM3, {}, 6 }, { 1, TO_ALL, {}, 7 }, // for all else, go to 7
	{  2, TERM1, AirportBlock::Term1, 7 },
	{  3, TERM2, AirportBlock::Term2, 6 },
	{  4, TERM3, AirportBlock::Term3, 5 },
	{  5, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 5, TERM2, AirportBlock::Term2, 3 }, { 5, TERM3, AirportBlock::Term3, 4 }, { 5, TO_ALL, {}, 6 },
	{  6, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 6, TERM2, AirportBlock::Term2, 3 }, { 6, TERM3, {}, 5 }, { 6, HANGAR, {}, 1 }, { 6, TO_ALL, {}, 7 },
	{  7, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 7, TERM1, AirportBlock::Term1, 2 }, { 7, TAKEOFF, {}, 8 }, { 7, HELITAKEOFF, {}, 23 }, { 7, HANGAR, {}, 1 }, { 7, TO_ALL, {}, 6 },
	{  8, 0, AirportBlock::OutWay, 9 },
	{  9, 0, AirportBlock::RunwayOut, 10 },
	/* takeoff */
	{ 10, TAKEOFF, AirportBlock::RunwayOut, 11 },
	{ 11, STARTTAKEOFF, AirportBlock::Nothing, 12 },
	{ 12, ENDTAKEOFF, AirportBlock::Nothing, 0 },
	/* landing */
	{ 13, FLYING, AirportBlock::Nothing, 19 }, { 13, LANDING, {}, 14 }, { 13, HELILANDING, {}, 25 },
	{ 14, LANDING, AirportBlock::RunwayIn, 15 },
	{ 15, TO_ALL, AirportBlock::RunwayIn, 16 },
	{ 16, TERMGROUP, AirportBlock::RunwayIn, 0 }, { 16, ENDLANDING, AirportBlock::InWay, 17 },
	{ 17, TERMGROUP, AirportBlock::RunwayOut, 0 }, { 17, ENDLANDING, AirportBlock::InWay, 18 },
	{ 18, ENDLANDING, AirportBlock::InWay, 27 },
	/* In Air */
	{ 19, TO_ALL, AirportBlock::Nothing, 20 },
	{ 20, TO_ALL, AirportBlock::Nothing, 21 },
	{ 21, TO_ALL, AirportBlock::Nothing, 22 },
	{ 22, TO_ALL, AirportBlock::Nothing, 13 },
	/* helicopter */
	{ 23, TO_ALL, AirportBlock::Nothing, 24 },
	{ 24, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 25, HELILANDING, AirportBlock::InWay, 26 },
	{ 26, HELIENDLANDING, AirportBlock::InWay, 18 },
	{ 27, TERMGROUP, AirportBlock::TaxiwayBusy, 27 }, { 27, TERM1, AirportBlock::Term1, 2 }, { 27, TO_ALL, {}, 7 },
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

static const HangarTileTable _airport_depots_international[] = { {{0, 3}, DIR_SE, 0}, {{6, 1}, DIR_SE, 1} };
static const uint8_t _airport_terminal_international[] = { 2, 3, 3 };
static const uint8_t _airport_entries_international[] = { 38, 37, 40, 39 };
static const AirportFTAbuildup _airport_fta_international[] = {
	{  0, HANGAR, AirportBlock::Nothing, 2 }, { 0, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 0, TERMGROUP, AirportBlock::TermGroup2Enter1, 1 }, { 0, HELITAKEOFF, AirportBlock::AirportEntrance, 2 }, { 0, TO_ALL, {}, 2 },
	{  1, HANGAR, AirportBlock::Nothing, 3 }, { 1, TERMGROUP, AirportBlock::Hangar2Area, 1 }, { 1, HELITAKEOFF, AirportBlock::Hangar2Area, 3 }, { 1, TO_ALL, {}, 3 },
	{  2, TERMGROUP, AirportBlock::AirportEntrance, 0 }, { 2, HANGAR, {}, 0 }, { 2, TERM4, {}, 12 }, { 2, TERM5, {}, 12 }, { 2, TERM6, {}, 12 }, { 2, HELIPAD1, {}, 12 }, { 2, HELIPAD2, {}, 12 }, { 2, HELITAKEOFF, {}, 51 }, { 2, TO_ALL, {}, 23 },
	{  3, TERMGROUP, AirportBlock::Hangar2Area, 0 }, { 3, HANGAR, {}, 1 }, { 3, HELITAKEOFF, {}, 52 }, { 3, TO_ALL, {}, 18 },
	{  4, TERM1, AirportBlock::Term1, 23 }, { 4, HANGAR, AirportBlock::AirportEntrance, 23 }, { 4, TO_ALL, {}, 23 },
	{  5, TERM2, AirportBlock::Term2, 24 }, { 5, HANGAR, AirportBlock::AirportEntrance, 24 }, { 5, TO_ALL, {}, 24 },
	{  6, TERM3, AirportBlock::Term3, 25 }, { 6, HANGAR, AirportBlock::AirportEntrance, 25 }, { 6, TO_ALL, {}, 25 },
	{  7, TERM4, AirportBlock::Term4, 16 }, { 7, HANGAR, AirportBlock::Hangar2Area, 16 }, { 7, TO_ALL, {}, 16 },
	{  8, TERM5, AirportBlock::Term5, 17 }, { 8, HANGAR, AirportBlock::Hangar2Area, 17 }, { 8, TO_ALL, {}, 17 },
	{  9, TERM6, AirportBlock::Term6, 18 }, { 9, HANGAR, AirportBlock::Hangar2Area, 18 }, { 9, TO_ALL, {}, 18 },
	{ 10, HELIPAD1, AirportBlock::Helipad1, 10 }, { 10, HANGAR, AirportBlock::Hangar2Area, 16 }, { 10, HELITAKEOFF, {}, 47 },
	{ 11, HELIPAD2, AirportBlock::Helipad2, 11 }, { 11, HANGAR, AirportBlock::Hangar2Area, 17 }, { 11, HELITAKEOFF, {}, 48 },
	{ 12, TO_ALL, AirportBlock::TermGroup2Enter1, 13 },
	{ 13, TO_ALL, AirportBlock::TermGroup2Enter1, 14 },
	{ 14, TO_ALL, AirportBlock::TermGroup2Enter2, 15 },
	{ 15, TO_ALL, AirportBlock::TermGroup2Enter2, 16 },
	{ 16, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 16, TERM4, AirportBlock::Term4, 7 }, { 16, HELIPAD1, AirportBlock::Helipad1, 10 }, { 16, HELITAKEOFF, AirportBlock::Helipad1, 10 }, { 16, TO_ALL, {}, 17 },
	{ 17, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 17, TERM5, AirportBlock::Term5, 8 }, { 17, TERM4, {}, 16 }, { 17, HELIPAD1, {}, 16 }, { 17, HELIPAD2, AirportBlock::Helipad2, 11 }, { 17, HELITAKEOFF, AirportBlock::Helipad2, 11 }, { 17, TO_ALL, {}, 18 },
	{ 18, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 18, TERM6, AirportBlock::Term6, 9 }, { 18, TAKEOFF, {}, 19 }, { 18, HANGAR, AirportBlock::Hangar2Area, 3 }, { 18, TO_ALL, {}, 17 },
	{ 19, TO_ALL, AirportBlock::TermGroup2Exit1, 20 },
	{ 20, TO_ALL, AirportBlock::TermGroup2Exit1, 21 },
	{ 21, TO_ALL, AirportBlock::TermGroup2Exit2, 22 },
	{ 22, TO_ALL, AirportBlock::TermGroup2Exit2, 26 },
	{ 23, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 23, TERM1, AirportBlock::Term1, 4 }, { 23, HANGAR, AirportBlock::AirportEntrance, 2 }, { 23, TO_ALL, {}, 24 },
	{ 24, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 24, TERM2, AirportBlock::Term2, 5 }, { 24, TERM1, {}, 23 }, { 24, HANGAR, {}, 23 }, { 24, TO_ALL, {}, 25 },
	{ 25, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 25, TERM3, AirportBlock::Term3, 6 }, { 25, TAKEOFF, {}, 26 }, { 25, TO_ALL, {}, 24 },
	{ 26, TERMGROUP, AirportBlock::TaxiwayBusy, 0 }, { 26, TAKEOFF, {}, 27 }, { 26, TO_ALL, {}, 25 },
	{ 27, TO_ALL, AirportBlock::OutWay, 28 },
	/* takeoff */
	{ 28, TAKEOFF, AirportBlock::OutWay, 29 },
	{ 29, TO_ALL, AirportBlock::RunwayOut, 30 },
	{ 30, STARTTAKEOFF, AirportBlock::Nothing, 31 },
	{ 31, ENDTAKEOFF, AirportBlock::Nothing, 0 },
	/* landing */
	{ 32, FLYING, AirportBlock::Nothing, 37 }, { 32, LANDING, {}, 33 }, { 32, HELILANDING, {}, 41 },
	{ 33, LANDING, AirportBlock::RunwayIn, 34 },
	{ 34, TO_ALL, AirportBlock::RunwayIn, 35 },
	{ 35, TO_ALL, AirportBlock::RunwayIn, 36 },
	{ 36, ENDLANDING, AirportBlock::InWay, 36 }, { 36, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 36, TERMGROUP, AirportBlock::TermGroup2Enter1, 1 }, { 36, TERM4, {}, 12 }, { 36, TERM5, {}, 12 }, { 36, TERM6, {}, 12 }, { 36, TO_ALL, {}, 2 },
	/* In Air */
	{ 37, TO_ALL, AirportBlock::Nothing, 38 },
	{ 38, TO_ALL, AirportBlock::Nothing, 39 },
	{ 39, TO_ALL, AirportBlock::Nothing, 40 },
	{ 40, TO_ALL, AirportBlock::Nothing, 32 },
	/* Helicopter -- stay in air in special place as a buffer to choose from helipads */
	{ 41, HELILANDING, AirportBlock::PreHelipad, 42 },
	{ 42, HELIENDLANDING, AirportBlock::PreHelipad, 42 }, { 42, HELIPAD1, {}, 43 }, { 42, HELIPAD2, {}, 44 }, { 42, HANGAR, {}, 49 },
	{ 43, TO_ALL, AirportBlock::Nothing, 45 },
	{ 44, TO_ALL, AirportBlock::Nothing, 46 },
	/* landing */
	{ 45, TERMGROUP, AirportBlock::Nothing, 0 }, { 45, HELIPAD1, AirportBlock::Helipad1, 10 },
	{ 46, TERMGROUP, AirportBlock::Nothing, 0 }, { 46, HELIPAD2, AirportBlock::Helipad2, 11 },
	/* Helicopter -- takeoff */
	{ 47, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 48, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 49, TO_ALL, AirportBlock::Hangar2Area, 50 }, // need to go to hangar when waiting in air
	{ 50, TO_ALL, AirportBlock::Hangar2Area, 3 },
	{ 51, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 52, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

/* intercontinental */
static const HangarTileTable _airport_depots_intercontinental[] = { {{0, 5}, DIR_SE, 0}, {{8, 4}, DIR_SE, 1} };
static const uint8_t _airport_terminal_intercontinental[] = { 2, 4, 4 };
static const uint8_t _airport_entries_intercontinental[] = { 44, 43, 46, 45 };
static const AirportFTAbuildup _airport_fta_intercontinental[] = {
	{  0, HANGAR, AirportBlock::Nothing, 2 }, { 0, TERMGROUP, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 0 }, { 0, TERMGROUP, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 1 }, { 0, TAKEOFF, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 2 }, { 0, TO_ALL, {}, 2 },
	{  1, HANGAR, AirportBlock::Nothing, 3 }, { 1, TERMGROUP, AirportBlock::Hangar2Area, 1 }, { 1, TERMGROUP, AirportBlock::Hangar2Area, 0 }, { 1, TO_ALL, {}, 3 },
	{  2, TERMGROUP, AirportBlock::Hangar1Area, 0 }, { 2, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 2, TERMGROUP, AirportBlock::TermGroup1, 1 }, { 2, HANGAR, {}, 0 }, { 2, TAKEOFF, AirportBlock::TermGroup1, 27 }, { 2, TERM5, {}, 26 }, { 2, TERM6, {}, 26 }, { 2, TERM7, {}, 26 }, { 2, TERM8, {}, 26 }, { 2, HELIPAD1, {}, 26 }, { 2, HELIPAD2, {}, 26 }, { 2, HELITAKEOFF, {}, 74 }, { 2, TO_ALL, {}, 27 },
	{  3, TERMGROUP, AirportBlock::Hangar2Area, 0 }, { 3, HANGAR, {}, 1 }, { 3, HELITAKEOFF, {}, 75 }, {3, TAKEOFF, {}, 59}, { 3, TO_ALL, {}, 20 },
	{  4, TERM1, AirportBlock::Term1, 26 }, { 4, HANGAR, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 26 }, { 4, TO_ALL, {}, 26 },
	{  5, TERM2, AirportBlock::Term2, 27 }, { 5, HANGAR, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 27 }, { 5, TO_ALL, {}, 27 },
	{  6, TERM3, AirportBlock::Term3, 28 }, { 6, HANGAR, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 28 }, { 6, TO_ALL, {}, 28 },
	{  7, TERM4, AirportBlock::Term4, 29 }, { 7, HANGAR, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 29 }, { 7, TO_ALL, {}, 29 },
	{  8, TERM5, AirportBlock::Term5, 18 }, { 8, HANGAR, AirportBlock::Hangar2Area, 18 }, { 8, TO_ALL, {}, 18 },
	{  9, TERM6, AirportBlock::Term6, 19 }, { 9, HANGAR, AirportBlock::Hangar2Area, 19 }, { 9, TO_ALL, {}, 19 },
	{ 10, TERM7, AirportBlock::Term7, 20 }, { 10, HANGAR, AirportBlock::Hangar2Area, 20 }, { 10, TO_ALL, {}, 20 },
	{ 11, TERM8, AirportBlock::Term8, 21 }, { 11, HANGAR, AirportBlock::Hangar2Area, 21 }, { 11, TO_ALL, {}, 21 },
	{ 12, HELIPAD1, AirportBlock::Helipad1, 12 }, { 12, HANGAR, {}, 70 }, { 12, HELITAKEOFF, {}, 72 },
	{ 13, HELIPAD2, AirportBlock::Helipad2, 13 }, { 13, HANGAR, {}, 71 }, { 13, HELITAKEOFF, {}, 73 },
	{ 14, TO_ALL, AirportBlock::TermGroup2Enter1, 15 },
	{ 15, TO_ALL, AirportBlock::TermGroup2Enter1, 16 },
	{ 16, TO_ALL, AirportBlock::TermGroup2Enter2, 17 },
	{ 17, TO_ALL, AirportBlock::TermGroup2Enter2, 18 },
	{ 18, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 18, TERM5, AirportBlock::Term5, 8 }, { 18, TAKEOFF, {}, 19 }, { 18, HELITAKEOFF, AirportBlock::Helipad1, 19 }, { 18, TO_ALL, AirportBlock::TermGroup2Exit1, 19 },
	{ 19, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 19, TERM6, AirportBlock::Term6, 9 }, { 19, TERM5, {}, 18 }, { 19, TAKEOFF, {}, 57 }, { 19, HELITAKEOFF, AirportBlock::Helipad1, 20 }, { 19, TO_ALL, AirportBlock::TermGroup2Exit1, 20 }, // add exit to runway out 2
	{ 20, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 20, TERM7, AirportBlock::Term7, 10 }, { 20, TERM5, {}, 19 }, { 20, TERM6, {}, 19 }, { 20, HANGAR, AirportBlock::Hangar2Area, 3 }, { 20, TAKEOFF, {}, 19 }, { 20, TO_ALL, AirportBlock::TermGroup2Exit1, 21 },
	{ 21, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 21, TERM8, AirportBlock::Term8, 11 }, { 21, HANGAR, AirportBlock::Hangar2Area, 20 }, { 21, TERM5, {}, 20 }, { 21, TERM6, {}, 20 }, { 21, TERM7, {}, 20 }, { 21, TAKEOFF, {}, 20 }, { 21, TO_ALL, AirportBlock::TermGroup2Exit1, 22 },
	{ 22, TERMGROUP, AirportBlock::TermGroup2, 0 }, { 22, HANGAR, {}, 21 }, { 22, TERM5, {}, 21 }, { 22, TERM6, {}, 21 }, { 22, TERM7, {}, 21 }, { 22, TERM8, {}, 21 }, { 22, TAKEOFF, {}, 21 }, { 22, TO_ALL, {}, 23 },
	{ 23, TO_ALL, AirportBlock::TermGroup2Exit1, 70 },
	{ 24, TO_ALL, AirportBlock::TermGroup2Exit2, 25 },
	{ 25, TERMGROUP, AirportBlock::TermGroup2Exit2, 0 }, { 25, HANGAR, {AirportBlock::Hangar1Area, AirportBlock::TermGroup1}, 29 }, { 25, TO_ALL, {}, 29 },
	{ 26, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 26, TERM1, AirportBlock::Term1, 4 }, { 26, HANGAR, AirportBlock::Hangar1Area, 27 }, { 26, TERM5, AirportBlock::TermGroup2Enter1, 14 }, { 26, TERM6, AirportBlock::TermGroup2Enter1, 14 }, { 26, TERM7, AirportBlock::TermGroup2Enter1, 14 }, { 26, TERM8, AirportBlock::TermGroup2Enter1, 14 }, { 26, HELIPAD1, AirportBlock::TermGroup2Enter1, 14 }, { 26, HELIPAD2, AirportBlock::TermGroup2Enter1, 14 }, { 26, HELITAKEOFF, AirportBlock::TermGroup2Enter1, 14 }, { 26, TO_ALL, {}, 27 },
	{ 27, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 27, TERM2, AirportBlock::Term2, 5 }, { 27, HANGAR, AirportBlock::Hangar1Area, 2 }, { 27, TERM1, {}, 26 }, { 27, TERM5, {}, 26 }, { 27, TERM6, {}, 26 }, { 27, TERM7, {}, 26 }, { 27, TERM8, {}, 26 }, { 27, HELIPAD1, {}, 14 }, { 27, HELIPAD2, {}, 14 }, { 27, TO_ALL, {}, 28 },
	{ 28, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 28, TERM3, AirportBlock::Term3, 6 }, { 28, HANGAR, AirportBlock::Hangar1Area, 27 }, { 28, TERM1, {}, 27 }, { 28, TERM2, {}, 27 }, { 28, TERM4, {}, 29 }, { 28, TERM5, {}, 14 }, { 28, TERM6, {}, 14 }, { 28, TERM7, {}, 14 }, { 28, TERM8, {}, 14 }, { 28, HELIPAD1, {}, 14 }, { 28, HELIPAD2, {}, 14 }, { 28, TO_ALL, {}, 29 },
	{ 29, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 29, TERM4, AirportBlock::Term4, 7 }, { 29, HANGAR, AirportBlock::Hangar1Area, 27 }, { 29, TAKEOFF, {}, 30 }, { 29, TO_ALL, {}, 28 },
	{ 30, TO_ALL, AirportBlock::OutWay3, 31 },
	{ 31, TO_ALL, AirportBlock::OutWay, 32 },
	/* takeoff */
	{ 32, TAKEOFF, AirportBlock::RunwayOut, 33 },
	{ 33, TO_ALL, AirportBlock::RunwayOut, 34 },
	{ 34, STARTTAKEOFF, AirportBlock::Nothing, 35 },
	{ 35, ENDTAKEOFF, AirportBlock::Nothing, 0 },
	/* landing */
	{ 36, TO_ALL, {}, 0 },
	{ 37, LANDING, AirportBlock::RunwayIn, 38 },
	{ 38, TO_ALL, AirportBlock::RunwayIn, 39 },
	{ 39, TO_ALL, AirportBlock::RunwayIn, 40 },
	{ 40, ENDLANDING, AirportBlock::RunwayIn, 41 },
	{ 41, TO_ALL, AirportBlock::InWay, 42 },
	{ 42, TERMGROUP, AirportBlock::InWay, 0 }, { 42, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 42, TERMGROUP, AirportBlock::TermGroup1, 1 }, { 42, HANGAR, {}, 2 }, { 42, TO_ALL, {}, 26 },
	/* In Air */
	{ 43, TO_ALL, {}, 44 },
	{ 44, FLYING, {}, 45 }, { 44, HELILANDING, {}, 47 }, { 44, LANDING, {}, 69 }, { 44, TO_ALL, {}, 45 },
	{ 45, TO_ALL, {}, 46 },
	{ 46, FLYING, {}, 43 }, { 46, LANDING, {}, 76 }, { 46, TO_ALL, {}, 43 },
	/* Helicopter -- stay in air in special place as a buffer to choose from helipads */
	{ 47, HELILANDING, AirportBlock::PreHelipad, 48 },
	{ 48, HELIENDLANDING, AirportBlock::PreHelipad, 48 }, { 48, HELIPAD1, {}, 49 }, { 48, HELIPAD2, {}, 50 }, { 48, HANGAR, {}, 55 },
	{ 49, TO_ALL, AirportBlock::Nothing, 51 },
	{ 50, TO_ALL, AirportBlock::Nothing, 52 },
	/* landing */
	{ 51, TERMGROUP, AirportBlock::Nothing, 0 }, { 51, HELIPAD1, AirportBlock::Helipad1, 12 }, { 51, HANGAR, {}, 55 }, { 51, TO_ALL, {}, 12 },
	{ 52, TERMGROUP, AirportBlock::Nothing, 0 }, { 52, HELIPAD2, AirportBlock::Helipad2, 13 }, { 52, HANGAR, {}, 55 }, { 52, TO_ALL, {}, 13 },
	/* Helicopter -- takeoff */
	{ 53, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 54, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 55, TO_ALL, AirportBlock::Hangar2Area, 56 }, // need to go to hangar when waiting in air
	{ 56, TO_ALL, AirportBlock::Hangar2Area, 3 },
	/* runway 2 out support */
	{ 57, TERMGROUP, AirportBlock::OutWay2, 0 }, { 57, TAKEOFF, {}, 58 }, { 57, TO_ALL, {}, 58 },
	{ 58, TO_ALL, AirportBlock::OutWay2, 59 },
	{ 59, TAKEOFF, AirportBlock::RunwayOut2, 60 }, // takeoff
	{ 60, TO_ALL, AirportBlock::RunwayOut2, 61 },
	{ 61, STARTTAKEOFF, AirportBlock::Nothing, 62 },
	{ 62, ENDTAKEOFF, AirportBlock::Nothing, 0 },
	/* runway 2 in support */
	{ 63, LANDING, AirportBlock::RunwayIn2, 64 },
	{ 64, TO_ALL, AirportBlock::RunwayIn2, 65 },
	{ 65, TO_ALL, AirportBlock::RunwayIn2, 66 },
	{ 66, ENDLANDING, AirportBlock::RunwayIn2, 0 }, { 66, TERMGROUP, {}, 1 }, { 66, TERMGROUP, {}, 0 }, { 66, TO_ALL, {}, 67 },
	{ 67, TO_ALL, AirportBlock::InWay2, 68 },
	{ 68, TERMGROUP, AirportBlock::InWay2, 0 }, { 68, TERMGROUP, AirportBlock::TermGroup2, 1 }, { 68, TERMGROUP, AirportBlock::TermGroup1, 0 }, { 68, HANGAR, AirportBlock::Hangar2Area, 22 }, { 68, TO_ALL, {}, 22 },
	{ 69, TERMGROUP, AirportBlock::RunwayIn2, 0 }, { 69, TO_ALL, AirportBlock::RunwayIn2, 63 },
	{ 70, TERMGROUP, AirportBlock::TermGroup2Exit1, 0 }, { 70, HELIPAD1, AirportBlock::Helipad1, 12 }, { 70, HELITAKEOFF, AirportBlock::Helipad1, 12 }, { 70, TO_ALL, {}, 71 },
	{ 71, TERMGROUP, AirportBlock::TermGroup2Exit1, 0 }, { 71, HELIPAD2, AirportBlock::Helipad2, 13 }, { 71, HELITAKEOFF, AirportBlock::Helipad1, 12 }, { 71, TO_ALL, {}, 24 },
	{ 72, TO_ALL, AirportBlock::Helipad1, 53 },
	{ 73, TO_ALL, AirportBlock::Helipad2, 54 },
	{ 74, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 75, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 76, TERMGROUP, AirportBlock::RunwayIn, 0 }, { 76, TO_ALL, AirportBlock::RunwayIn, 37 },
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};


/* heliports, oilrigs don't have depots */
static const uint8_t _airport_entries_heliport[] = { 7, 7, 7, 7 };
static const AirportFTAbuildup _airport_fta_heliport[] = {
	{ 0, HELIPAD1, AirportBlock::Helipad1, 1 },
	{ 1, HELITAKEOFF, AirportBlock::Nothing, 0 }, // takeoff
	{ 2, TERMGROUP, AirportBlock::AirportBusy, 0 }, { 2, HELILANDING, {}, 3 }, { 2, HELITAKEOFF, {}, 1 },
	{ 3, HELILANDING, AirportBlock::AirportBusy, 4 },
	{ 4, HELIENDLANDING, AirportBlock::AirportBusy, 4 }, { 4, HELIPAD1, AirportBlock::Helipad1, 0 }, { 4, HELITAKEOFF, {}, 2 },
	/* In Air */
	{ 5, TO_ALL, AirportBlock::Nothing, 6 },
	{ 6, TO_ALL, AirportBlock::Nothing, 7 },
	{ 7, TO_ALL, AirportBlock::Nothing, 8 },
	{ 8, FLYING, AirportBlock::Nothing, 5 }, { 8, HELILANDING, AirportBlock::Helipad1, 2 }, // landing
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};
#define _airport_entries_oilrig _airport_entries_heliport
#define _airport_fta_oilrig _airport_fta_heliport

/* helidepots */
static const HangarTileTable _airport_depots_helidepot[] = { {{1, 0}, DIR_SE, 0} };
static const uint8_t _airport_entries_helidepot[] = { 4, 4, 4, 4 };
static const AirportFTAbuildup _airport_fta_helidepot[] = {
	{  0, HANGAR, AirportBlock::Nothing, 1 },
	{  1, TERMGROUP, AirportBlock::Hangar2Area, 0 }, { 1, HANGAR, {}, 0 }, { 1, HELIPAD1, AirportBlock::Helipad1, 14 }, { 1, HELITAKEOFF, {}, 15 }, { 1, TO_ALL, {}, 0 },
	{  2, FLYING, AirportBlock::Nothing, 3 }, { 2, HELILANDING, AirportBlock::PreHelipad, 7 }, { 2, HANGAR, {}, 12 }, { 2, HELITAKEOFF, AirportBlock::Nothing, 16 },
	/* In Air */
	{  3, TO_ALL, AirportBlock::Nothing, 4 },
	{  4, TO_ALL, AirportBlock::Nothing, 5 },
	{  5, TO_ALL, AirportBlock::Nothing, 6 },
	{  6, TO_ALL, AirportBlock::Nothing, 2 },
	/* Helicopter -- stay in air in special place as a buffer to choose from helipads */
	{  7, HELILANDING, AirportBlock::PreHelipad, 8 },
	{  8, HELIENDLANDING, AirportBlock::PreHelipad, 8 }, { 8, HELIPAD1, {}, 9 }, { 8, HANGAR, {}, 12 }, { 8, TO_ALL, {}, 2 },
	{  9, TO_ALL, AirportBlock::Nothing, 10 },
	/* landing */
	{ 10, TERMGROUP, AirportBlock::Nothing, 10 }, { 10, HELIPAD1, AirportBlock::Helipad1, 14 }, { 10, HANGAR, {}, 1 }, { 10, TO_ALL, {}, 14 },
	/* Helicopter -- takeoff */
	{ 11, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 12, TO_ALL, AirportBlock::Hangar2Area, 13 }, // need to go to hangar when waiting in air
	{ 13, TO_ALL, AirportBlock::Hangar2Area, 1 },
	{ 14, HELIPAD1, AirportBlock::Helipad1, 14 }, { 14, HANGAR, {}, 1 }, { 14, HELITAKEOFF, {}, 17 },
	{ 15, HELITAKEOFF, AirportBlock::Nothing, 0 }, // takeoff outside depot
	{ 16, HELITAKEOFF, {}, 14 },
	{ 17, TO_ALL, AirportBlock::Nothing, 11 },
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

/* helistation */
static const HangarTileTable _airport_depots_helistation[] = { {{0, 0}, DIR_SE, 0} };
static const uint8_t _airport_entries_helistation[] = { 25, 25, 25, 25 };
static const AirportFTAbuildup _airport_fta_helistation[] = {
	{  0, HANGAR, AirportBlock::Nothing, 8 },    { 0, HELIPAD1, {}, 1 }, { 0, HELIPAD2, {}, 1 }, { 0, HELIPAD3, {}, 1 }, { 0, HELITAKEOFF, {}, 1 }, { 0, TO_ALL, {}, 0 },
	{  1, TERMGROUP, AirportBlock::Hangar2Area, 0 },  { 1, HANGAR, {}, 0 }, { 1, HELITAKEOFF, {}, 3 }, { 1, TO_ALL, {}, 4 },
	/* landing */
	{  2, FLYING, AirportBlock::Nothing, 28 },   { 2, HELILANDING, {}, 15 }, { 2, TO_ALL, {}, 28 },
	/* helicopter side */
	{  3, HELITAKEOFF, AirportBlock::Nothing, 0 }, // helitakeoff outside hangar2
	{  4, TERMGROUP, AirportBlock::TaxiwayBusy, 0 },  { 4, HANGAR, AirportBlock::Hangar2Area, 1 }, { 4, HELITAKEOFF, {}, 1 }, { 4, TO_ALL, {}, 5 },
	{  5, TERMGROUP, AirportBlock::TaxiwayBusy, 0 },  { 5, HELIPAD1, AirportBlock::Helipad1, 6 }, { 5, HELIPAD2, AirportBlock::Helipad2, 7 }, { 5, HELIPAD3, AirportBlock::Helipad3, 8 }, { 5, TO_ALL, {}, 4 },
	{  6, HELIPAD1, AirportBlock::Helipad1, 5 }, { 6, HANGAR, AirportBlock::Hangar2Area, 5 }, { 6, HELITAKEOFF, {}, 9 }, { 6, TO_ALL, {}, 6 },
	{  7, HELIPAD2, AirportBlock::Helipad2, 5 }, { 7, HANGAR, AirportBlock::Hangar2Area, 5 }, { 7, HELITAKEOFF, {}, 10 }, { 7, TO_ALL, {}, 7 },
	{  8, HELIPAD3, AirportBlock::Helipad3, 5 }, { 8, HANGAR, AirportBlock::Hangar2Area, 5 }, { 8, HELITAKEOFF, {}, 11 }, { 8, TO_ALL, {}, 8 },
	{  9, TO_ALL, AirportBlock::Helipad1, 12 },
	{ 10, TO_ALL, AirportBlock::Helipad2, 13 },
	{ 11, TO_ALL, AirportBlock::Helipad3, 14 },
	{ 12, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 13, HELITAKEOFF, AirportBlock::Nothing, 0 },
	{ 14, HELITAKEOFF, AirportBlock::Nothing, 0 },
	/* heli - in flight moves */
	{ 15, HELILANDING, AirportBlock::PreHelipad, 16 },
	{ 16, HELIENDLANDING, AirportBlock::PreHelipad, 16 }, { 16, HELIPAD1, {}, 17 }, { 16, HELIPAD2, {}, 18 }, { 16, HELIPAD3, {}, 19 }, { 16, HANGAR, {}, 23 },
	{ 17, TO_ALL, AirportBlock::Nothing, 20 },
	{ 18, TO_ALL, AirportBlock::Nothing, 21 },
	{ 19, TO_ALL, AirportBlock::Nothing, 22 },
	/* heli landing */
	{ 20, TERMGROUP, AirportBlock::Nothing, 0 }, { 20, HELIPAD1, AirportBlock::Helipad1, 6 }, { 20, HANGAR, {}, 23 }, { 20, TO_ALL, {}, 6 },
	{ 21, TERMGROUP, AirportBlock::Nothing, 0 }, { 21, HELIPAD2, AirportBlock::Helipad2, 7 }, { 21, HANGAR, {}, 23 }, { 21, TO_ALL, {}, 7 },
	{ 22, TERMGROUP, AirportBlock::Nothing, 0 }, { 22, HELIPAD3, AirportBlock::Helipad3, 8 }, { 22, HANGAR, {}, 23 }, { 22, TO_ALL, {}, 8 },
	{ 23, TO_ALL, AirportBlock::Hangar2Area, 24 }, // need to go to helihangar when waiting in air
	{ 24, TO_ALL, AirportBlock::Hangar2Area, 1 },
	{ 25, TO_ALL, AirportBlock::Nothing, 26 },
	{ 26, TO_ALL, AirportBlock::Nothing, 27 },
	{ 27, TO_ALL, AirportBlock::Nothing, 2 },
	{ 28, TO_ALL, AirportBlock::Nothing, 29 },
	{ 29, TO_ALL, AirportBlock::Nothing, 30 },
	{ 30, TO_ALL, AirportBlock::Nothing, 31 },
	{ 31, TO_ALL, AirportBlock::Nothing, 32 },
	{ 32, TO_ALL, AirportBlock::Nothing, 25 },
	{ MAX_ELEMENTS, TO_ALL, {}, 0 } // end marker. DO NOT REMOVE
};

#endif /* AIRPORT_MOVEMENT_H */
