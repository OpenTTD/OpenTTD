/* $Id$ */

/** @file vehiclelist.h Functions and type for generating vehicle lists. */

#ifndef VEHICLELIST_H
#define VEHICLELIST_H

#include "core/smallvec_type.hpp"

typedef SmallVector<const Vehicle *, 32> VehicleList;

void GenerateVehicleSortList(VehicleList *list, VehicleType type, Owner owner, uint32 index, uint16 window_type);
void BuildDepotVehicleList(VehicleType type, TileIndex tile, VehicleList *engine_list, VehicleList *wagon_list, bool individual_wagons = false);

#endif /* VEHICLELIST_H */
