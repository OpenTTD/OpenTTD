/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehiclelist_func.h Functions and type for generating vehicle lists. */

#ifndef VEHICLELIST_FUNC_H
#define VEHICLELIST_FUNC_H

#include "order_base.h"
#include "vehicle_base.h"

/**
 * Find vehicles matching an order.
 * This can be used, e.g. to find all vehicles that stop at a particular station.
 * @param veh_pred Vehicle selection predicate. This is called only for the first vehicle using the order list.
 * @param ord_pred Order selection predicate.
 * @param veh_func Called for each vehicle that matches both vehicle and order predicates.
 **/
template <class VehiclePredicate, class OrderPredicate, class VehicleFunc>
void FindVehiclesWithOrder(VehiclePredicate veh_pred, OrderPredicate ord_pred, VehicleFunc veh_func)
{
	for (const OrderList *orderlist : OrderList::Iterate()) {

		/* We assume all vehicles sharing an order list match the condition. */
		const Vehicle *v = orderlist->GetFirstSharedVehicle();
		if (!veh_pred(v)) continue;

		/* Vehicle is a candidate, search for a matching order. */
		for (const Order *order = orderlist->GetFirstOrder(); order != nullptr; order = order->next) {

			if (!ord_pred(order)) continue;

			/* An order matches, we can add all shared vehicles to the list. */
			for (; v != nullptr; v = v->NextShared()) {
				veh_func(v);
			}
			break;
		}
	}
}

#endif /* VEHICLELIST_FUNC_H */
