/* $Id$ */

/** @file ai_order.cpp Implementation of AIOrder. */

#include "ai_order.hpp"
#include "ai_map.hpp"
#include "ai_vehicle.hpp"
#include "../ai_instance.hpp"
#include "../../openttd.h"
#include "../../debug.h"
#include "../../vehicle_base.h"
#include "../../depot_base.h"
#include "../../landscape.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../station_map.h"
#include "../../water_map.h"
#include "../../waypoint.h"

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
		case MP_STATION: return OT_GOTO_STATION; break;
		case MP_WATER:   if (::IsShipDepot(t)) return OT_GOTO_DEPOT; break;
		case MP_ROAD:    if (::GetRoadTileType(t) == ROAD_TILE_DEPOT) return OT_GOTO_DEPOT; break;
		case MP_RAILWAY:
			switch (::GetRailTileType(t)) {
				case RAIL_TILE_DEPOT:    return OT_GOTO_DEPOT;
				case RAIL_TILE_WAYPOINT: return OT_GOTO_WAYPOINT;
				default: break;
			}
			break;
	}

	return OT_END;
}

/* static */ bool AIOrder::IsValidVehicleOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	return AIVehicle::IsValidVehicle(vehicle_id) && order_position >= 0 && (order_position < ::GetVehicle(vehicle_id)->GetNumOrders() || order_position == CURRENT_ORDER);
}

/* static */ AIOrder::OrderPosition AIOrder::ResolveOrderPosition(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!AIVehicle::IsValidVehicle(vehicle_id)) return INVALID_ORDER;

	if (order_position == CURRENT_ORDER) return (AIOrder::OrderPosition)::GetVehicle(vehicle_id)->cur_order_index;
	return (order_position >= 0 && order_position < ::GetVehicle(vehicle_id)->GetNumOrders()) ? order_position : INVALID_ORDER;
}


/* static */ bool AIOrder::AreOrderFlagsValid(TileIndex destination, AIOrderFlags order_flags)
{
	switch (::GetOrderTypeByTile(destination)) {
		case OT_GOTO_STATION:
			return ((order_flags & ~(AIOF_NON_STOP_FLAGS | AIOF_UNLOAD_FLAGS | AIOF_LOAD_FLAGS)) == 0) &&
					/* Test the different mutual exclusive flags. */
					(((order_flags & AIOF_TRANSFER)      == 0) || ((order_flags & AIOF_UNLOAD)    == 0)) &&
					(((order_flags & AIOF_TRANSFER)      == 0) || ((order_flags & AIOF_NO_UNLOAD) == 0)) &&
					(((order_flags & AIOF_UNLOAD)        == 0) || ((order_flags & AIOF_NO_UNLOAD) == 0)) &&
					(((order_flags & AIOF_UNLOAD)        == 0) || ((order_flags & AIOF_NO_UNLOAD) == 0)) &&
					(((order_flags & AIOF_NO_UNLOAD)     == 0) || ((order_flags & AIOF_NO_LOAD)   == 0)) &&
					(((order_flags & AIOF_FULL_LOAD_ANY) == 0) || ((order_flags & AIOF_NO_LOAD)   == 0));

		case OT_GOTO_DEPOT:    return (order_flags & ~(AIOF_NON_STOP_FLAGS | AIOF_SERVICE_IF_NEEDED)) == 0;
		case OT_GOTO_WAYPOINT: return (order_flags & ~(AIOF_NON_STOP_FLAGS)) == 0;
		default:               return false;
	}
}

/* static */ int32 AIOrder::GetOrderCount(VehicleID vehicle_id)
{
	return AIVehicle::IsValidVehicle(vehicle_id) ? ::GetVehicle(vehicle_id)->GetNumOrders() : -1;
}

/* static */ TileIndex AIOrder::GetOrderDestination(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return INVALID_TILE;

	const Order *order;
	const Vehicle *v = ::GetVehicle(vehicle_id);
	if (order_position == CURRENT_ORDER) {
		order = &v->current_order;
	} else {
		order = v->GetFirstOrder();
		for (int i = 0; i < order_position; i++) order = order->next;
	}

	switch (order->GetType()) {
		case OT_GOTO_DEPOT:
			if (v->type != VEH_AIRCRAFT) return ::GetDepot(order->GetDestination())->xy;
			/* FALL THROUGH: aircraft's hangars are referenced by StationID, not DepotID */

		case OT_GOTO_STATION:  return ::GetStation(order->GetDestination())->xy;
		case OT_GOTO_WAYPOINT: return ::GetWaypoint(order->GetDestination())->xy;
		default:               return INVALID_TILE;
	}
}

/* static */ AIOrder::AIOrderFlags AIOrder::GetOrderFlags(VehicleID vehicle_id, OrderPosition order_position)
{
	if (!IsValidVehicleOrder(vehicle_id, order_position)) return AIOF_INVALID;

	const Order *order;
	if (order_position == CURRENT_ORDER) {
		order = &::GetVehicle(vehicle_id)->current_order;
	} else {
		order = ::GetVehicle(vehicle_id)->GetFirstOrder();
		for (int i = 0; i < order_position; i++) order = order->next;
	}

	AIOrderFlags order_flags = AIOF_NONE;
	order_flags |= (AIOrderFlags)order->GetNonStopType();
	switch (order->GetType()) {
		case OT_GOTO_DEPOT:
			if (order->GetDepotOrderType() & ODTFB_SERVICE) order_flags |= AIOF_SERVICE_IF_NEEDED;
			break;

		case OT_GOTO_STATION:
			order_flags |= (AIOrderFlags)(order->GetLoadType()   << 5);
			order_flags |= (AIOrderFlags)(order->GetUnloadType() << 2);
			break;

		default: break;
	}

	return order_flags;
}

/* static */ bool AIOrder::AppendOrder(VehicleID vehicle_id, TileIndex destination, AIOrderFlags order_flags)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	return InsertOrder(vehicle_id, (AIOrder::OrderPosition)::GetVehicle(vehicle_id)->GetNumOrders(), destination, order_flags);
}

/* static */ bool AIOrder::InsertOrder(VehicleID vehicle_id, OrderPosition order_position, TileIndex destination, AIOrder::AIOrderFlags order_flags)
{
	/* IsValidVehicleOrder is not good enough because it does not allow appending. */
	if (order_position == CURRENT_ORDER) order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);

	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, order_position >= 0 && order_position <= ::GetVehicle(vehicle_id)->GetNumOrders());
	EnforcePrecondition(false, AreOrderFlagsValid(destination, order_flags));

	Order order;
	switch (::GetOrderTypeByTile(destination)) {
		case OT_GOTO_DEPOT:
			order.MakeGoToDepot(::GetDepotByTile(destination)->index, (OrderDepotTypeFlags)(ODTFB_PART_OF_ORDERS | ((order_flags & AIOF_SERVICE_IF_NEEDED) ? ODTFB_SERVICE : 0)));
			break;

		case OT_GOTO_STATION:
			order.MakeGoToStation(::GetStationIndex(destination));
			order.SetLoadType((OrderLoadFlags)GB(order_flags, 5, 3));
			order.SetUnloadType((OrderUnloadFlags)GB(order_flags, 2, 3));
			break;

		case OT_GOTO_WAYPOINT:
			order.MakeGoToWaypoint(::GetWaypointIndex(destination));
			break;

		default:
			return false;
	}

	order.SetNonStopType((OrderNonStopFlags)GB(order_flags, 0, 2));

	return AIObject::DoCommand(0, vehicle_id | (order_position << 16), order.Pack(), CMD_INSERT_ORDER);
}

/* static */ bool AIOrder::RemoveOrder(VehicleID vehicle_id, OrderPosition order_position)
{
	order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);

	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));

	return AIObject::DoCommand(0, vehicle_id, order_position, CMD_DELETE_ORDER);
}

/**
 * Callback handler as ChangeOrder possibly needs multiple DoCommand calls
 * to be able to set all order flags correctly. As we need to wait till the
 * command has completed before we know the next bits to change we need to
 * call the function multiple times. Each time it'll reduce the difference
 * between the wanted and the current order.
 * @param instance The AI we are doing the callback for.
 */
static void _DoCommandReturnChangeOrder(class AIInstance *instance)
{
	AIObject::SetLastCommandRes(AIOrder::_ChangeOrder());
	AIInstance::DoCommandReturn(instance);
}

/* static */ bool AIOrder::_ChangeOrder()
{
	/* Make sure we don't go into an infinite loop */
	int retry = AIObject::GetCallbackVariable(3) - 1;
	if (retry < 0) {
		DEBUG(ai, 0, "Possible infinite loop in ChangeOrder detected");
		return false;
	}
	AIObject::SetCallbackVariable(3, retry);

	VehicleID vehicle_id = (VehicleID)AIObject::GetCallbackVariable(0);
	OrderPosition order_position = (OrderPosition)AIObject::GetCallbackVariable(1);
	AIOrderFlags order_flags = (AIOrderFlags)AIObject::GetCallbackVariable(2);

	order_position = AIOrder::ResolveOrderPosition(vehicle_id, order_position);

	EnforcePrecondition(false, IsValidVehicleOrder(vehicle_id, order_position));
	EnforcePrecondition(false, AreOrderFlagsValid(GetOrderDestination(vehicle_id, order_position), order_flags));

	Order *order = ::GetVehicle(vehicle_id)->GetFirstOrder();
	for (int i = 0; i < order_position; i++) order = order->next;

	AIOrderFlags current = GetOrderFlags(vehicle_id, order_position);

	if ((current & AIOF_NON_STOP_FLAGS) != (order_flags & AIOF_NON_STOP_FLAGS)) {
		return AIObject::DoCommand(0, vehicle_id | (order_position << 16), (order_flags & AIOF_NON_STOP_FLAGS) << 4 | MOF_NON_STOP, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnChangeOrder);
	}

	switch (order->GetType()) {
		case OT_GOTO_DEPOT:
			if ((current & AIOF_SERVICE_IF_NEEDED) != (order_flags & AIOF_SERVICE_IF_NEEDED)) {
				return AIObject::DoCommand(0, vehicle_id | (order_position << 16), MOF_DEPOT_ACTION, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnChangeOrder);
			}
			break;

		case OT_GOTO_STATION:
			if ((current & AIOF_UNLOAD_FLAGS) != (order_flags & AIOF_UNLOAD_FLAGS)) {
				return AIObject::DoCommand(0, vehicle_id | (order_position << 16), (order_flags & AIOF_UNLOAD_FLAGS) << 2 | MOF_UNLOAD, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnChangeOrder);
			}
			if ((current & AIOF_LOAD_FLAGS) != (order_flags & AIOF_LOAD_FLAGS)) {
				return AIObject::DoCommand(0, vehicle_id | (order_position << 16), (order_flags & AIOF_LOAD_FLAGS) >> 1 | MOF_LOAD, CMD_MODIFY_ORDER, NULL, &_DoCommandReturnChangeOrder);
			}
			break;

		default: break;
	}

	assert(GetOrderFlags(vehicle_id, order_position) == order_flags);

	return true;
}

/* static */ bool AIOrder::ChangeOrder(VehicleID vehicle_id, OrderPosition order_position, AIOrder::AIOrderFlags order_flags)
{
	AIObject::SetCallbackVariable(0, vehicle_id);
	AIObject::SetCallbackVariable(1, order_position);
	AIObject::SetCallbackVariable(2, order_flags);
	/* In case another client(s) change orders at the same time we could
	 * end in an infinite loop. This stops that from happening ever. */
	AIObject::SetCallbackVariable(3, 8);
	return AIOrder::_ChangeOrder();
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

	return AIObject::DoCommand(0, vehicle_id | (main_vehicle_id << 16), CO_COPY, CMD_CLONE_ORDER);
}

/* static */ bool AIOrder::ShareOrders(VehicleID vehicle_id, VehicleID main_vehicle_id)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(main_vehicle_id));

	return AIObject::DoCommand(0, vehicle_id | (main_vehicle_id << 16), CO_SHARE, CMD_CLONE_ORDER);
}

/* static */ bool AIOrder::UnshareOrders(VehicleID vehicle_id)
{
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));

	return AIObject::DoCommand(0, vehicle_id, CO_UNSHARE, CMD_CLONE_ORDER);
}
