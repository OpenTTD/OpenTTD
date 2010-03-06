/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ground_vehicle.hpp Base class and functions for all vehicles that move through ground. */

#ifndef GROUND_VEHICLE_HPP
#define GROUND_VEHICLE_HPP

#include "vehicle_base.h"

/** What is the status of our acceleration? */
enum AccelStatus {
	AS_ACCEL, ///< We want to go faster, if possible of course.
	AS_BRAKE  ///< We want to stop.
};

/**
 * Cached acceleration values.
 * All of these values except cached_slope_resistance are set only for the first part of a vehicle.
 */
struct AccelerationCache {
	/* Cached values, recalculated when the cargo on a vehicle changes (in addition to the conditions below) */
	uint32 cached_weight;           ///< Total weight of the consist.
	uint32 cached_slope_resistance; ///< Resistance caused by weight when this vehicle part is at a slope.
	uint32 cached_max_te;           ///< Maximum tractive effort of consist.

	/* Cached values, recalculated on load and each time a vehicle is added to/removed from the consist. */
	uint32 cached_power;            ///< Total power of the consist.
	uint32 cached_air_drag;         ///< Air drag coefficient of the vehicle.
	uint16 cached_axle_resistance;  ///< Resistance caused by the axles of the vehicle.
	uint16 cached_max_track_speed;  ///< Maximum consist speed limited by track type.
};

/** Ground vehicle flags. */
enum GroundVehicleFlags {
	GVF_GOINGUP_BIT       = 0,
	GVF_GOINGDOWN_BIT     = 1,
};

/**
 * Base class for all vehicles that move through ground.
 *
 * Child classes must define all of the following functions.
 * These functions are not defined as pure virtual functions at this class to improve performance.
 *
 * virtual uint16      GetPower() const = 0;
 * virtual uint16      GetPoweredPartPower(const T *head) const = 0;
 * virtual uint16      GetWeight() const = 0;
 * virtual byte        GetTractiveEffort() const = 0;
 * virtual AccelStatus GetAccelerationStatus() const = 0;
 * virtual uint16      GetCurrentSpeed() const = 0;
 * virtual uint32      GetRollingFriction() const = 0;
 * virtual int         GetAccelerationType() const = 0;
 * virtual int32       GetSlopeSteepness() const = 0;
 * virtual uint16      GetInitialMaxSpeed() const = 0;
 * virtual uint16      GetMaxTrackSpeed() const = 0;
 */
template <class T, VehicleType Type>
struct GroundVehicle : public SpecializedVehicle<T, Type> {
	AccelerationCache acc_cache;
	uint16 gv_flags; ///< @see GroundVehicleFlags

	/**
	 * The constructor at SpecializedVehicle must be called.
	 */
	GroundVehicle() : SpecializedVehicle<T, Type>() {}

	void PowerChanged();
	void CargoChanged();
	int GetAcceleration() const;

 	/**
	 * Calculates the total slope resistance for this vehicle.
	 * @return Slope resistance.
	 */
	FORCEINLINE int32 GetSlopeResistance() const
	{
		int32 incl = 0;

		for (const T *u = T::From(this); u != NULL; u = u->Next()) {
			if (HasBit(u->gv_flags, GVF_GOINGUP_BIT)) {
				incl += u->acc_cache.cached_slope_resistance;
			} else if (HasBit(u->gv_flags, GVF_GOINGDOWN_BIT)) {
				incl -= u->acc_cache.cached_slope_resistance;
			}
		}

		return incl;
	}
};

#endif /* GROUND_VEHICLE_HPP */
