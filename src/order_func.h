/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_func.h Functions related to orders. */

#ifndef ORDER_FUNC_H
#define ORDER_FUNC_H

#include "order_type.h"
#include "vehicle_type.h"
#include "company_type.h"

/* Functions */
void RemoveOrderFromAllVehicles(OrderType type, DestinationID destination, bool hangar = false);
void InvalidateVehicleOrder(const Vehicle *v, int data);
void CheckOrders(const Vehicle*);
void DeleteVehicleOrders(Vehicle *v, bool keep_orderlist = false, bool reset_order_indices = true);
bool ProcessOrders(Vehicle *v);
bool UpdateOrderDest(Vehicle *v, const Order *order, int conditional_depth = 0, bool pbs_look_ahead = false);
VehicleOrderID ProcessConditionalOrder(const Order *order, const Vehicle *v);
uint GetOrderDistance(const Order *prev, const Order *cur, const Vehicle *v, int conditional_depth = 0);

void DrawOrderString(const Vehicle *v, const Order *order, int order_index, int y, bool selected, bool timetable, int left, int middle, int right);

static const uint DEF_SERVINT_DAYS_TRAINS   = 150;
static const uint DEF_SERVINT_DAYS_ROADVEH  = 150;
static const uint DEF_SERVINT_DAYS_AIRCRAFT = 100;
static const uint DEF_SERVINT_DAYS_SHIPS    = 360;
static const uint MIN_SERVINT_DAYS          = 30;
static const uint MAX_SERVINT_DAYS          = 800;

static const uint DEF_SERVINT_PERCENT = 50;
static const uint MIN_SERVINT_PERCENT = 5;
static const uint MAX_SERVINT_PERCENT = 90;

uint16_t GetServiceIntervalClamped(uint interval, bool ispercent);

#endif /* ORDER_FUNC_H */
