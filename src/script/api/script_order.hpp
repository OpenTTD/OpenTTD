/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_order.hpp Everything to query and build orders. */

#ifndef SCRIPT_ORDER_HPP
#define SCRIPT_ORDER_HPP

#include "script_vehicle.hpp"
#include "../../order_type.h"

/**
 * Class that handles all order related functions.
 * @api ai game
 */
class ScriptOrder : public ScriptObject {
public:
	/**
	 * All order related error messages.
	 */
	enum ErrorMessages {
		/** Base for all order related errors */
		ERR_ORDER_BASE = ScriptError::ERR_CAT_ORDER << ScriptError::ERR_CAT_BIT_SIZE,

		/** No more space for orders */
		ERR_ORDER_TOO_MANY,                                  // [STR_ERROR_NO_MORE_SPACE_FOR_ORDERS]

		/** Destination of new order is to far away from the previous order */
		ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION,    // [STR_ERROR_TOO_FAR_FROM_PREVIOUS_DESTINATION]

		/** Aircraft has not enough range to copy/share orders. */
		ERR_ORDER_AIRCRAFT_NOT_ENOUGH_RANGE,                 // [STR_ERROR_AIRCRAFT_NOT_ENOUGH_RANGE]
	};

	/**
	 * Flags that can be used to modify the behaviour of orders.
	 */
	enum ScriptOrderFlags {
		/** Just go to the station/depot, stop unload if possible and load if needed. */
		OF_NONE              = 0,

		/** Do not stop at the stations that are passed when going to the destination. Only for trains and road vehicles. */
		OF_NON_STOP_INTERMEDIATE = 1 << 0,
		/** Do not stop at the destination station. Only for trains and road vehicles. */
		OF_NON_STOP_DESTINATION  = 1 << 1,

		/** Always unload the vehicle; only for stations. Cannot be set when OF_TRANSFER or OF_NO_UNLOAD is set. */
		OF_UNLOAD            = 1 << 2,
		/** Transfer instead of deliver the goods; only for stations. Cannot be set when OF_UNLOAD or OF_NO_UNLOAD is set. */
		OF_TRANSFER          = 1 << 3,
		/** Never unload the vehicle; only for stations. Cannot be set when OF_UNLOAD, OF_TRANSFER or OF_NO_LOAD is set. */
		OF_NO_UNLOAD         = 1 << 4,

		/** Wt till the vehicle is fully loaded; only for stations. Cannot be set when OF_NO_LOAD is set. */
		OF_FULL_LOAD         = 2 << 5,
		/** Wt till at least one cargo of the vehicle is fully loaded; only for stations. Cannot be set when OF_NO_LOAD is set. */
		OF_FULL_LOAD_ANY     = 3 << 5,
		/** Do not load any cargo; only for stations. Cannot be set when OF_NO_UNLOAD, OF_FULL_LOAD or OF_FULL_LOAD_ANY is set. */
		OF_NO_LOAD           = 1 << 7,

		/** Service the vehicle when needed, otherwise skip this order; only for depots. */
		OF_SERVICE_IF_NEEDED = 1 << 2,
		/** Stop in the depot instead of only go there for servicing; only for depots. */
		OF_STOP_IN_DEPOT     = 1 << 3,
		/** Go to nearest depot. */
		OF_GOTO_NEAREST_DEPOT = 1 << 8,

		/** All flags related to non-stop settings. */
		OF_NON_STOP_FLAGS    = OF_NON_STOP_INTERMEDIATE | OF_NON_STOP_DESTINATION,
		/** All flags related to unloading. */
		OF_UNLOAD_FLAGS      = OF_TRANSFER | OF_UNLOAD | OF_NO_UNLOAD,
		/** All flags related to loading. */
		OF_LOAD_FLAGS        = OF_FULL_LOAD | OF_FULL_LOAD_ANY | OF_NO_LOAD,
		/** All flags related to depots. */
		OF_DEPOT_FLAGS       = OF_SERVICE_IF_NEEDED | OF_STOP_IN_DEPOT | OF_GOTO_NEAREST_DEPOT,

		/** For marking invalid order flags */
		OF_INVALID           = 0xFFFF,
	};

	/**
	 * All conditions a conditional order can depend on.
	 */
	enum OrderCondition {
		/* Note: these values represent part of the in-game OrderConditionVariable enum */
		OC_LOAD_PERCENTAGE     = ::OCV_LOAD_PERCENTAGE,    ///< Skip based on the amount of load, value is in tons.
		OC_RELIABILITY         = ::OCV_RELIABILITY,        ///< Skip based on the reliability, value is percent (0..100).
		OC_MAX_RELIABILITY     = ::OCV_MAX_RELIABILITY,    ///< Skip based on the maximum reliability.  Value in percent
		OC_MAX_SPEED           = ::OCV_MAX_SPEED,          ///< Skip based on the maximum speed, value is in OpenTTD's internal speed unit, see ScriptEngine::GetMaxSpeed.
		OC_AGE                 = ::OCV_AGE,                ///< Skip based on the age, value is in years.
		OC_REQUIRES_SERVICE    = ::OCV_REQUIRES_SERVICE,   ///< Skip when the vehicle requires service, no value.
		OC_UNCONDITIONALLY     = ::OCV_UNCONDITIONALLY,    ///< Always skip, no compare function, no value.
		OC_REMAINING_LIFETIME  = ::OCV_REMAINING_LIFETIME, ///< Skip based on the remaining lifetime

		/* Custom added value, only valid for this API */
		OC_INVALID             = -1,                       ///< An invalid condition, do not use.
	};

	/**
	 * Comparators for conditional orders.
	 */
	enum CompareFunction {
		/* Note: these values represent part of the in-game OrderConditionComparator enum */
		CF_EQUALS        = ::OCC_EQUALS,       ///< Skip if both values are equal
		CF_NOT_EQUALS    = ::OCC_NOT_EQUALS,   ///< Skip if both values are not equal
		CF_LESS_THAN     = ::OCC_LESS_THAN,    ///< Skip if the value is less than the limit
		CF_LESS_EQUALS   = ::OCC_LESS_EQUALS,  ///< Skip if the value is less or equal to the limit
		CF_MORE_THAN     = ::OCC_MORE_THAN,    ///< Skip if the value is more than the limit
		CF_MORE_EQUALS   = ::OCC_MORE_EQUALS,  ///< Skip if the value is more or equal to the limit
		CF_IS_TRUE       = ::OCC_IS_TRUE,      ///< Skip if the variable is true
		CF_IS_FALSE      = ::OCC_IS_FALSE,     ///< Skip if the variable is false

		/* Custom added value, only valid for this API */
		CF_INVALID       = -1,                 ///< Invalid compare function, do not use.
	};

	/**
	 * Index in the list of orders for a vehicle. The first order has index 0, the second
	 * order index 1, etc. The current order can be queried by using ORDER_CURRENT. Do not
	 * use ORDER_INVALID yourself, it's used as return value by for example ResolveOrderPosition.
	 * @note Automatic orders are hidden from scripts, so OrderPosition 0 will always be the first
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
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
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
	 * Checks whether the given order has a valid refit cargo.
	 * @param vehicle_id The vehicle to check.
	 * @param order_position The order index to check.
	 * @pre order_position != ORDER_CURRENT && IsValidVehicleOrder(vehicle_id, order_position).
	 * @return True if and only if the order is has a valid refit cargo.
	 */
	static bool IsRefitOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the current order is part of the orderlist.
	 * @param vehicle_id The vehicle to check.
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
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
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
	 * @return The resolved order index.
	 */
	static OrderPosition ResolveOrderPosition(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Checks whether the given order flags are valid for the given destination.
	 * @param destination The destination of the order.
	 * @param order_flags The flags given to the order.
	 * @return True if and only if the order_flags are valid for the given location.
	 */
	static bool AreOrderFlagsValid(TileIndex destination, ScriptOrderFlags order_flags);

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
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
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
	 * Gets the ScriptOrderFlags of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the destination for.
	 * @param order_position The order to get the destination for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position == ORDER_CURRENT || (!IsConditionalOrder(vehicle_id, order_position) && !IsVoidOrder(vehicle_id, order_position)).
	 * @note Giving ORDER_CURRENT as order_position will give the order that is
	 *  currently being executed by the vehicle. This is not necessarily the
	 *  current order as given by ResolveOrderPosition (the current index in the
	 *  order list) as manual or autoservicing depot orders do not show up
	 *  in the orderlist, but they can be the current order of a vehicle.
	 * @return The ScriptOrderFlags of the order.
	 */
	static ScriptOrderFlags GetOrderFlags(VehicleID vehicle_id, OrderPosition order_position);

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
	 * @pre ScriptVehicle::GetVehicleType(vehicle_id) == ScriptVehicle::VT_RAIL.
	 * @pre IsGotoStationOrder(vehicle_id, order_position).
	 * @return The relative position where the train will stop inside a station.
	 */
	static StopLocation GetStopLocation(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Gets the refit cargo type of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to get the refit cargo for.
	 * @param order_position The order to get the refit cargo for.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre order_position == ORDER_CURRENT || IsGotoStationOrder(vehicle_id, order_position) || IsGotoDepotOrder(vehicle_id, order_position).
	 * @note Giving ORDER_CURRENT as order_position will give the order that is
	 *  currently being executed by the vehicle. This is not necessarily the
	 *  current order as given by ResolveOrderPosition (the current index in the
	 *  order list) as manual or autoservicing depot orders do not show up
	 *  in the orderlist, but they can be the current order of a vehicle.
	 * @return The refit cargo of the order or CT_NO_REFIT if no refit is set.
	 */
	static CargoID GetOrderRefit(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Sets the OrderPosition to jump to if the check succeeds of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to set the OrderPosition for.
	 * @param order_position The order to set the OrderPosition for.
	 * @param jump_to The order to jump to if the check succeeds.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre IsValidVehicleOrder(vehicle_id, jump_to).
	 * @pre order_position != ORDER_CURRENT && IsConditionalOrder(vehicle_id, order_position).
	 * @return Whether the order has been/can be changed.
	 * @api -game
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
	 * @api -game
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
	 * @api -game
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
	 * @api -game
	 */
	static bool SetOrderCompareValue(VehicleID vehicle_id, OrderPosition order_position, int32 value);

	/**
	 * Sets the stoplocation of the given order for the given train.
	 * @param vehicle_id The vehicle to get the value for.
	 * @param order_position The order to get the value for.
	 * @param stop_location The relative position where a train will stop inside a station.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre ScriptVehicle::GetVehicleType(vehicle_id) == ScriptVehicle::VT_RAIL.
	 * @pre IsGotoStationOrder(vehicle_id, order_position).
	 * @pre stop_location >= STOPLOCATION_NEAR && stop_location <= STOPLOCATION_FAR
	 * @return Whether the order has been/can be changed.
	 * @api -game
	 */
	static bool SetStopLocation(VehicleID vehicle_id, OrderPosition order_position, StopLocation stop_location);

	/**
	 * Sets the refit cargo type of the given order for the given vehicle.
	 * @param vehicle_id The vehicle to set the refit cargo for.
	 * @param order_position The order to set the refit cargo for.
	 * @param refit_cargo The cargo to refit to. The refit can be cleared by passing CT_NO_REFIT.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre IsGotoStationOrder(vehicle_id, order_position) || (IsGotoDepotOrder(vehicle_id, order_position) && refit_cargo != CT_AUTO_REFIT).
	 * @pre ScriptCargo::IsValidCargo(refit_cargo) || refit_cargo == CT_AUTO_REFIT || refit_cargo == CT_NO_REFIT
	 * @return Whether the order has been/can be changed.
	 * @api -game
	 */
	static bool SetOrderRefit(VehicleID vehicle_id, OrderPosition order_position, CargoID refit_cargo);

	/**
	 * Appends an order to the end of the vehicle's order list.
	 * @param vehicle_id The vehicle to append the order to.
	 * @param destination The destination of the order.
	 * @param order_flags The flags given to the order.
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
	 * @pre AreOrderFlagsValid(destination, order_flags).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptOrder::ERR_ORDER_TOO_MANY
	 * @exception ScriptOrder::ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION
	 * @return True if and only if the order was appended.
	 * @api -game
	 */
	static bool AppendOrder(VehicleID vehicle_id, TileIndex destination, ScriptOrderFlags order_flags);

	/**
	 * Appends a conditional order to the end of the vehicle's order list.
	 * @param vehicle_id The vehicle to append the order to.
	 * @param jump_to The OrderPosition to jump to if the condition is true.
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
	 * @pre IsValidVehicleOrder(vehicle_id, jump_to).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptOrder::ERR_ORDER_TOO_MANY
	 * @return True if and only if the order was appended.
	 * @api -game
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
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptOrder::ERR_ORDER_TOO_MANY
	 * @exception ScriptOrder::ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION
	 * @return True if and only if the order was inserted.
	 * @api -game
	 */
	static bool InsertOrder(VehicleID vehicle_id, OrderPosition order_position, TileIndex destination, ScriptOrderFlags order_flags);

	/**
	 * Appends a conditional order before the given order_position into the vehicle's order list.
	 * @param vehicle_id The vehicle to add the order to.
	 * @param order_position The order to place the new order before.
	 * @param jump_to The OrderPosition to jump to if the condition is true.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre IsValidVehicleOrder(vehicle_id, jump_to).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptOrder::ERR_ORDER_TOO_MANY
	 * @return True if and only if the order was inserted.
	 * @api -game
	 */
	static bool InsertConditionalOrder(VehicleID vehicle_id, OrderPosition order_position, OrderPosition jump_to);

	/**
	 * Removes an order from the vehicle's order list.
	 * @param vehicle_id The vehicle to remove the order from.
	 * @param order_position The order to remove from the order list.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was removed.
	 * @api -game
	 */
	static bool RemoveOrder(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Internal function to help SetOrderFlags.
	 * @api -all
	 */
	static bool _SetOrderFlags();

	/**
	 * Changes the order flags of the given order.
	 * @param vehicle_id The vehicle to change the order of.
	 * @param order_position The order to change.
	 * @param order_flags The new flags given to the order.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre AreOrderFlagsValid(GetOrderDestination(vehicle_id, order_position), order_flags).
	 * @pre (order_flags & OF_GOTO_NEAREST_DEPOT) == (GetOrderFlags(vehicle_id, order_position) & OF_GOTO_NEAREST_DEPOT).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was changed.
	 * @api -game
	 */
	static bool SetOrderFlags(VehicleID vehicle_id, OrderPosition order_position, ScriptOrderFlags order_flags);

	/**
	 * Move an order inside the orderlist
	 * @param vehicle_id The vehicle to move the orders.
	 * @param order_position_move The order to move.
	 * @param order_position_target The target order
	 * @pre IsValidVehicleOrder(vehicle_id, order_position_move).
	 * @pre IsValidVehicleOrder(vehicle_id, order_position_target).
	 * @pre order_position_move != order_position_target.
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was moved.
	 * @note If the order is moved to a lower place (e.g. from 7 to 2)
	 *  the target order is moved upwards (e.g. 3). If the order is moved
	 *  to a higher place (e.g. from 7 to 9) the target will be moved
	 *  downwards (e.g. 8).
	 * @api -game
	 */
	static bool MoveOrder(VehicleID vehicle_id, OrderPosition order_position_move, OrderPosition order_position_target);

	/**
	 * Make a vehicle execute next_order instead of its current order.
	 * @param vehicle_id The vehicle that should skip some orders.
	 * @param next_order The order the vehicle should skip to.
	 * @pre IsValidVehicleOrder(vehicle_id, next_order).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only the current order was changed.
	 * @api -game
	 */
	static bool SkipToOrder(VehicleID vehicle_id, OrderPosition next_order);

	/**
	 * Copies the orders from another vehicle. The orders of the main vehicle
	 *  are going to be the orders of the changed vehicle.
	 * @param vehicle_id The vehicle to copy the orders to.
	 * @param main_vehicle_id The vehicle to copy the orders from.
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
	 * @pre ScriptVehicle::IsValidVehicle(main_vehicle_id).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptOrder::ERR_ORDER_TOO_MANY
	 * @exception ScriptOrder::ERR_ORDER_AIRCRAFT_NOT_ENOUGH_RANGE
	 * @return True if and only if the copying succeeded.
	 * @api -game
	 */
	static bool CopyOrders(VehicleID vehicle_id, VehicleID main_vehicle_id);

	/**
	 * Shares the orders between two vehicles. The orders of the main
	 * vehicle are going to be the orders of the changed vehicle.
	 * @param vehicle_id The vehicle to add to the shared order list.
	 * @param main_vehicle_id The vehicle to share the orders with.
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
	 * @pre ScriptVehicle::IsValidVehicle(main_vehicle_id).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptOrder::ERR_ORDER_AIRCRAFT_NOT_ENOUGH_RANGE
	 * @return True if and only if the sharing succeeded.
	 * @api -game
	 */
	static bool ShareOrders(VehicleID vehicle_id, VehicleID main_vehicle_id);

	/**
	 * Removes the given vehicle from a shared orders list.
	 * After unsharing orders, the orders list of the vehicle is empty.
	 * @param vehicle_id The vehicle to remove from the shared order list.
	 * @pre ScriptVehicle::IsValidVehicle(vehicle_id).
	 * @return True if and only if the unsharing succeeded.
	 * @api -game
	 */
	static bool UnshareOrders(VehicleID vehicle_id);

	/**
	 * Get the distance between two points for a vehicle type.
	 * Use this function to compute the distance between two tiles wrt. a vehicle type.
	 * These vehicle-type specific distances are independent from other map distances, you may
	 * use the result of this function to compare it with the result of
	 * ScriptEngine::GetMaximumOrderDistance or ScriptVehicle::GetMaximumOrderDistance.
	 * @param vehicle_type The vehicle type to get the distance for.
	 * @param origin_tile Origin, can be any tile or a tile of a specific station.
	 * @param dest_tile Destination, can be any tile or a tile of a specific station.
	 * @return The distance between the origin and the destination for a
	 *         vehicle of the given vehicle type.
	 * @note   The unit of the order distances is unspecified and should
	 *         not be compared with map distances
	 * @see ScriptEngine::GetMaximumOrderDistance and ScriptVehicle::GetMaximumOrderDistance
	 */
	static uint GetOrderDistance(ScriptVehicle::VehicleType vehicle_type, TileIndex origin_tile, TileIndex dest_tile);
};
DECLARE_ENUM_AS_BIT_SET(ScriptOrder::ScriptOrderFlags)

#endif /* SCRIPT_ORDER_HPP */
