/* $Id$ */

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
#include "date_type.h"

/** Various front vehicle properties that are preserved when autoreplacing, using order-backup or switching front engines within a consist. */
struct BaseConsist {
	char *name;                         ///< Name of vehicle

	/* Used for timetabling. */
	uint32 current_order_time;          ///< How many ticks have passed since this order started.
	int32 lateness_counter;             ///< How many ticks late (or early if negative) this vehicle is.
	Date timetable_start;               ///< When the vehicle is supposed to start the timetable.

	uint16 service_interval;            ///< The interval for (automatic) servicing; either in days or %.

	VehicleOrderID cur_real_order_index;///< The index to the current real (non-implicit) order
	VehicleOrderID cur_implicit_order_index;///< The index to the current implicit order

	uint16 vehicle_flags;               ///< Used for gradual loading and other miscellaneous things (@see VehicleFlags enum)

	BaseConsist() : name(nullptr) {}
	virtual ~BaseConsist();

	void CopyConsistPropertiesFrom(const BaseConsist *src);
};

#endif /* BASE_CONSIST_H */
