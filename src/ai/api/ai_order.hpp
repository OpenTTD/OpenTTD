/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_order.hpp Everything to query and build orders. */

#ifndef AI_ORDER_HPP
#define AI_ORDER_HPP

#include "ai_error.hpp"

/**
 * Class that handles all order related functions.
 */
class AIOrder : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIOrder"; }

	/**
	 * All order related error messages.
	 */
	enum ErrorMessages {
		/** Base for all order related errors */
		ERR_ORDER_BASE = AIError::ERR_CAT_ORDER << AIError::ERR_CAT_BIT_SIZE,

		/** No more space for orders */
		ERR_ORDER_TOO_MANY,                                  // [STR_ERROR_NO_MORE_SPACE_FOR_ORDERS]

		/** Destination of new order is to far away from the previous order */
		ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION,    // [STR_ERROR_TOO_FAR_FROM_PREVIOUS_DESTINATION]
	};

	/**
	 * Flags that can be used to modify the behaviour of orders.
	 */
	enum AIOrderFlags {
		/** Just go to the station/depot, stop unload if possible and load if needed. */
		AIOF_NONE              = 0,

		/** Do not stop at the stations that are passed when going to the destination. Only for trains and road vehicles. */
		AIOF_NON_STOP_INTERMEDIATE = 1 << 0,
		/** Do not stop at the destionation station. Only for trains and road vehicles. */
		AIOF_NON_STOP_DESTINATION  = 1 << 1,

		/** Always unload the vehicle; only for stations. Cannot be set when AIOF_TRANSFER or AIOF_NO_UNLOAD is set. */
		AIOF_UNLOAD            = 1 << 2,
		/** Transfer instead of deliver the goods; only for stations. Cannot be set when AIOF_UNLOAD or AIOF_NO_UNLOAD is set. */
		AIOF_TRANSFER          = 1 << 3,
		/** Never unload the vehicle; only for stations. Cannot be set when AIOF_UNLOAD, AIOF_TRANSFER or AIOF_NO_LOAD is set. */
		AIOF_NO_UNLOAD         = 1 << 4,

		/** Wait till the vehicle is fully loaded; only for stations. Cannot be set when AIOF_NO_LOAD is set. */
		AIOF_FULL_LOAD         = 2 << 5,
		/** Wait till at least one cargo of the vehicle is fully loaded; only for stations. Cannot be set when AIOF_NO_LOAD is set. */
		AIOF_FULL_LOAD_ANY     = 3 << 5,
		/** Do not load any cargo; only for stations. Cannot be set when AIOF_NO_UNLOAD, AIOF_FULL_LOAD or AIOF_FULL_LOAD_ANY is set. */
		AIOF_NO_LOAD           = 1 << 7,

		/** Service the vehicle when needed, otherwise skip this order; only for depots. */
		AIOF_SERVICE_IF_NEEDED = 1 << 2,
		/** Stop in the depot instead of only go there for servicing; only for depots. */
		AIOF_STOP_IN_DEPOT     = 1 << 3,
		/** Go to nearest depot. */
		AIOF_GOTO_NEAREST_DEPOT = 1 << 8,

		/** All flags related to non-stop settings. */
		AIOF_NON_STOP_FLAGS    = AIOF_NON_STOP_INTERMEDIATE | AIOF_NON_STOP_DESTINATION,
		/** All flags related to unloading. */
		AIOF_UNLOAD_FLAGS      = AIOF_TRANSFER | AIOF_UNLOAD | AIOF_NO_UNLOAD,
		/** All flags related to loading. */
		AIOF_LOAD_FLAGS        = AIOF_FULL_LOAD | AIOF_FULL_LOAD_ANY | AIOF_NO_LOAD,
		/** All flags related to depots. */
		AIOF_DEPOT_FLAGS       = AIOF_SERVICE_IF_NEEDED | AIOF_STOP_IN_DEPOT | AIOF_GOTO_NEAREST_DEPOT,

		/** For marking invalid order flags */
		AIOF_INVALID           = 0xFFFF,
	};

	/**
	 * All conditions a conditional order can depend on.
	 */
	enum OrderCondition {
		/* Order _is_ important, as it's based on OrderConditionVariable in order_type.h. */
		OC_LOAD_PERCENTAGE,  ///< Skip based on the amount of load, value is in tons.
		OC_RELIABILITY,      ///< Skip based on the reliability, value is percent (0..100).
		OC_MAX_SPEED,        ///< Skip based on the maximum speed, value is in OpenTTD's internal speed unit, see AIEngine::GetMaxSpeed.
		OC_AGE,              ///< Skip based on the age, value is in years.
		OC_REQUIRES_SERVICE, ///< Skip when the vehicle requires service, no value.
		OC_UNCONDITIONALLY,  ///< Always skip, no compare function, no value.
		OC_INVALID = -1,     ///< An invalid condition, do not use.
	};

	/**
	 * Comparators for conditional orders.
	 */
	enum CompareFunction {
		/* Order _is_ important, as it's based on OrderConditionComparator in order_type.h. */
		CF_EQUALS,       ///< Skip if both values are equal
		CF_NOT_EQUALS,   ///< Skip if both values are not equal
		CF_LESS_THAN,    ///< Skip if the value is less than the limit
		CF_LESS_EQUALS,  ///< Skip if the value is less or equal to the limit
		CF_MORE_THAN,    ///< Skip if the value is more than the limit
		CF_MORE_EQUALS,  ///< Skip if the value is more or equal to the limit
		CF_IS_TRUE,      ///< Skip if the variable is true
		CF_IS_FALSE,     ///< Skip if the variable is false
		CF_INVALID = -1, ///< Invalid compare function, do not use.
	};

	/**
	 * Index in the list of orders for a vehicle. The first order has index 0, the second
	 * order index 1, etc. The current order can be queried by using ORDER_CURRENT. Do not
	 * use ORDER_INVALID yourself, it's used as return value by for example ResolveOrderPosition.
	 * @note Automatic orders are hidden from AIs, so OrderPosition 0 will always be the first
	 * manual order.
	 */
	enum OrderPosition {
		ORDER_CURRENT = 0xFF, ///< Constant that gets resolved to the current order.
		ORDER_INVALID = -1,   ///< An invalid order.
	};

	/** Where to stop trains in a station that's longer than the train */
	enum StopLocation {
		STOPLOCATION_NEAR,         ///< Stop the train as soon as it's completely in the station
		STOPLOCATION_MIDDLE,       ///< Stop the train in the middle of the station
		STOPLOCATION_FAR,          ///< Stop the train at the far end of the station
		STOPLOCATION_INVALID = -1, ///< An invalid stop location
	};

	/**
	 * Checks whether the given order id is valid for the given vehicle.
	 * @param vehicle_id The vehicle to check the order index for.
	 * @param order_position The order index to check.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @return True if and only if the order_position is valid for the given vehicle.
	 */
	static bool IsValidVehicleOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the given order is a goto-station order.
	 * @param vehicle_id The vehicle to check.
	 * @param order_position The order index to check.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @return True if and only if the order is a goto-station order.
	 */
	static bool IsGotoStationOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the given order is a goto-depot order.
	 * @param vehicle_id The vehicle to check.
	 * @param order_position The order index to check.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @return True if and only if the order is a goto-depot order.
	 */
	static bool IsGotoDepotOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the given order is a goto-waypoint order.
	 * @param vehicle_id The vehicle to check.
	 * @param order_position The order index to check.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @return True if and only if the order is a goto-waypoint order.
	 */
	static bool IsGotoWaypointOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the given order is a conditional order.
	 * @param vehicle_id The vehicle to check.
	 * @param order_position The order index to check.
	 * @pre order_position != ORDER_CURRENT && IsValidVehicleOrder(vehicle_id, order_position).
	 * @return True if and only if the order is a conditional order.
	 */
	static bool IsConditionalOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the given order is a void order.
	 * A void order is an order that used to be a goto station, depot or waypoint order but
	 * its destination got removed. In OpenTTD these orders as shown as "(Invalid Order)"
	 * in the order list of a vehicle.
	 * @param vehicle_id The vehicle to check.
	 * @param order_position The order index to check.
	 * @pre order_position != ORDER_CURRENT && IsValidVehicleOrder(vehicle_id, order_position).
	 * @return True if and only if the order is a void order.
	 */
	static bool IsVoidOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the current order is part of the orderlist.
	 * @param vehicle_id The vehicle to check.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @return True if and only if the current order is part of the order list.
	 * @note If the order is a non-'non-stop' order, and the vehicle is currently
	 * (un)loading at a station that is not the final destination, this function
	 * will still return true.
	 */
	static bool IsCurrentOrderPartOfOrderList(VehicleID vehicle_id);

	/**
	 * Resolves the given order index to the correct index for the given vehicle.
	 *  If the order index was ORDER_CURRENT it will be resolved to the index of
	 *  the current order (as shown in the order list). If the order with the
	 *  given index does not exist it will return ORDER_INVALID.
	 * @param vehicle_id The vehicle to check the order index for.
	 * @param order_position The order index to resolve.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @return The resolved order index.
	 */
	static OrderPosition ResolveOrderPosition(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the given order flags are valid for the given destination.
	 * @param destination The destination of the order.
	 * @param order_flags The flags given to the order.
	 * @return True if and only if the order_flags are valid for the given location.
	 */
	static bool AreOrderFlagsValid(TileIndex destination, AIOrderFlags order_flags);

	/**
	 * Checks whether the given combination of condition and compare function is valid.
	 * @param condition The condition to check.
	 * @param compare The compare function to check.
	 * @return True if and only if the combination of condition and compare function is valid.
	 */
	static bool IsValidConditionalOrder(OrderCondition condition, CompareFunction compare);

	/**
	 * Returns the number of orders for the given vehicle.
	 * @param vehicle_id The vehicle to get the order count of.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @return The number of orders for the given vehicle or a negative
	 *   value when the vehicle does not exist.
	 */
	static int32 GetOrderCount(VehicleID vehicle_id);

	/**
	 * Gets the destination of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the destination for.
	 * @param order_position The order to get the destination for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position == ORDER_CURRENT || !IsConditionalOrder(vehicle_id, order_position).
	 * @note Giving ORDER_CURRENT as order_position will give the order that is
	 *  currently being executed by the vehicle. This is not necessarily the
	 *  current order as given by ResolveOrderPosition (the current index in the
	 *  order list) as manual or autoservicing depot orders do not show up
	 *  in the orderlist, but they can be the current order of a vehicle.
	 * @return The destination tile of the order.
	 */
	static TileIndex GetOrderDestination(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Gets the AIOrderFlags of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the destination for.
	 * @param order_position The order to get the destination for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position == ORDER_CURRENT || (!IsConditionalOrder(vehicle_id, order_position) && !IsVoidOrder(vehicle_id, order_position)).
	 * @note Giving ORDER_CURRENT as order_position will give the order that is
	 *  currently being executed by the vehicle. This is not necessarily the
	 *  current order as given by ResolveOrderPosition (the current index in the
	 *  order list) as manual or autoservicing depot orders do not show up
	 *  in the orderlist, but they can be the current order of a vehicle.
	 * @return The AIOrderFlags of the order.
	 */
	static AIOrderFlags GetOrderFlags(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Gets the OrderPosition to jump to if the check succeeds of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the OrderPosition for.
	 * @param order_position The order to get the OrderPosition for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @return The target of the conditional jump.
	 */
	static OrderPosition GetOrderJumpTo(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Gets the OrderCondition of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the condition type for.
	 * @param order_position The order to get the condition type for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @return The OrderCondition of the order.
	 */
	static OrderCondition GetOrderCondition(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Gets the CompareFunction of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the compare function for.
	 * @param order_position The order to get the compare function for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @return The CompareFunction of the order.
	 */
	static CompareFunction GetOrderCompareFunction(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Gets the value to compare against of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the value for.
	 * @param order_position The order to get the value for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @return The value to compare against of the order.
	 */
	static int32 GetOrderCompareValue(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Gets the stoplocation of the given order for the given train.
	 * @param vehicle_id The vehicle to get the value for.
	 * @param order_position The order to get the value for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre AIVehicle::GetVehicleType(vehicle_id) == AIVehicle::VT_RAIL.
	 * @pre IsGotoStationOrder(vehicle_id, order_position).
	 * @return The relative position where the train will stop inside a station.
	 */
	static StopLocation GetStopLocation(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Sets the OrderPosition to jump to if the check succeeds of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to set the OrderPosition for.
	 * @param order_position The order to set the OrderPosition for.
	 * @param jump_to The order to jump to if the check succeeds.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre IsValidVehicleOrder(vehicle_id, jump_to).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @return Whether the order has been/can be changed.
	 */
	static bool SetOrderJumpTo(VehicleID vehicle_id, OrderPosition order_position, OrderPosition jump_to);

	/**
	 * Sets the OrderCondition of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to set the condition type for.
	 * @param order_position The order to set the condition type for.
	 * @param condition The condition to compare on.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @pre condition >= OC_LOAD_PERCENTAGE && condition <= OC_UNCONDITIONALLY.
	 * @return Whether the order has been/can be changed.
	 */
	static bool SetOrderCondition(VehicleID vehicle_id, OrderPosition order_position, OrderCondition condition);

	/**
	 * Sets the CompareFunction of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to set the compare function for.
	 * @param order_position The order to set the compare function for.
	 * @param compare The new compare function of the order.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @pre compare >= CF_EQUALS && compare <= CF_IS_FALSE.
	 * @return Whether the order has been/can be changed.
	 */
	static bool SetOrderCompareFunction(VehicleID vehicle_id, OrderPosition order_position, CompareFunction compare);

	/**
	 * Sets the value to compare against of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to set the value for.
	 * @param order_position The order to set the value for.
	 * @param value The value to compare against.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @pre value >= 0 && value < 2048.
	 * @return Whether the order has been/can be changed.
	 */
	static bool SetOrderCompareValue(VehicleID vehicle_id, OrderPosition order_position, int32 value);

	/**
	 * Sets the stoplocation of the given order for the given train.
	 * @param vehicle_id The vehicle to get the value for.
	 * @param order_position The order to get the value for.
	 * @param stop_location The relative position where a train will stop inside a station.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre AIVehicle::GetVehicleType(vehicle_id) == AIVehicle::VT_RAIL.
	 * @pre IsGotoStationOrder(vehicle_id, order_position).
	 * @pre stop_location >= STOPLOCATION_NEAR && stop_location <= STOPLOCATION_FAR
	 * @return Whether the order has been/can be changed.
	 */
	static bool SetStopLocation(VehicleID vehicle_id, OrderPosition order_position, StopLocation stop_location);

	/**
	 * Appends an order to the end of the vehicle's order list.
	 * @param vehicle_id The vehicle to append the order to.
	 * @param destination The destination of the order.
	 * @param order_flags The flags given to the order.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @pre AreOrderFlagsValid(destination, order_flags).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_TOO_MANY
	 * @exception AIOrder::ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION
	 * @return True if and only if the order was appended.
	 */
	static bool AppendOrder(VehicleID vehicle_id, TileIndex destination, AIOrderFlags order_flags);

	/**
	 * Appends a conditional order to the end of the vehicle's order list.
	 * @param vehicle_id The vehicle to append the order to.
	 * @param jump_to The OrderPosition to jump to if the condition is true.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @pre IsValidVehicleOrder(vehicle_id, jump_to).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_TOO_MANY
	 * @return True if and only if the order was appended.
	 */
	static bool AppendConditionalOrder(VehicleID vehicle_id, OrderPosition jump_to);

	/**
	 * Inserts an order before the given order_position into the vehicle's order list.
	 * @param vehicle_id The vehicle to add the order to.
	 * @param order_position The order to place the new order before.
	 * @param destination The destination of the order.
	 * @param order_flags The flags given to the order.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre AreOrderFlagsValid(destination, order_flags).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_TOO_MANY
	 * @exception AIOrder::ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION
	 * @return True if and only if the order was inserted.
	 */
	static bool InsertOrder(VehicleID vehicle_id, OrderPosition order_position, TileIndex destination, AIOrderFlags order_flags);

	/**
	 * Appends a conditional order before the given order_position into the vehicle's order list.
	 * @param vehicle_id The vehicle to add the order to.
	 * @param order_position The order to place the new order before.
	 * @param jump_to The OrderPosition to jump to if the condition is true.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre IsValidVehicleOrder(vehicle_id, jump_to).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_TOO_MANY
	 * @return True if and only if the order was inserted.
	 */
	static bool InsertConditionalOrder(VehicleID vehicle_id, OrderPosition order_position, OrderPosition jump_to);

	/**
	 * Removes an order from the vehicle's order list.
	 * @param vehicle_id The vehicle to remove the order from.
	 * @param order_position The order to remove from the order list.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was removed.
	 */
	static bool RemoveOrder(VehicleID vehicle_id, OrderPosition order_position);

#ifndef DOXYGEN_SKIP
	/**
	 * Internal function to help SetOrderFlags.
	 */
	static bool _SetOrderFlags();
#endif /* DOXYGEN_SKIP */

	/**
	 * Changes the order flags of the given order.
	 * @param vehicle_id The vehicle to change the order of.
	 * @param order_position The order to change.
	 * @param order_flags The new flags given to the order.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre AreOrderFlagsValid(GetOrderDestination(vehicle_id, order_position), order_flags).
	 * @pre (order_flags & AIOF_GOTO_NEAREST_DEPOT) == (GetOrderFlags(vehicle_id, order_position) & AIOF_GOTO_NEAREST_DEPOT).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was changed.
	 */
	static bool SetOrderFlags(VehicleID vehicle_id, OrderPosition order_position, AIOrderFlags order_flags);

	/**
	 * Move an order inside the orderlist
	 * @param vehicle_id The vehicle to move the orders.
	 * @param order_position_move The order to move.
	 * @param order_position_target The target order
	 * @pre IsValidVehicleOrder(vehicle_id, order_position_move).
	 * @pre IsValidVehicleOrder(vehicle_id, order_position_target).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was moved.
	 * @note If the order is moved to a lower place (e.g. from 7 to 2)
	 *  the target order is moved upwards (e.g. 3). If the order is moved
	 *  to a higher place (e.g. from 7 to 9) the target will be moved
	 *  downwards (e.g. 8).
	 */
	static bool MoveOrder(VehicleID vehicle_id, OrderPosition order_position_move, OrderPosition order_position_target);

	/**
	 * Make a vehicle execute next_order instead of its current order.
	 * @param vehicle_id The vehicle that should skip some orders.
	 * @param next_order The order the vehicle should skip to.
	 * @pre IsValidVehicleOrder(vehicle_id, next_order).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only the current order was changed.
	 */
	static bool SkipToOrder(VehicleID vehicle_id, OrderPosition next_order);

	/**
	 * Copies the orders from another vehicle. The orders of the main vehicle
	 *  are going to be the orders of the changed vehicle.
	 * @param vehicle_id The vehicle to copy the orders to.
	 * @param main_vehicle_id The vehicle to copy the orders from.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @pre AIVehicle::IsValidVehicle(main_vehicle_id).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_TOO_MANY
	 * @return True if and only if the copying succeeded.
	 */
	static bool CopyOrders(VehicleID vehicle_id, VehicleID main_vehicle_id);

	/**
	 * Shares the orders between two vehicles. The orders of the main
	 * vehicle are going to be the orders of the changed vehicle.
	 * @param vehicle_id The vehicle to add to the shared order list.
	 * @param main_vehicle_id The vehicle to share the orders with.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @pre AIVehicle::IsValidVehicle(main_vehicle_id).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the sharing succeeded.
	 */
	static bool ShareOrders(VehicleID vehicle_id, VehicleID main_vehicle_id);

	/**
	 * Removes the given vehicle from a shared orders list.
	 * @param vehicle_id The vehicle to remove from the shared order list.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @return True if and only if the unsharing succeeded.
	 */
	static bool UnshareOrders(VehicleID vehicle_id);
};
DECLARE_ENUM_AS_BIT_SET(AIOrder::AIOrderFlags)

#endif /* AI_ORDER_HPP */
