/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file order_cmd.cpp Handling of orders. */

#include "stdafx.h"
#include "debug.h"
#include "command_func.h"
#include "company_func.h"
#include "news_func.h"
#include "strings_func.h"
#include "timetable.h"
#include "vehicle_func.h"
#include "depot_base.h"
#include "core/pool_func.hpp"
#include "aircraft.h"
#include "roadveh.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "company_base.h"
#include "order_backup.h"
#include "cheat_type.h"
#include "order_cmd.h"
#include "train_cmd.h"

#include "table/strings.h"

#include "safeguards.h"

/* DestinationID must be at least as large as every these below, because it can
 * be any of them
 */
static_assert(sizeof(DestinationID) >= sizeof(DepotID));
static_assert(sizeof(DestinationID) >= sizeof(StationID));

OrderListPool _orderlist_pool("OrderList");
INSTANTIATE_POOL_METHODS(OrderList)

/**
 * 'Free' the order
 * @note ONLY use on "current_order" vehicle orders!
 */
void Order::Free()
{
	this->type  = OT_NOTHING;
	this->flags = 0;
	this->dest  = 0;
}

/**
 * Makes this order a Go To Station order.
 * @param destination the station to go to.
 */
void Order::MakeGoToStation(StationID destination)
{
	this->type = OT_GOTO_STATION;
	this->flags = 0;
	this->dest = destination;
}

/**
 * Makes this order a Go To Depot order.
 * @param destination   the depot to go to.
 * @param order         is this order a 'default' order, or an overridden vehicle order?
 * @param non_stop_type how to get to the depot?
 * @param action        what to do in the depot?
 * @param cargo         the cargo type to change to.
 */
void Order::MakeGoToDepot(DestinationID destination, OrderDepotTypeFlags order, OrderNonStopFlags non_stop_type, OrderDepotActionFlags action, CargoType cargo)
{
	this->type = OT_GOTO_DEPOT;
	this->SetDepotOrderType(order);
	this->SetDepotActionType(action);
	this->SetNonStopType(non_stop_type);
	this->dest = destination;
	this->SetRefit(cargo);
}

/**
 * Makes this order a Go To Waypoint order.
 * @param destination the waypoint to go to.
 */
void Order::MakeGoToWaypoint(StationID destination)
{
	this->type = OT_GOTO_WAYPOINT;
	this->flags = 0;
	this->dest = destination;
}

/**
 * Makes this order a Loading order.
 * @param ordered is this an ordered stop?
 */
void Order::MakeLoading(bool ordered)
{
	this->type = OT_LOADING;
	if (!ordered) this->flags = 0;
}

/**
 * Makes this order a Leave Station order.
 */
void Order::MakeLeaveStation()
{
	this->type = OT_LEAVESTATION;
	this->flags = 0;
}

/**
 * Makes this order a Dummy order.
 */
void Order::MakeDummy()
{
	this->type = OT_DUMMY;
	this->flags = 0;
}

/**
 * Makes this order an conditional order.
 * @param order the order to jump to.
 */
void Order::MakeConditional(VehicleOrderID order)
{
	this->type = OT_CONDITIONAL;
	this->flags = order;
	this->dest = 0;
}

/**
 * Makes this order an implicit order.
 * @param destination the station to go to.
 */
void Order::MakeImplicit(StationID destination)
{
	this->type = OT_IMPLICIT;
	this->dest = destination;
}

/**
 * Make this depot/station order also a refit order.
 * @param cargo   the cargo type to change to.
 * @pre IsType(OT_GOTO_DEPOT) || IsType(OT_GOTO_STATION).
 */
void Order::SetRefit(CargoType cargo)
{
	this->refit_cargo = cargo;
}

/**
 * Does this order have the same type, flags and destination?
 * @param other the second order to compare to.
 * @return true if the type, flags and destination match.
 */
bool Order::Equals(const Order &other) const
{
	/* In case of go to nearest depot orders we need "only" compare the flags
	 * with the other and not the nearest depot order bit or the actual
	 * destination because those get clear/filled in during the order
	 * evaluation. If we do not do this the order will continuously be seen as
	 * a different order and it will try to find a "nearest depot" every tick. */
	if ((this->IsType(OT_GOTO_DEPOT) && this->type == other.type) &&
			(this->GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot) ||
			 other.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot))) {
		return this->GetDepotOrderType() == other.GetDepotOrderType() &&
				this->GetDepotActionType().Reset(OrderDepotActionFlag::NearestDepot) == other.GetDepotActionType().Reset(OrderDepotActionFlag::NearestDepot);
	}

	return this->type == other.type && this->flags == other.flags && this->dest == other.dest;
}

/**
 * Pack this order into a 16 bits integer as close to the TTD
 * representation as possible.
 * @return the TTD-like packed representation.
 */
uint16_t Order::MapOldOrder() const
{
	uint16_t order = this->GetType();
	switch (this->GetType()) {
		case OT_GOTO_STATION:
			if (this->GetUnloadType() == OrderUnloadType::Unload) SetBit(order, 5);
			if (this->IsFullLoadOrder()) SetBit(order, 6);
			if (this->GetNonStopType().Test(OrderNonStopFlag::NoIntermediate)) SetBit(order, 7);
			order |= GB(this->GetDestination().value, 0, 8) << 8;
			break;
		case OT_GOTO_DEPOT:
			if (!this->GetDepotOrderType().Test(OrderDepotTypeFlag::PartOfOrders)) SetBit(order, 6);
			SetBit(order, 7);
			order |= GB(this->GetDestination().value, 0, 8) << 8;
			break;
		case OT_LOADING:
			if (this->IsFullLoadOrder()) SetBit(order, 6);
			/* If both "no load" and "no unload" are set, return nothing order instead */
			if (this->GetLoadType() == OrderLoadType::NoLoad && this->GetUnloadType() == OrderUnloadType::NoUnload) {
				order = OT_NOTHING;
			}
			break;
		default:
			break;
	}
	return order;
}

/**
 *
 * Updates the widgets of a vehicle which contains the order-data
 *
 */
void InvalidateVehicleOrder(const Vehicle *v, int data)
{
	SetWindowDirty(WC_VEHICLE_VIEW, v->index);

	if (data != 0) {
		/* Calls SetDirty() too */
		InvalidateWindowData(WC_VEHICLE_ORDERS,    v->index, data);
		InvalidateWindowData(WC_VEHICLE_TIMETABLE, v->index, data);
		return;
	}

	SetWindowDirty(WC_VEHICLE_ORDERS,    v->index);
	SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
}

/**
 *
 * Assign data to an order (from another order)
 *   This function makes sure that the index is maintained correctly
 * @param other the data to copy (except next pointer).
 *
 */
void Order::AssignOrder(const Order &other)
{
	this->type  = other.type;
	this->flags = other.flags;
	this->dest  = other.dest;

	this->refit_cargo   = other.refit_cargo;

	this->wait_time   = other.wait_time;
	this->travel_time = other.travel_time;
	this->max_speed   = other.max_speed;
}

/**
 * Recomputes everything.
 * @param chain first order in the chain
 * @param v one of vehicle that is using this orderlist
 */
void OrderList::Initialize(Vehicle *v)
{
	this->first_shared = v;

	this->num_manual_orders = 0;
	this->num_vehicles = 1;
	this->timetable_duration = 0;

	for (const Order &o : this->orders) {
		if (!o.IsType(OT_IMPLICIT)) ++this->num_manual_orders;
		this->total_duration += o.GetWaitTime() + o.GetTravelTime();
	}

	this->RecalculateTimetableDuration();

	for (Vehicle *u = this->first_shared->PreviousShared(); u != nullptr; u = u->PreviousShared()) {
		++this->num_vehicles;
		this->first_shared = u;
	}

	for (const Vehicle *u = v->NextShared(); u != nullptr; u = u->NextShared()) ++this->num_vehicles;
}

/**
 * Recomputes Timetable duration.
 * Split out into a separate function so it can be used by afterload.
 */
void OrderList::RecalculateTimetableDuration()
{
	this->timetable_duration = 0;
	for (const Order &o : this->orders) {
		this->timetable_duration += o.GetTimetabledWait() + o.GetTimetabledTravel();
	}
}

/**
 * Free a complete order chain.
 * @param keep_orderlist If this is true only delete the orders, otherwise also delete the OrderList.
 * @note do not use on "current_order" vehicle orders!
 */
void OrderList::FreeChain(bool keep_orderlist)
{
	/* We can visit oil rigs and buoys that are not our own. They will be shown in
	 * the list of stations. So, we need to invalidate that window if needed. */
	for (Order &order: this->orders) {
		if (order.IsType(OT_GOTO_STATION) || order.IsType(OT_GOTO_WAYPOINT)) {
			BaseStation *bs = BaseStation::GetIfValid(order.GetDestination().ToStationID());
			if (bs != nullptr && bs->owner == OWNER_NONE) {
				InvalidateWindowClassesData(WC_STATION_LIST, 0);
				break;
			}
		}
	}

	if (keep_orderlist) {
		this->orders.clear();
		this->num_manual_orders = 0;
		this->timetable_duration = 0;
	} else {
		delete this;
	}
}

/**
 * Get the next order which will make the given vehicle stop at a station
 * or refit at a depot or evaluate a non-trivial condition.
 * @param next The order to start looking at.
 * @param hops The number of orders we have already looked at.
 * @return Either of
 *         \li a station order
 *         \li a refitting depot order
 *         \li a non-trivial conditional order
 *         \li INVALID_VEH_ORDER_ID if the vehicle won't stop anymore.
 */
VehicleOrderID OrderList::GetNextDecisionNode(VehicleOrderID next, uint hops) const
{
	if (hops > this->GetNumOrders() || next >= this->GetNumOrders()) return INVALID_VEH_ORDER_ID;

	const Order &order_next = this->orders[next];
	if (order_next.IsType(OT_CONDITIONAL)) {
		if (order_next.GetConditionVariable() != OrderConditionVariable::Unconditionally) return next;

		/* We can evaluate trivial conditions right away. They're conceptually
		 * the same as regular order progression. */
		return this->GetNextDecisionNode(
				order_next.GetConditionSkipToOrder(),
				hops + 1);
	}

	if (order_next.IsType(OT_GOTO_DEPOT)) {
		if (order_next.GetDepotActionType().Test(OrderDepotActionFlag::Halt)) return INVALID_VEH_ORDER_ID;
		if (order_next.IsRefit()) return next;
	}

	if (!order_next.CanLoadOrUnload()) {
		return this->GetNextDecisionNode(this->GetNext(next), hops + 1);
	}

	return next;
}

/**
 * Recursively determine the next deterministic station to stop at.
 * @param v The vehicle we're looking at.
 * @param first Order to start searching at or INVALID_VEH_ORDER_ID to start at cur_implicit_order_index + 1.
 * @param hops Number of orders we have already looked at.
 * @return Next stopping station or StationID::Invalid().
 * @pre The vehicle is currently loading and v->last_station_visited is meaningful.
 * @note This function may draw a random number. Don't use it from the GUI.
 */
void OrderList::GetNextStoppingStation(std::vector<StationID> &next_station, const Vehicle *v, VehicleOrderID first, uint hops) const
{
	VehicleOrderID next = first;
	if (first == INVALID_VEH_ORDER_ID) {
		next = v->cur_implicit_order_index;
		if (next >= this->GetNumOrders()) {
			next = this->GetFirstOrder();
			if (next == INVALID_VEH_ORDER_ID) return;
		} else {
			/* GetNext never returns INVALID_VEH_ORDER_ID if there is a valid station in the list.
			 * As the given "next" is already valid and a station in the list, we
			 * don't have to check for INVALID_VEH_ORDER_ID here. */
			next = this->GetNext(next);
			assert(next != INVALID_VEH_ORDER_ID);
		}
	}

	auto orders = v->Orders();
	do {
		next = this->GetNextDecisionNode(next, ++hops);

		/* Resolve possibly nested conditionals by estimation. */
		while (next != INVALID_VEH_ORDER_ID && orders[next].IsType(OT_CONDITIONAL)) {
			/* We return both options of conditional orders. */
			VehicleOrderID skip_to = this->GetNextDecisionNode(orders[next].GetConditionSkipToOrder(), hops);
			VehicleOrderID advance = this->GetNextDecisionNode(this->GetNext(next), hops);
			if (advance == INVALID_VEH_ORDER_ID || advance == first || skip_to == advance) {
				next = (skip_to == first) ? INVALID_VEH_ORDER_ID : skip_to;
			} else if (skip_to == INVALID_VEH_ORDER_ID || skip_to == first) {
				next = (advance == first) ? INVALID_VEH_ORDER_ID : advance;
			} else {
				this->GetNextStoppingStation(next_station, v, skip_to, hops);
				this->GetNextStoppingStation(next_station, v, advance, hops);
				return;
			}
			++hops;
		}

		/* Don't return a next stop if the vehicle has to unload everything. */
		if (next == INVALID_VEH_ORDER_ID || ((orders[next].IsType(OT_GOTO_STATION) || orders[next].IsType(OT_IMPLICIT)) &&
				orders[next].GetDestination() == v->last_station_visited &&
				(orders[next].GetUnloadType() == OrderUnloadType::Transfer || orders[next].GetUnloadType() == OrderUnloadType::Unload))) {
			return;
		}
	} while (orders[next].IsType(OT_GOTO_DEPOT) || orders[next].GetDestination() == v->last_station_visited);

	next_station.push_back(orders[next].GetDestination().ToStationID());
}

/**
 * Insert a new order into the order chain.
 * @param new_order is the order to insert into the chain.
 * @param index is the position where the order is supposed to be inserted.
 */
void OrderList::InsertOrderAt(Order &&order, VehicleOrderID index)
{
	auto it = std::ranges::next(std::begin(this->orders), index, std::end(this->orders));
	auto new_order = this->orders.emplace(it, std::move(order));

	if (!new_order->IsType(OT_IMPLICIT)) ++this->num_manual_orders;
	this->timetable_duration += new_order->GetTimetabledWait() + new_order->GetTimetabledTravel();
	this->total_duration += new_order->GetWaitTime() + new_order->GetTravelTime();

	/* We can visit oil rigs and buoys that are not our own. They will be shown in
	 * the list of stations. So, we need to invalidate that window if needed. */
	if (new_order->IsType(OT_GOTO_STATION) || new_order->IsType(OT_GOTO_WAYPOINT)) {
		BaseStation *bs = BaseStation::Get(new_order->GetDestination().ToStationID());
		if (bs->owner == OWNER_NONE) InvalidateWindowClassesData(WC_STATION_LIST, 0);
	}
}


/**
 * Remove an order from the order list and delete it.
 * @param index is the position of the order which is to be deleted.
 */
void OrderList::DeleteOrderAt(VehicleOrderID index)
{
	auto to_remove = std::ranges::next(std::begin(this->orders), index, std::end(this->orders));
	if (to_remove == std::end(this->orders)) return;

	if (!to_remove->IsType(OT_IMPLICIT)) --this->num_manual_orders;

	this->timetable_duration -= (to_remove->GetTimetabledWait() + to_remove->GetTimetabledTravel());
	this->total_duration -= (to_remove->GetWaitTime() + to_remove->GetTravelTime());

	this->orders.erase(to_remove);
}

/**
 * Move an order to another position within the order list.
 * @param from is the zero-based position of the order to move.
 * @param to is the zero-based position where the order is moved to.
 */
void OrderList::MoveOrder(VehicleOrderID from, VehicleOrderID to)
{
	if (from == to) return;
	if (from >= this->GetNumOrders()) return;
	if (to >= this->GetNumOrders()) return;

	auto it = std::begin(this->orders);
	if (from < to) {
		std::rotate(it + from, it + from + 1, it + to + 1);
	} else {
		std::rotate(it + to, it + from, it + from + 1);
	}
}

/**
 * Removes the vehicle from the shared order list.
 * @note This is supposed to be called when the vehicle is still in the chain
 * @param v vehicle to remove from the list
 */
void OrderList::RemoveVehicle(Vehicle *v)
{
	--this->num_vehicles;
	if (v == this->first_shared) this->first_shared = v->NextShared();
}

/**
 * Checks whether all orders of the list have a filled timetable.
 * @return whether all orders have a filled timetable.
 */
bool OrderList::IsCompleteTimetable() const
{
	for (const Order &o : this->orders) {
		/* Implicit orders are, by definition, not timetabled. */
		if (o.IsType(OT_IMPLICIT)) continue;
		if (!o.IsCompletelyTimetabled()) return false;
	}
	return true;
}

#ifdef WITH_ASSERT
/**
 * Checks for internal consistency of order list. Triggers assertion if something is wrong.
 */
void OrderList::DebugCheckSanity() const
{
	VehicleOrderID check_num_orders = 0;
	VehicleOrderID check_num_manual_orders = 0;
	uint check_num_vehicles = 0;
	TimerGameTick::Ticks check_timetable_duration = 0;
	TimerGameTick::Ticks check_total_duration = 0;

	Debug(misc, 6, "Checking OrderList {} for sanity...", this->index);

	for (const Order &o : this->orders) {
		++check_num_orders;
		if (!o.IsType(OT_IMPLICIT)) ++check_num_manual_orders;
		check_timetable_duration += o.GetTimetabledWait() + o.GetTimetabledTravel();
		check_total_duration += o.GetWaitTime() + o.GetTravelTime();
	}
	assert(this->GetNumOrders() == check_num_orders);
	assert(this->num_manual_orders == check_num_manual_orders);
	assert(this->timetable_duration == check_timetable_duration);
	assert(this->total_duration == check_total_duration);

	for (const Vehicle *v = this->first_shared; v != nullptr; v = v->NextShared()) {
		++check_num_vehicles;
		assert(v->orders == this);
	}
	assert(this->num_vehicles == check_num_vehicles);
	Debug(misc, 6, "... detected {} orders ({} manual), {} vehicles, {} timetabled, {} total",
			(uint)this->GetNumOrders(), (uint)this->num_manual_orders,
			this->num_vehicles, this->timetable_duration, this->total_duration);
}
#endif

/**
 * Checks whether the order goes to a station or not, i.e. whether the
 * destination is a station
 * @param v the vehicle to check for
 * @param o the order to check
 * @return true if the destination is a station
 */
static inline bool OrderGoesToStation(const Vehicle *v, const Order &o)
{
	return o.IsType(OT_GOTO_STATION) ||
			(v->type == VEH_AIRCRAFT && o.IsType(OT_GOTO_DEPOT) && o.GetDestination() != StationID::Invalid());
}

/**
 * Delete all news items regarding defective orders about a vehicle
 * This could kill still valid warnings (for example about void order when just
 * another order gets added), but assume the company will notice the problems,
 * when they're changing the orders.
 */
static void DeleteOrderWarnings(const Vehicle *v)
{
	DeleteVehicleNews(v->index, AdviceType::Order);
}

/**
 * Returns a tile somewhat representing the order destination (not suitable for pathfinding).
 * @param v The vehicle to get the location for.
 * @param airport Get the airport tile and not the station location for aircraft.
 * @return destination of order, or INVALID_TILE if none.
 */
TileIndex Order::GetLocation(const Vehicle *v, bool airport) const
{
	switch (this->GetType()) {
		case OT_GOTO_WAYPOINT:
		case OT_GOTO_STATION:
		case OT_IMPLICIT:
			if (airport && v->type == VEH_AIRCRAFT) return Station::Get(this->GetDestination().ToStationID())->airport.tile;
			return BaseStation::Get(this->GetDestination().ToStationID())->xy;

		case OT_GOTO_DEPOT:
			if (this->GetDestination() == DepotID::Invalid()) return INVALID_TILE;
			return (v->type == VEH_AIRCRAFT) ? Station::Get(this->GetDestination().ToStationID())->xy : Depot::Get(this->GetDestination().ToDepotID())->xy;

		default:
			return INVALID_TILE;
	}
}

/**
 * Get the distance between two orders of a vehicle. Conditional orders are resolved
 * and the bigger distance of the two order branches is returned.
 * @param prev Origin order.
 * @param cur Destination order.
 * @param v The vehicle to get the distance for.
 * @param conditional_depth Internal param for resolving conditional orders.
 * @return Maximum distance between the two orders.
 */
uint GetOrderDistance(VehicleOrderID prev, VehicleOrderID cur, const Vehicle *v, int conditional_depth)
{
	assert(v->orders != nullptr);
	const OrderList &orderlist = *v->orders;
	auto orders = orderlist.GetOrders();

	if (orders[cur].IsType(OT_CONDITIONAL)) {
		if (conditional_depth > v->GetNumOrders()) return 0;

		conditional_depth++;

		int dist1 = GetOrderDistance(prev, orders[cur].GetConditionSkipToOrder(), v, conditional_depth);
		int dist2 = GetOrderDistance(prev, orderlist.GetNext(cur), v, conditional_depth);
		return std::max(dist1, dist2);
	}

	TileIndex prev_tile = orders[prev].GetLocation(v, true);
	TileIndex cur_tile = orders[cur].GetLocation(v, true);
	if (prev_tile == INVALID_TILE || cur_tile == INVALID_TILE) return 0;
	return v->type == VEH_AIRCRAFT ? DistanceSquare(prev_tile, cur_tile) : DistanceManhattan(prev_tile, cur_tile);
}

/**
 * Add an order to the orderlist of a vehicle.
 * @param flags operation to perform
 * @param veh ID of the vehicle
 * @param sel_ord the selected order (if any). If the last order is given,
 *                        the order will be inserted before that one
 *                        the maximum vehicle order id is 254.
 * @param new_order order to insert
 * @return the cost of this operation or an error
 */
CommandCost CmdInsertOrder(DoCommandFlags flags, VehicleID veh, VehicleOrderID sel_ord, const Order &new_order)
{
	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	/* Validate properties we don't want to have different from default as they are set by other commands. */
	if (new_order.GetRefitCargo() != CARGO_NO_REFIT || new_order.GetWaitTime() != 0 || new_order.GetTravelTime() != 0 || new_order.GetMaxSpeed() != UINT16_MAX) return CMD_ERROR;

	/* Check if the inserted order is to the correct destination (owner, type),
	 * and has the correct flags if any */
	switch (new_order.GetType()) {
		case OT_GOTO_STATION: {
			const Station *st = Station::GetIfValid(new_order.GetDestination().ToStationID());
			if (st == nullptr) return CMD_ERROR;

			if (st->owner != OWNER_NONE) {
				ret = CheckOwnership(st->owner);
				if (ret.Failed()) return ret;
			}

			if (!CanVehicleUseStation(v, st)) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER, GetVehicleCannotUseStationReason(v, st));
			for (Vehicle *u = v->FirstShared(); u != nullptr; u = u->NextShared()) {
				if (!CanVehicleUseStation(u, st)) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER_SHARED, GetVehicleCannotUseStationReason(u, st));
			}

			/* Non stop only allowed for ground vehicles. */
			if (new_order.GetNonStopType().Any() && !v->IsGroundVehicle()) return CMD_ERROR;

			/* Filter invalid load/unload types. */
			switch (new_order.GetLoadType()) {
				case OrderLoadType::LoadIfPossible:
				case OrderLoadType::NoLoad:
					break;

				case OrderLoadType::FullLoad:
				case OrderLoadType::FullLoadAny:
					if (v->HasUnbunchingOrder()) return CommandCost(STR_ERROR_UNBUNCHING_NO_FULL_LOAD);
					break;

				default:
					return CMD_ERROR;
			}
			switch (new_order.GetUnloadType()) {
				case OrderUnloadType::UnloadIfPossible:
				case OrderUnloadType::Unload:
				case OrderUnloadType::Transfer:
				case OrderUnloadType::NoUnload:
					break;

				default:
					return CMD_ERROR;
			}

			/* Filter invalid stop locations */
			switch (new_order.GetStopLocation()) {
				case OrderStopLocation::NearEnd:
				case OrderStopLocation::Middle:
					if (v->type != VEH_TRAIN) return CMD_ERROR;
					[[fallthrough]];

				case OrderStopLocation::FarEnd:
					break;

				default:
					return CMD_ERROR;
			}

			break;
		}

		case OT_GOTO_DEPOT: {
			if (!new_order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)) {
				if (v->type == VEH_AIRCRAFT) {
					const Station *st = Station::GetIfValid(new_order.GetDestination().ToStationID());

					if (st == nullptr) return CMD_ERROR;

					ret = CheckOwnership(st->owner);
					if (ret.Failed()) return ret;

					if (!CanVehicleUseStation(v, st) || !st->airport.HasHangar()) {
						return CMD_ERROR;
					}
				} else {
					const Depot *dp = Depot::GetIfValid(new_order.GetDestination().ToDepotID());

					if (dp == nullptr) return CMD_ERROR;

					ret = CheckOwnership(GetTileOwner(dp->xy));
					if (ret.Failed()) return ret;

					switch (v->type) {
						case VEH_TRAIN:
							if (!IsRailDepotTile(dp->xy)) return CMD_ERROR;
							break;

						case VEH_ROAD:
							if (!IsRoadDepotTile(dp->xy)) return CMD_ERROR;
							break;

						case VEH_SHIP:
							if (!IsShipDepotTile(dp->xy)) return CMD_ERROR;
							break;

						default: return CMD_ERROR;
					}
				}
			}

			if (new_order.GetNonStopType().Any() && !v->IsGroundVehicle()) return CMD_ERROR;

			/* Check depot order type is valid. */
			OrderDepotTypeFlags depot_order_type = new_order.GetDepotOrderType();
			if (depot_order_type.Test(OrderDepotTypeFlag::PartOfOrders)) depot_order_type.Reset(OrderDepotTypeFlag::Service);
			depot_order_type.Reset(OrderDepotTypeFlag::PartOfOrders);
			if (depot_order_type.Any()) return CMD_ERROR;

			/* Check depot action type is valid. */
			if (new_order.GetDepotActionType().Reset({OrderDepotActionFlag::Halt, OrderDepotActionFlag::NearestDepot, OrderDepotActionFlag::Unbunch}).Any()) return CMD_ERROR;

			/* Vehicles cannot have a "service if needed" order that also has a depot action. */
			if (new_order.GetDepotOrderType().Test(OrderDepotTypeFlag::Service) && new_order.GetDepotActionType().Any({OrderDepotActionFlag::Halt, OrderDepotActionFlag::Unbunch})) return CMD_ERROR;

			/* Check if we're allowed to have a new unbunching order. */
			if (new_order.GetDepotActionType().Test(OrderDepotActionFlag::Unbunch)) {
				if (v->HasFullLoadOrder()) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER, STR_ERROR_UNBUNCHING_NO_UNBUNCHING_FULL_LOAD);
				if (v->HasUnbunchingOrder()) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER, STR_ERROR_UNBUNCHING_ONLY_ONE_ALLOWED);
				if (v->HasConditionalOrder()) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER, STR_ERROR_UNBUNCHING_NO_UNBUNCHING_CONDITIONAL);
			}
			break;
		}

		case OT_GOTO_WAYPOINT: {
			const Waypoint *wp = Waypoint::GetIfValid(new_order.GetDestination().ToStationID());
			if (wp == nullptr) return CMD_ERROR;

			switch (v->type) {
				default: return CMD_ERROR;

				case VEH_TRAIN: {
					if (!wp->facilities.Test(StationFacility::Train)) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER, STR_ERROR_NO_RAIL_WAYPOINT);

					ret = CheckOwnership(wp->owner);
					if (ret.Failed()) return ret;
					break;
				}

				case VEH_ROAD: {
					if (!wp->facilities.Test(StationFacility::BusStop) && !wp->facilities.Test(StationFacility::TruckStop)) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER, STR_ERROR_NO_ROAD_WAYPOINT);

					ret = CheckOwnership(wp->owner);
					if (ret.Failed()) return ret;
					break;
				}

				case VEH_SHIP:
					if (!wp->facilities.Test(StationFacility::Dock)) return CommandCost(STR_ERROR_CAN_T_ADD_ORDER, STR_ERROR_NO_BUOY);
					if (wp->owner != OWNER_NONE) {
						ret = CheckOwnership(wp->owner);
						if (ret.Failed()) return ret;
					}
					break;
			}

			/* Order flags can be any of the following for waypoints:
			 * [non-stop]
			 * non-stop orders (if any) are only valid for trains and road vehicles */
			if (new_order.GetNonStopType().Any() && !v->IsGroundVehicle()) return CMD_ERROR;
			break;
		}

		case OT_CONDITIONAL: {
			VehicleOrderID skip_to = new_order.GetConditionSkipToOrder();
			if (skip_to != 0 && skip_to >= v->GetNumOrders()) return CMD_ERROR; // Always allow jumping to the first (even when there is no order).
			if (new_order.GetConditionVariable() >= OrderConditionVariable::End) return CMD_ERROR;
			if (v->HasUnbunchingOrder()) return CommandCost(STR_ERROR_UNBUNCHING_NO_CONDITIONAL);

			OrderConditionComparator occ = new_order.GetConditionComparator();
			if (occ >= OrderConditionComparator::End) return CMD_ERROR;
			switch (new_order.GetConditionVariable()) {
				case OrderConditionVariable::RequiresService:
					if (occ != OrderConditionComparator::IsTrue && occ != OrderConditionComparator::IsFalse) return CMD_ERROR;
					break;

				case OrderConditionVariable::Unconditionally:
					if (occ != OrderConditionComparator::Equal) return CMD_ERROR;
					if (new_order.GetConditionValue() != 0) return CMD_ERROR;
					break;

				case OrderConditionVariable::LoadPercentage:
				case OrderConditionVariable::Reliability:
					if (new_order.GetConditionValue() > 100) return CMD_ERROR;
					[[fallthrough]];

				default:
					if (occ == OrderConditionComparator::IsTrue || occ == OrderConditionComparator::IsFalse) return CMD_ERROR;
					break;
			}
			break;
		}

		default: return CMD_ERROR;
	}

	if (sel_ord > v->GetNumOrders()) return CMD_ERROR;

	if (v->GetNumOrders() >= MAX_VEH_ORDER_ID) return CommandCost(STR_ERROR_TOO_MANY_ORDERS);
	if (v->orders == nullptr && !OrderList::CanAllocateItem()) return CommandCost(STR_ERROR_NO_MORE_SPACE_FOR_ORDERS);

	if (flags.Test(DoCommandFlag::Execute)) {
		InsertOrder(v, Order(new_order), sel_ord);
	}

	return CommandCost();
}

/**
 * Insert a new order but skip the validation.
 * @param v       The vehicle to insert the order to.
 * @param new_o   The new order.
 * @param sel_ord The position the order should be inserted at.
 */
void InsertOrder(Vehicle *v, Order &&new_o, VehicleOrderID sel_ord)
{
	/* Create new order and link in list */
	if (v->orders == nullptr) {
		v->orders = new OrderList(std::move(new_o), v);
	} else {
		v->orders->InsertOrderAt(std::move(new_o), sel_ord);
	}

	Vehicle *u = v->FirstShared();
	DeleteOrderWarnings(u);
	for (; u != nullptr; u = u->NextShared()) {
		assert(v->orders == u->orders);

		/* If there is added an order before the current one, we need
		 * to update the selected order. We do not change implicit/real order indices though.
		 * If the new order is between the current implicit order and real order, the implicit order will
		 * later skip the inserted order. */
		if (sel_ord <= u->cur_real_order_index) {
			uint cur = u->cur_real_order_index + 1;
			/* Check if we don't go out of bound */
			if (cur < u->GetNumOrders()) {
				u->cur_real_order_index = cur;
			}
		}
		if (sel_ord == u->cur_implicit_order_index && u->IsGroundVehicle()) {
			/* We are inserting an order just before the current implicit order.
			 * We do not know whether we will reach current implicit or the newly inserted order first.
			 * So, disable creation of implicit orders until we are on track again. */
			uint16_t &gv_flags = u->GetGroundVehicleFlags();
			SetBit(gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS);
		}
		if (sel_ord <= u->cur_implicit_order_index) {
			uint cur = u->cur_implicit_order_index + 1;
			/* Check if we don't go out of bound */
			if (cur < u->GetNumOrders()) {
				u->cur_implicit_order_index = cur;
			}
		}
		/* Unbunching data is no longer valid. */
		u->ResetDepotUnbunching();

		/* Update any possible open window of the vehicle */
		InvalidateVehicleOrder(u, INVALID_VEH_ORDER_ID | (sel_ord << 8));
	}

	/* As we insert an order, the order to skip to will be 'wrong'. */
	VehicleOrderID cur_order_id = 0;
	for (Order &order : v->Orders()) {
		if (order.IsType(OT_CONDITIONAL)) {
			VehicleOrderID order_id = order.GetConditionSkipToOrder();
			if (order_id >= sel_ord) {
				order.SetConditionSkipToOrder(order_id + 1);
			}
			if (order_id == cur_order_id) {
				order.SetConditionSkipToOrder((order_id + 1) % v->GetNumOrders());
			}
		}
		cur_order_id++;
	}

	/* Make sure to rebuild the whole list */
	InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 0);
}

/**
 * Declone an order-list
 * @param *dst delete the orders of this vehicle
 * @param flags execution flags
 */
static CommandCost DecloneOrder(Vehicle *dst, DoCommandFlags flags)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		DeleteVehicleOrders(dst);
		InvalidateVehicleOrder(dst, VIWD_REMOVE_ALL_ORDERS);
		InvalidateWindowClassesData(GetWindowClassForVehicleType(dst->type), 0);
	}
	return CommandCost();
}

/**
 * Delete an order from the orderlist of a vehicle.
 * @param flags operation to perform
 * @param veh_id the ID of the vehicle
 * @param sel_ord the order to delete (max 255)
 * @return the cost of this operation or an error
 */
CommandCost CmdDeleteOrder(DoCommandFlags flags, VehicleID veh_id, VehicleOrderID sel_ord)
{
	Vehicle *v = Vehicle::GetIfValid(veh_id);

	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	/* If we did not select an order, we maybe want to de-clone the orders */
	if (sel_ord >= v->GetNumOrders()) return DecloneOrder(v, flags);

	if (v->GetOrder(sel_ord) == nullptr) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) DeleteOrder(v, sel_ord);
	return CommandCost();
}

/**
 * Cancel the current loading order of the vehicle as the order was deleted.
 * @param v the vehicle
 */
static void CancelLoadingDueToDeletedOrder(Vehicle *v)
{
	assert(v->current_order.IsType(OT_LOADING));
	/* NON-stop flag is misused to see if a train is in a station that is
	 * on its order list or not */
	v->current_order.SetNonStopType({});
	/* When full loading, "cancel" that order so the vehicle doesn't
	 * stay indefinitely at this station anymore. */
	if (v->current_order.IsFullLoadOrder()) v->current_order.SetLoadType(OrderLoadType::LoadIfPossible);
}

/**
 * Delete an order but skip the parameter validation.
 * @param v       The vehicle to delete the order from.
 * @param sel_ord The id of the order to be deleted.
 */
void DeleteOrder(Vehicle *v, VehicleOrderID sel_ord)
{
	v->orders->DeleteOrderAt(sel_ord);

	Vehicle *u = v->FirstShared();
	DeleteOrderWarnings(u);
	for (; u != nullptr; u = u->NextShared()) {
		assert(v->orders == u->orders);

		if (sel_ord == u->cur_real_order_index && u->current_order.IsType(OT_LOADING)) {
			CancelLoadingDueToDeletedOrder(u);
		}

		if (sel_ord < u->cur_real_order_index) {
			u->cur_real_order_index--;
		} else if (sel_ord == u->cur_real_order_index) {
			u->UpdateRealOrderIndex();
		}

		if (sel_ord < u->cur_implicit_order_index) {
			u->cur_implicit_order_index--;
		} else if (sel_ord == u->cur_implicit_order_index) {
			/* Make sure the index is valid */
			if (u->cur_implicit_order_index >= u->GetNumOrders()) u->cur_implicit_order_index = 0;

			/* Skip non-implicit orders for the implicit-order-index (e.g. if the current implicit order was deleted */
			while (u->cur_implicit_order_index != u->cur_real_order_index && !u->GetOrder(u->cur_implicit_order_index)->IsType(OT_IMPLICIT)) {
				u->cur_implicit_order_index++;
				if (u->cur_implicit_order_index >= u->GetNumOrders()) u->cur_implicit_order_index = 0;
			}
		}
		/* Unbunching data is no longer valid. */
		u->ResetDepotUnbunching();

		/* Update any possible open window of the vehicle */
		InvalidateVehicleOrder(u, sel_ord | (INVALID_VEH_ORDER_ID << 8));
	}

	/* As we delete an order, the order to skip to will be 'wrong'. */
	VehicleOrderID cur_order_id = 0;
	for (Order &order : v->Orders()) {
		if (order.IsType(OT_CONDITIONAL)) {
			VehicleOrderID order_id = order.GetConditionSkipToOrder();
			if (order_id >= sel_ord) {
				order_id = std::max(order_id - 1, 0);
			}
			if (order_id == cur_order_id) {
				order_id = (order_id + 1) % v->GetNumOrders();
			}
			order.SetConditionSkipToOrder(order_id);
		}
		cur_order_id++;
	}

	InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 0);
}

/**
 * Goto order of order-list.
 * @param flags operation to perform
 * @param veh_id The ID of the vehicle which order is skipped
 * @param sel_ord the selected order to which we want to skip
 * @return the cost of this operation or an error
 */
CommandCost CmdSkipToOrder(DoCommandFlags flags, VehicleID veh_id, VehicleOrderID sel_ord)
{
	Vehicle *v = Vehicle::GetIfValid(veh_id);

	if (v == nullptr || !v->IsPrimaryVehicle() || sel_ord == v->cur_implicit_order_index || sel_ord >= v->GetNumOrders() || v->GetNumOrders() < 2) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (flags.Test(DoCommandFlag::Execute)) {
		if (v->current_order.IsType(OT_LOADING)) v->LeaveStation();

		v->cur_implicit_order_index = v->cur_real_order_index = sel_ord;
		v->UpdateRealOrderIndex();

		/* Unbunching data is no longer valid. */
		v->ResetDepotUnbunching();

		InvalidateVehicleOrder(v, VIWD_MODIFY_ORDERS);

		/* We have an aircraft/ship, they have a mini-schedule, so update them all */
		if (v->type == VEH_AIRCRAFT) SetWindowClassesDirty(WC_AIRCRAFT_LIST);
		if (v->type == VEH_SHIP) SetWindowClassesDirty(WC_SHIPS_LIST);
	}

	return CommandCost();
}

/**
 * Move an order inside the orderlist
 * @param flags operation to perform
 * @param veh the ID of the vehicle
 * @param moving_order the order to move
 * @param target_order the target order
 * @return the cost of this operation or an error
 * @note The target order will move one place down in the orderlist
 *  if you move the order upwards else it'll move it one place down
 */
CommandCost CmdMoveOrder(DoCommandFlags flags, VehicleID veh, VehicleOrderID moving_order, VehicleOrderID target_order)
{
	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	/* Don't make senseless movements */
	if (moving_order >= v->GetNumOrders() || target_order >= v->GetNumOrders() ||
			moving_order == target_order || v->GetNumOrders() <= 1) return CMD_ERROR;

	Order *moving_one = v->GetOrder(moving_order);
	/* Don't move an empty order */
	if (moving_one == nullptr) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		v->orders->MoveOrder(moving_order, target_order);

		/* Update shared list */
		Vehicle *u = v->FirstShared();

		DeleteOrderWarnings(u);

		for (; u != nullptr; u = u->NextShared()) {
			/* Update the current order.
			 * There are multiple ways to move orders, which result in cur_implicit_order_index
			 * and cur_real_order_index to not longer make any sense. E.g. moving another
			 * real order between them.
			 *
			 * Basically one could choose to preserve either of them, but not both.
			 * While both ways are suitable in this or that case from a human point of view, neither
			 * of them makes really sense.
			 * However, from an AI point of view, preserving cur_real_order_index is the most
			 * predictable and transparent behaviour.
			 *
			 * With that decision it basically does not matter what we do to cur_implicit_order_index.
			 * If we change orders between the implicit- and real-index, the implicit orders are mostly likely
			 * completely out-dated anyway. So, keep it simple and just keep cur_implicit_order_index as well.
			 * The worst which can happen is that a lot of implicit orders are removed when reaching current_order.
			 */
			if (u->cur_real_order_index == moving_order) {
				u->cur_real_order_index = target_order;
			} else if (u->cur_real_order_index > moving_order && u->cur_real_order_index <= target_order) {
				u->cur_real_order_index--;
			} else if (u->cur_real_order_index < moving_order && u->cur_real_order_index >= target_order) {
				u->cur_real_order_index++;
			}

			if (u->cur_implicit_order_index == moving_order) {
				u->cur_implicit_order_index = target_order;
			} else if (u->cur_implicit_order_index > moving_order && u->cur_implicit_order_index <= target_order) {
				u->cur_implicit_order_index--;
			} else if (u->cur_implicit_order_index < moving_order && u->cur_implicit_order_index >= target_order) {
				u->cur_implicit_order_index++;
			}
			/* Unbunching data is no longer valid. */
			u->ResetDepotUnbunching();


			assert(v->orders == u->orders);
			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u, moving_order | (target_order << 8));
		}

		/* As we move an order, the order to skip to will be 'wrong'. */
		for (Order &order : v->Orders()) {
			if (order.IsType(OT_CONDITIONAL)) {
				VehicleOrderID order_id = order.GetConditionSkipToOrder();
				if (order_id == moving_order) {
					order_id = target_order;
				} else if (order_id > moving_order && order_id <= target_order) {
					order_id--;
				} else if (order_id < moving_order && order_id >= target_order) {
					order_id++;
				}
				order.SetConditionSkipToOrder(order_id);
			}
		}

		/* Make sure to rebuild the whole list */
		InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 0);
	}

	return CommandCost();
}

/**
 * Modify an order in the orderlist of a vehicle.
 * @param flags operation to perform
 * @param veh ID of the vehicle
 * @param sel_ord the selected order (if any). If the last order is given,
 *                the order will be inserted before that one
 *                the maximum vehicle order id is 254.
 * @param mof what data to modify (@see ModifyOrderFlags)
 * @param data the data to modify
 * @return the cost of this operation or an error
 */
CommandCost CmdModifyOrder(DoCommandFlags flags, VehicleID veh, VehicleOrderID sel_ord, ModifyOrderFlags mof, uint16_t data)
{
	if (mof >= MOF_END) return CMD_ERROR;

	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	/* Is it a valid order? */
	if (sel_ord >= v->GetNumOrders()) return CMD_ERROR;

	Order *order = v->GetOrder(sel_ord);
	assert(order != nullptr);
	switch (order->GetType()) {
		case OT_GOTO_STATION:
			if (mof != MOF_NON_STOP && mof != MOF_STOP_LOCATION && mof != MOF_UNLOAD && mof != MOF_LOAD) return CMD_ERROR;
			break;

		case OT_GOTO_DEPOT:
			if (mof != MOF_NON_STOP && mof != MOF_DEPOT_ACTION) return CMD_ERROR;
			break;

		case OT_GOTO_WAYPOINT:
			if (mof != MOF_NON_STOP) return CMD_ERROR;
			break;

		case OT_CONDITIONAL:
			if (mof != MOF_COND_VARIABLE && mof != MOF_COND_COMPARATOR && mof != MOF_COND_VALUE && mof != MOF_COND_DESTINATION) return CMD_ERROR;
			break;

		default:
			return CMD_ERROR;
	}

	switch (mof) {
		default: NOT_REACHED();

		case MOF_NON_STOP: {
			if (!v->IsGroundVehicle()) return CMD_ERROR;

			OrderNonStopFlags nonstop_flags = static_cast<OrderNonStopFlags>(data);
			if (nonstop_flags == order->GetNonStopType()) return CMD_ERROR;

			/* Test for invalid flags. */
			nonstop_flags.Reset({OrderNonStopFlag::NoIntermediate, OrderNonStopFlag::NoDestination});
			if (nonstop_flags.Any()) return CMD_ERROR;
			break;
		}

		case MOF_STOP_LOCATION:
			if (v->type != VEH_TRAIN) return CMD_ERROR;
			if (data >= to_underlying(OrderStopLocation::End)) return CMD_ERROR;
			break;

		case MOF_UNLOAD: {
			if (order->GetNonStopType().Test(OrderNonStopFlag::NoDestination)) return CMD_ERROR;

			OrderUnloadType unload_type = static_cast<OrderUnloadType>(data);
			if (unload_type == order->GetUnloadType()) return CMD_ERROR;

			/* Test for invalid types. */
			switch (unload_type) {
				case OrderUnloadType::UnloadIfPossible:
				case OrderUnloadType::Unload:
				case OrderUnloadType::Transfer:
				case OrderUnloadType::NoUnload:
					break;

				default: return CMD_ERROR;
			}
			break;
		}

		case MOF_LOAD: {
			if (order->GetNonStopType().Test(OrderNonStopFlag::NoDestination)) return CMD_ERROR;

			OrderLoadType load_type = static_cast<OrderLoadType>(data);
			if (load_type == order->GetLoadType()) return CMD_ERROR;

			/* Test for invalid types. */
			switch (load_type) {
				case OrderLoadType::LoadIfPossible:
				case OrderLoadType::NoLoad:
					break;

				case OrderLoadType::FullLoad:
				case OrderLoadType::FullLoadAny:
					if (v->HasUnbunchingOrder()) return CommandCost(STR_ERROR_UNBUNCHING_NO_FULL_LOAD);
					break;

				default: return CMD_ERROR;
			}
			break;
		}

		case MOF_DEPOT_ACTION: {
			OrderDepotAction depot_action = static_cast<OrderDepotAction>(data);
			if (depot_action >= OrderDepotAction::End) return CMD_ERROR;
			/* Check if we are allowed to add unbunching. We are always allowed to remove it. */
			if (depot_action == OrderDepotAction::Unbunch) {
				/* Only one unbunching order is allowed in a vehicle's orders. If this order already has an unbunching action, no error is needed. */
				if (v->HasUnbunchingOrder() && !order->GetDepotActionType().Test(OrderDepotActionFlag::Unbunch)) return CommandCost(STR_ERROR_UNBUNCHING_ONLY_ONE_ALLOWED);
				/* We don't allow unbunching if the vehicle has a conditional order. */
				if (v->HasConditionalOrder()) return CommandCost(STR_ERROR_UNBUNCHING_NO_UNBUNCHING_CONDITIONAL);
				/* We don't allow unbunching if the vehicle has a full load order. */
				if (v->HasFullLoadOrder()) return CommandCost(STR_ERROR_UNBUNCHING_NO_UNBUNCHING_FULL_LOAD);
			}
			break;
		}

		case MOF_COND_VARIABLE: {
			OrderConditionVariable cond_variable = static_cast<OrderConditionVariable>(data);
			if (cond_variable >= OrderConditionVariable::End) return CMD_ERROR;
			break;
		}

		case MOF_COND_COMPARATOR: {
			OrderConditionComparator cond_comparator = static_cast<OrderConditionComparator>(data);
			if (cond_comparator >= OrderConditionComparator::End) return CMD_ERROR;
			switch (order->GetConditionVariable()) {
				case OrderConditionVariable::Unconditionally: return CMD_ERROR;

				case OrderConditionVariable::RequiresService:
					if (cond_comparator != OrderConditionComparator::IsTrue && cond_comparator != OrderConditionComparator::IsFalse) return CMD_ERROR;
					break;

				default:
					if (cond_comparator == OrderConditionComparator::IsTrue || cond_comparator == OrderConditionComparator::IsFalse) return CMD_ERROR;
					break;
			}
			break;
		}

		case MOF_COND_VALUE:
			switch (order->GetConditionVariable()) {
				case OrderConditionVariable::Unconditionally:
				case OrderConditionVariable::RequiresService:
					return CMD_ERROR;

				case OrderConditionVariable::LoadPercentage:
				case OrderConditionVariable::Reliability:
					if (data > 100) return CMD_ERROR;
					break;

				default:
					if (data > 2047) return CMD_ERROR;
					break;
			}
			break;

		case MOF_COND_DESTINATION:
			if (data >= v->GetNumOrders()) return CMD_ERROR;
			break;
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		switch (mof) {
			case MOF_NON_STOP:
				order->SetNonStopType(static_cast<OrderNonStopFlags>(data));
				if (order->GetNonStopType().Test(OrderNonStopFlag::NoDestination)) {
					order->SetRefit(CARGO_NO_REFIT);
					order->SetLoadType(OrderLoadType::LoadIfPossible);
					order->SetUnloadType(OrderUnloadType::UnloadIfPossible);
				}
				break;

			case MOF_STOP_LOCATION:
				order->SetStopLocation(static_cast<OrderStopLocation>(data));
				break;

			case MOF_UNLOAD:
				order->SetUnloadType(static_cast<OrderUnloadType>(data));
				break;

			case MOF_LOAD:
				order->SetLoadType(static_cast<OrderLoadType>(data));
				if (order->GetLoadType() == OrderLoadType::NoLoad) order->SetRefit(CARGO_NO_REFIT);
				break;

			case MOF_DEPOT_ACTION: {
				switch (static_cast<OrderDepotAction>(data)) {
					case OrderDepotAction::AlwaysGo:
						order->SetDepotOrderType(order->GetDepotOrderType().Reset(OrderDepotTypeFlag::Service));
						order->SetDepotActionType(order->GetDepotActionType().Reset({OrderDepotActionFlag::Halt, OrderDepotActionFlag::Unbunch}));
						break;

					case OrderDepotAction::Service:
						order->SetDepotOrderType(order->GetDepotOrderType().Set(OrderDepotTypeFlag::Service));
						order->SetDepotActionType(order->GetDepotActionType().Reset({OrderDepotActionFlag::Halt, OrderDepotActionFlag::Unbunch}));
						order->SetRefit(CARGO_NO_REFIT);
						break;

					case OrderDepotAction::Stop:
						order->SetDepotOrderType(order->GetDepotOrderType().Reset(OrderDepotTypeFlag::Service));
						order->SetDepotActionType(order->GetDepotActionType().Set(OrderDepotActionFlag::Halt).Reset(OrderDepotActionFlag::Unbunch));
						order->SetRefit(CARGO_NO_REFIT);
						break;

					case OrderDepotAction::Unbunch:
						order->SetDepotOrderType(order->GetDepotOrderType().Reset(OrderDepotTypeFlag::Service));
						order->SetDepotActionType(order->GetDepotActionType().Reset(OrderDepotActionFlag::Halt).Set(OrderDepotActionFlag::Unbunch));
						break;

					default:
						NOT_REACHED();
				}
				break;
			}

			case MOF_COND_VARIABLE: {
				order->SetConditionVariable((OrderConditionVariable)data);

				OrderConditionComparator occ = order->GetConditionComparator();
				switch (order->GetConditionVariable()) {
					case OrderConditionVariable::Unconditionally:
						order->SetConditionComparator(OrderConditionComparator::Equal);
						order->SetConditionValue(0);
						break;

					case OrderConditionVariable::RequiresService:
						if (occ != OrderConditionComparator::IsTrue && occ != OrderConditionComparator::IsFalse) order->SetConditionComparator(OrderConditionComparator::IsTrue);
						order->SetConditionValue(0);
						break;

					case OrderConditionVariable::LoadPercentage:
					case OrderConditionVariable::Reliability:
						if (order->GetConditionValue() > 100) order->SetConditionValue(100);
						[[fallthrough]];

					default:
						if (occ == OrderConditionComparator::IsTrue || occ == OrderConditionComparator::IsFalse) order->SetConditionComparator(OrderConditionComparator::Equal);
						break;
				}
				break;
			}

			case MOF_COND_COMPARATOR:
				order->SetConditionComparator((OrderConditionComparator)data);
				break;

			case MOF_COND_VALUE:
				order->SetConditionValue(data);
				break;

			case MOF_COND_DESTINATION:
				order->SetConditionSkipToOrder(data);
				break;

			default: NOT_REACHED();
		}

		/* Update the windows and full load flags, also for vehicles that share the same order list */
		Vehicle *u = v->FirstShared();
		DeleteOrderWarnings(u);
		for (; u != nullptr; u = u->NextShared()) {
			/* Toggle u->current_order "Full load" flag if it changed.
			 * However, as the same flag is used for depot orders, check
			 * whether we are not going to a depot as there are three
			 * cases where the full load flag can be active and only
			 * one case where the flag is used for depot orders. In the
			 * other cases for the OrderType the flags are not used,
			 * so do not care and those orders should not be active
			 * when this function is called.
			 */
			if (sel_ord == u->cur_real_order_index &&
					(u->current_order.IsType(OT_GOTO_STATION) || u->current_order.IsType(OT_LOADING)) &&
					u->current_order.GetLoadType() != order->GetLoadType()) {
				u->current_order.SetLoadType(order->GetLoadType());
			}

			/* Unbunching data is no longer valid. */
			u->ResetDepotUnbunching();

			InvalidateVehicleOrder(u, VIWD_MODIFY_ORDERS);
		}
	}

	return CommandCost();
}

/**
 * Check if an aircraft has enough range for an order list.
 * @param v_new Aircraft to check.
 * @param v_order Vehicle currently holding the order list.
 * @param first First order in the source order list.
 * @return True if the aircraft has enough range for the orders, false otherwise.
 */
static bool CheckAircraftOrderDistance(const Aircraft *v_new, const Vehicle *v_order)
{
	if (v_new->acache.cached_max_range == 0) return true;
	if (v_order->GetNumOrders() == 0) return true;

	const OrderList &orderlist = *v_order->orders;
	auto orders = orderlist.GetOrders();

	/* Iterate over all orders to check the distance between all
	 * 'goto' orders and their respective next order (of any type). */
	for (VehicleOrderID cur = 0; cur < orderlist.GetNumOrders(); ++cur) {
		switch (orders[cur].GetType()) {
			case OT_GOTO_STATION:
			case OT_GOTO_DEPOT:
			case OT_GOTO_WAYPOINT:
				/* If we don't have a next order, we've reached the end and must check the first order instead. */
				if (GetOrderDistance(cur, orderlist.GetNext(cur), v_order) > v_new->acache.cached_max_range_sqr) return false;
				break;

			default: break;
		}
	}

	return true;
}

/**
 * Clone/share/copy an order-list of another vehicle.
 * @param flags operation to perform
 * @param action action to perform
 * @param veh_dst destination vehicle to clone orders to
 * @param veh_src source vehicle to clone orders from, if any (none for CO_UNSHARE)
 * @return the cost of this operation or an error
 */
CommandCost CmdCloneOrder(DoCommandFlags flags, CloneOptions action, VehicleID veh_dst, VehicleID veh_src)
{
	Vehicle *dst = Vehicle::GetIfValid(veh_dst);
	if (dst == nullptr || !dst->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(dst->owner);
	if (ret.Failed()) return ret;

	switch (action) {
		case CO_SHARE: {
			Vehicle *src = Vehicle::GetIfValid(veh_src);

			/* Sanity checks */
			if (src == nullptr || !src->IsPrimaryVehicle() || dst->type != src->type || dst == src) return CMD_ERROR;

			ret = CheckOwnership(src->owner);
			if (ret.Failed()) return ret;

			/* Trucks can't share orders with busses (and visa versa) */
			if (src->type == VEH_ROAD && RoadVehicle::From(src)->IsBus() != RoadVehicle::From(dst)->IsBus()) {
				return CMD_ERROR;
			}

			/* Is the vehicle already in the shared list? */
			if (src->FirstShared() == dst->FirstShared()) return CMD_ERROR;

			for (const Order &order : src->Orders()) {
				if (!OrderGoesToStation(dst, order)) continue;

				/* Allow copying unreachable destinations if they were already unreachable for the source.
				 * This is basically to allow cloning / autorenewing / autoreplacing vehicles, while the stations
				 * are temporarily invalid due to reconstruction. */
				const Station *st = Station::Get(order.GetDestination().ToStationID());
				if (CanVehicleUseStation(src, st) && !CanVehicleUseStation(dst, st)) {
					return CommandCost(STR_ERROR_CAN_T_COPY_SHARE_ORDER, GetVehicleCannotUseStationReason(dst, st));
				}
			}

			/* Check for aircraft range limits. */
			if (dst->type == VEH_AIRCRAFT && !CheckAircraftOrderDistance(Aircraft::From(dst), src)) {
				return CommandCost(STR_ERROR_AIRCRAFT_NOT_ENOUGH_RANGE);
			}

			if (src->orders == nullptr && !OrderList::CanAllocateItem()) {
				return CommandCost(STR_ERROR_NO_MORE_SPACE_FOR_ORDERS);
			}

			if (flags.Test(DoCommandFlag::Execute)) {
				/* If the destination vehicle had a OrderList, destroy it.
				 * We only reset the order indices, if the new orders are obviously different.
				 * (We mainly do this to keep the order indices valid and in range.) */
				DeleteVehicleOrders(dst, false, dst->GetNumOrders() != src->GetNumOrders());

				dst->orders = src->orders;

				/* Link this vehicle in the shared-list */
				dst->AddToShared(src);

				InvalidateVehicleOrder(dst, VIWD_REMOVE_ALL_ORDERS);
				InvalidateVehicleOrder(src, VIWD_MODIFY_ORDERS);

				InvalidateWindowClassesData(GetWindowClassForVehicleType(dst->type), 0);
			}
			break;
		}

		case CO_COPY: {
			Vehicle *src = Vehicle::GetIfValid(veh_src);

			/* Sanity checks */
			if (src == nullptr || !src->IsPrimaryVehicle() || dst->type != src->type || dst == src) return CMD_ERROR;

			ret = CheckOwnership(src->owner);
			if (ret.Failed()) return ret;

			/* Trucks can't copy all the orders from busses (and visa versa),
			 * and neither can helicopters and aircraft. */
			for (const Order &order : src->Orders()) {
				if (!OrderGoesToStation(dst, order)) continue;
				Station *st = Station::Get(order.GetDestination().ToStationID());
				if (!CanVehicleUseStation(dst, st)) {
					return CommandCost(STR_ERROR_CAN_T_COPY_SHARE_ORDER, GetVehicleCannotUseStationReason(dst, st));
				}
			}

			/* Check for aircraft range limits. */
			if (dst->type == VEH_AIRCRAFT && !CheckAircraftOrderDistance(Aircraft::From(dst), src)) {
				return CommandCost(STR_ERROR_AIRCRAFT_NOT_ENOUGH_RANGE);
			}

			/* make sure there are orders available */
			if (!OrderList::CanAllocateItem()) {
				return CommandCost(STR_ERROR_NO_MORE_SPACE_FOR_ORDERS);
			}

			if (flags.Test(DoCommandFlag::Execute)) {
				/* If the destination vehicle had an order list, destroy the chain but keep the OrderList.
				 * We only reset the order indices, if the new orders are obviously different.
				 * (We mainly do this to keep the order indices valid and in range.) */
				DeleteVehicleOrders(dst, true, dst->GetNumOrders() != src->GetNumOrders());

				std::vector<Order> dst_orders;
				for (const Order &order : src->Orders()) {
					dst_orders.emplace_back(order);
				}

				if (dst->orders != nullptr) {
					assert(dst->orders->GetNumOrders() == 0);
					assert(!dst->orders->IsShared());
					delete dst->orders;
				}

				assert(OrderList::CanAllocateItem());
				dst->orders = new OrderList(std::move(dst_orders), dst);

				InvalidateVehicleOrder(dst, VIWD_REMOVE_ALL_ORDERS);

				InvalidateWindowClassesData(GetWindowClassForVehicleType(dst->type), 0);
			}
			break;
		}

		case CO_UNSHARE: return DecloneOrder(dst, flags);
		default: return CMD_ERROR;
	}

	return CommandCost();
}

/**
 * Add/remove refit orders from an order
 * @param flags operation to perform
 * @param veh VehicleIndex of the vehicle having the order
 * @param order_number number of order to modify
 * @param cargo CargoType
 * @return the cost of this operation or an error
 */
CommandCost CmdOrderRefit(DoCommandFlags flags, VehicleID veh, VehicleOrderID order_number, CargoType cargo)
{
	if (cargo >= NUM_CARGO && cargo != CARGO_NO_REFIT && cargo != CARGO_AUTO_REFIT) return CMD_ERROR;

	const Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	Order *order = v->GetOrder(order_number);
	if (order == nullptr) return CMD_ERROR;

	/* Automatic refit cargo is only supported for goto station orders. */
	if (cargo == CARGO_AUTO_REFIT && !order->IsType(OT_GOTO_STATION)) return CMD_ERROR;

	if (order->GetLoadType() == OrderLoadType::NoLoad) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		order->SetRefit(cargo);

		/* Make the depot order an 'always go' order. */
		if (cargo != CARGO_NO_REFIT && order->IsType(OT_GOTO_DEPOT)) {
			order->SetDepotOrderType(order->GetDepotOrderType().Reset(OrderDepotTypeFlag::Service));
			order->SetDepotActionType(order->GetDepotActionType().Reset(OrderDepotActionFlag::Halt));
		}

		for (Vehicle *u = v->FirstShared(); u != nullptr; u = u->NextShared()) {
			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u, VIWD_MODIFY_ORDERS);

			/* If the vehicle already got the current depot set as current order, then update current order as well */
			if (u->cur_real_order_index == order_number && u->current_order.GetDepotOrderType().Test(OrderDepotTypeFlag::PartOfOrders)) {
				u->current_order.SetRefit(cargo);
			}
		}
	}

	return CommandCost();
}


/**
 *
 * Check the orders of a vehicle, to see if there are invalid orders and stuff
 *
 */
void CheckOrders(const Vehicle *v)
{
	/* Does the user wants us to check things? */
	if (_settings_client.gui.order_review_system == 0) return;

	/* Do nothing for crashed vehicles */
	if (v->vehstatus.Test(VehState::Crashed)) return;

	/* Do nothing for stopped vehicles if setting is '1' */
	if (_settings_client.gui.order_review_system == 1 && v->vehstatus.Test(VehState::Stopped)) return;

	/* do nothing we we're not the first vehicle in a share-chain */
	if (v->FirstShared() != v) return;

	/* Only check every 20 days, so that we don't flood the message log */
	if (v->owner == _local_company && v->day_counter % 20 == 0) {
		StringID message = INVALID_STRING_ID;

		/* Check the order list */
		int n_st = 0;

		for (const Order &order : v->Orders()) {
			/* Dummy order? */
			if (order.IsType(OT_DUMMY)) {
				message = STR_NEWS_VEHICLE_HAS_VOID_ORDER;
				break;
			}
			/* Does station have a load-bay for this vehicle? */
			if (order.IsType(OT_GOTO_STATION)) {
				const Station *st = Station::Get(order.GetDestination().ToStationID());

				n_st++;
				if (!CanVehicleUseStation(v, st)) {
					message = STR_NEWS_VEHICLE_HAS_INVALID_ENTRY;
				} else if (v->type == VEH_AIRCRAFT &&
							(AircraftVehInfo(v->engine_type)->subtype & AIR_FAST) &&
							st->airport.GetFTA()->flags.Test(AirportFTAClass::Flag::ShortStrip) &&
							!_cheats.no_jetcrash.value &&
							message == INVALID_STRING_ID) {
					message = STR_NEWS_PLANE_USES_TOO_SHORT_RUNWAY;
				}
			}
		}

		/* Check if the last and the first order are the same */
		if (v->GetNumOrders() > 1) {
			auto orders = v->Orders();

			if (orders.front().Equals(orders.back())) {
				message = STR_NEWS_VEHICLE_HAS_DUPLICATE_ENTRY;
			}
		}

		/* Do we only have 1 station in our order list? */
		if (n_st < 2 && message == INVALID_STRING_ID) message = STR_NEWS_VEHICLE_HAS_TOO_FEW_ORDERS;

#ifdef WITH_ASSERT
		if (v->orders != nullptr) v->orders->DebugCheckSanity();
#endif

		/* We don't have a problem */
		if (message == INVALID_STRING_ID) return;

		AddVehicleAdviceNewsItem(AdviceType::Order, GetEncodedString(message, v->index), v->index);
	}
}

/**
 * Removes an order from all vehicles. Triggers when, say, a station is removed.
 * @param type The type of the order (OT_GOTO_[STATION|DEPOT|WAYPOINT]).
 * @param destination The destination. Can be a StationID, DepotID or WaypointID.
 * @param hangar Only used for airports in the destination.
 *               When false, remove airport and hangar orders.
 *               When true, remove either airport or hangar order.
 */
void RemoveOrderFromAllVehicles(OrderType type, DestinationID destination, bool hangar)
{
	/* Aircraft have StationIDs for depot orders and never use DepotIDs
	 * This fact is handled specially below
	 */

	/* Go through all vehicles */
	for (Vehicle *v : Vehicle::Iterate()) {
		if ((v->type == VEH_AIRCRAFT && v->current_order.IsType(OT_GOTO_DEPOT) && !hangar ? OT_GOTO_STATION : v->current_order.GetType()) == type &&
				(!hangar || v->type == VEH_AIRCRAFT) && v->current_order.GetDestination() == destination) {
			v->current_order.MakeDummy();
			SetWindowDirty(WC_VEHICLE_VIEW, v->index);
		}

		if (v->orders == nullptr) continue;

		/* Clear the order from the order-list */
		for (VehicleOrderID id = 0, next_id = 0; id < v->GetNumOrders(); id = next_id) {
			next_id = id + 1;
			Order *order = v->orders->GetOrderAt(id);
			OrderType ot = order->GetType();
			if (ot == OT_GOTO_DEPOT && order->GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)) continue;
			if (ot == OT_GOTO_DEPOT && hangar && v->type != VEH_AIRCRAFT) continue; // Not an aircraft? Can't have a hangar order.
			if (ot == OT_IMPLICIT || (v->type == VEH_AIRCRAFT && ot == OT_GOTO_DEPOT && !hangar)) ot = OT_GOTO_STATION;
			if (ot == type && order->GetDestination() == destination) {
				/* We want to clear implicit orders, but we don't want to make them
				 * dummy orders. They should just vanish. Also check the actual order
				 * type as ot is currently OT_GOTO_STATION. */
				if (order->IsType(OT_IMPLICIT)) {
					DeleteOrder(v, id);
					next_id = id;
					continue;
				}

				/* Clear wait time */
				v->orders->UpdateTotalDuration(-order->GetWaitTime());
				if (order->IsWaitTimetabled()) {
					v->orders->UpdateTimetableDuration(-order->GetTimetabledWait());
					order->SetWaitTimetabled(false);
				}
				order->SetWaitTime(0);

				/* Clear order, preserving travel time */
				bool travel_timetabled = order->IsTravelTimetabled();
				order->MakeDummy();
				order->SetTravelTimetabled(travel_timetabled);

				for (const Vehicle *w = v->FirstShared(); w != nullptr; w = w->NextShared()) {
					/* In GUI, simulate by removing the order and adding it back */
					InvalidateVehicleOrder(w, id | (INVALID_VEH_ORDER_ID << 8));
					InvalidateVehicleOrder(w, (INVALID_VEH_ORDER_ID << 8) | id);
				}
			}
		}
	}

	OrderBackup::RemoveOrder(type, destination, hangar);
}

/**
 * Checks if a vehicle has a depot in its order list.
 * @return True iff at least one order is a depot order.
 */
bool Vehicle::HasDepotOrder() const
{
	return std::ranges::any_of(this->Orders(), [](const Order &order) { return order.IsType(OT_GOTO_DEPOT); });
}

/**
 * Delete all orders from a vehicle
 * @param v                   Vehicle whose orders to reset
 * @param keep_orderlist      If true, do not free the order list, only empty it.
 * @param reset_order_indices If true, reset cur_implicit_order_index and cur_real_order_index
 *                            and cancel the current full load order (if the vehicle is loading).
 *                            If false, _you_ have to make sure the order indices are valid after
 *                            your messing with them!
 */
void DeleteVehicleOrders(Vehicle *v, bool keep_orderlist, bool reset_order_indices)
{
	DeleteOrderWarnings(v);

	if (v->IsOrderListShared()) {
		/* Remove ourself from the shared order list. */
		v->RemoveFromShared();
		v->orders = nullptr;
	} else if (v->orders != nullptr) {
		/* Remove the orders */
		v->orders->FreeChain(keep_orderlist);
		if (!keep_orderlist) v->orders = nullptr;
	}

	/* Unbunching data is no longer valid. */
	v->ResetDepotUnbunching();

	if (reset_order_indices) {
		v->cur_implicit_order_index = v->cur_real_order_index = 0;
		if (v->current_order.IsType(OT_LOADING)) {
			CancelLoadingDueToDeletedOrder(v);
		}
	}
}

/**
 * Clamp the service interval to the correct min/max. The actual min/max values
 * depend on whether it's in days, minutes, or percent.
 * @param interval The proposed service interval.
 * @param ispercent Whether the interval is a percent.
 * @return The service interval clamped to use the chosen units.
 */
uint16_t GetServiceIntervalClamped(int interval, bool ispercent)
{
	/* Service intervals are in percents. */
	if (ispercent) return Clamp(interval, MIN_SERVINT_PERCENT, MAX_SERVINT_PERCENT);

	/* Service intervals are in minutes. */
	if (TimerGameEconomy::UsingWallclockUnits(_game_mode == GM_MENU)) return Clamp(interval, MIN_SERVINT_MINUTES, MAX_SERVINT_MINUTES);

	/* Service intervals are in days. */
	return Clamp(interval, MIN_SERVINT_DAYS, MAX_SERVINT_DAYS);
}

/**
 *
 * Check if a vehicle has any valid orders
 *
 * @return false if there are no valid orders
 * @note Conditional orders are not considered valid destination orders
 *
 */
static bool CheckForValidOrders(const Vehicle *v)
{
	return std::ranges::any_of(v->Orders(), [](const Order &order) { return order.IsGotoOrder(); });
}

/**
 * Compare the variable and value based on the given comparator.
 */
static bool OrderConditionCompare(OrderConditionComparator occ, int variable, int value)
{
	switch (occ) {
		case OrderConditionComparator::Equal: return variable == value;
		case OrderConditionComparator::NotEqual: return variable != value;
		case OrderConditionComparator::LessThan: return variable < value;
		case OrderConditionComparator::LessThanOrEqual: return variable <= value;
		case OrderConditionComparator::MoreThan: return variable > value;
		case OrderConditionComparator::MoreThanOrEqual: return variable >= value;
		case OrderConditionComparator::IsTrue: return variable != 0;
		case OrderConditionComparator::IsFalse: return variable == 0;
		default: NOT_REACHED();
	}
}

static bool OrderConditionCompare(OrderConditionComparator occ, ConvertibleThroughBase auto variable, int value)
{
	return OrderConditionCompare(occ, variable.base(), value);
}

/**
 * Process a conditional order and determine the next order.
 * @param order the order the vehicle currently has
 * @param v the vehicle to update
 * @return index of next order to jump to, or INVALID_VEH_ORDER_ID to use the next order
 */
VehicleOrderID ProcessConditionalOrder(const Order *order, const Vehicle *v)
{
	if (order->GetType() != OT_CONDITIONAL) return INVALID_VEH_ORDER_ID;

	bool skip_order = false;
	OrderConditionComparator occ = order->GetConditionComparator();
	uint16_t value = order->GetConditionValue();

	switch (order->GetConditionVariable()) {
		case OrderConditionVariable::LoadPercentage: skip_order = OrderConditionCompare(occ, CalcPercentVehicleFilled(v, nullptr), value); break;
		case OrderConditionVariable::Reliability: skip_order = OrderConditionCompare(occ, ToPercent16(v->reliability), value); break;
		case OrderConditionVariable::MaxReliability: skip_order = OrderConditionCompare(occ, ToPercent16(v->GetEngine()->reliability), value); break;
		case OrderConditionVariable::MaxSpeed: skip_order = OrderConditionCompare(occ, v->GetDisplayMaxSpeed() * 10 / 16, value); break;
		case OrderConditionVariable::Age: skip_order = OrderConditionCompare(occ, TimerGameCalendar::DateToYear(v->age), value); break;
		case OrderConditionVariable::RequiresService: skip_order = OrderConditionCompare(occ, v->NeedsServicing(), value); break;
		case OrderConditionVariable::Unconditionally: skip_order = true; break;
		case OrderConditionVariable::RemainingLifetime: skip_order = OrderConditionCompare(occ, std::max(TimerGameCalendar::DateToYear(v->max_age - v->age + CalendarTime::DAYS_IN_LEAP_YEAR - 1), TimerGameCalendar::Year(0)), value); break;
		default: NOT_REACHED();
	}

	return skip_order ? order->GetConditionSkipToOrder() : (VehicleOrderID)INVALID_VEH_ORDER_ID;
}

/**
 * Update the vehicle's destination tile from an order.
 * @param order the order the vehicle currently has
 * @param v the vehicle to update
 * @param conditional_depth the depth (amount of steps) to go with conditional orders. This to prevent infinite loops.
 * @param pbs_look_ahead Whether we are forecasting orders for pbs reservations in advance. If true, the order indices must not be modified.
 */
bool UpdateOrderDest(Vehicle *v, const Order *order, int conditional_depth, bool pbs_look_ahead)
{
	if (conditional_depth > v->GetNumOrders()) {
		v->current_order.Free();
		v->SetDestTile(TileIndex{});
		return false;
	}

	switch (order->GetType()) {
		case OT_GOTO_STATION:
			v->SetDestTile(v->GetOrderStationLocation(order->GetDestination().ToStationID()));
			return true;

		case OT_GOTO_DEPOT:
			if (order->GetDepotOrderType().Test(OrderDepotTypeFlag::Service) && !v->NeedsServicing()) {
				assert(!pbs_look_ahead);
				UpdateVehicleTimetable(v, true);
				v->IncrementRealOrderIndex();
				break;
			}

			if (v->current_order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)) {
				/* If the vehicle can't find its destination, delay its next search.
				 * In case many vehicles are in this state, use the vehicle index to spread out pathfinder calls. */
				if (v->dest_tile == 0 && TimerGameEconomy::date_fract != (v->index % Ticks::DAY_TICKS)) break;

				/* We need to search for the nearest depot (hangar). */
				ClosestDepot closest_depot = v->FindClosestDepot();

				if (closest_depot.found) {
					/* PBS reservations cannot reverse */
					if (pbs_look_ahead && closest_depot.reverse) return false;

					v->SetDestTile(closest_depot.location);
					v->current_order.SetDestination(closest_depot.destination);

					/* If there is no depot in front, reverse automatically (trains only) */
					if (v->type == VEH_TRAIN && closest_depot.reverse) Command<CMD_REVERSE_TRAIN_DIRECTION>::Do(DoCommandFlag::Execute, v->index, false);

					if (v->type == VEH_AIRCRAFT) {
						Aircraft *a = Aircraft::From(v);
						if (a->state == FLYING && a->targetairport != closest_depot.destination) {
							/* The aircraft is now heading for a different hangar than the next in the orders */
							AircraftNextAirportPos_and_Order(a);
						}
					}
					return true;
				}

				/* If there is no depot, we cannot help PBS either. */
				if (pbs_look_ahead) return false;

				UpdateVehicleTimetable(v, true);
				v->IncrementRealOrderIndex();
			} else {
				if (v->type != VEH_AIRCRAFT) {
					v->SetDestTile(Depot::Get(order->GetDestination().ToStationID())->xy);
				} else {
					Aircraft *a = Aircraft::From(v);
					DestinationID destination = a->current_order.GetDestination();
					if (a->targetairport != destination) {
						/* The aircraft is now heading for a different hangar than the next in the orders */
						a->SetDestTile(a->GetOrderStationLocation(destination.ToStationID()));
					}
				}
				return true;
			}
			break;

		case OT_GOTO_WAYPOINT:
			v->SetDestTile(Waypoint::Get(order->GetDestination().ToStationID())->xy);
			return true;

		case OT_CONDITIONAL: {
			assert(!pbs_look_ahead);
			VehicleOrderID next_order = ProcessConditionalOrder(order, v);
			if (next_order != INVALID_VEH_ORDER_ID) {
				/* Jump to next_order. cur_implicit_order_index becomes exactly that order,
				 * cur_real_order_index might come after next_order. */
				UpdateVehicleTimetable(v, false);
				v->cur_implicit_order_index = v->cur_real_order_index = next_order;
				v->UpdateRealOrderIndex();
				v->current_order_time += v->GetOrder(v->cur_real_order_index)->GetTimetabledTravel();

				/* Disable creation of implicit orders.
				 * When inserting them we do not know that we would have to make the conditional orders point to them. */
				if (v->IsGroundVehicle()) {
					uint16_t &gv_flags = v->GetGroundVehicleFlags();
					SetBit(gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS);
				}
			} else {
				UpdateVehicleTimetable(v, true);
				v->IncrementRealOrderIndex();
			}
			break;
		}

		default:
			v->SetDestTile(TileIndex{});
			return false;
	}

	assert(v->cur_implicit_order_index < v->GetNumOrders());
	assert(v->cur_real_order_index < v->GetNumOrders());

	/* Get the current order */
	order = v->GetOrder(v->cur_real_order_index);
	if (order != nullptr && order->IsType(OT_IMPLICIT)) {
		assert(v->GetNumManualOrders() == 0);
		order = nullptr;
	}

	if (order == nullptr) {
		v->current_order.Free();
		v->SetDestTile(TileIndex{});
		return false;
	}

	v->current_order = *order;
	return UpdateOrderDest(v, order, conditional_depth + 1, pbs_look_ahead);
}

/**
 * Handle the orders of a vehicle and determine the next place
 * to go to if needed.
 * @param v the vehicle to do this for.
 * @return true *if* the vehicle is eligible for reversing
 *              (basically only when leaving a station).
 */
bool ProcessOrders(Vehicle *v)
{
	switch (v->current_order.GetType()) {
		case OT_GOTO_DEPOT:
			/* Let a depot order in the orderlist interrupt. */
			if (!v->current_order.GetDepotOrderType().Test(OrderDepotTypeFlag::PartOfOrders)) return false;
			break;

		case OT_LOADING:
			return false;

		case OT_LEAVESTATION:
			if (v->type != VEH_AIRCRAFT) return false;
			break;

		default: break;
	}

	/**
	 * Reversing because of order change is allowed only just after leaving a
	 * station (and the difficulty setting to allowed, of course)
	 * this can be detected because only after OT_LEAVESTATION, current_order
	 * will be reset to nothing. (That also happens if no order, but in that case
	 * it won't hit the point in code where may_reverse is checked)
	 */
	bool may_reverse = v->current_order.IsType(OT_NOTHING);

	/* Check if we've reached a 'via' destination. */
	if (((v->current_order.IsType(OT_GOTO_STATION) && v->current_order.GetNonStopType().Test(OrderNonStopFlag::NoDestination)) || v->current_order.IsType(OT_GOTO_WAYPOINT)) &&
			IsTileType(v->tile, MP_STATION) &&
			v->current_order.GetDestination() == GetStationIndex(v->tile)) {
		v->DeleteUnreachedImplicitOrders();
		/* We set the last visited station here because we do not want
		 * the train to stop at this 'via' station if the next order
		 * is a no-non-stop order; in that case not setting the last
		 * visited station will cause the vehicle to still stop. */
		v->last_station_visited = v->current_order.GetDestination().ToStationID();
		UpdateVehicleTimetable(v, true);
		v->IncrementImplicitOrderIndex();
	}

	/* Get the current order */
	assert(v->cur_implicit_order_index == 0 || v->cur_implicit_order_index < v->GetNumOrders());
	v->UpdateRealOrderIndex();

	const Order *order = v->GetOrder(v->cur_real_order_index);
	if (order != nullptr && order->IsType(OT_IMPLICIT)) {
		assert(v->GetNumManualOrders() == 0);
		order = nullptr;
	}

	/* If no order, do nothing. */
	if (order == nullptr || (v->type == VEH_AIRCRAFT && !CheckForValidOrders(v))) {
		if (v->type == VEH_AIRCRAFT) {
			/* Aircraft do something vastly different here, so handle separately */
			HandleMissingAircraftOrders(Aircraft::From(v));
			return false;
		}

		v->current_order.Free();
		v->SetDestTile(TileIndex{});
		return false;
	}

	/* If it is unchanged, keep it. */
	if (order->Equals(v->current_order) && (v->type == VEH_AIRCRAFT || v->dest_tile != 0) &&
			(v->type != VEH_SHIP || !order->IsType(OT_GOTO_STATION) || Station::Get(order->GetDestination().ToStationID())->ship_station.tile != INVALID_TILE)) {
		return false;
	}

	/* Otherwise set it, and determine the destination tile. */
	v->current_order = *order;

	InvalidateVehicleOrder(v, VIWD_MODIFY_ORDERS);
	switch (v->type) {
		default:
			NOT_REACHED();

		case VEH_ROAD:
		case VEH_TRAIN:
			break;

		case VEH_AIRCRAFT:
		case VEH_SHIP:
			SetWindowClassesDirty(GetWindowClassForVehicleType(v->type));
			break;
	}

	return UpdateOrderDest(v, order) && may_reverse;
}

/**
 * Check whether the given vehicle should stop at the given station
 * based on this order and the non-stop settings.
 * @param v       the vehicle that might be stopping.
 * @param station the station to stop at.
 * @return true if the vehicle should stop.
 */
bool Order::ShouldStopAtStation(const Vehicle *v, StationID station) const
{
	bool is_dest_station = this->IsType(OT_GOTO_STATION) && this->dest == station;

	return (!this->IsType(OT_GOTO_DEPOT) || this->GetDepotOrderType().Test(OrderDepotTypeFlag::PartOfOrders)) &&
			v->last_station_visited != station && // Do stop only when we've not just been there
			/* Finally do stop when there is no non-stop flag set for this type of station. */
			!this->GetNonStopType().Test(is_dest_station ? OrderNonStopFlag::NoDestination : OrderNonStopFlag::NoIntermediate);
}

bool Order::CanLoadOrUnload() const
{
	return (this->IsType(OT_GOTO_STATION) || this->IsType(OT_IMPLICIT)) &&
			!this->GetNonStopType().Test(OrderNonStopFlag::NoDestination) &&
			(this->GetLoadType() != OrderLoadType::NoLoad ||
			this->GetUnloadType() != OrderUnloadType::NoUnload);
}

/**
 * A vehicle can leave the current station with cargo if:
 * 1. it can load cargo here OR
 * 2a. it could leave the last station with cargo AND
 * 2b. it doesn't have to unload all cargo here.
 */
bool Order::CanLeaveWithCargo(bool has_cargo) const
{
	return this->GetLoadType() != OrderLoadType::NoLoad || (has_cargo &&
			this->GetUnloadType() != OrderUnloadType::Unload &&
			this->GetUnloadType() != OrderUnloadType::Transfer);
}
