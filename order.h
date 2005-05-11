#ifndef ORDER_H
#define ORDER_H

#include "pool.h"

/* Order types */
enum {
	OT_NOTHING       = 0,
	OT_GOTO_STATION  = 1,
	OT_GOTO_DEPOT    = 2,
	OT_LOADING       = 3,
	OT_LEAVESTATION  = 4,
	OT_DUMMY         = 5,
	OT_GOTO_WAYPOINT = 6
};

/* Order flags -- please use OFB instead OF and use HASBIT/SETBIT/CLEARBIT */

/* Order flag masks - these are for direct bit operations */
enum {
	//Flags for stations:
	OF_UNLOAD         = 0x2,
	OF_FULL_LOAD      = 0x4, // Also used when to force an aircraft into a depot

	//Flags for depots:
	OF_PART_OF_ORDERS	= 0x2,
	OF_HALT_IN_DEPOT  = 0x4,

	//Common flags
	OF_NON_STOP  = 0x8
};

/* Order flags bits - these are for the *BIT macros */
enum {
	OFB_UNLOAD         = 1,
	OFB_FULL_LOAD      = 2,
	OFB_PART_OF_ORDERS = 1,
	OFB_HALT_IN_DEPOT  = 2,
	OFB_NON_STOP       = 3
};


/* Possible clone options */
enum {
	CO_SHARE   = 0,
	CO_COPY    = 1,
	CO_UNSHARE = 2
};

/* Modes for the order checker */
enum {
	OC_INIT     = 0, //the order checker can initialize a news message
	OC_VALIDATE = 1, //the order checker validates a news message
};

/* If you change this, keep in mind that it is saved on 3 places:
    - Load_ORDR, all the global orders
    - Vehicle -> current_order
    - REF_SHEDULE (all REFs are currently limited to 16 bits!!) */
typedef struct Order {
	uint8  type;
	uint8  flags;
	uint16 station;

	struct Order *next;   //! Pointer to next order. If NULL, end of list

	uint16 index;         //! Index of the order, is not saved or anything, just for reference
} Order;

typedef struct {
	VehicleID clone;
	OrderID orderindex;
	Order order[41];
	uint16 service_interval;
	char name[32];
} BackuppedOrders;

VARDEF TileIndex _backup_orders_tile;
VARDEF BackuppedOrders _backup_orders_data[1];

extern MemoryPool _order_pool;

/**
 * Get the pointer to the order with index 'index'
 */
static inline Order *GetOrder(uint index)
{
	return (Order*)GetItemFromPool(&_order_pool, index);
}

/**
 * Get the current size of the OrderPool
 */
static inline uint16 GetOrderPoolSize(void)
{
	return _order_pool.total_items;
}

#define FOR_ALL_ORDERS_FROM(order, start) for (order = GetOrder(start); order != NULL; order = (order->index + 1 < GetOrderPoolSize()) ? GetOrder(order->index + 1) : NULL)
#define FOR_ALL_ORDERS(order) FOR_ALL_ORDERS_FROM(order, 0)


#define FOR_VEHICLE_ORDERS(v, order) for (order = v->orders; order != NULL; order = order->next)

static inline bool HasOrderPoolFree(uint amount)
{
	const Order *order;

	/* There is always room if not all blocks in the pool are reserved */
	if (_order_pool.current_blocks < _order_pool.max_blocks)
		return true;

	FOR_ALL_ORDERS(order)
		if (order->type == OT_NOTHING)
			if (--amount == 0)
				return true;

	return false;
}

static inline bool IsOrderPoolFull(void)
{
	return !HasOrderPoolFree(1);
}

/* Pack and unpack routines */

static inline uint32 PackOrder(const Order *order)
{
	return order->station << 16 | order->flags << 8 | order->type;
}

static inline Order UnpackOrder(uint32 packed)
{
	Order order;
	order.type    = (packed & 0x000000FF);
	order.flags   = (packed & 0x0000FF00) >> 8;
	order.station = (packed & 0xFFFF0000) >> 16;
	order.next    = NULL;
	return order;
}

/* Functions */
void BackupVehicleOrders(Vehicle *v, BackuppedOrders *order);
void RestoreVehicleOrders(Vehicle *v, BackuppedOrders *order);
void DeleteDestinationFromVehicleOrder(Order dest);
void InvalidateVehicleOrder(const Vehicle *v);
bool VehicleHasDepotOrders(const Vehicle *v);
bool CheckOrders(uint data_a, uint data_b);
void DeleteVehicleOrders(Vehicle *v);
bool IsOrderListShared(const Vehicle *v);
void AssignOrder(Order *order, Order data);
bool CheckForValidOrders(Vehicle *v);

Order UnpackVersion4Order(uint16 packed);
Order UnpackOldOrder(uint16 packed);

#endif /* ORDER_H */
