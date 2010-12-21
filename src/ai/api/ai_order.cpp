/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_order.cpp Implementation of AIOrder. */

#include "ai_vehicle.hpp"
#include "../ai_instance.hpp"
#include "../../debug.h"
#include "../../vehicle_base.h"
#include "../../roadstop_base.h"
#include "../../depot_base.h"
#include "../../station_base.h"
#include "../../waypoint_base.h"

/**
 * Gets the order type given a tile
 * @param t the tile to get the order from
 * @return the order type, or OT_END when there is no order
 */
static OrderType GetOrderTypeByTile(TileIndex t)
{
	if (!::IsValidTile(t)) return OT_END;

	switch (::GetTileType(t)) {
		default: break;
		case MP_STATION:
			if (IsBuoy(t) || IsRailWaypoint(t)) return OT_GOTO_WAYPOINT;
			if (IsHangar(t)) return OT_GOTO_DEPOT;
			return OT_GOTO_STATION;

		case MP_WATER:   if (::IsShipDepot(t)) return OT_GOTO_DEPOT; break;
		case MP_ROAD:    if (::GetRoadTileType(t) == ROAD_TILE_DEPOT) return OT_GOTO_DEPOT; break;
		case MP_RAILWAY:
			if (IsRailDepot(t)) return OT_GOTO_DEPOT;
			break;
	}

	return OT_END;
}

/* static */ bool AIOrder::IsValidVehicleOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	return AIVehicle::IsValidVehicle(vehicle_id) && order_position >= 0 && (order_position < ::Vehicle::Get(vehicle_id)->GetNumOrders() || order_position == ORDER_CURRENT);
}

/**
 * Get the current order the vehicle is executing. If the current order is in
 *  the order list, return the order from the orderlist. If the current order
 *  was a manual order, return the current order.
 */
static const Order *ResolveOrder(VehicleID vehicle_id, AIOrder::OrderPosition order_position)
{
	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	if (order_position == AIOrder::ORDER_CURRENT) {
		const Order *order = &v->current_order;
		if (order->GetType() == OT_GOTO_DEPOT && !(order->GetDepotOrderType() & ODTFB_PART_OF_ORDERS)) return order;
		order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);
		if (order_position == AIOrder::ORDER_INVALID) return NULL;
	}
	return v->GetOrder(order_position);
}

/* static */ bool AIOrder::IsGotoStationOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return false;

	const Order *order = ::ResolveOrder(vehicle_id, order_position);
	return order != NULL && order->GetType() == OT_GOTO_STATION;
}

/* static */ bool AIOrder::IsGotoDepotOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return false;

	const Order *order = ::ResolveOrder(vehicle_id, order_position);
	return order != NULL && order->GetType() == OT_GOTO_DEPOT;
}

/* static */ bool AIOrder::IsGotoWaypointOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return false;

	const Order *order = ::ResolveOrder(vehicle_id, order_position);
	return order != NULL && order->GetType() == OT_GOTO_WAYPOINT;
}

/* static */ bool AIOrder::IsConditionalOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	if (order_position == ORDER_CURRENT) return false;
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return false;

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);
	return order->GetType() == OT_CONDITIONAL;
}

/* static */ bool AIOrder::IsVoidOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	if (order_position == ORDER_CURRENT) return false;
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return false;

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);
	return order->GetType() == OT_DUMMY;
}

/* static */ bool AIOrder::IsCurrentOrderPartOfOrderList(VehicleID vehicle_id)
{
	if (AIVehicle::IsValidVehicle(vehicle_id)) return false;
	if (GetOrderCount(vehicle_id) == 0) return false;

	const Order *order = &::Vehicle::Get(vehicle_id)->current_order;
	if (order->GetType() != OT_GOTO_DEPOT) return true;
	return (order->GetDepotOrderType() & ODTFB_PART_OF_ORDERS) != 0;
}

/* static */ AIOrder::OrderPosition AIOrder::ResolveOrderPosition(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!AIVehicle::IsValidVehicle(vehicle_id)) return ORDER_INVALID;

	if (order_position == ORDER_CURRENT) return (AIOrder::OrderPosition)::Vehicle::Get(vehicle_id)->cur_order_index;
	return (order_position >= 0 && order_position < ::Vehicle::Get(vehicle_id)->GetNumOrders()) ? order_position : ORDER_INVALID;
}


/* static */ bool AIOrder::AreOrderFlagsValid(TileIndex destination, AIOrderFlags order_flags)
{
	OrderType ot = (order_flags & AIOF_GOTO_NEAREST_DEPOT) ? OT_GOTO_DEPOT : ::GetOrderTypeByTile(destination);
	switch (ot) {
		case OT_GOTO_STATION:
			return (order_flags & ~(AIOF_NON_STOP_FLAGS | AIOF_UNLOAD_FLAGS | AIOF_LOAD_FLAGS)) == 0 &&
					/* Test the different mutual exclusive flags. */
					((order_flags & AIOF_TRANSFER)      == 0 || (order_flags & AIOF_UNLOAD)    == 0) &&
					((order_flags & AIOF_TRANSFER)      == 0 || (order_flags & AIOF_NO_UNLOAD) == 0) &&
					((order_flags & AIOF_UNLOAD)        == 0 || (order_flags & AIOF_NO_UNLOAD) == 0) &&
					((order_flags & AIOF_UNLOAD)        == 0 || (order_flags & AIOF_NO_UNLOAD) == 0) &&
					((order_flags & AIOF_NO_UNLOAD)     == 0 || (order_flags & AIOF_NO_LOAD)   == 0) &&
					((order_flags & AIOF_FULL_LOAD_ANY) == 0 || (order_flags & AIOF_NO_LOAD)   == 0);

		case OT_GOTO_DEPOT:
			return (order_flags & ~(AIOF_NON_STOP_FLAGS | AIOF_DEPOT_FLAGS)) == 0 &&
					((order_flags & AIOF_SERVICE_IF_NEEDED) == 0 || (order_flags & AIOF_STOP_IN_DEPOT) == 0);

		case OT_GOTO_WAYPOINT: return (order_flags & ~(AIOF_NON_STOP_FLAGS)) == 0;
		default:               return false;
	}
}

/* static */ bool AIOrder::IsValidConditionalOrder(OrderCondition condition, CompareFunction compare)
{
	switch (condition) {
		case OC_LOAD_PERCENTAGE:
		case OC_RELIABILITY:
		case OC_MAX_SPEED:
		case OC_AGE:
			return compare >= CF_EQUALS && compare <= CF_MORE_EQUALS;

		case OC_REQUIRES_SERVICE:
			return compare == CF_IS_TRUE || compare == CF_IS_FALSE;

		case OC_UNCONDITIONALLY:
			return true;

		default: return false;
	}
}

/* static */ int32 AIOrder::GetOrderCount(VehicleID vehicle_id)
{
	return AIVehicle::IsValidVehicle(vehicle_id) ? ::Vehicle::Get(vehicle_id)->GetNumOrders() : -1;
}

/* static */ TileIndex AIOrder::GetOrderDestination(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return INVALID_TILE;

	const Order *order = ::ResolveOrder(vehicle_id, order_position);
	if (order == NULL || order->GetType() == OT_CONDITIONAL) return INVALID_TILE;
	const Vehicle *v = ::Vehicle::Get(vehicle_id);

	switch (order->GetType()) {
		case OT_GOTO_DEPOT: {
			/* We don't know where the nearest depot is... (yet) */
			if (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) return INVALID_TILE;

			if (v->type != VEH_AIRCRAFT) return ::Depot::Get(order->GetDestination())->xy;
			/* Aircraft's hangars are referenced by StationID, not DepotID */
			const Station *st = ::Station::Get(order->GetDestination());
			if (!st->airport.HasHangar()) return INVALID_TILE;
			return st->airport.GetHangarTile(0);
		}

		case OT_GOTO_STATION: {
			const Station *st = ::Station::Get(order->GetDestination());
			if (st->train_station.tile != INVALID_TILE) {
				TILE_AREA_LOOP(t, st->train_station) {
					if (st->TileBelongsToRailStation(t)) return t;
				}
			} else if (st->dock_tile != INVALID_TILE) {
				return st->dock_tile;
			} else if (st->bus_stops != NULL) {
				return st->bus_stops->xy;
			} else if (st->truck_stops != NULL) {
				return st->truck_stops->xy;
			} else if (st->airport.tile != INVALID_TILE) {
				TILE_AREA_LOOP(tile, st->airport) {
					if (st->TileBelongsToAirport(tile) && !::IsHangar(tile)) return tile;
				}
			}
			return INVALID_TILE;
		}

		case OT_GOTO_WAYPOINT: {
			const Waypoint *wp = ::Waypoint::Get(order->GetDestination());
			if (wp->train_station.tile != INVALID_TILE) {
				TILE_AREA_LOOP(t, wp->train_station) {
					if (wp->TileBelongsToRailStation(t)) return t;
				}
			}
			/* If the waypoint has no rail waypoint tiles, it must have a buoy */
			return wp->xy;
		}
		default:               return INVALID_TILE;
	}
}

/* static */ AIOrder::AIOrderFlags AIOrder::GetOrderFlags(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return AIOF_INVALID;

	const Order *order = ::ResolveOrder(vehicle_id, order_position);
	if (order == NULL || order->GetType() == OT_CONDITIONAL || order->GetType() == OT_DUMMY) return AIOF_INVALID;

	AIOrderFlags order_flags = AIOF_NONE;
	order_flags |= (AIOrderFlags)order->GetNonStopType();
	switch (order->GetType()) {
		case OT_GOTO_DEPOT:
			if (order->GetDepotOrderType() & ODTFB_SERVICE) order_flags |= AIOF_SERVICE_IF_NEEDED;
			if (order->GetDepotActionType() & ODATFB_HALT) order_flags |= AIOF_STOP_IN_DEPOT;
			if (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) order_flags |= AIOF_GOTO_NEAREST_DEPOT;
			break;

		case OT_GOTO_STATION:
			order_flags |= (AIOrderFlags)(order->GetLoadType()   << 5);
			order_flags |= (AIOrderFlags)(order->GetUnloadType() << 2);
			break;

		default: break;
	}

	return order_flags;
}

/* static */ AIOrder::OrderPosition AIOrder::GetOrderJumpTo(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return ORDER_INVALID;
	if (order_position == ORDER_CURRENT || !IsConditionalOrder(vehicle_id, order_position)) return ORDER_INVALID;

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);
	return (OrderPosition)order->GetConditionSkipToOrder();
}

/* static */ AIOrder::OrderCondition AIOrder::GetOrderCondition(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return OC_INVALID;
	if (order_position == ORDER_CURRENT || !IsConditionalOrder(vehicle_id, order_position)) return OC_INVALID;

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);
	return (OrderCondition)order->GetConditionVariable();
}

/* static */ AIOrder::CompareFunction AIOrder::GetOrderCompareFunction(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return CF_INVALID;
	if (order_position == ORDER_CURRENT || !IsConditionalOrder(vehicle_id, order_position)) return CF_INVALID;

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);
	return (CompareFunction)order->GetConditionComparator();
}

/* static */ int32 AIOrder::GetOrderCompareValue(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return -1;
	if (order_position == ORDER_CURRENT || !IsConditionalOrder(vehicle_id, order_position)) return -1;

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);
	int32 value = order->GetConditionValue();
	if (order->GetConditionVariable() == OCV_MAX_SPEED) value = value * 16 / 10;
	return value;
}

/* static */ AIOrder::StopLocation AIOrder::GetStopLocation(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return STOPLOCATION_INVALID;
	if (AIVehicle::GetVehicleType(vehicle_id) != AIVehicle::VT_RAIL) return STOPLOCATION_INVALID;
	if (!IsGotoStationOrder(vehicle_id, order_position)) return STOPLOCATION_INVALID;

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);
	return (AIOrder::StopLocation)order->GetStopLocation();
}

/* static */ bool AIOrder::SetOrderJumpTo(VehicleID vehicle_id, OrderPosition order_position, OrderPosition jump_to)
{
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));
	EnforcePrecondition(false, order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position));
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, jump_to) && jump_to != ORDER_CURRENT);

	return AIObject::DoCommand(0, vehicle_id | (order_position << 20), MOF_COND_DESTINATION | (jump_to << 4), CMD_MODIFY_ORDER);
}

/* static */ bool AIOrder::SetOrderCondition(VehicleID vehicle_id, OrderPosition order_position, OrderCondition condition)
{
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));
	EnforcePrecondition(false, order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position));
	EnforcePrecondition(false, condition >= OC_LOAD_PERCENTAGE && condition <= OC_UNCONDITIONALLY);

	return AIObject::DoCommand(0, vehicle_id | (order_position << 20), MOF_COND_VARIABLE | (condition << 4), CMD_MODIFY_ORDER);
}

/* static */ bool AIOrder::SetOrderCompareFunction(VehicleID vehicle_id, OrderPosition order_position, CompareFunction compare)
{
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));
	EnforcePrecondition(false, order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position));
	EnforcePrecondition(false, compare >= CF_EQUALS && compare <= CF_IS_FALSE);

	return AIObject::DoCommand(0, vehicle_id | (order_position << 20), MOF_COND_COMPARATOR | (compare << 4), CMD_MODIFY_ORDER);
}

/* static */ bool AIOrder::SetOrderCompareValue(VehicleID vehicle_id, OrderPosition order_position, int32 value)
{
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));
	EnforcePrecondition(false, order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position));
	EnforcePrecondition(false, value >= 0 && value < 2048);
	if (GetOrderCondition(vehicle_id, order_position) == OC_MAX_SPEED) value = value * 10 / 16;

	return AIObject::DoCommand(0, vehicle_id | (order_position << 20), MOF_COND_VALUE | (value << 4), CMD_MODIFY_ORDER);
}

/* static */ bool AIOrder::SetStopLocation(VehicleID vehicle_id, OrderPosition order_position, StopLocation stop_location)
{
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));
	EnforcePrecondition(false, AIVehicle::GetVehicleType(vehicle_id) == AIVehicle::VT_RAIL);
	EnforcePrecondition(false, IsGotoStationOrder(vehicle_id, order_position));
	EnforcePrecondition(false, stop_location >= STOPLOCATION_NEAR && stop_location <= STOPLOCATION_FAR);

	uint32 p1 = vehicle_id | (order_position << 20);
	uint32 p2 = MOF_STOP_LOCATION | (stop_location << 4);
	return AIObject::DoCommand(0, p1, p2, CMD_MODIFY_ORDER);
}

/* static */ bool AIOrder::AppendOrder(VehicleID vehicle_id, TileIndex destination, AIOrderFlags order_flags)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, AreOrderFlagsValid(destination, order_flags));

	return InsertOrder(vehicle_id, (AIOrder::OrderPosition)::Vehicle::Get(vehicle_id)->GetNumOrders(), destination, order_flags);
}

/* static */ bool AIOrder::AppendConditionalOrder(VehicleID vehicle_id, OrderPosition jump_to)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, jump_to));

	return InsertConditionalOrder(vehicle_id, (AIOrder::OrderPosition)::Vehicle::Get(vehicle_id)->GetNumOrders(), jump_to);
}

/* static */ bool AIOrder::InsertOrder(VehicleID vehicle_id, OrderPosition order_position, TileIndex destination, AIOrder::AIOrderFlags order_flags)
{
	/* IsValidVehicleOrder is not good enough because it does not allow appending. */
	if (order_position == ORDER_CURRENT) order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);

	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, order_position >= 0 && order_position <= ::Vehicle::Get(vehicle_id)->GetNumOrders());
	EnforcePrecondition(false, AreOrderFlagsValid(destination, order_flags));

	Order order;
	OrderType ot = (order_flags & AIOF_GOTO_NEAREST_DEPOT) ? OT_GOTO_DEPOT : ::GetOrderTypeByTile(destination);
	switch (ot) {
		case OT_GOTO_DEPOT: {
			OrderDepotTypeFlags odtf = (OrderDepotTypeFlags)(ODTFB_PART_OF_ORDERS | ((order_flags & AIOF_SERVICE_IF_NEEDED) ? ODTFB_SERVICE : 0));
			OrderDepotActionFlags odaf = (OrderDepotActionFlags)(ODATF_SERVICE_ONLY | ((order_flags & AIOF_STOP_IN_DEPOT) ? ODATFB_HALT : 0));
			if (order_flags & AIOF_GOTO_NEAREST_DEPOT) odaf |= ODATFB_NEAREST_DEPOT;
			OrderNonStopFlags onsf = (OrderNonStopFlags)((order_flags & AIOF_NON_STOP_INTERMEDIATE) ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
			if (order_flags & AIOF_GOTO_NEAREST_DEPOT) {
				order.MakeGoToDepot(0, odtf, onsf, odaf);
			} else {
				/* Check explicitly if the order is to a station (for aircraft) or
				 * to a depot (other vehicle types). */
				if (::Vehicle::Get(vehicle_id)->type == VEH_AIRCRAFT) {
					if (!::IsTileType(destination, MP_STATION)) return false;
					order.MakeGoToDepot(::GetStationIndex(destination), odtf, onsf, odaf);
				} else {
					if (::IsTileType(destination, MP_STATION)) return false;
					order.MakeGoToDepot(::GetDepotIndex(destination), odtf, onsf, odaf);
				}
			}
			break;
		}

		case OT_GOTO_STATION:
			order.MakeGoToStation(::GetStationIndex(destination));
			order.SetLoadType((OrderLoadFlags)GB(order_flags, 5, 3));
			order.SetUnloadType((OrderUnloadFlags)GB(order_flags, 2, 3));
			order.SetStopLocation(OSL_PLATFORM_FAR_END);
			break;

		case OT_GOTO_WAYPOINT:
			order.MakeGoToWaypoint(::GetStationIndex(destination));
			break;

		default:
			return false;
	}

	order.SetNonStopType((OrderNonStopFlags)GB(order_flags, 0, 2));

	return AIObject::DoCommand(0, vehicle_id | (order_position << 20), order.Pack(), CMD_INSERT_ORDER);
}

/* static */ bool AIOrder::InsertConditionalOrder(VehicleID vehicle_id, OrderPosition order_position, OrderPosition jump_to)
{
	/* IsValidVehicleOrder is not good enough because it does not allow appending. */
	if (order_position == ORDER_CURRENT) order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);

	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, jump_to));

	Order order;
	order.MakeConditional(jump_to);

	return AIObject::DoCommand(0, vehicle_id | (order_position << 20), order.Pack(), CMD_INSERT_ORDER);
}

/* static */ bool AIOrder::RemoveOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);

	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));

	return AIObject::DoCommand(0, vehicle_id, order_position, CMD_DELETE_ORDER);
}

/* static */ bool AIOrder::SkipToOrder(VehicleID vehicle_id, OrderPosition next_order)
{
	next_order = AIOrder::ResolveOrderPosition(vehicle_id, next_order);

	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, next_order));

	return AIObject::DoCommand(0, vehicle_id, next_order, CMD_SKIP_TO_ORDER);
}

/**
 * Callback handler as SetOrderFlags possibly needs multiple DoCommand calls
 * to be able to set all order flags correctly. As we need to wait till the
 * command has completed before we know the next bits to change we need to
 * call the function multiple times. Each time it'll reduce the difference
 * between the wanted and the current order.
 * @param instance The AI we are doing the callback for.
 */
static void _DoCommandReturnSetOrderFlags(class AIInstance *instance)
{
	AIObject::SetLastCommandRes(AIOrder::_SetOrderFlags());
	AIInstance::DoCommandReturn(instance);
}

/* static */ bool AIOrder::_SetOrderFlags()
{
	/* Make sure we don't go into an infinite loop */
	int retry = AIObject::GetCallbackVariable(3) - 1;
	if (retry < 0) {
		DEBUG(ai, 0, "Possible infinite loop in SetOrderFlags() detected");
		return false;
	}
	AIObject::SetCallbackVariable(3, retry);

	VehicleID vehicle_id = (VehicleID)AIObject::GetCallbackVariable(0);
	OrderPosition order_position = (OrderPosition)AIObject::GetCallbackVariable(1);
	AIOrderFlags order_flags = (AIOrderFlags)AIObject::GetCallbackVariable(2);

	order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);

	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));
	EnforcePrecondition(false, AreOrderFlagsValid(GetOrderDestination(vehicle_id, order_position), order_flags));

	const Order *order = Vehicle::Get(vehicle_id)->GetOrder(order_position);

	AIOrderFlags current = GetOrderFlags(vehicle_id, order_position);

	if ((current & AIOF_NON_STOP_FLAGS) != (order_flags & AIOF_NON_STOP_FLAGS)) {
		return AIObject::DoCommand(0, vehicle_id | (order_position << 20), (order_flags & AIOF_NON_STOP_FLAGS) << 4 | MOF_NON_STOP, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnSetOrderFlags);
	}

	switch (order->GetType()) {
		case OT_GOTO_DEPOT:
			if ((current & AIOF_DEPOT_FLAGS) != (order_flags & AIOF_DEPOT_FLAGS)) {
				uint data = DA_ALWAYS_GO;
				if (order_flags & AIOF_SERVICE_IF_NEEDED) data = DA_SERVICE;
				if (order_flags & AIOF_STOP_IN_DEPOT) data = DA_STOP;
				return AIObject::DoCommand(0, vehicle_id | (order_position << 20), (data << 4) | MOF_DEPOT_ACTION, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnSetOrderFlags);
			}
			break;

		case OT_GOTO_STATION:
			if ((current & AIOF_UNLOAD_FLAGS) != (order_flags & AIOF_UNLOAD_FLAGS)) {
				return AIObject::DoCommand(0, vehicle_id | (order_position << 20), (order_flags & AIOF_UNLOAD_FLAGS) << 2 | MOF_UNLOAD, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnSetOrderFlags);
			}
			if ((current & AIOF_LOAD_FLAGS) != (order_flags & AIOF_LOAD_FLAGS)) {
				return AIObject::DoCommand(0, vehicle_id | (order_position << 20), (order_flags & AIOF_LOAD_FLAGS) >> 1 | MOF_LOAD, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnSetOrderFlags);
			}
			break;

		default: break;
	}

	assert(GetOrderFlags(vehicle_id, order_position) == order_flags);

	return true;
}

/* static */ bool AIOrder::SetOrderFlags(VehicleID vehicle_id, OrderPosition order_position, AIOrder::AIOrderFlags order_flags)
{
	AIObject::SetCallbackVariable(0, vehicle_id);
	AIObject::SetCallbackVariable(1, order_position);
	AIObject::SetCallbackVariable(2, order_flags);
	/* In case another client(s) change orders at the same time we could
	 * end in an infinite loop. This stops that from happening ever. */
	AIObject::SetCallbackVariable(3, 8);
	return AIOrder::_SetOrderFlags();
}

/* static */ bool AIOrder::MoveOrder(VehicleID vehicle_id, OrderPosition order_position_move, OrderPosition order_position_target)
{
	order_position_move   = AIOrder::ResolveOrderPosition(vehicle_id, order_position_move);
	order_position_target = AIOrder::ResolveOrderPosition(vehicle_id, order_position_target);

	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position_move));
	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position_target));

	return AIObject::DoCommand(0, vehicle_id, order_position_move | (order_position_target << 16), CMD_MOVE_ORDER);
}

/* static */ bool AIOrder::CopyOrders(VehicleID vehicle_id, VehicleID main_vehicle_id)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(main_vehicle_id));

	return AIObject::DoCommand(0, vehicle_id | CO_COPY << 30, main_vehicle_id, CMD_CLONE_ORDER);
}

/* static */ bool AIOrder::ShareOrders(VehicleID vehicle_id, VehicleID main_vehicle_id)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(main_vehicle_id));

	return AIObject::DoCommand(0, vehicle_id | CO_SHARE << 30, main_vehicle_id, CMD_CLONE_ORDER);
}

/* static */ bool AIOrder::UnshareOrders(VehicleID vehicle_id)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));

	return AIObject::DoCommand(0, vehicle_id | CO_UNSHARE << 30, 0, CMD_CLONE_ORDER);
}
