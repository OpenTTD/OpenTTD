/* $Id$ */

/** @file ai_order.hpp Everything to query and build orders. */

#ifndef AI_ORDER_HPP
#define AI_ORDER_HPP

#include "ai_object.hpp"
#include "ai_error.hpp"

/**
 * Class that handles all order related functions.
 */
class AIOrder : public AIObject {
public:
	static const char *GetClassName() { return "AIOrder"; }

	/**
	 * All order related error messages.
	 */
	enum ErrorMessages {
		/** Base for all order related errors */
		ERR_ORDER_BASE = AIError::ERR_CAT_ORDER << AIError::ERR_CAT_BIT_SIZE,

		/** No more space for orders */
		ERR_ORDER_TOO_MANY,                                  // [STR_8831_NO_MORE_SPACE_FOR_ORDERS]

		/** Destination of new order is to far away from the previous order */
		ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION,    // [STR_0210_TOO_FAR_FROM_PREVIOUS_DESTINATIO]
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

		/** All flags related to non-stop settings. */
		AIOF_NON_STOP_FLAGS    = AIOF_NON_STOP_INTERMEDIATE | AIOF_NON_STOP_DESTINATION,
		/** All flags related to unloading. */
		AIOF_UNLOAD_FLAGS      = AIOF_TRANSFER | AIOF_UNLOAD | AIOF_NO_UNLOAD,
		/** All flags related to loading. */
		AIOF_LOAD_FLAGS        = AIOF_FULL_LOAD | AIOF_FULL_LOAD_ANY | AIOF_NO_LOAD,

		/** For marking invalid order flags */
		AIOF_INVALID           = 0xFFFF,
	};

	/** Different constants related to the OrderPosition */
	enum OrderPosition {
		ORDER_CURRENT = 0xFF, //!< Constant that gets resolved to the current order.
		ORDER_INVALID = -1,   //!< An invalid order.
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
	 * Resolves the given order index to the correct index for the given vehicle.
	 *  If the order index was CURRENT_ORDER it will be resolved to the index of
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
	 * @note Giving CURRENT_ORDER as order_position will give the order that is
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
	 * @note Giving CURRENT_ORDER as order_position will give the order that is
	 *  currently being executed by the vehicle. This is not necessarily the
	 *  current order as given by ResolveOrderPosition (the current index in the
	 *  order list) as manual or autoservicing depot orders do not show up
	 *  in the orderlist, but they can be the current order of a vehicle.
	 * @return The AIOrderFlags of the order.
	 */
	static AIOrderFlags GetOrderFlags(VehicleID vehicle_id, OrderPosition order_position);

	/**
	 * Appends an order to the end of the vehicle's order list.
	 * @param vehicle_id The vehicle to append the order to.
	 * @param destination The destination of the order.
	 * @param order_flags The flags given to the order.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @pre AreOrderFlagsValid(destination, order_flags).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_NO_MORE_SPACE
	 * @exception AIOrder::ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION
	 * @return True if and only if the order was appended.
	 */
	static bool AppendOrder(VehicleID vehicle_id, TileIndex destination, AIOrderFlags order_flags);

	/**
	 * Inserts an order before the given order_position into the vehicle's order list.
	 * @param vehicle_id The vehicle to add the order to.
	 * @param order_position The order to place the new order before.
	 * @param destination The destination of the order.
	 * @param order_flags The flags given to the order.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre AreOrderFlagsValid(destination, order_flags).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_NO_MORE_SPACE
	 * @exception AIOrder::ERR_ORDER_TOO_FAR_AWAY_FROM_PREVIOUS_DESTINATION
	 * @return True if and only if the order was inserted.
	 */
	static bool InsertOrder(VehicleID vehicle_id, OrderPosition order_position, TileIndex destination, AIOrderFlags order_flags);

	/**
	 * Removes an order from the vehicle's order list.
	 * @param vehicle_id The vehicle to remove the order from.
	 * @param order_position The order to remove from the order list.
	 * @pre AIVehicle::IsValidVehicleOrder(vehicle_id, order_position).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was removed.
	 */
	static bool RemoveOrder(VehicleID vehicle_id, OrderPosition order_position);

#ifndef DOXYGEN_SKIP
	/**
	 * Internal function to help ChangeOrder.
	 */
	static bool _ChangeOrder();
#endif

	/**
	 * Changes the order flags of the given order.
	 * @param vehicle_id The vehicle to change the order of.
	 * @param order_position The order to change.
	 * @param order_flags The new flags given to the order.
	 * @pre IsValidVehicleOrder(vehicle_id, order_position).
	 * @pre AreOrderFlagsValid(GetOrderDestination(vehicle_id, order_position), order_flags).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return True if and only if the order was changed.
	 */
	static bool ChangeOrder(VehicleID vehicle_id, OrderPosition order_position, AIOrderFlags order_flags);

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
	 * Copies the orders from another vehicle. The orders of the main vehicle
	 *  are going to be the orders of the changed vehicle.
	 * @param vehicle_id The vehicle to copy the orders to.
	 * @param main_vehicle_id The vehicle to copy the orders from.
	 * @pre AIVehicle::IsValidVehicle(vehicle_id).
	 * @pre AIVehicle::IsValidVehicle(main_vehicle_id).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIOrder::ERR_ORDER_NO_MORE_SPACE
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
DECLARE_ENUM_AS_BIT_SET(AIOrder::AIOrderFlags);

#endif /* AI_ORDER_HPP */
