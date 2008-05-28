/* $Id$ */

/** @file vehicle.cpp Base implementations of all vehicles. */

#include "stdafx.h"
#include "openttd.h"
#include "vehicle_type.h"
#include "vehicle_func.h"
#include "vehicle_base.h"
#include "vehicle_gui.h"
#include "core/alloc_func.hpp"
#include "train.h"
#include "vehiclelist.h"

/**
 * Generate a list of vehicles inside a depot.
 * @param type    Type of vehicle
 * @param tile    The tile the depot is located on
 * @param engines Pointer to list to add vehicles to
 * @param wagons  Pointer to list to add wagons to (can be NULL)
 */
void BuildDepotVehicleList(VehicleType type, TileIndex tile, VehicleList *engines, VehicleList *wagons)
{
	engines->Clear();
	if (wagons != NULL && wagons != engines) wagons->Clear();

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* General tests for all vehicle types */
		if (v->type != type) continue;
		if (v->tile != tile) continue;

		switch (type) {
			case VEH_TRAIN:
				if (v->u.rail.track != TRACK_BIT_DEPOT) continue;
				if (wagons != NULL && IsFreeWagon(v)) {
					*wagons->Append() = v;
					continue;
				}
				break;

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
 * @param list        Pointer to list to add vehicles to
 * @param type        Type of vehicle
 * @param owner       Player to generate list for
 * @param index       This parameter has different meanings depending on window_type
 *    <ul>
 *      <li>VLW_STATION_LIST:  index of station to generate a list for</li>
 *      <li>VLW_SHARED_ORDERS: index of order to generate a list for<li>
 *      <li>VLW_STANDARD: not used<li>
 *      <li>VLW_DEPOT_LIST: TileIndex of the depot/hangar to make the list for</li>
 *      <li>VLW_GROUP_LIST: index of group to generate a list for</li>
 *    </ul>
 * @param window_type The type of window the list is for, using the VLW_ flags in vehicle_gui.h
 */
void GenerateVehicleSortList(VehicleList *list, VehicleType type, PlayerID owner, uint32 index, uint16 window_type)
{
	list->Clear();

	const Vehicle *v;

	switch (window_type) {
		case VLW_STATION_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->IsType(OT_GOTO_STATION) && order->GetDestination() == index) {
							*list->Append() = v;
							break;
						}
					}
				}
			}
			break;

		case VLW_SHARED_ORDERS:
			FOR_ALL_VEHICLES(v) {
				/* Find a vehicle with the order in question */
				if (v->orders != NULL && v->orders->index == index) {
					/* Add all vehicles from this vehicle's shared order list */
					for (v = GetFirstVehicleFromSharedList(v); v != NULL; v = v->next_shared) {
						*list->Append() = v;
					}
					break;
				}
			}
			break;

		case VLW_STANDARD:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->owner == owner && v->IsPrimaryVehicle()) {
					*list->Append() = v;
				}
			}
			break;

		case VLW_DEPOT_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->IsType(OT_GOTO_DEPOT) && order->GetDestination() == index) {
							*list->Append() = v;
							break;
						}
					}
				}
			}
			break;

		case VLW_GROUP_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->IsPrimaryVehicle() &&
						v->owner == owner && v->group_id == index) {
					*list->Append() = v;
				}
			}
			break;

		default: NOT_REACHED(); break;
	}

	list->Compact();
}
