/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_animation_type.h Definitions related to NewGRF animation. */

#ifndef NEWGRF_ANIMATION_TYPE_H
#define NEWGRF_ANIMATION_TYPE_H

static const uint8_t ANIM_STATUS_NON_LOOPING  = 0x00; ///< Animation is not looping.
static const uint8_t ANIM_STATUS_LOOPING      = 0x01; ///< Animation is looping.
static const uint8_t ANIM_STATUS_NO_ANIMATION = 0xFF; ///< There is no animation.

/** Information about animation. */
struct AnimationInfo {
	uint8_t  frames;   ///< The number of frames.
	uint8_t  status;   ///< Status; 0: no looping, 1: looping, 0xFF: no animation.
	uint8_t  speed;    ///< The speed, i.e. the amount of time between frames.
	uint16_t triggers; ///< The triggers that trigger animation.
};

/** Animation triggers for station. */
enum StationAnimationTrigger {
	SAT_BUILT,         ///< Trigger tile when built.
	SAT_NEW_CARGO,     ///< Trigger station on new cargo arrival.
	SAT_CARGO_TAKEN,   ///< Trigger station when cargo is completely taken.
	SAT_TRAIN_ARRIVES, ///< Trigger platform when train arrives.
	SAT_TRAIN_DEPARTS, ///< Trigger platform when train leaves.
	SAT_TRAIN_LOADS,   ///< Trigger platform when train loads/unloads.
	SAT_250_TICKS,     ///< Trigger station every 250 ticks.
};

/** Animation triggers of the industries. */
enum IndustryAnimationTrigger {
	IAT_CONSTRUCTION_STATE_CHANGE,  ///< Trigger whenever the construction state changes.
	IAT_TILELOOP,                   ///< Trigger in the periodic tile loop.
	IAT_INDUSTRY_TICK,              ///< Trigger every tick.
	IAT_INDUSTRY_RECEIVED_CARGO,    ///< Trigger when cargo is received .
	IAT_INDUSTRY_DISTRIBUTES_CARGO, ///< Trigger when cargo is distributed.
};

/** Animation triggers for airport tiles */
enum AirpAnimationTrigger {
	AAT_BUILT,                   ///< Triggered when the airport is built (for all tiles at the same time).
	AAT_TILELOOP,                ///< Triggered in the periodic tile loop.
	AAT_STATION_NEW_CARGO,       ///< Triggered when new cargo arrives at the station (for all tiles at the same time).
	AAT_STATION_CARGO_TAKEN,     ///< Triggered when a cargo type is completely removed from the station (for all tiles at the same time).
	AAT_STATION_250_TICKS,       ///< Triggered every 250 ticks (for all tiles at the same time).
	AAT_STATION_AIRPLANE_LAND,   ///< Triggered when an airplane (not a helicopter) touches down at the airport (for single tile).
};

/** Animation triggers for objects. */
enum ObjectAnimationTrigger {
	OAT_BUILT,     ///< Triggered when the object is built (for all tiles at the same time).
	OAT_TILELOOP,  ///< Triggered in the periodic tile loop.
	OAT_256_TICKS, ///< Triggered every 256 ticks (for all tiles at the same time).
};

#endif /* NEWGRF_ANIMATION_TYPE_H */
