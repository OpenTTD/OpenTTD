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

/**
 * Base class for all vehicles that move through ground.
 */
template <class T, VehicleType Type>
struct GroundVehicle : public SpecializedVehicle<T, Type> {

	/**
	 * The constructor at SpecializedVehicle must be called.
	 */
	GroundVehicle() : SpecializedVehicle<T, Type>() {}

};

#endif /* GROUND_VEHICLE_HPP */
