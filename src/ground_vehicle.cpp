/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ground_vehicle.cpp Implementation of GroundVehicle. */

#include "stdafx.h"
#include "train.h"
#include "roadveh.h"
#include "vehicle_gui.h"
#include "window_func.h"

/**
 * Recalculates the cached total power of a vehicle. Should be called when the consist is changed.
 */
template <class T, VehicleType Type>
void GroundVehicle<T, Type>::PowerChanged()
{
	assert(this->First() == this);
	const T *v = T::From(this);

	uint32 total_power = 0;
	uint32 max_te = 0;
	uint32 number_of_parts = 0;
	uint16 max_track_speed = v->GetDisplayMaxSpeed();

	for (const T *u = v; u != NULL; u = u->Next()) {
		uint32 current_power = u->GetPower() + u->GetPoweredPartPower(u);
		total_power += current_power;

		/* Only powered parts add tractive effort. */
		if (current_power > 0) max_te += u->GetWeight() * u->GetTractiveEffort();
		number_of_parts++;

		/* Get minimum max speed for this track. */
		uint16 track_speed = u->GetMaxTrackSpeed();
		if (track_speed > 0) max_track_speed = min(max_track_speed, track_speed);
	}

	byte air_drag;
	byte air_drag_value = v->GetAirDrag();

	/* If air drag is set to zero (default), the resulting air drag coefficient is dependent on max speed. */
	if (air_drag_value == 0) {
		uint16 max_speed = v->GetDisplayMaxSpeed();
		/* Simplification of the method used in TTDPatch. It uses <= 10 to change more steadily from 128 to 196. */
		air_drag = (max_speed <= 10) ? 192 : max(2048 / max_speed, 1);
	} else {
		/* According to the specs, a value of 0x01 in the air drag property means "no air drag". */
		air_drag = (air_drag_value == 1) ? 0 : air_drag_value;
	}

	this->gcache.cached_air_drag = air_drag + 3 * air_drag * number_of_parts / 20;

	max_te *= 10000; // Tractive effort in (tonnes * 1000 * 10 =) N.
	max_te /= 256;   // Tractive effort is a [0-255] coefficient.
	if (this->gcache.cached_power != total_power || this->gcache.cached_max_te != max_te) {
		/* Stop the vehicle if it has no power. */
		if (total_power == 0) this->vehstatus |= VS_STOPPED;

		this->gcache.cached_power = total_power;
		this->gcache.cached_max_te = max_te;
		SetWindowDirty(WC_VEHICLE_DETAILS, this->index);
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);
	}

	this->gcache.cached_max_track_speed = max_track_speed;
}

/**
 * Recalculates the cached weight of a vehicle and its parts. Should be called each time the cargo on
 * the consist changes.
 */
template <class T, VehicleType Type>
void GroundVehicle<T, Type>::CargoChanged()
{
	assert(this->First() == this);
	uint32 weight = 0;

	for (T *u = T::From(this); u != NULL; u = u->Next()) {
		uint32 current_weight = u->GetWeight();
		weight += current_weight;
		/* Slope steepness is in percent, result in N. */
		u->gcache.cached_slope_resistance = current_weight * u->GetSlopeSteepness() * 100;
	}

	/* Store consist weight in cache. */
	this->gcache.cached_weight = max<uint32>(1, weight);
	/* Friction in bearings and other mechanical parts is 0.1% of the weight (result in N). */
	this->gcache.cached_axle_resistance = 10 * weight;

	/* Now update vehicle power (tractive effort is dependent on weight). */
	this->PowerChanged();
}

/**
 * Calculates the acceleration of the vehicle under its current conditions.
 * @return Current acceleration of the vehicle.
 */
template <class T, VehicleType Type>
int GroundVehicle<T, Type>::GetAcceleration() const
{
	/* Templated class used for function calls for performance reasons. */
	const T *v = T::From(this);
	int32 speed = v->GetCurrentSpeed(); // [km/h-ish]

	/* Weight is stored in tonnes. */
	int32 mass = this->gcache.cached_weight;

	/* Power is stored in HP, we need it in watts. */
	int32 power = this->gcache.cached_power * 746;

	int32 resistance = 0;

	bool maglev = v->GetAccelerationType() == 2;

	const int area = v->GetAirDragArea();
	if (!maglev) {
		/* Static resistance plus rolling friction. */
		resistance = this->gcache.cached_axle_resistance;
		resistance += mass * v->GetRollingFriction();
	}
	/* Air drag; the air drag coefficient is in an arbitrary NewGRF-unit,
	 * so we need some magic conversion factor. */
	resistance += (area * this->gcache.cached_air_drag * speed * speed) / 500;

	resistance += this->GetSlopeResistance();

	/* This value allows to know if the vehicle is accelerating or braking. */
	AccelStatus mode = v->GetAccelerationStatus();

	const int max_te = this->gcache.cached_max_te; // [N]
	int force;
	if (speed > 0) {
		if (!maglev) {
			/* Conversion factor from km/h to m/s is 5/18 to get [N] in the end. */
			force = power * 18 / (speed * 5);
			if (mode == AS_ACCEL && force > max_te) force = max_te;
		} else {
			force = power / 25;
		}
	} else {
		/* "Kickoff" acceleration. */
		force = (mode == AS_ACCEL && !maglev) ? min(max_te, power) : power;
		force = max(force, (mass * 8) + resistance);
	}

	if (mode == AS_ACCEL) {
		return (force - resistance) / (mass * 2);
	} else {
		return min(-force - resistance, -10000) / mass;
	}
}

/* Instantiation for Train */
template struct GroundVehicle<Train, VEH_TRAIN>;
/* Instantiation for RoadVehicle */
template struct GroundVehicle<RoadVehicle, VEH_ROAD>;
