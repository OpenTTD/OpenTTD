/* $Id$ */

/** @file order.h */

#ifndef ORDER_H
#define ORDER_H

#include "oldpool.h"
#include "core/bitmath_func.hpp"

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


/* Order flags -- please use OFB instead OF and use HASBIT/SETBIT/CLEARBIT */

/** Order flag masks - these are for direct bit operations */
enum OrderFlagMasks {
	//Flags for stations:
	/** vehicle will transfer cargo (i. e. not deliver to nearby industry/town even if accepted there) */
	OF_TRANSFER           = 0x1,
	/** If OF_TRANSFER is not set, drop any cargo loaded. If accepted, deliver, otherwise cargo remains at the station.
      * No new cargo is loaded onto the vehicle whatsoever */
	OF_UNLOAD             = 0x2,
	/** Wait for full load of all vehicles, or of at least one cargo type, depending on patch setting
	  * @todo make this two different flags */
	OF_FULL_LOAD          = 0x4,

	//Flags for depots:
	/** The current depot-order was initiated because it was in the vehicle's order list */
	OF_PART_OF_ORDERS     = 0x2,
	/** if OF_PART_OF_ORDERS is not set, this will cause the vehicle to be stopped in the depot */
	OF_HALT_IN_DEPOT      = 0x4,
	/** if OF_PART_OF_ORDERS is set, this will cause the order only be come active if the vehicle needs servicing */
	OF_SERVICE_IF_NEEDED  = 0x4, //used when OF_PART_OF_ORDERS is set.

	//Common flags
	/** This causes the vehicle not to stop at intermediate OR the destination station (depending on patch settings)
	  * @todo make this two different flags */
	OF_NON_STOP           = 0x8
};

/** Order flags bits - these are for the *BIT macros
 * for descrption of flags, see OrderFlagMasks
 * @see OrderFlagMasks
 */
enum {
	OFB_TRANSFER          = 0,
	OFB_UNLOAD            = 1,
	OFB_FULL_LOAD         = 2,
	OFB_PART_OF_ORDERS    = 1,
	OFB_HALT_IN_DEPOT     = 2,
	OFB_SERVICE_IF_NEEDED = 2,
	OFB_NON_STOP          = 3
};


/* Possible clone options */
enum {
	CO_SHARE   = 0,
	CO_COPY    = 1,
	CO_UNSHARE = 2
};

struct Order;
DECLARE_OLD_POOL(Order, Order, 6, 1000)

/* If you change this, keep in mind that it is saved on 3 places:
 * - Load_ORDR, all the global orders
 * - Vehicle -> current_order
 * - REF_ORDER (all REFs are currently limited to 16 bits!!)
 */
struct Order : PoolItem<Order, OrderID, &_Order_pool> {
	Order *next;          ///< Pointer to next order. If NULL, end of list

	OrderTypeByte type;
	uint8  flags;
	DestinationID dest;   ///< The destionation of the order.

	CargoID refit_cargo; // Refit CargoID
	byte refit_subtype; // Refit subtype

	uint16 wait_time;    ///< How long in ticks to wait at the destination.
	uint16 travel_time;  ///< How long in ticks the journey to this destination should take.

	Order() : refit_cargo(CT_NO_REFIT) {}
	~Order() { this->type = OT_NOTHING; }

	/**
	 * Check if a Order really exists.
	 */
	inline bool IsValid() const { return this->type != OT_NOTHING; }

	void Free();
	void FreeChain();
};

struct BackuppedOrders {
	BackuppedOrders() : order(NULL) { }

	VehicleID clone;
	VehicleOrderID orderindex;
	Order *order;
	uint16 service_interval;
	char name[32];
};

VARDEF TileIndex _backup_orders_tile;
VARDEF BackuppedOrders _backup_orders_data;

static inline VehicleOrderID GetMaxOrderIndex()
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetOrderPoolSize() - 1;
}

static inline VehicleOrderID GetNumOrders()
{
	return GetOrderPoolSize();
}

inline void Order::Free()
{
	this->type  = OT_NOTHING;
	this->flags = 0;
	this->dest  = 0;
	this->next  = NULL;
}

inline void Order::FreeChain()
{
	if (next != NULL) next->FreeChain();
	delete this;
}

#define FOR_ALL_ORDERS_FROM(order, start) for (order = GetOrder(start); order != NULL; order = (order->index + 1U < GetOrderPoolSize()) ? GetOrder(order->index + 1U) : NULL) if (order->IsValid())
#define FOR_ALL_ORDERS(order) FOR_ALL_ORDERS_FROM(order, 0)


#define FOR_VEHICLE_ORDERS(v, order) for (order = v->orders; order != NULL; order = order->next)

static inline bool HasOrderPoolFree(uint amount)
{
	const Order *order;

	/* There is always room if not all blocks in the pool are reserved */
	if (_Order_pool.CanAllocateMoreBlocks()) return true;

	FOR_ALL_ORDERS(order) if (!order->IsValid() && --amount == 0) return true;

	return false;
}


/* Pack and unpack routines */

static inline uint32 PackOrder(const Order *order)
{
	return order->dest << 16 | order->flags << 8 | order->type;
}

static inline Order UnpackOrder(uint32 packed)
{
	Order order;
	order.type    = (OrderType)GB(packed,  0,  8);
	order.flags   = GB(packed,  8,  8);
	order.dest    = GB(packed, 16, 16);
	order.next    = NULL;
	order.index   = 0; // avoid compiler warning
	order.refit_cargo   = CT_NO_REFIT;
	order.refit_subtype = 0;
	order.wait_time     = 0;
	order.travel_time   = 0;
	return order;
}

/* Functions */
void BackupVehicleOrders(const Vehicle *v, BackuppedOrders *order = &_backup_orders_data);
void RestoreVehicleOrders(const Vehicle *v, const BackuppedOrders *order = &_backup_orders_data);
void RemoveOrderFromAllVehicles(OrderType type, DestinationID destination);
void InvalidateVehicleOrder(const Vehicle *v);
bool VehicleHasDepotOrders(const Vehicle *v);
void CheckOrders(const Vehicle*);
void DeleteVehicleOrders(Vehicle *v);
bool IsOrderListShared(const Vehicle *v);
void AssignOrder(Order *order, Order data);
bool CheckForValidOrders(const Vehicle* v);

Order UnpackOldOrder(uint16 packed);

#endif /* ORDER_H */
