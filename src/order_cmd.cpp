/* $Id$ */

/** @file order_cmd.cpp Handling of orders. */

#include "stdafx.h"
#include "debug.h"
#include "command_func.h"
#include "company_func.h"
#include "news_func.h"
#include "vehicle_gui.h"
#include "cargotype.h"
#include "station_map.h"
#include "vehicle_base.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "newgrf_cargo.h"
#include "timetable.h"
#include "vehicle_func.h"
#include "oldpool_func.h"
#include "depot_base.h"
#include "settings_type.h"

#include "table/strings.h"

/* DestinationID must be at least as large as every these below, because it can
 * be any of them
 */
assert_compile(sizeof(DestinationID) >= sizeof(DepotID));
assert_compile(sizeof(DestinationID) >= sizeof(WaypointID));
assert_compile(sizeof(DestinationID) >= sizeof(StationID));

TileIndex _backup_orders_tile;
BackuppedOrders _backup_orders_data;

DEFINE_OLD_POOL_GENERIC(Order, Order);
DEFINE_OLD_POOL_GENERIC(OrderList, OrderList);

void Order::Free()
{
	this->type  = OT_NOTHING;
	this->flags = 0;
	this->dest  = 0;
	this->next  = NULL;
}

void Order::MakeGoToStation(StationID destination)
{
	this->type = OT_GOTO_STATION;
	this->flags = 0;
	this->dest = destination;
}

void Order::MakeGoToDepot(DepotID destination, OrderDepotTypeFlags order, OrderDepotActionFlags action, CargoID cargo, byte subtype)
{
	this->type = OT_GOTO_DEPOT;
	this->SetDepotOrderType(order);
	this->SetDepotActionType(action);
	if (!(order & ODTFB_PART_OF_ORDERS)) {
		this->SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
	}
	this->dest = destination;
	this->SetRefit(cargo, subtype);
}

void Order::MakeGoToWaypoint(WaypointID destination)
{
	this->type = OT_GOTO_WAYPOINT;
	this->flags = 0;
	this->dest = destination;
}

void Order::MakeLoading(bool ordered)
{
	this->type = OT_LOADING;
	if (!ordered) this->flags = 0;
}

void Order::MakeLeaveStation()
{
	this->type = OT_LEAVESTATION;
	this->flags = 0;
}

void Order::MakeDummy()
{
	this->type = OT_DUMMY;
	this->flags = 0;
}

void Order::MakeConditional(VehicleOrderID order)
{
	this->type = OT_CONDITIONAL;
	this->flags = order;
	this->dest = 0;
}

void Order::SetRefit(CargoID cargo, byte subtype)
{
	this->refit_cargo = cargo;
	this->refit_subtype = subtype;
}

bool Order::Equals(const Order &other) const
{
	/* In case of go to nearest depot orders we need "only" compare the flags
	 * with the other and not the nearest depot order bit or the actual
	 * destination because those get clear/filled in during the order
	 * evaluation. If we do not do this the order will continuously be seen as
	 * a different order and it will try to find a "nearest depot" every tick. */
	if ((this->type == OT_GOTO_DEPOT && this->type == other.type) &&
			((this->GetDepotActionType() & ODATFB_NEAREST_DEPOT) != 0 ||
			 (other.GetDepotActionType() & ODATFB_NEAREST_DEPOT) != 0)) {
		return
			this->GetDepotOrderType() == other.GetDepotOrderType() &&
			(this->GetDepotActionType() & ~ODATFB_NEAREST_DEPOT) == (other.GetDepotActionType() & ~ODATFB_NEAREST_DEPOT);
	}

	return
			this->type  == other.type &&
			this->flags == other.flags &&
			this->dest  == other.dest;
}

uint32 Order::Pack() const
{
	return this->dest << 16 | this->flags << 8 | this->type;
}

Order::Order(uint32 packed)
{
	this->type    = (OrderType)GB(packed,  0,  8);
	this->flags   = GB(packed,  8,  8);
	this->dest    = GB(packed, 16, 16);
	this->next    = NULL;
	this->refit_cargo   = CT_NO_REFIT;
	this->refit_subtype = 0;
	this->wait_time     = 0;
	this->travel_time   = 0;
}

/**
 *
 * Updates the widgets of a vehicle which contains the order-data
 *
 */
void InvalidateVehicleOrder(const Vehicle *v, int data)
{
	InvalidateWindow(WC_VEHICLE_VIEW, v->index);

	if (data != 0) {
		/* Calls SetDirty() too */
		InvalidateWindowData(WC_VEHICLE_ORDERS,    v->index, data);
		InvalidateWindowData(WC_VEHICLE_TIMETABLE, v->index, data);
		return;
	}

	InvalidateWindow(WC_VEHICLE_ORDERS,    v->index);
	InvalidateWindow(WC_VEHICLE_TIMETABLE, v->index);
}

/**
 *
 * Assign data to an order (from an other order)
 *   This function makes sure that the index is maintained correctly
 *
 */
void Order::AssignOrder(const Order &other)
{
	this->type  = other.type;
	this->flags = other.flags;
	this->dest  = other.dest;

	this->refit_cargo   = other.refit_cargo;
	this->refit_subtype = other.refit_subtype;

	this->wait_time   = other.wait_time;
	this->travel_time = other.travel_time;
}


OrderList::OrderList(Order *chain, Vehicle *v) :
		first(chain), num_orders(0), num_vehicles(1), first_shared(v),
		timetable_duration(0)
{
	for (Order *o = this->first; o != NULL; o = o->next) {
		++this->num_orders;
		this->timetable_duration += o->wait_time + o->travel_time;
	}

	for (Vehicle *u = v->PreviousShared(); u != NULL; u = u->PreviousShared()) {
		++this->num_vehicles;
		this->first_shared = u;
	}

	for (const Vehicle *u = v->NextShared(); u != NULL; u = u->NextShared()) ++this->num_vehicles;
}

void OrderList::FreeChain(bool keep_orderlist)
{
	Order *next;
	for(Order *o = this->first; o != NULL; o = next) {
		next = o->next;
		delete o;
	}

	if (keep_orderlist) {
		this->first = NULL;
		this->num_orders = 0;
		this->timetable_duration = 0;
	} else {
		delete this;
	}
}

Order *OrderList::GetOrderAt(int index) const
{
	if (index < 0) return NULL;

	Order *order = this->first;

	while (order != NULL && index-- > 0)
		order = order->next;

	return order;
}

void OrderList::InsertOrderAt(Order *new_order, int index)
{
	if (this->first == NULL) {
		this->first = new_order;
	} else {
		if (index == 0) {
			/* Insert as first or only order */
			new_order->next = this->first;
			this->first = new_order;
		} else if (index >= this->num_orders) {
			/* index is after the last order, add it to the end */
			this->GetLastOrder()->next = new_order;
		} else {
			/* Put the new order in between */
			Order *order = this->GetOrderAt(index - 1);
			new_order->next = order->next;
			order->next = new_order;
		}
	}
	++this->num_orders;
	this->timetable_duration += new_order->wait_time + new_order->travel_time;
}


void OrderList::DeleteOrderAt(int index)
{
	if (index >= this->num_orders) return;

	Order *to_remove;

	if (index == 0) {
		to_remove = this->first;
		this->first = to_remove->next;
	} else {
		Order *prev = GetOrderAt(index - 1);
		to_remove = prev->next;
		prev->next = to_remove->next;
	}
	--this->num_orders;
	this->timetable_duration -= (to_remove->wait_time + to_remove->travel_time);
	delete to_remove;
}

void OrderList::MoveOrder(int from, int to)
{
	if (from >= this->num_orders || to >= this->num_orders || from == to) return;

	Order *moving_one;

	/* Take the moving order out of the pointer-chain */
	if (from == 0) {
		moving_one = this->first;
		this->first = moving_one->next;
	} else {
		Order *one_before = GetOrderAt(from - 1);
		moving_one = one_before->next;
		one_before->next = moving_one->next;
	}

	/* Insert the moving_order again in the pointer-chain */
	if (to == 0) {
		moving_one->next = this->first;
		this->first = moving_one;
	} else {
		Order *one_before = GetOrderAt(to - 1);
		moving_one->next = one_before->next;
		one_before->next = moving_one;
	}
}

void OrderList::RemoveVehicle(Vehicle *v)
{
	--this->num_vehicles;
	if (v == this->first_shared) this->first_shared = v->NextShared();
}

bool OrderList::IsVehicleInSharedOrdersList(const Vehicle *v) const
{
	for (const Vehicle *v_shared = this->first_shared; v_shared != NULL; v_shared = v_shared->NextShared()) {
		if (v_shared == v) return true;
	}

	return false;
}

int OrderList::GetPositionInSharedOrderList(const Vehicle *v) const
{
	int count = 0;
	for (const Vehicle *v_shared = v->PreviousShared(); v_shared != NULL; v_shared = v_shared->PreviousShared()) count++;
	return count;
}

bool OrderList::IsCompleteTimetable() const
{
	for (Order *o = this->first; o != NULL; o = o->next) {
		if (!o->IsCompletelyTimetabled()) return false;
	}
	return true;
}

void OrderList::DebugCheckSanity() const
{
	VehicleOrderID check_num_orders = 0;
	uint check_num_vehicles = 0;
	uint check_timetable_duration = 0;

	DEBUG(misc, 6, "Checking OrderList %hu for sanity...", this->index);

	for (const Order *o = this->first; o != NULL; o = o->next) {
		++check_num_orders;
		check_timetable_duration += o->wait_time + o->travel_time;
	}
	assert(this->num_orders == check_num_orders);
	assert(this->timetable_duration == check_timetable_duration);

	for (const Vehicle *v = this->first_shared; v != NULL; v = v->NextShared()) {
		++check_num_vehicles;
		assert(v->orders.list == this);
	}
	assert(this->num_vehicles == check_num_vehicles);
	DEBUG(misc, 6, "... detected %u orders, %u vehicles, %u ticks", (uint)this->num_orders,
	      this->num_vehicles, this->timetable_duration);
}

/**
 * Checks whether the order goes to a station or not, i.e. whether the
 * destination is a station
 * @param v the vehicle to check for
 * @param o the order to check
 * @return true if the destination is a station
 */
static inline bool OrderGoesToStation(const Vehicle *v, const Order *o)
{
	return o->IsType(OT_GOTO_STATION) ||
			(v->type == VEH_AIRCRAFT && o->IsType(OT_GOTO_DEPOT) && !(o->GetDepotActionType() & ODATFB_NEAREST_DEPOT));
}

/**
 * Delete all news items regarding defective orders about a vehicle
 * This could kill still valid warnings (for example about void order when just
 * another order gets added), but assume the company will notice the problems,
 * when (s)he's changing the orders.
 */
static void DeleteOrderWarnings(const Vehicle *v)
{
	DeleteVehicleNews(v->index, STR_VEHICLE_HAS_TOO_FEW_ORDERS);
	DeleteVehicleNews(v->index, STR_VEHICLE_HAS_VOID_ORDER);
	DeleteVehicleNews(v->index, STR_VEHICLE_HAS_DUPLICATE_ENTRY);
	DeleteVehicleNews(v->index, STR_VEHICLE_HAS_INVALID_ENTRY);
}


static TileIndex GetOrderLocation(const Order& o)
{
	switch (o.GetType()) {
		default: NOT_REACHED();
		case OT_GOTO_STATION: return GetStation(o.GetDestination())->xy;
		case OT_GOTO_DEPOT:   return GetDepot(o.GetDestination())->xy;
	}
}

static uint GetOrderDistance(const Order *prev, const Order *cur, const Vehicle *v, int conditional_depth = 0)
{
	if (cur->IsType(OT_CONDITIONAL)) {
		if (conditional_depth > v->GetNumOrders()) return 0;

		conditional_depth++;

		int dist1 = GetOrderDistance(prev, GetVehicleOrder(v, cur->GetConditionSkipToOrder()), v, conditional_depth);
		int dist2 = GetOrderDistance(prev, cur->next == NULL ? v->orders.list->GetFirstOrder() : cur->next, v, conditional_depth);
		return max(dist1, dist2);
	}

	return DistanceManhattan(GetOrderLocation(*prev), GetOrderLocation(*cur));
}

/** Add an order to the orderlist of a vehicle.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 - 15) - ID of the vehicle
 * - p1 = (bit 16 - 31) - the selected order (if any). If the last order is given,
 *                        the order will be inserted before that one
 *                        the maximum vehicle order id is 254.
 * @param p2 packed order to insert
 */
CommandCost CmdInsertOrder(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v;
	VehicleID veh   = GB(p1,  0, 16);
	VehicleOrderID sel_ord = GB(p1, 16, 16);
	Order new_order(p2);

	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	v = GetVehicle(veh);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	/* Check if the inserted order is to the correct destination (owner, type),
	 * and has the correct flags if any */
	switch (new_order.GetType()) {
		case OT_GOTO_STATION: {
			if (!IsValidStationID(new_order.GetDestination())) return CMD_ERROR;

			const Station *st = GetStation(new_order.GetDestination());

			if (st->owner != OWNER_NONE && !CheckOwnership(st->owner)) return CMD_ERROR;

			if (!CanVehicleUseStation(v, st)) return_cmd_error(STR_CAN_T_ADD_ORDER);
			for (Vehicle *u = v->FirstShared(); u != NULL; u = u->NextShared()) {
				if (!CanVehicleUseStation(u, st)) return_cmd_error(STR_CAN_T_ADD_ORDER_SHARED);
			}

			/* Non stop not allowed for non-trains. */
			if (new_order.GetNonStopType() != ONSF_STOP_EVERYWHERE && v->type != VEH_TRAIN && v->type != VEH_ROAD) return CMD_ERROR;

			/* No load and no unload are mutual exclusive. */
			if ((new_order.GetLoadType() & OLFB_NO_LOAD) && (new_order.GetUnloadType() & OUFB_NO_UNLOAD)) return CMD_ERROR;

			/* Filter invalid load/unload types. */
			switch (new_order.GetLoadType()) {
				case OLF_LOAD_IF_POSSIBLE: case OLFB_FULL_LOAD: case OLF_FULL_LOAD_ANY: case OLFB_NO_LOAD: break;
				default: return CMD_ERROR;
			}
			switch (new_order.GetUnloadType()) {
				case OUF_UNLOAD_IF_POSSIBLE: case OUFB_UNLOAD: case OUFB_TRANSFER: case OUFB_NO_UNLOAD: break;
				default: return CMD_ERROR;
			}
			break;
		}

		case OT_GOTO_DEPOT: {
			if (new_order.GetDepotActionType() != ODATFB_NEAREST_DEPOT) {
				if (v->type == VEH_AIRCRAFT) {
					if (!IsValidStationID(new_order.GetDestination())) return CMD_ERROR;

					const Station *st = GetStation(new_order.GetDestination());

					if (!CheckOwnership(st->owner) ||
							!CanVehicleUseStation(v, st) ||
							st->Airport()->nof_depots == 0) {
						return CMD_ERROR;
					}
				} else {
					if (!IsValidDepotID(new_order.GetDestination())) return CMD_ERROR;

					const Depot *dp = GetDepot(new_order.GetDestination());

					if (!CheckOwnership(GetTileOwner(dp->xy))) return CMD_ERROR;

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
			} else {
				if (!IsCompanyBuildableVehicleType(v)) return CMD_ERROR;
			}

			if (new_order.GetNonStopType() != ONSF_STOP_EVERYWHERE && v->type != VEH_TRAIN && v->type != VEH_ROAD) return CMD_ERROR;
			if (new_order.GetDepotOrderType() & ~(ODTFB_PART_OF_ORDERS | ((new_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) != 0 ? ODTFB_SERVICE : 0))) return CMD_ERROR;
			if (new_order.GetDepotActionType() & ~ODATFB_NEAREST_DEPOT) return CMD_ERROR;
			break;
		}

		case OT_GOTO_WAYPOINT: {
			if (v->type != VEH_TRAIN) return CMD_ERROR;

			if (!IsValidWaypointID(new_order.GetDestination())) return CMD_ERROR;
			const Waypoint *wp = GetWaypoint(new_order.GetDestination());

			if (!CheckOwnership(wp->owner)) return CMD_ERROR;

			/* Order flags can be any of the following for waypoints:
			 * [non-stop]
			 * non-stop orders (if any) are only valid for trains */
			if (new_order.GetNonStopType() != ONSF_STOP_EVERYWHERE && v->type != VEH_TRAIN) return CMD_ERROR;
			break;
		}

		case OT_CONDITIONAL: {
			VehicleOrderID skip_to = new_order.GetConditionSkipToOrder();
			if (skip_to != 0 && skip_to >= v->GetNumOrders()) return CMD_ERROR; // Always allow jumping to the first (even when there is no order).
			if (new_order.GetConditionVariable() > OCV_END) return CMD_ERROR;

			OrderConditionComparator occ = new_order.GetConditionComparator();
			if (occ > OCC_END) return CMD_ERROR;
			switch (new_order.GetConditionVariable()) {
				case OCV_REQUIRES_SERVICE:
					if (occ != OCC_IS_TRUE && occ != OCC_IS_FALSE) return CMD_ERROR;
					break;

				case OCV_UNCONDITIONALLY:
					if (occ != OCC_EQUALS) return CMD_ERROR;
					if (new_order.GetConditionValue() != 0) return CMD_ERROR;
					break;

				case OCV_LOAD_PERCENTAGE:
				case OCV_RELIABILITY:
					if (new_order.GetConditionValue() > 100) return CMD_ERROR;
					/* FALL THROUGH */
				default:
					if (occ == OCC_IS_TRUE || occ == OCC_IS_FALSE) return CMD_ERROR;
					break;
			}
		} break;

		default: return CMD_ERROR;
	}

	if (sel_ord > v->GetNumOrders()) return CMD_ERROR;

	if (v->GetNumOrders() >= MAX_VEH_ORDER_ID) return_cmd_error(STR_8832_TOO_MANY_ORDERS);
	if (!Order::CanAllocateItem()) return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);
	if (v->orders.list == NULL && !OrderList::CanAllocateItem()) return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);

	if (v->type == VEH_SHIP && _settings_game.pf.pathfinder_for_ships != VPF_NPF) {
		/* Make sure the new destination is not too far away from the previous */
		const Order *prev = NULL;
		uint n = 0;

		/* Find the last goto station or depot order before the insert location.
		 * If the order is to be inserted at the beginning of the order list this
		 * finds the last order in the list. */
		const Order *o;
		FOR_VEHICLE_ORDERS(v, o) {
			if (o->IsType(OT_GOTO_STATION) || o->IsType(OT_GOTO_DEPOT)) prev = o;
			if (++n == sel_ord && prev != NULL) break;
		}
		if (prev != NULL) {
			uint dist = GetOrderDistance(prev, &new_order, v);
			if (dist >= 130) {
				return_cmd_error(STR_0210_TOO_FAR_FROM_PREVIOUS_DESTINATIO);
			}
		}
	}

	if (flags & DC_EXEC) {
		Order *new_o = new Order();
		new_o->AssignOrder(new_order);

		/* Create new order and link in list */
		if (v->orders.list == NULL) {
			v->orders.list = new OrderList(new_o, v);
		} else {
			v->orders.list->InsertOrderAt(new_o, sel_ord);
		}

		Vehicle *u = v->FirstShared();
		DeleteOrderWarnings(u);
		for (; u != NULL; u = u->NextShared()) {
			assert(v->orders.list == u->orders.list);

			/* If there is added an order before the current one, we need
			to update the selected order */
			if (sel_ord <= u->cur_order_index) {
				uint cur = u->cur_order_index + 1;
				/* Check if we don't go out of bound */
				if (cur < u->GetNumOrders())
					u->cur_order_index = cur;
			}
			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u, INVALID_VEH_ORDER_ID | (sel_ord << 8));
		}

		/* As we insert an order, the order to skip to will be 'wrong'. */
		VehicleOrderID cur_order_id = 0;
		Order *order;
		FOR_VEHICLE_ORDERS(v, order) {
			if (order->IsType(OT_CONDITIONAL)) {
				VehicleOrderID order_id = order->GetConditionSkipToOrder();
				if (order_id >= sel_ord) {
					order->SetConditionSkipToOrder(order_id + 1);
				}
				if (order_id == cur_order_id) {
					order->SetConditionSkipToOrder((order_id + 1) % v->GetNumOrders());
				}
			}
			cur_order_id++;
		}

		/* Make sure to rebuild the whole list */
		InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 0);
	}

	return CommandCost();
}

/** Declone an order-list
 * @param *dst delete the orders of this vehicle
 * @param flags execution flags
 */
static CommandCost DecloneOrder(Vehicle *dst, DoCommandFlag flags)
{
	if (flags & DC_EXEC) {
		DeleteVehicleOrders(dst);
		InvalidateVehicleOrder(dst, -1);
		InvalidateWindowClassesData(GetWindowClassForVehicleType(dst->type), 0);
	}
	return CommandCost();
}

/** Delete an order from the orderlist of a vehicle.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the ID of the vehicle
 * @param p2 the order to delete (max 255)
 */
CommandCost CmdDeleteOrder(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v;
	VehicleID veh_id = p1;
	VehicleOrderID sel_ord = p2;
	Order *order;

	if (!IsValidVehicleID(veh_id)) return CMD_ERROR;

	v = GetVehicle(veh_id);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	/* If we did not select an order, we maybe want to de-clone the orders */
	if (sel_ord >= v->GetNumOrders())
		return DecloneOrder(v, flags);

	order = GetVehicleOrder(v, sel_ord);
	if (order == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->orders.list->DeleteOrderAt(sel_ord);

		Vehicle *u = v->FirstShared();
		DeleteOrderWarnings(u);
		for (; u != NULL; u = u->NextShared()) {
			if (sel_ord < u->cur_order_index)
				u->cur_order_index--;

			assert(v->orders.list == u->orders.list);

			/* NON-stop flag is misused to see if a train is in a station that is
			 * on his order list or not */
			if (sel_ord == u->cur_order_index && u->current_order.IsType(OT_LOADING)) {
				u->current_order.SetNonStopType(ONSF_STOP_EVERYWHERE);
			}

			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u, sel_ord | (INVALID_VEH_ORDER_ID << 8));
		}

		/* As we delete an order, the order to skip to will be 'wrong'. */
		VehicleOrderID cur_order_id = 0;
		FOR_VEHICLE_ORDERS(v, order) {
			if (order->IsType(OT_CONDITIONAL)) {
				VehicleOrderID order_id = order->GetConditionSkipToOrder();
				if (order_id >= sel_ord) {
					order->SetConditionSkipToOrder(max(order_id - 1, 0));
				}
				if (order_id == cur_order_id) {
					order->SetConditionSkipToOrder((order_id + 1) % v->GetNumOrders());
				}
			}
			cur_order_id++;
		}

		InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 0);
	}

	return CommandCost();
}

/** Goto order of order-list.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 The ID of the vehicle which order is skipped
 * @param p2 the selected order to which we want to skip
 */
CommandCost CmdSkipToOrder(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v;
	VehicleID veh_id = p1;
	VehicleOrderID sel_ord = p2;

	if (!IsValidVehicleID(veh_id)) return CMD_ERROR;

	v = GetVehicle(veh_id);

	if (!CheckOwnership(v->owner) || sel_ord == v->cur_order_index ||
			sel_ord >= v->GetNumOrders() || v->GetNumOrders() < 2) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->cur_order_index = sel_ord;

		if (v->type == VEH_ROAD) ClearSlot(v);

		if (v->current_order.IsType(OT_LOADING)) v->LeaveStation();

		InvalidateVehicleOrder(v, 0);
	}

	/* We have an aircraft/ship, they have a mini-schedule, so update them all */
	if (v->type == VEH_AIRCRAFT) InvalidateWindowClasses(WC_AIRCRAFT_LIST);
	if (v->type == VEH_SHIP) InvalidateWindowClasses(WC_SHIPS_LIST);

	return CommandCost();
}

/**
 * Move an order inside the orderlist
 * @param tile unused
 * @param p1 the ID of the vehicle
 * @param p2 order to move and target
 *           bit 0-15  : the order to move
 *           bit 16-31 : the target order
 * @note The target order will move one place down in the orderlist
 *  if you move the order upwards else it'll move it one place down
 */
CommandCost CmdMoveOrder(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID veh = p1;
	VehicleOrderID moving_order = GB(p2,  0, 16);
	VehicleOrderID target_order = GB(p2, 16, 16);

	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	/* Don't make senseless movements */
	if (moving_order >= v->GetNumOrders() || target_order >= v->GetNumOrders() ||
			moving_order == target_order || v->GetNumOrders() <= 1)
		return CMD_ERROR;

	Order *moving_one = GetVehicleOrder(v, moving_order);
	/* Don't move an empty order */
	if (moving_one == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->orders.list->MoveOrder(moving_order, target_order);

		/* Update shared list */
		Vehicle *u = v->FirstShared();

		DeleteOrderWarnings(u);

		for (; u != NULL; u = u->NextShared()) {
			/* Update the current order */
			if (u->cur_order_index == moving_order) {
				u->cur_order_index = target_order;
			} else if(u->cur_order_index > moving_order && u->cur_order_index <= target_order) {
				u->cur_order_index--;
			} else if(u->cur_order_index < moving_order && u->cur_order_index >= target_order) {
				u->cur_order_index++;
			}

			assert(v->orders.list == u->orders.list);
			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u, moving_order | (target_order << 8));
		}

		/* As we move an order, the order to skip to will be 'wrong'. */
		Order *order;
		FOR_VEHICLE_ORDERS(v, order) {
			if (order->IsType(OT_CONDITIONAL)) {
				VehicleOrderID order_id = order->GetConditionSkipToOrder();
				if (order_id == moving_order) {
					order_id = target_order;
				} else if(order_id > moving_order && order_id <= target_order) {
					order_id--;
				} else if(order_id < moving_order && order_id >= target_order) {
					order_id++;
				}
				order->SetConditionSkipToOrder(order_id);
			}
		}

		/* Make sure to rebuild the whole list */
		InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 0);
	}

	return CommandCost();
}

/** Modify an order in the orderlist of a vehicle.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 - 15) - ID of the vehicle
 * - p1 = (bit 16 - 31) - the selected order (if any). If the last order is given,
 *                        the order will be inserted before that one
 *                        only the first 8 bits used currently (bit 16 - 23) (max 255)
 * @param p2 various bitstuffed elements
 *  - p2 = (bit 0 -  3) - what data to modify (@see ModifyOrderFlags)
 *  - p2 = (bit 4 - 15) - the data to modify
 */
CommandCost CmdModifyOrder(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleOrderID sel_ord = GB(p1, 16, 16); // XXX - automatically truncated to 8 bits.
	VehicleID veh          = GB(p1,  0, 16);
	ModifyOrderFlags mof   = (ModifyOrderFlags)GB(p2,  0,  4);
	uint16 data             = GB(p2, 4, 11);

	if (mof >= MOF_END) return CMD_ERROR;
	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	/* Is it a valid order? */
	if (sel_ord >= v->GetNumOrders()) return CMD_ERROR;

	Order *order = GetVehicleOrder(v, sel_ord);
	switch (order->GetType()) {
		case OT_GOTO_STATION:
			if (mof == MOF_COND_VARIABLE || mof == MOF_COND_COMPARATOR || mof == MOF_DEPOT_ACTION || mof == MOF_COND_VALUE || GetStation(order->GetDestination())->IsBuoy()) return CMD_ERROR;
			break;

		case OT_GOTO_DEPOT:
			if (mof != MOF_NON_STOP && mof != MOF_DEPOT_ACTION) return CMD_ERROR;
			break;

		case OT_GOTO_WAYPOINT:
			if (mof != MOF_NON_STOP) return CMD_ERROR;
			break;

		case OT_CONDITIONAL:
			if (mof != MOF_COND_VARIABLE && mof != MOF_COND_COMPARATOR && mof != MOF_COND_VALUE) return CMD_ERROR;
			break;

		default:
			return CMD_ERROR;
	}

	switch (mof) {
		default: NOT_REACHED();

		case MOF_NON_STOP:
			if (v->type != VEH_TRAIN && v->type != VEH_ROAD) return CMD_ERROR;
			if (data >= ONSF_END) return CMD_ERROR;
			if (data == order->GetNonStopType()) return CMD_ERROR;
			break;

		case MOF_UNLOAD:
			if ((data & ~(OUFB_UNLOAD | OUFB_TRANSFER | OUFB_NO_UNLOAD)) != 0) return CMD_ERROR;
			/* Unload and no-unload are mutual exclusive and so are transfer and no unload. */
			if (data != 0 && ((data & (OUFB_UNLOAD | OUFB_TRANSFER)) != 0) == ((data & OUFB_NO_UNLOAD) != 0)) return CMD_ERROR;
			if (data == order->GetUnloadType()) return CMD_ERROR;
			break;

		case MOF_LOAD:
			if (data > OLFB_NO_LOAD || data == 1) return CMD_ERROR;
			if (data == order->GetLoadType()) return CMD_ERROR;
			break;

		case MOF_DEPOT_ACTION:
			if (data >= DA_END) return CMD_ERROR;
			break;

		case MOF_COND_VARIABLE:
			if (data >= OCV_END) return CMD_ERROR;
			break;

		case MOF_COND_COMPARATOR:
			if (data >= OCC_END) return CMD_ERROR;
			switch (order->GetConditionVariable()) {
				case OCV_UNCONDITIONALLY: return CMD_ERROR;

				case OCV_REQUIRES_SERVICE:
					if (data != OCC_IS_TRUE && data != OCC_IS_FALSE) return CMD_ERROR;
					break;

				default:
					if (data == OCC_IS_TRUE || data == OCC_IS_FALSE) return CMD_ERROR;
					break;
			}
			break;

		case MOF_COND_VALUE:
			switch (order->GetConditionVariable()) {
				case OCV_UNCONDITIONALLY: return CMD_ERROR;

				case OCV_LOAD_PERCENTAGE:
				case OCV_RELIABILITY:
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

	if (flags & DC_EXEC) {
		switch (mof) {
			case MOF_NON_STOP:
				order->SetNonStopType((OrderNonStopFlags)data);
				break;

			case MOF_UNLOAD:
				order->SetUnloadType((OrderUnloadFlags)data);
				if ((data & OUFB_NO_UNLOAD) != 0 && (order->GetLoadType() & OLFB_NO_LOAD) != 0) {
					order->SetLoadType((OrderLoadFlags)(order->GetLoadType() & ~OLFB_NO_LOAD));
				}
				break;

			case MOF_LOAD:
				order->SetLoadType((OrderLoadFlags)data);
				if ((data & OLFB_NO_LOAD) != 0 && (order->GetUnloadType() & OUFB_NO_UNLOAD) != 0) {
					/* No load + no unload isn't compatible */
					order->SetUnloadType((OrderUnloadFlags)(order->GetUnloadType() & ~OUFB_NO_UNLOAD));
				}
				break;

			case MOF_DEPOT_ACTION: {
				switch (data) {
					case DA_ALWAYS_GO:
						order->SetDepotOrderType((OrderDepotTypeFlags)(order->GetDepotOrderType() & ~ODTFB_SERVICE));
						order->SetDepotActionType((OrderDepotActionFlags)(order->GetDepotActionType() & ~ODATFB_HALT));
						break;

					case DA_SERVICE:
						order->SetDepotOrderType((OrderDepotTypeFlags)(order->GetDepotOrderType() | ODTFB_SERVICE));
						order->SetDepotActionType((OrderDepotActionFlags)(order->GetDepotActionType() & ~ODATFB_HALT));
						break;

					case DA_STOP:
						order->SetDepotOrderType((OrderDepotTypeFlags)(order->GetDepotOrderType() & ~ODTFB_SERVICE));
						order->SetDepotActionType((OrderDepotActionFlags)(order->GetDepotActionType() | ODATFB_HALT));
						break;

					default:
						NOT_REACHED();
				}
			} break;

			case MOF_COND_VARIABLE: {
				order->SetConditionVariable((OrderConditionVariable)data);

				OrderConditionComparator occ = order->GetConditionComparator();
				switch (order->GetConditionVariable()) {
					case OCV_UNCONDITIONALLY:
						order->SetConditionComparator(OCC_EQUALS);
						order->SetConditionValue(0);
						break;

					case OCV_REQUIRES_SERVICE:
						if (occ != OCC_IS_TRUE && occ != OCC_IS_FALSE) order->SetConditionComparator(OCC_IS_TRUE);
						break;

					case OCV_LOAD_PERCENTAGE:
					case OCV_RELIABILITY:
						if (order->GetConditionValue() > 100) order->SetConditionValue(100);
						/* FALL THROUGH */
					default:
						if (occ == OCC_IS_TRUE || occ == OCC_IS_FALSE) order->SetConditionComparator(OCC_EQUALS);
						break;
				}
			} break;

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
		for (; u != NULL; u = u->NextShared()) {
			/* Toggle u->current_order "Full load" flag if it changed.
			 * However, as the same flag is used for depot orders, check
			 * whether we are not going to a depot as there are three
			 * cases where the full load flag can be active and only
			 * one case where the flag is used for depot orders. In the
			 * other cases for the OrderTypeByte the flags are not used,
			 * so do not care and those orders should not be active
			 * when this function is called.
			 */
			if (sel_ord == u->cur_order_index &&
					(u->current_order.IsType(OT_GOTO_STATION) || u->current_order.IsType(OT_LOADING)) &&
					u->current_order.GetLoadType() != order->GetLoadType()) {
				u->current_order.SetLoadType(order->GetLoadType());
			}
			InvalidateVehicleOrder(u, 0);
		}
	}

	return CommandCost();
}

/** Clone/share/copy an order-list of an other vehicle.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0-15) - destination vehicle to clone orders to (p1 & 0xFFFF)
 * - p1 = (bit 16-31) - source vehicle to clone orders from, if any (none for CO_UNSHARE)
 * @param p2 mode of cloning: CO_SHARE, CO_COPY, or CO_UNSHARE
 */
CommandCost CmdCloneOrder(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *dst;
	VehicleID veh_src = GB(p1, 16, 16);
	VehicleID veh_dst = GB(p1,  0, 16);

	if (!IsValidVehicleID(veh_dst)) return CMD_ERROR;

	dst = GetVehicle(veh_dst);

	if (!CheckOwnership(dst->owner)) return CMD_ERROR;

	switch (p2) {
		case CO_SHARE: {
			Vehicle *src;

			if (!IsValidVehicleID(veh_src)) return CMD_ERROR;

			src = GetVehicle(veh_src);

			/* Sanity checks */
			if (!CheckOwnership(src->owner) || dst->type != src->type || dst == src)
				return CMD_ERROR;

			/* Trucks can't share orders with busses (and visa versa) */
			if (src->type == VEH_ROAD) {
				if (src->cargo_type != dst->cargo_type && (IsCargoInClass(src->cargo_type, CC_PASSENGERS) || IsCargoInClass(dst->cargo_type, CC_PASSENGERS)))
					return CMD_ERROR;
			}

			/* Is the vehicle already in the shared list? */
			if (src->FirstShared() == dst->FirstShared()) return CMD_ERROR;

			const Order *order;

			FOR_VEHICLE_ORDERS(src, order) {
				if (OrderGoesToStation(dst, order) &&
						!CanVehicleUseStation(dst, GetStation(order->GetDestination()))) {
					return_cmd_error(STR_CAN_T_COPY_SHARE_ORDER);
				}
			}

			if (flags & DC_EXEC) {
				/* If the destination vehicle had a OrderList, destroy it */
				DeleteVehicleOrders(dst);

				dst->orders.list = src->orders.list;

				/* Link this vehicle in the shared-list */
				dst->AddToShared(src);

				InvalidateVehicleOrder(dst, -1);
				InvalidateVehicleOrder(src, 0);

				InvalidateWindowClassesData(GetWindowClassForVehicleType(dst->type), 0);
			}
		} break;

		case CO_COPY: {
			Vehicle *src;
			int delta;

			if (!IsValidVehicleID(veh_src)) return CMD_ERROR;

			src = GetVehicle(veh_src);

			/* Sanity checks */
			if (!CheckOwnership(src->owner) || dst->type != src->type || dst == src)
				return CMD_ERROR;

			/* Trucks can't copy all the orders from busses (and visa versa),
			 * and neither can helicopters and aircarft. */
			const Order *order;
			FOR_VEHICLE_ORDERS(src, order) {
				if (OrderGoesToStation(dst, order) &&
						!CanVehicleUseStation(dst, GetStation(order->GetDestination()))) {
					return_cmd_error(STR_CAN_T_COPY_SHARE_ORDER);
				}
			}

			/* make sure there are orders available */
			delta = dst->IsOrderListShared() ? src->GetNumOrders() + 1 : src->GetNumOrders() - dst->GetNumOrders();
			if (!Order::CanAllocateItem(delta) ||
					((dst->orders.list == NULL || dst->IsOrderListShared()) && !OrderList::CanAllocateItem())) {
				return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);
			}

			if (flags & DC_EXEC) {
				const Order *order;
				Order *first = NULL;
				Order **order_dst;

				/* If the destination vehicle had an order list, destroy the chain but keep the OrderList */
				DeleteVehicleOrders(dst, true);

				order_dst = &first;
				FOR_VEHICLE_ORDERS(src, order) {
					*order_dst = new Order();
					(*order_dst)->AssignOrder(*order);
					order_dst = &(*order_dst)->next;
				}
				if (dst->orders.list == NULL) dst->orders.list = new OrderList(first, dst);
				else {
					assert(dst->orders.list->GetFirstOrder() == NULL);
					new (dst->orders.list) OrderList(first, dst);
				}

				InvalidateVehicleOrder(dst, -1);

				InvalidateWindowClassesData(GetWindowClassForVehicleType(dst->type), 0);
			}
		} break;

		case CO_UNSHARE: return DecloneOrder(dst, flags);
		default: return CMD_ERROR;
	}

	return CommandCost();
}

/** Add/remove refit orders from an order
 * @param tile Not used
 * @param flags operation to perform
 * @param p1 VehicleIndex of the vehicle having the order
 * @param p2 bitmask
 *   - bit 0-7 CargoID
 *   - bit 8-15 Cargo subtype
 *   - bit 16-23 number of order to modify
 */
CommandCost CmdOrderRefit(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	const Vehicle *v;
	Order *order;
	VehicleID veh = GB(p1, 0, 16);
	VehicleOrderID order_number  = GB(p2, 16, 8);
	CargoID cargo = GB(p2, 0, 8);
	byte subtype  = GB(p2, 8, 8);

	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	v = GetVehicle(veh);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	order = GetVehicleOrder(v, order_number);
	if (order == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		order->SetRefit(cargo, subtype);

		for (Vehicle *u = v->FirstShared(); u != NULL; u = u->NextShared()) {
			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u, 0);

			/* If the vehicle already got the current depot set as current order, then update current order as well */
			if (u->cur_order_index == order_number && u->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) {
				u->current_order.SetRefit(cargo, subtype);
			}
		}
	}

	return CommandCost();
}

/**
 *
 * Backup a vehicle order-list, so you can replace a vehicle
 *  without losing the order-list
 *
 */
void BackupVehicleOrders(const Vehicle *v, BackuppedOrders *bak)
{
	/* Make sure we always have freed the stuff */
	free(bak->order);
	bak->order = NULL;
	free(bak->name);
	bak->name = NULL;

	/* Save general info */
	bak->orderindex       = v->cur_order_index;
	bak->group            = v->group_id;
	bak->service_interval = v->service_interval;
	if (v->name != NULL) bak->name = strdup(v->name);

	/* If we have shared orders, store it on a special way */
	if (v->IsOrderListShared()) {
		const Vehicle *u = (v->FirstShared() == v) ? v->NextShared() : v->FirstShared();

		bak->clone = u->index;
	} else {
		/* Else copy the orders */

		/* We do not have shared orders */
		bak->clone = INVALID_VEHICLE;


		/* Count the number of orders */
		uint cnt = 0;
		const Order *order;
		FOR_VEHICLE_ORDERS(v, order) cnt++;

		/* Allocate memory for the orders plus an end-of-orders marker */
		bak->order = MallocT<Order>(cnt + 1);

		Order *dest = bak->order;

		/* Copy the orders */
		FOR_VEHICLE_ORDERS(v, order) {
			memcpy(dest, order, sizeof(Order));
			dest++;
		}
		/* End the list with an empty order */
		dest->Free();
	}
}

/**
 *
 * Restore vehicle orders that are backupped via BackupVehicleOrders
 *
 */
void RestoreVehicleOrders(const Vehicle *v, const BackuppedOrders *bak)
{
	/* If we have a custom name, process that */
	if (bak->name != NULL) DoCommandP(0, v->index, 0, CMD_RENAME_VEHICLE, NULL, bak->name);

	/* If we had shared orders, recover that */
	if (bak->clone != INVALID_VEHICLE) {
		DoCommandP(0, v->index | (bak->clone << 16), CO_SHARE, CMD_CLONE_ORDER);
	} else {

		/* CMD_NO_TEST_IF_IN_NETWORK is used here, because CMD_INSERT_ORDER checks if the
		 *  order number is one more than the current amount of orders, and because
		 *  in network the commands are queued before send, the second insert always
		 *  fails in test mode. By bypassing the test-mode, that no longer is a problem. */
		for (uint i = 0; bak->order[i].IsValid(); i++) {
			Order o = bak->order[i];
			/* Conditional orders need to have their destination to be valid on insertion. */
			if (o.IsType(OT_CONDITIONAL)) o.SetConditionSkipToOrder(0);

			if (!DoCommandP(0, v->index + (i << 16), o.Pack(),
					CMD_INSERT_ORDER | CMD_NO_TEST_IF_IN_NETWORK)) {
				break;
			}

			/* Copy timetable if enabled */
			if (_settings_game.order.timetabling && !DoCommandP(0, v->index | (i << 16) | (1 << 25),
					o.wait_time << 16 | o.travel_time,
					CMD_CHANGE_TIMETABLE | CMD_NO_TEST_IF_IN_NETWORK)) {
				break;
			}
		}

			/* Fix the conditional orders' destination. */
		for (uint i = 0; bak->order[i].IsValid(); i++) {
			if (!bak->order[i].IsType(OT_CONDITIONAL)) continue;

			if (!DoCommandP(0, v->index + (i << 16), MOF_LOAD | (bak->order[i].GetConditionSkipToOrder() << 4),
					CMD_MODIFY_ORDER | CMD_NO_TEST_IF_IN_NETWORK)) {
				break;
			}
		}
	}

	/* Restore vehicle order-index and service interval */
	DoCommandP(0, v->index, bak->orderindex | (bak->service_interval << 16) , CMD_RESTORE_ORDER_INDEX);

	/* Restore vehicle group */
	DoCommandP(0, bak->group, v->index, CMD_ADD_VEHICLE_GROUP);
}

/** Restore the current order-index of a vehicle and sets service-interval.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the ID of the vehicle
 * @param p2 various bistuffed elements
 * - p2 = (bit  0-15) - current order-index (p2 & 0xFFFF)
 * - p2 = (bit 16-31) - service interval (p2 >> 16)
 * @todo Unfortunately you cannot safely restore the unitnumber or the old vehicle
 * as far as I can see. We can store it in BackuppedOrders, and restore it, but
 * but we have no way of seeing it has been tampered with or not, as we have no
 * legit way of knowing what that ID was.@n
 * If we do want to backup/restore it, just add UnitID uid to BackuppedOrders, and
 * restore it as parameter 'y' (ugly hack I know) for example. "v->unitnumber = y;"
 */
CommandCost CmdRestoreOrderIndex(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v;
	VehicleOrderID cur_ord = GB(p2,  0, 16);
	uint16 serv_int = GB(p2, 16, 16);

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	/* Check the vehicle type and ownership, and if the service interval and order are in range */
	if (!CheckOwnership(v->owner)) return CMD_ERROR;
	if (serv_int != GetServiceIntervalClamped(serv_int) || cur_ord >= v->GetNumOrders()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->cur_order_index = cur_ord;
		v->service_interval = serv_int;
	}

	return CommandCost();
}


static TileIndex GetStationTileForVehicle(const Vehicle *v, const Station *st)
{
	if (!CanVehicleUseStation(v, st)) return INVALID_TILE;

	switch (v->type) {
		default: NOT_REACHED();
		case VEH_TRAIN:     return st->train_tile;
		case VEH_AIRCRAFT:  return st->airport_tile;
		case VEH_SHIP:      return st->dock_tile;
		case VEH_ROAD:      return st->GetPrimaryRoadStop(v)->xy;
	}
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
	if (v->vehstatus & VS_CRASHED) return;

	/* Do nothing for stopped vehicles if setting is '1' */
	if (_settings_client.gui.order_review_system == 1 && v->vehstatus & VS_STOPPED)
		return;

	/* do nothing we we're not the first vehicle in a share-chain */
	if (v->FirstShared() != v) return;

	/* Only check every 20 days, so that we don't flood the message log */
	if (v->owner == _local_company && v->day_counter % 20 == 0) {
		int n_st, problem_type = -1;
		const Order *order;
		int message = 0;

		/* Check the order list */
		n_st = 0;

		FOR_VEHICLE_ORDERS(v, order) {
			/* Dummy order? */
			if (order->IsType(OT_DUMMY)) {
				problem_type = 1;
				break;
			}
			/* Does station have a load-bay for this vehicle? */
			if (order->IsType(OT_GOTO_STATION)) {
				const Station *st = GetStation(order->GetDestination());
				TileIndex required_tile = GetStationTileForVehicle(v, st);

				n_st++;
				if (required_tile == INVALID_TILE) problem_type = 3;
			}
		}

		/* Check if the last and the first order are the same */
		if (v->GetNumOrders() > 1) {
			const Order *last = GetLastVehicleOrder(v);

			if (v->orders.list->GetFirstOrder()->Equals(*last)) {
				problem_type = 2;
			}
		}

		/* Do we only have 1 station in our order list? */
		if (n_st < 2 && problem_type == -1) problem_type = 0;

#ifndef NDEBUG
		if (v->orders.list != NULL) v->orders.list->DebugCheckSanity();
#endif

		/* We don't have a problem */
		if (problem_type < 0) return;

		message = STR_VEHICLE_HAS_TOO_FEW_ORDERS + problem_type;
		//DEBUG(misc, 3, "Triggered News Item for vehicle %d", v->index);

		SetDParam(0, v->index);
		AddNewsItem(
			message,
			NS_ADVICE,
			v->index,
			0
		);
	}
}

/**
 * Removes an order from all vehicles. Triggers when, say, a station is removed.
 * @param type The type of the order (OT_GOTO_[STATION|DEPOT|WAYPOINT]).
 * @param destination The destination. Can be a StationID, DepotID or WaypointID.
 */
void RemoveOrderFromAllVehicles(OrderType type, DestinationID destination)
{
	Vehicle *v;

	/* Aircraft have StationIDs for depot orders and never use DepotIDs
	 * This fact is handled specially below
	 */

	/* Go through all vehicles */
	FOR_ALL_VEHICLES(v) {
		Order *order;

		/* Forget about this station if this station is removed */
		if (v->last_station_visited == destination && type == OT_GOTO_STATION) {
			v->last_station_visited = INVALID_STATION;
		}

		order = &v->current_order;
		if ((v->type == VEH_AIRCRAFT && order->IsType(OT_GOTO_DEPOT) ? OT_GOTO_STATION : order->GetType()) == type &&
				v->current_order.GetDestination() == destination) {
			order->MakeDummy();
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}

		/* Clear the order from the order-list */
		int id = -1;
		FOR_VEHICLE_ORDERS(v, order) {
			id++;
			if (order->IsType(OT_GOTO_DEPOT) && (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) != 0) continue;
			if ((v->type == VEH_AIRCRAFT && order->IsType(OT_GOTO_DEPOT) ? OT_GOTO_STATION : order->GetType()) == type &&
					order->GetDestination() == destination) {
				order->MakeDummy();
				for (const Vehicle *w = v->FirstShared(); w != NULL; w = w->NextShared()) {
					/* In GUI, simulate by removing the order and adding it back */
					InvalidateVehicleOrder(w, id | (INVALID_VEH_ORDER_ID << 8));
					InvalidateVehicleOrder(w, (INVALID_VEH_ORDER_ID << 8) | id);
				}
			}
		}
	}
}

/**
 *
 * Checks if a vehicle has a GOTO_DEPOT in his order list
 *
 * @return True if this is true (lol ;))
 *
 */
bool VehicleHasDepotOrders(const Vehicle *v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order) {
		if (order->IsType(OT_GOTO_DEPOT))
			return true;
	}

	return false;
}

/**
 *
 * Delete all orders from a vehicle
 *
 */
void DeleteVehicleOrders(Vehicle *v, bool keep_orderlist)
{
	DeleteOrderWarnings(v);

	if (v->IsOrderListShared()) {
		/* Remove ourself from the shared order list. */
		v->RemoveFromShared();
		v->orders.list = NULL;
	} else if (v->orders.list != NULL) {
		/* Remove the orders */
		v->orders.list->FreeChain(keep_orderlist);
		if (!keep_orderlist) v->orders.list = NULL;
	}
}

Date GetServiceIntervalClamped(uint index)
{
	return (_settings_game.vehicle.servint_ispercent) ? Clamp(index, MIN_SERVINT_PERCENT, MAX_SERVINT_PERCENT) : Clamp(index, MIN_SERVINT_DAYS, MAX_SERVINT_DAYS);
}

/**
 *
 * Check if a vehicle has any valid orders
 *
 * @return false if there are no valid orders
 *
 */
static bool CheckForValidOrders(const Vehicle *v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order) if (!order->IsType(OT_DUMMY)) return true;

	return false;
}

/**
 * Compare the variable and value based on the given comparator.
 */
static bool OrderConditionCompare(OrderConditionComparator occ, int variable, int value)
{
	switch (occ) {
		case OCC_EQUALS:      return variable == value;
		case OCC_NOT_EQUALS:  return variable != value;
		case OCC_LESS_THAN:   return variable <  value;
		case OCC_LESS_EQUALS: return variable <= value;
		case OCC_MORE_THAN:   return variable >  value;
		case OCC_MORE_EQUALS: return variable >= value;
		case OCC_IS_TRUE:     return variable != 0;
		case OCC_IS_FALSE:    return variable == 0;
		default: NOT_REACHED();
	}
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
	uint16 value = order->GetConditionValue();

	switch (order->GetConditionVariable()) {
		case OCV_LOAD_PERCENTAGE:  skip_order = OrderConditionCompare(occ, CalcPercentVehicleFilled(v, NULL), value); break;
		case OCV_RELIABILITY:      skip_order = OrderConditionCompare(occ, v->reliability * 100 >> 16,        value); break;
		case OCV_MAX_SPEED:        skip_order = OrderConditionCompare(occ, v->GetDisplayMaxSpeed() * 10 / 16, value); break;
		case OCV_AGE:              skip_order = OrderConditionCompare(occ, v->age / DAYS_IN_LEAP_YEAR,        value); break;
		case OCV_REQUIRES_SERVICE: skip_order = OrderConditionCompare(occ, v->NeedsServicing(),               value); break;
		case OCV_UNCONDITIONALLY:  skip_order = true; break;
		default: NOT_REACHED();
	}

	return skip_order ? order->GetConditionSkipToOrder() : (VehicleOrderID)INVALID_VEH_ORDER_ID;
}

/**
 * Update the vehicle's destination tile from an order.
 * @param order the order the vehicle currently has
 * @param v the vehicle to update
 */
bool UpdateOrderDest(Vehicle *v, const Order *order, int conditional_depth)
{
	switch (order->GetType()) {
		case OT_GOTO_STATION:
			v->dest_tile = v->GetOrderStationLocation(order->GetDestination());
			break;

		case OT_GOTO_DEPOT:
			if (v->current_order.GetDepotActionType() & ODATFB_NEAREST_DEPOT) {
				/* We need to search for the nearest depot (hangar). */
				TileIndex location;
				DestinationID destination;
				bool reverse;

				if (v->FindClosestDepot(&location, &destination, &reverse)) {
					v->dest_tile = location;
					v->current_order.MakeGoToDepot(destination, v->current_order.GetDepotOrderType(), (OrderDepotActionFlags)(v->current_order.GetDepotActionType() & ~ODATFB_NEAREST_DEPOT), v->current_order.GetRefitCargo(), v->current_order.GetRefitSubtype());

					/* If there is no depot in front, reverse automatically (trains only) */
					if (v->type == VEH_TRAIN && reverse) DoCommand(v->tile, v->index, 0, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);

					if (v->type == VEH_AIRCRAFT && v->u.air.state == FLYING && v->u.air.targetairport != destination) {
						/* The aircraft is now heading for a different hangar than the next in the orders */
						extern void AircraftNextAirportPos_and_Order(Vehicle *v);
						AircraftNextAirportPos_and_Order(v);
					}
				} else {
					UpdateVehicleTimetable(v, true);
					v->cur_order_index++;
				}
			} else if (v->type != VEH_AIRCRAFT) {
				v->dest_tile = GetDepot(order->GetDestination())->xy;
			}
			break;

		case OT_GOTO_WAYPOINT:
			v->dest_tile = GetWaypoint(order->GetDestination())->xy;
			break;

		case OT_CONDITIONAL: {
			if (conditional_depth > v->GetNumOrders()) return false;

			VehicleOrderID next_order = ProcessConditionalOrder(order, v);
			if (next_order != INVALID_VEH_ORDER_ID) {
				UpdateVehicleTimetable(v, false);
				v->cur_order_index = next_order;
				v->current_order_time += GetVehicleOrder(v, next_order)->travel_time;
			} else {
				UpdateVehicleTimetable(v, true);
				v->cur_order_index++;
			}

			/* Get the current order */
			if (v->cur_order_index >= v->GetNumOrders()) v->cur_order_index = 0;

			const Order *order = GetVehicleOrder(v, v->cur_order_index);
			v->current_order = *order;
			return UpdateOrderDest(v, order, conditional_depth + 1);
		}

		default:
			v->dest_tile = 0;
			return false;
	}
	return true;
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
			if (!(v->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS)) return false;

			if ((v->current_order.GetDepotOrderType() & ODTFB_SERVICE) && !v->NeedsServicing()) {
				UpdateVehicleTimetable(v, true);
				v->cur_order_index++;
			}
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

	/* Check if we've reached the waypoint? */
	if (v->current_order.IsType(OT_GOTO_WAYPOINT) && v->tile == v->dest_tile) {
		UpdateVehicleTimetable(v, true);
		v->cur_order_index++;
	}

	/* Check if we've reached a non-stop station.. */
	if (v->current_order.IsType(OT_GOTO_STATION) && (v->current_order.GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) &&
			IsTileType(v->tile, MP_STATION) &&
			v->current_order.GetDestination() == GetStationIndex(v->tile)) {
		v->last_station_visited = v->current_order.GetDestination();
		UpdateVehicleTimetable(v, true);
		v->cur_order_index++;
	}

	/* Get the current order */
	if (v->cur_order_index >= v->GetNumOrders()) v->cur_order_index = 0;

	const Order *order = GetVehicleOrder(v, v->cur_order_index);

	/* If no order, do nothing. */
	if (order == NULL || (v->type == VEH_AIRCRAFT && order->IsType(OT_DUMMY) && !CheckForValidOrders(v))) {
		if (v->type == VEH_AIRCRAFT) {
			/* Aircraft do something vastly different here, so handle separately */
			extern void HandleMissingAircraftOrders(Vehicle *v);
			HandleMissingAircraftOrders(v);
			return false;
		}

		v->current_order.Free();
		v->dest_tile = 0;
		if (v->type == VEH_ROAD) ClearSlot(v);
		return false;
	}

	/* If it is unchanged, keep it. */
	if (order->Equals(v->current_order) && (v->type == VEH_AIRCRAFT || v->dest_tile != 0) &&
			(v->type != VEH_SHIP || !order->IsType(OT_GOTO_STATION) || GetStation(order->GetDestination())->dock_tile != INVALID_TILE)) {
		return false;
	}

	/* Otherwise set it, and determine the destination tile. */
	v->current_order = *order;

	InvalidateVehicleOrder(v, 0);
	switch (v->type) {
		default:
			NOT_REACHED();

		case VEH_ROAD:
		case VEH_TRAIN:
			break;

		case VEH_AIRCRAFT:
		case VEH_SHIP:
			InvalidateWindowClasses(GetWindowClassForVehicleType(v->type));
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
	return
			(!this->IsType(OT_GOTO_DEPOT) || (this->GetDepotOrderType() & ODTFB_PART_OF_ORDERS) != 0) &&
			v->last_station_visited != station && // Do stop only when we've not just been there
			/* Finally do stop when there is no non-stop flag set for this type of station. */
			!(this->GetNonStopType() & (is_dest_station ? ONSF_NO_STOP_AT_DESTINATION_STATION : ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS));
}

void InitializeOrders()
{
	_Order_pool.CleanPool();
	_Order_pool.AddBlockToPool();

	_OrderList_pool.CleanPool();
	_OrderList_pool.AddBlockToPool();

	_backup_orders_tile = 0;
}
