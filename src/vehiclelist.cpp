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
 * Pack a VehicleListIdentifier in a single uint32.
 * @return The packed identifier.
 */
uint32 VehicleListIdentifier::Pack()
{
	assert(this->company < (1 <<  4));
	assert(this->type    < (1 <<  3));
	assert(this->vtype   < (1 <<  2));
	assert(this->index   < (1 << 20));

	return this->company << 28 | this->type << 23 | this->vtype << 26 | this->index;
}

/**
 * Unpack a VehicleListIdentifier from a single uint32.
 * @param data The data to unpack.
 * @return true iff the data was valid (enough).
 */
bool VehicleListIdentifier::Unpack(uint32 data)
{
	this->company = (CompanyID)GB(data, 28, 4);
	this->type    = (VehicleListType)GB(data, 23, 3);
	this->vtype   = (VehicleType)GB(data, 26, 2);
	this->index   = GB(data, 0, 20);

	return this->type < VLT_END;
}

/**
 * Decode a packed vehicle list identifier into a new one.
 * @param data The data to unpack.
 */
VehicleListIdentifier::VehicleListIdentifier(uint32 data)
{
	bool ret = this->Unpack(data);
	assert(ret);
}

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
				if (v->type == vli.vtype && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if ((order->IsType(OT_GOTO_STATION) || order->IsType(OT_GOTO_WAYPOINT) || order->IsType(OT_AUTOMATIC))
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
			if (v == NULL || v->type != vli.vtype || !v->IsPrimaryVehicle()) return false;

			for (; v != NULL; v = v->NextShared()) {
				*list->Append() = v;
			}
			break;

		case VL_GROUP_LIST:
			if (vli.index != ALL_GROUP) {
				FOR_ALL_VEHICLES(v) {
					if (v->type == vli.vtype && v->IsPrimaryVehicle() &&
							v->owner == vli.company && v->group_id == vli.index) {
						*list->Append() = v;
					}
				}
				break;
			}
			/* FALL THROUGH */

		case VL_STANDARD:
			FOR_ALL_VEHICLES(v) {
				if (v->type == vli.vtype && v->owner == vli.company && v->IsPrimaryVehicle()) {
					*list->Append() = v;
				}
			}
			break;

		case VL_DEPOT_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == vli.vtype && v->IsPrimaryVehicle()) {
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

		default: return false;
	}

	list->Compact();
	return true;
}
