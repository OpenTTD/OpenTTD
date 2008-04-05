/* $Id$ */

/** @file order_base.h */

#ifndef ORDER_BASE_H
#define ORDER_BASE_H

#include "order_type.h"
#include "oldpool.h"
#include "core/bitmath_func.hpp"
#include "cargo_type.h"
#include "depot_type.h"
#include "station_type.h"
#include "vehicle_type.h"
#include "waypoint_type.h"

DECLARE_OLD_POOL(Order, Order, 6, 1000)

/* If you change this, keep in mind that it is saved on 3 places:
 * - Load_ORDR, all the global orders
 * - Vehicle -> current_order
 * - REF_ORDER (all REFs are currently limited to 16 bits!!)
 */
struct Order : PoolItem<Order, OrderID, &_Order_pool> {
private:
	friend const struct SaveLoad *GetVehicleDescription(VehicleType vt); ///< Saving and loading the current order of vehicles.
	friend void Load_VEHS();                                             ///< Loading of ancient vehicles.
	friend const struct SaveLoad *GetOrderDescription();                 ///< Saving and loading of orders.
	friend uint32 PackOrder(const Order *order);                         ///< 'Compressing' an order.
	friend Order UnpackOrder(uint32 packed);                             ///< 'Uncompressing' an order.
	friend Order UnpackOldOrder(uint16 packed);                          ///< 'Uncompressing' a loaded old order.
	friend Order UnpackVersion4Order(uint16 packed);                     ///< 'Uncompressing' a loaded ancient order.

	OrderTypeByte type;

public:
	Order *next;          ///< Pointer to next order. If NULL, end of list

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
	 * @return true if the order is valid.
	 */
	inline bool IsValid() const { return this->type != OT_NOTHING; }

	/**
	 * Check whether this order is of the given type.
	 * @param type the type to check against.
	 * @return true if the order matches.
	 */
	inline bool IsType(OrderType type) const { return this->type == type; }

	/**
	 * Get the type of order of this order.
	 * @return the order type.
	 */
	inline OrderType GetType() const { return this->type; }

	/**
	 * 'Free' the order
	 * @note ONLY use on "current_order" vehicle orders!
	 */
	void Free();

	/**
	 * Makes this order a Go To Station order.
	 * @param destsination the station to go to.
	 */
	void MakeGoToStation(StationID destination);

	/**
	 * Makes this order a Go To Depot order.
	 * @param destination the depot to go to.
	 * @param order       is this order a 'default' order, or an overriden vehicle order?
	 */
	void MakeGoToDepot(DepotID destination, bool order);

	/**
	 * Makes this order a Go To Waypoint order.
	 * @param destination the waypoint to go to.
	 */
	void MakeGoToWaypoint(WaypointID destination);

	/**
	 * Makes this order a Loading order.
	 */
	void MakeLoading();

	/**
	 * Makes this order a Leave Station order.
	 */
	void MakeLeaveStation();

	/**
	 * Makes this order a Dummy order.
	 */
	void MakeDummy();

	/**
	 * Free a complete order chain.
	 * @note do not use on "current_order" vehicle orders!
	 */
	void FreeChain();

	bool ShouldStopAtStation(const Vehicle *v, StationID station) const;

	/**
	 * Assign the given order to this one.
	 * @param other the data to copy (except next pointer).
	 */
	void AssignOrder(const Order &other);

	/**
	 * Does this order have the same type, flags and destination?
	 * @param other the second order to compare to.
	 * @return true if the type, flags and destination match.
	 */
	bool Equals(const Order &other) const;
};

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

#define FOR_ALL_ORDERS_FROM(order, start) for (order = GetOrder(start); order != NULL; order = (order->index + 1U < GetOrderPoolSize()) ? GetOrder(order->index + 1U) : NULL) if (order->IsValid())
#define FOR_ALL_ORDERS(order) FOR_ALL_ORDERS_FROM(order, 0)


#define FOR_VEHICLE_ORDERS(v, order) for (order = v->orders; order != NULL; order = order->next)

/* (Un)pack routines */
uint32 PackOrder(const Order *order);
Order UnpackOrder(uint32 packed);
Order UnpackOldOrder(uint16 packed);

#endif /* ORDER_H */
