#ifndef ORDER_H
#define ORDER_H

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
enum {
	OF_UNLOAD    = 0x2,
	OF_FULL_LOAD = 0x4, // Also used when to force an aircraft into a depot
	OF_NON_STOP  = 0x8
};

/* Order flags bits */
enum {
	OFB_UNLOAD    = 1,
	OFB_FULL_LOAD = 2,
	OFB_NON_STOP  = 3
};

/* Possible clone options */
enum {
	CO_SHARE   = 0,
	CO_COPY    = 1,
	CO_UNSHARE = 2
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
	byte orderindex;
	Order order[41];
	uint16 service_interval;
	char name[32];
} BackuppedOrders;

VARDEF TileIndex _backup_orders_tile;
VARDEF BackuppedOrders _backup_orders_data[1];

VARDEF Order _orders[5000];
VARDEF uint32 _orders_size;

static inline Order *GetOrder(uint index)
{
	assert(index < _orders_size);
	return &_orders[index];
}

#define FOR_ALL_ORDERS_FROM(o, from) for(o = GetOrder(from); o != &_orders[_orders_size]; o++)
#define FOR_ALL_ORDERS(o) FOR_ALL_ORDERS_FROM(o, 0)

#define FOR_VEHICLE_ORDERS(v, order) for (order = v->orders; order != NULL; order = order->next)

static inline bool HasOrderPoolFree(uint amount)
{
	const Order *order;

	FOR_ALL_ORDERS(order)
		if (order->type == OT_NOTHING)
			if (--amount == 0)
				return true;

	return false;
}

static inline bool IsOrderPoolFull()
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
bool CheckOrders(const Vehicle *v);
void DeleteVehicleOrders(Vehicle *v);
bool IsOrderListShared(const Vehicle *v);
void AssignOrder(Order *order, Order data);
bool CheckForValidOrders(Vehicle *v);

Order UnpackVersion4Order(uint16 packed);
Order UnpackOldOrder(uint16 packed);

#endif /* ORDER_H */
