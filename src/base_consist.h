/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_consist.h Properties for front vehicles/consists. */

#ifndef BASE_CONSIST_H
#define BASE_CONSIST_H

#include "order_type.h"
#include "timer/timer_game_tick.h"

/** Various front vehicle properties that are preserved when autoreplacing, using order-backup or switching front engines within a consist. */
struct BaseConsist {
	std::string name;                   ///< Name of vehicle

	/* Used for timetabling. */
	TimerGameTick::Ticks current_order_time;    ///< How many ticks have passed since this order started.
	TimerGameTick::Ticks lateness_counter;      ///< How many ticks late (or early if negative) this vehicle is.
	TimerGameTick::TickCounter timetable_start; ///< At what tick of TimerGameTick::counter the vehicle should start its timetable.

	TimerGameTick::TickCounter depot_unbunching_last_departure; ///< When the vehicle last left its unbunching depot.
	TimerGameTick::TickCounter depot_unbunching_next_departure; ///< When the vehicle will next try to leave its unbunching depot.
	TimerGameTick::Ticks round_trip_time;  ///< How many ticks for a single circumnavigation of the orders.

	uint16_t service_interval;            ///< The interval for (automatic) servicing; either in days or %.

	VehicleOrderID cur_real_order_index;///< The index to the current real (non-implicit) order
	VehicleOrderID cur_implicit_order_index;///< The index to the current implicit order

	uint16_t vehicle_flags;               ///< Used for gradual loading and other miscellaneous things (@see VehicleFlags enum)

	virtual ~BaseConsist() = default;

	void CopyConsistPropertiesFrom(const BaseConsist *src);
	void ResetDepotUnbunching();
};

#endif /* BASE_CONSIST_H */
