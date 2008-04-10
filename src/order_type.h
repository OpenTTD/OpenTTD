/* $Id$ */

/** @file order_type.h Types related to orders. */

#ifndef ORDER_TYPE_H
#define ORDER_TYPE_H

#include "core/enum_type.hpp"

typedef byte VehicleOrderID;  ///< The index of an order within its current vehicle (not pool related)
typedef uint16 OrderID;
typedef uint16 DestinationID;

enum {
	INVALID_VEH_ORDER_ID = 0xFF,
};

static const OrderID INVALID_ORDER = 0xFFFF;

/* Order types */
enum OrderType {
	OT_BEGIN         = 0,
	OT_NOTHING       = 0,
	OT_GOTO_STATION  = 1,
	OT_GOTO_DEPOT    = 2,
	OT_LOADING       = 3,
	OT_LEAVESTATION  = 4,
	OT_DUMMY         = 5,
	OT_GOTO_WAYPOINT = 6,
	OT_END
};

/* It needs to be 8bits, because we save and load it as such */
/** Define basic enum properties */
template <> struct EnumPropsT<OrderType> : MakeEnumPropsT<OrderType, byte, OT_BEGIN, OT_END, OT_END> {};
typedef TinyEnumT<OrderType> OrderTypeByte;


/**
 * Flags related to the unloading order.
 */
enum OrderUnloadFlags {
	OUF_UNLOAD_IF_POSSIBLE = 0,      ///< Unload all cargo that the station accepts.
	OUFB_UNLOAD            = 1 << 0, ///< Force unloading all cargo onto the platform, possibly not getting paid.
	OUFB_TRANSFER          = 1 << 1, ///< Transfer all cargo onto the platform.
	OUFB_NO_UNLOAD         = 1 << 2, ///< Totally no unloading will be done.
};

/**
 * Flags related to the loading order.
 */
enum OrderLoadFlags {
	OLF_LOAD_IF_POSSIBLE = 0,      ///< Load as long as there is cargo that fits in the train.
	OLFB_FULL_LOAD       = 1 << 1, ///< Full load the complete the consist.
	OLF_FULL_LOAD_ANY    = 3,      ///< Full load the a single cargo of the consist.
	OLFB_NO_LOAD         = 4,      ///< Do not load anything.
};

/**
 * Non-stop order flags.
 */
enum OrderNonStopFlags {
	ONSF_STOP_EVERYWHERE                  = 0, ///< The vehicle will stop at any station it passes and the destination.
	ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS = 1, ///< The vehicle will not stop at any stations it passes except the destination.
	ONSF_NO_STOP_AT_DESTINATION_STATION   = 2, ///< The vehicle will stop at any station it passes except the destination.
	ONSF_NO_STOP_AT_ANY_STATION           = 3, ///< The vehicle will not stop at any stations it passes including the destination.
	ONSF_END
};

/**
 * Reasons that could cause us to go to the depot.
 */
enum OrderDepotTypeFlags {
	ODTF_MANUAL          = 0,      ///< The player initiated this order manually.
	ODTFB_SERVICE        = 1 << 0, ///< This depot order is because of the servicing limit.
	ODTFB_PART_OF_ORDERS = 1 << 1, ///< This depot order is because of a regular order.
};

/**
 * Actions that can be performed when the vehicle enters the depot.
 */
enum OrderDepotActionFlags {
	ODATF_SERVICE_ONLY   = 0,      ///< Only service the vehicle.
	ODATFB_HALT          = 1 << 0, ///< Service the vehicle and then halt it.
};

/**
 * Enumeration for the data to set in CmdModifyOrder.
 */
enum ModifyOrderFlags {
	MOF_NON_STOP,      ///< Passes a OrderNonStopFlags.
	MOF_UNLOAD,        ///< Passes an OrderUnloadType.
	MOF_LOAD,          ///< Passes an OrderLoadType
	MOF_DEPOT_ACTION,  ///< Toggle the 'service' if needed flag.
};


/* Possible clone options */
enum {
	CO_SHARE   = 0,
	CO_COPY    = 1,
	CO_UNSHARE = 2
};

struct Order;

#endif /* ORDER_TYPE_H */
