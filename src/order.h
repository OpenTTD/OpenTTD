/* $Id$ */

/** @file order.h */

#ifndef ORDER_H
#define ORDER_H

#include "order_type.h"
#include "oldpool.h"
#include "core/bitmath_func.hpp"
#include "cargo_type.h"
#include "vehicle_type.h"
#include "tile_type.h"
#include "date_type.h"
#include "group_type.h"

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
	BackuppedOrders() : order(NULL), name(NULL) { }
	~BackuppedOrders() { free(order); free(name); }

	VehicleID clone;
	VehicleOrderID orderindex;
	GroupID group;
	Order *order;
	uint16 service_interval;
	char *name;
};

extern TileIndex _backup_orders_tile;
extern BackuppedOrders _backup_orders_data;

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
void AssignOrder(Order *order, Order data);
bool CheckForValidOrders(const Vehicle* v);

Order UnpackOldOrder(uint16 packed);

#define MIN_SERVINT_PERCENT  5
#define MAX_SERVINT_PERCENT 90
#define MIN_SERVINT_DAYS    30
#define MAX_SERVINT_DAYS   800

/**
 * Get the service interval domain.
 * Get the new proposed service interval for the vehicle is indeed, clamped
 * within the given bounds. @see MIN_SERVINT_PERCENT ,etc.
 * @param index proposed service interval
 * @return service interval
 */
Date GetServiceIntervalClamped(uint index);

#endif /* ORDER_H */
