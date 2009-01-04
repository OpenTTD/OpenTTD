/* $Id$ */

/** @file order_base.h Base class for orders. */

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
DECLARE_OLD_POOL(OrderList, OrderList, 4, 4000)

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

	uint8 type;           ///< The type of order + non-stop flags
	uint8 flags;          ///< Load/unload types, depot order/action types.
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
	inline bool IsType(OrderType type) const { return this->GetType() == type; }

	/**
	 * Get the type of order of this order.
	 * @return the order type.
	 */
	inline OrderType GetType() const { return (OrderType)GB(this->type, 0, 4); }

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
	 * @param action      what to do in the depot?
	 * @param cargo       the cargo type to change to.
	 * @param subtype     the subtype to change to.
	 */
	void MakeGoToDepot(DepotID destination, OrderDepotTypeFlags order, OrderDepotActionFlags action = ODATF_SERVICE_ONLY, CargoID cargo = CT_NO_REFIT, byte subtype = 0);

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
	 * Makes this order an conditional order.
	 * @param order the order to jump to.
	 */
	void MakeConditional(VehicleOrderID order);

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
	inline OrderLoadFlags GetLoadType() const { return (OrderLoadFlags)GB(this->flags, 4, 4); }
	/** How must the consist be unloaded? */
	inline OrderUnloadFlags GetUnloadType() const { return (OrderUnloadFlags)GB(this->flags, 0, 4); }
	/** Where must we stop? */
	inline OrderNonStopFlags GetNonStopType() const { return (OrderNonStopFlags)GB(this->type, 6, 2); }
	/** What caused us going to the depot? */
	inline OrderDepotTypeFlags GetDepotOrderType() const { return (OrderDepotTypeFlags)GB(this->flags, 0, 4); }
	/** What are we going to do when in the depot. */
	inline OrderDepotActionFlags GetDepotActionType() const { return (OrderDepotActionFlags)GB(this->flags, 4, 4); }
	/** What variable do we have to compare? */
	inline OrderConditionVariable GetConditionVariable() const { return (OrderConditionVariable)GB(this->dest, 11, 5); }
	/** What is the comparator to use? */
	inline OrderConditionComparator GetConditionComparator() const { return (OrderConditionComparator)GB(this->type, 5, 3); }
	/** Get the order to skip to. */
	inline VehicleOrderID GetConditionSkipToOrder() const { return this->flags; }
	/** Get the value to base the skip on. */
	inline uint16 GetConditionValue() const { return GB(this->dest, 0, 11); }

	/** Set how the consist must be loaded. */
	inline void SetLoadType(OrderLoadFlags load_type) { SB(this->flags, 4, 4, load_type); }
	/** Set how the consist must be unloaded. */
	inline void SetUnloadType(OrderUnloadFlags unload_type) { SB(this->flags, 0, 4, unload_type); }
	/** Set whether we must stop at stations or not. */
	inline void SetNonStopType(OrderNonStopFlags non_stop_type) { SB(this->type, 6, 2, non_stop_type); }
	/** Set the cause to go to the depot. */
	inline void SetDepotOrderType(OrderDepotTypeFlags depot_order_type) { SB(this->flags, 0, 4, depot_order_type); }
	/** Set what we are going to do in the depot. */
	inline void SetDepotActionType(OrderDepotActionFlags depot_service_type) { SB(this->flags, 4, 4, depot_service_type); }
	/** Set variable we have to compare. */
	inline void SetConditionVariable(OrderConditionVariable condition_variable) { SB(this->dest, 11, 5, condition_variable); }
	/** Set the comparator to use. */
	inline void SetConditionComparator(OrderConditionComparator condition_comparator) { SB(this->type, 5, 3, condition_comparator); }
	/** Get the order to skip to. */
	inline void SetConditionSkipToOrder(VehicleOrderID order_id) { this->flags = order_id; }
	/** Set the value to base the skip on. */
	inline void SetConditionValue(uint16 value) { SB(this->dest, 0, 11, value); }

	bool ShouldStopAtStation(const Vehicle *v, StationID station) const;

	/** Checks if this order has travel_time and if needed wait_time set. */
	inline bool IsCompletelyTimetabled() const
	{
		if (this->travel_time == 0 && !this->IsType(OT_CONDITIONAL)) return false;
		if (this->wait_time == 0 && this->IsType(OT_GOTO_STATION) && !(this->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) return false;
		return true;
	}

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

	/**
	 * Converts this order from an old savegame's version;
	 * it moves all bits to the new location.
	 */
	void ConvertFromOldSavegame();
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

/** Shared order list linking together the linked list of orders and the list
 *  of vehicles sharing this order list.
 */
struct OrderList : PoolItem<OrderList, OrderListID, &_OrderList_pool> {
private:
	friend void AfterLoadVehicles(bool part_of_load); ///< For instantiating the shared vehicle chain
	friend const struct SaveLoad *GetOrderListDescription(); ///< Saving and loading of order lists.

	Order *first;                   ///< First order of the order list
	VehicleOrderID num_orders;      ///< NOSAVE: How many orders there are in the list
	uint num_vehicles;              ///< NOSAVE: Number of vehicles that share this order list
	Vehicle *first_shared;          ///< NOSAVE: pointer to the first vehicle in the shared order chain

	uint timetable_duration;        ///< NOSAVE: Total duration of the order list

public:
	/** Default constructor producing an invalid order list. */
	OrderList()
		: first(NULL), num_orders(INVALID_VEH_ORDER_ID), num_vehicles(0), first_shared(NULL),
		  timetable_duration(0) { }

	/** Create an order list with the given order chain for the given vehicle.
	 *  @param chain is the pointer to the first order of the order chain
	 *  @param v is any vehicle of the shared order vehicle chain (does not need to be the first)
	 */
	OrderList(Order *chain, Vehicle *v);

	/** Destructor. Invalidates OrderList for re-usage by the pool. */
	~OrderList() { this->num_orders = INVALID_VEH_ORDER_ID; }

	/** Checks, if this is a valid order list. */
	inline bool IsValid() const { return this->num_orders != INVALID_VEH_ORDER_ID; }

	/**
	 * Get the first order of the order chain.
	 * @return the first order of the chain.
	 */
	inline Order *GetFirstOrder() const { return this->first; }

	/**
	 * Get a certain order of the order chain.
	 * @param index zero-based index of the order within the chain.
	 * @return the order at position index.
	 */
	Order *GetOrderAt(int index) const;

	/**
	 * Get the last order of the order chain.
	 * @return the last order of the chain.
	 */
	inline Order *GetLastOrder() const { return this->GetOrderAt(this->num_orders - 1); }

	/**
	 * Get number of orders in the order list.
	 * @return number of orders in the chain. */
	inline VehicleOrderID GetNumOrders() const { return this->num_orders; }

	/**
	 * Insert a new order into the order chain.
	 * @param new_order is the order to insert into the chain.
	 * @param index is the position where the order is supposed to be inserted. */
	void InsertOrderAt(Order *new_order, int index);

	/**
	 * Remove an order from the order list and delete it.
	 * @param index is the position of the order which is to be deleted.
	 */
	void DeleteOrderAt(int index);

	/**
	 * Move an order to another position within the order list.
	 * @param from is the zero-based position of the order to move.
	 * @param to is the zero-based position where the order is moved to. */
	void MoveOrder(int from, int to);

	/**
	 * Is this a shared order list?
	 * @return whether this order list is shared among multiple vehicles
	 */
	inline bool IsShared() const { return this->num_vehicles > 1; };

	/**
	 * Get the first vehicle of this vehicle chain.
	 * @return the first vehicle of the chain.
	 */
	inline Vehicle *GetFirstSharedVehicle() const { return this->first_shared; }

	/**
	 * Return the number of vehicles that share this orders list
	 * @return the count of vehicles that use this shared orders list
	 */
	inline uint GetNumVehicles() const { return this->num_vehicles; }

	/**
	 * Checks whether a vehicle is part of the shared vehicle chain.
	 * @param v is the vehicle to search in the shared vehicle chain.
	 */
	bool IsVehicleInSharedOrdersList(const Vehicle *v) const;

	/**
	 * Gets the position of the given vehicle within the shared order vehicle list.
	 * @param v is the vehicle of which to get the position
	 * @return position of v within the shared vehicle chain.
	 */
	int GetPositionInSharedOrderList(const Vehicle *v) const;

	/**
	 * Adds the given vehicle to this shared order list.
	 * @note This is supposed to be called after the vehicle has been inserted
	 *       into the shared vehicle chain.
	 * @param v vehicle to add to the list
	 */
	inline void AddVehicle(Vehicle *v) { ++this->num_vehicles; }

	/**
	 * Removes the vehicle from the shared order list.
	 * @note This is supposed to be called when the vehicle is still in the chain
	 * @param v vehicle to remove from the list
	 */
	void RemoveVehicle(Vehicle *v);

	/**
	 * Checks whether all orders of the list have a filled timetable.
	 * @return whether all orders have a filled timetable.
	 */
	bool IsCompleteTimetable() const;

	/**
	 * Gets the total duration of the vehicles timetable or -1 is the timetable is not complete.
	 * @return total timetable duration or -1 for incomplete timetables
	 */
	inline int GetTimetableTotalDuration() const { return this->IsCompleteTimetable() ? (int)this->timetable_duration : -1; }

	/**
	 * Gets the known duration of the vehicles timetable even if the timetable is not complete.
	 * @return known timetable duration
	 */
	inline int GetTimetableDurationIncomplete() const { return this->timetable_duration; }

	/**
	 * Must be called if an order's timetable is changed to update internal book keeping.
	 * @param delta By how many ticks has the timetable duration changed
	 */
	void UpdateOrderTimetable(int delta) { this->timetable_duration += delta; }

	/**
	 * Must be called if the whole timetable is cleared to update internal book keeping.
	 */
	void ResetOrderTimetable() { this->timetable_duration = 0; }

	/**
	 * Free a complete order chain.
	 * @param keep_orderlist If this is true only delete the orders, otherwise also delete the OrderList.
	 * @note do not use on "current_order" vehicle orders!
	 */
	void FreeChain(bool keep_orderlist = false);

	/**
	 * Checks for internal consistency of order list. Triggers assertion if something is wrong.
	 */
	void DebugCheckSanity() const;
};

static inline bool IsValidOrderListID(uint index)
{
	return index < GetOrderListPoolSize() && GetOrderList(index)->IsValid();
}

#define FOR_ALL_ORDERS_FROM(order, start) for (order = GetOrder(start); order != NULL; order = (order->index + 1U < GetOrderPoolSize()) ? GetOrder(order->index + 1U) : NULL) if (order->IsValid())
#define FOR_ALL_ORDERS(order) FOR_ALL_ORDERS_FROM(order, 0)


#define FOR_VEHICLE_ORDERS(v, order) for (order = (v->orders.list == NULL) ? NULL : v->orders.list->GetFirstOrder(); order != NULL; order = order->next)


#define FOR_ALL_ORDER_LISTS_FROM(ol, start) for (ol = GetOrderList(start); ol != NULL; ol = (ol->index + 1U < GetOrderListPoolSize()) ? GetOrderList(ol->index + 1U) : NULL) if (ol->IsValid())
#define FOR_ALL_ORDER_LISTS(ol) FOR_ALL_ORDER_LISTS_FROM(ol, 0)

#endif /* ORDER_H */
