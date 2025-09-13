/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot.cpp Handling of depots. */

#include "stdafx.h"
#include "depot_base.h"
#include "order_backup.h"
#include "order_func.h"
#include "train.h"
#include "window_func.h"
#include "core/pool_func.hpp"
#include "vehicle_gui.h"
#include "vehiclelist.h"

#include "safeguards.h"

/** All our depots tucked away in a pool. */
DepotPool _depot_pool("Depot");
INSTANTIATE_POOL_METHODS(Depot)

/**
 * Clean up a depot
 */
Depot::~Depot()
{
	if (CleaningPool()) return;

	if (!IsDepotTile(this->xy) || GetDepotIndex(this->xy) != this->index) {
		/* It can happen there is no depot here anymore (TTO/TTD savegames) */
		return;
	}

	/* Clear the order backup. */
	OrderBackup::Reset(this->xy, false);

	/* Clear the depot from all order-lists */
	RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, this->index);

	/* Delete the depot-window */
	CloseWindowById(WC_VEHICLE_DEPOT, this->xy);

	/* Delete the depot list */
	VehicleType vt = GetDepotVehicleType(this->xy);
	CloseWindowById(GetWindowClassForVehicleType(vt), VehicleListIdentifier(VL_DEPOT_LIST, vt, GetTileOwner(this->xy), this->index).ToWindowNumber());
}

/**
 * Count how many vehicles are queued in each depot.
 * @param type    Type of vehicle
 */
void RebuildDepotOccupancyCache(VehicleType type)
{
	for (Depot *d : Depot::Iterate()) {
		d->running_vehicles = 0;
	}

	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type != type) continue;

		switch (type) {
			case VEH_TRAIN: {
				const Train *t = Train::From(v);
				if (t->IsArticulatedPart() || t->IsRearDualheaded()) continue;
				if (!t->IsInDepot()) continue;
				if (!t->IsPrimaryVehicle()) continue;
				if (t->vehstatus.Test(VehState::Stopped)) continue;
				break;
			}

			default:
				if (!v->IsPrimaryVehicle()) continue;
				if (!v->IsInDepot()) continue;
				if (v->vehstatus.Test(VehState::Stopped)) continue;
				break;
		}

		Depot *d = Depot::GetByTile(v->tile);
		if (!d) continue;
		d->running_vehicles++;
	}
}
