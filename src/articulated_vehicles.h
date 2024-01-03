/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file articulated_vehicles.h Functions related to articulated vehicles. */

#ifndef ARTICULATED_VEHICLES_H
#define ARTICULATED_VEHICLES_H

#include "vehicle_type.h"
#include "engine_type.h"

uint CountArticulatedParts(EngineID engine_type, bool purchase_window);
CargoArray GetCapacityOfArticulatedParts(EngineID engine);
CargoTypes GetCargoTypesOfArticulatedParts(EngineID engine);
void AddArticulatedParts(Vehicle *first);
void GetArticulatedRefitMasks(EngineID engine, bool include_initial_cargo_type, CargoTypes *union_mask, CargoTypes *intersection_mask);
CargoTypes GetUnionOfArticulatedRefitMasks(EngineID engine, bool include_initial_cargo_type);
CargoTypes GetCargoTypesOfArticulatedVehicle(const Vehicle *v, CargoID *cargo_type);
bool IsArticulatedVehicleRefittable(EngineID engine);
bool IsArticulatedEngine(EngineID engine_type);
void CheckConsistencyOfArticulatedVehicle(const Vehicle *v);


#endif /* ARTICULATED_VEHICLES_H */
