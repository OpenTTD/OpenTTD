/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehiclelist.cpp Lists of vehicles. */

#include "stdafx.h"
#include "vehicle_gui.h"
#include "train.h"
#include "vehiclelist.h"

/**
 * Generate a list of vehicles inside a depot.
 * @param type    Type of vehicle
 * @param tile    The tile the depot is located on
 * @param engines Pointer to list to add vehicles to
 * @param wagons  Pointer to list to add wagons to (can be NULL)
 * @param individual_wagons If true add every wagon to #wagons which is not attached to an engine. If false only add the first wagon of every row.
 */
void BuildDepotVehicleList(VehicleType type, TileIndex tile, VehicleList *engines, VehicleList *wagons, bool individual_wagons)
{
	engines->Clear();
	if (wagons != NULL && wagons != engines) wagons->Clear();

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* General tests for all vehicle types */
		if (v->type != type) continue;
		if (v->tile != tile) continue;

		switch (type) {
			case VEH_TRAIN: {
				const Train *t = Train::From(v);
				if (t->IsArticulatedPart() || t->IsRearDualheaded()) continue;
				if (t->track != TRACK_BIT_DEPOT) continue;
				if (wagons != NULL && t->First()->IsFreeWagon()) {
					if (individual_wagons || t->IsFreeWagon()) *wagons->Append() = t;
					continue;
				}
				break;
			}

			default:
				if (!v->IsInDepot()) continue;
				break;
		}

		if (!v->IsPrimaryVehicle()) continue;

		*engines->Append() = v;
	}

	/* Ensure the lists are not wasting too much space. If the lists are fresh
	 * (i.e. built within a command) then this will actually do nothing. */
	engines->Compact();
	if (wagons != NULL && wagons != engines) wagons->Compact();
}

/**
 * Generate a list of vehicles based on window type.
 * @param list Pointer to list to add vehicles to
 * @param vli  The identifier of this vehicle list.
 * @return false if invalid list is requested
 */
bool GenerateVehicleSortList(VehicleList *list, const VehicleListIdentifier &vli)
{
	list->Clear();

	const Vehicle *v;

	switch (vli.type) {
		case VL_STATION_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == vli.type && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if ((order->IsType(OT_GOTO_STATION) || order->IsType(OT_GOTO_WAYPOINT))
								&& order->GetDestination() == vli.index) {
							*list->Append() = v;
							break;
						}
					}
				}
			}
			break;

		case VL_SHARED_ORDERS:
			/* Add all vehicles from this vehicle's shared order list */
			v = Vehicle::GetIfValid(vli.index);
			if (v == NULL || v->type != vli.type || !v->IsPrimaryVehicle()) return false;

			for (; v != NULL; v = v->NextShared()) {
				*list->Append() = v;
			}
			break;

		case VL_STANDARD:
			FOR_ALL_VEHICLES(v) {
				if (v->type == vli.type && v->owner == vli.company && v->IsPrimaryVehicle()) {
					*list->Append() = v;
				}
			}
			break;

		case VL_DEPOT_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == vli.type && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->IsType(OT_GOTO_DEPOT) && !(order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) && order->GetDestination() == vli.index) {
							*list->Append() = v;
							break;
						}
					}
				}
			}
			break;

		case VL_GROUP_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == vli.type && v->IsPrimaryVehicle() &&
						v->owner == vli.company && v->group_id == vli.index) {
					*list->Append() = v;
				}
			}
			break;

		default: return false;
	}

	list->Compact();
	return true;
}
