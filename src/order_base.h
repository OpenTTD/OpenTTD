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

	OrderTypeByte type;   ///< The type of order
	uint8 flags;          ///< 'Sub'type of order
	DestinationID dest;   ///< The destination of the order.

	CargoID refit_cargo;  ///< Refit CargoID
	byte refit_subtype;   ///< Refit subtype

public:
	Order *next;          ///< Pointer to next order. If NULL, end of list

	uint16 wait_time;    ///< How long in ticks to wait at the destination.
	uint16 travel_time;  ///< How long in ticks the journey to this destination should take.

	Order() : refit_cargo(CT_NO_REFIT) {}
	~Order() { this->type = OT_NOTHING; }

	/**
	 * Create an order based on a packed representation of that order.
	 * @param packed the packed representation.
	 */
	Order(uint32 packed);

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
	 * @param cargo       the cargo type to change to.
	 * @param subtype     the subtype to change to.
	 */
	void MakeGoToDepot(DepotID destination, OrderDepotTypeFlags order, CargoID cargo = CT_NO_REFIT, byte subtype = 0);

	/**
	 * Makes this order a Go To Waypoint order.
	 * @param destination the waypoint to go to.
	 */
	void MakeGoToWaypoint(WaypointID destination);

	/**
	 * Makes this order a Loading order.
	 * @param ordered is this an ordered stop?
	 */
	void MakeLoading(bool ordered);

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

	/**
	 * Gets the destination of this order.
	 * @pre IsType(OT_GOTO_WAYPOINT) || IsType(OT_GOTO_DEPOT) || IsType(OT_GOTO_STATION).
	 * @return the destination of the order.
	 */
	inline DestinationID GetDestination() const { return this->dest; }

	/**
	 * Sets the destination of this order.
	 * @param destination the new destination of the order.
	 * @pre IsType(OT_GOTO_WAYPOINT) || IsType(OT_GOTO_DEPOT) || IsType(OT_GOTO_STATION).
	 */
	inline void SetDestination(DestinationID destination) { this->dest = destination; }

	/**
	 * Is this order a refit order.
	 * @pre IsType(OT_GOTO_DEPOT)
	 * @return true if a refit should happen.
	 */
	inline bool IsRefit() const { return this->refit_cargo < NUM_CARGO; }

	/**
	 * Get the cargo to to refit to.
	 * @pre IsType(OT_GOTO_DEPOT)
	 * @return the cargo type.
	 */
	inline CargoID GetRefitCargo() const { return this->refit_cargo; }

	/**
	 * Get the cargo subtype to to refit to.
	 * @pre IsType(OT_GOTO_DEPOT)
	 * @return the cargo subtype.
	 */
	inline byte GetRefitSubtype() const { return this->refit_subtype; }

	/**
	 * Make this depot order also a refit order.
	 * @param cargo   the cargo type to change to.
	 * @param subtype the subtype to change to.
	 * @pre IsType(OT_GOTO_DEPOT).
	 */
	void SetRefit(CargoID cargo, byte subtype = 0);

	/** How must the consist be loaded? */
	inline OrderLoadFlags GetLoadType() const { return (OrderLoadFlags)(this->flags & OLFB_FULL_LOAD); }
	/** How must the consist be unloaded? */
	inline OrderUnloadFlags GetUnloadType() const { return (OrderUnloadFlags)GB(this->flags, 0, 2); }
	/** Where must we stop? */
	OrderNonStopFlags GetNonStopType() const;
	/** What caused us going to the depot? */
	inline OrderDepotTypeFlags GetDepotOrderType() const { return (OrderDepotTypeFlags)this->flags; }
	/** What are we going to do when in the depot. */
	inline OrderDepotActionFlags GetDepotActionType() const { return (OrderDepotActionFlags)this->flags; }

	/** Set how the consist must be loaded. */
	inline void SetLoadType(OrderLoadFlags load_type) { SB(this->flags, 2, 1, !!load_type); }
	/** Set how the consist must be unloaded. */
	inline void SetUnloadType(OrderUnloadFlags unload_type) { SB(this->flags, 0, 2, unload_type); }
	/** Set whether we must stop at stations or not. */
	inline void SetNonStopType(OrderNonStopFlags non_stop_type) { SB(this->flags, 3, 1, !!non_stop_type); }
	/** Set the cause to go to the depot. */
	inline void SetDepotOrderType(OrderDepotTypeFlags depot_order_type) { this->flags = depot_order_type; }
	/** Set what we are going to do in the depot. */
	inline void SetDepotActionType(OrderDepotActionFlags depot_service_type) { this->flags = depot_service_type; }

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

	/**
	 * Pack this order into a 32 bits integer, or actually only
	 * the type, flags and destination.
	 * @return the packed representation.
	 * @note unpacking is done in the constructor.
	 */
	uint32 Pack() const;
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
Order UnpackOldOrder(uint16 packed);

#endif /* ORDER_H */
