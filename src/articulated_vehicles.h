/* $Id$ */

/** @file articulated_vehicles.h Functions related to articulated vehicles. */

#ifndef ARTICULATED_VEHICLES_H
#define ARTICULATED_VEHICLES_H

#include "vehicle_type.h"
#include "engine_type.h"

uint CountArticulatedParts(EngineID engine_type, bool purchase_window);
uint16 *GetCapacityOfArticulatedParts(EngineID engine, VehicleType type);
void AddArticulatedParts(Vehicle **vl, VehicleType type);

#endif /* ARTICULATED_VEHICLES_H */
