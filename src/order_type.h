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


/* Order flags -- please use OF instead OF and use HASBIT/SETBIT/CLEARBIT */

/** Order flag masks - these are for direct bit operations */
enum OrderFlagMasks {
	//Flags for stations:
	/** vehicle will transfer cargo (i. e. not deliver to nearby industry/town even if accepted there) */
	OFB_TRANSFER           = 0x1,
	/** If OFB_TRANSFER is not set, drop any cargo loaded. If accepted, deliver, otherwise cargo remains at the station.
      * No new cargo is loaded onto the vehicle whatsoever */
	OFB_UNLOAD             = 0x2,
	/** Wait for full load of all vehicles, or of at least one cargo type, depending on patch setting
	  * @todo make this two different flags */
	OFB_FULL_LOAD          = 0x4,
};

/**
 * Non-stop order flags.
 */
enum OrderNonStopFlags {
	ONSF_STOP_EVERYWHERE                  = 0,
	ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS = 1,
	ONSF_NO_STOP_AT_DESTINATION_STATION   = 2,
	ONSF_NO_STOP_AT_ANY_STATION           = 3
};

/**
 * Reasons that could cause us to go to the depot.
 */
enum OrderDepotTypeFlags {
	ODTF_MANUAL          = 0,      ///< The player initiated this order manually.
	ODTFB_SERVICE        = 1 << 2, ///< This depot order is because of the servicing limit.
	ODTFB_PART_OF_ORDERS = 1 << 1, ///< This depot order is because of a regular order.
};

/**
 * Actions that can be performed when the vehicle enters the depot.
 */
enum OrderDepotActionFlags {
	ODATF_SERVICE_ONLY   = 0,      ///< Only service the vehicle.
	ODATFB_HALT          = 1 << 2, ///< Service the vehicle and then halt it.
};

/** Order flags bits - these are for the *BIT macros
 * for descrption of flags, see OrderFlagMasks
 * @see OrderFlagMasks
 */
enum {
	OF_TRANSFER          = 0,
	OF_UNLOAD            = 1,
	OF_FULL_LOAD         = 2,
	OF_NON_STOP          = 3
};


/* Possible clone options */
enum {
	CO_SHARE   = 0,
	CO_COPY    = 1,
	CO_UNSHARE = 2
};

struct Order;

#endif /* ORDER_TYPE_H */
