/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file order_type.h Types related to orders. */

#ifndef ORDER_TYPE_H
#define ORDER_TYPE_H

#include "core/enum_type.hpp"
#include "depot_type.h"
#include "core/pool_type.hpp"
#include "station_type.h"

typedef uint8_t VehicleOrderID;  ///< The index of an order within its current vehicle (not pool related)
using OrderListID = PoolID<uint16_t, struct OrderListIDTag, 64000, 0xFFFF>;

struct DestinationID {
	using BaseType = uint16_t;
	BaseType value = 0;

	explicit DestinationID() = default;
	constexpr DestinationID(size_t index) : value(static_cast<BaseType>(index)) {}
	constexpr DestinationID(DepotID depot) : value(depot.base()) {}
	constexpr DestinationID(StationID station) : value(station.base()) {}

	constexpr DepotID ToDepotID() const noexcept { return static_cast<DepotID>(this->value); }
	constexpr StationID ToStationID() const noexcept { return static_cast<StationID>(this->value); }
	constexpr BaseType base() const noexcept { return this->value; }

	constexpr bool operator ==(const DestinationID &destination) const { return this->value == destination.value; }
	constexpr bool operator ==(const StationID &station) const { return this->value == station; }
};

/** Invalid vehicle order index (sentinel) */
static const VehicleOrderID INVALID_VEH_ORDER_ID = 0xFF;
/** Last valid VehicleOrderID. */
static const VehicleOrderID MAX_VEH_ORDER_ID     = INVALID_VEH_ORDER_ID - 1;

/**
 * Maximum number of orders in implicit-only lists before we start searching
 * harder for duplicates.
 */
static const uint IMPLICIT_ORDER_ONLY_CAP = 32;

/** Order types. It needs to be 8bits, because we save and load it as such */
enum OrderType : uint8_t {
	OT_BEGIN         = 0,
	OT_NOTHING       = 0,
	OT_GOTO_STATION  = 1,
	OT_GOTO_DEPOT    = 2,
	OT_LOADING       = 3,
	OT_LEAVESTATION  = 4,
	OT_DUMMY         = 5,
	OT_GOTO_WAYPOINT = 6,
	OT_CONDITIONAL   = 7,
	OT_IMPLICIT      = 8,
	OT_END
};

/**
 * Unloading order types.
 */
enum class OrderUnloadType : uint8_t {
	UnloadIfPossible = 0, ///< Unload all cargo that the station accepts.
	Unload = 1, ///< Force unloading all cargo onto the platform, possibly not getting paid.
	Transfer = 2, ///< Transfer all cargo onto the platform.
	NoUnload = 4, ///< Totally no unloading will be done.
};

/**
 * Loading order types.
 */
enum class OrderLoadType : uint8_t {
	LoadIfPossible = 0, ///< Load as long as there is cargo that fits in the train.
	FullLoad = 2, ///< Full load all cargoes of the consist.
	FullLoadAny = 3, ///< Full load a single cargo of the consist.
	NoLoad = 4, ///< Do not load anything.
};

/**
 * Non-stop order flags.
 */
enum class OrderNonStopFlag : uint8_t {
	NoIntermediate = 0, ///< The vehicle will not stop at any stations it passes except the destination, aka non-stop.
	NoDestination = 1, ///< The vehicle will stop at any station it passes except the destination, aka via.
};

using OrderNonStopFlags = EnumBitSet<OrderNonStopFlag, uint8_t>;

/**
 * Where to stop the trains.
 */
enum class OrderStopLocation : uint8_t {
	NearEnd = 0, ///< Stop at the near end of the platform
	Middle = 1, ///< Stop at the middle of the platform
	FarEnd = 2, ///< Stop at the far end of the platform
	End,
};

/**
 * Reasons that could cause us to go to the depot.
 */
enum class OrderDepotTypeFlag : uint8_t {
	Service = 0, ///< This depot order is because of the servicing limit.
	PartOfOrders = 1, ///< This depot order is because of a regular order.
};

using OrderDepotTypeFlags = EnumBitSet<OrderDepotTypeFlag, uint8_t>;

/**
 * Actions that can be performed when the vehicle enters the depot.
 */
enum class OrderDepotActionFlag : uint8_t {
	Halt = 0, ///< Service the vehicle and then halt it.
	NearestDepot = 1, ///< Send the vehicle to the nearest depot.
	Unbunch = 2, ///< Service the vehicle and then unbunch it.
};

using OrderDepotActionFlags = EnumBitSet<OrderDepotActionFlag, uint8_t>;

/**
 * Variables (of a vehicle) to 'cause' skipping on.
 */
enum class OrderConditionVariable : uint8_t {
	LoadPercentage = 0, ///< Skip based on the amount of load
	Reliability = 1, ///< Skip based on the reliability
	MaxSpeed = 2, ///< Skip based on the maximum speed
	Age = 3, ///< Skip based on the age
	RequiresService = 4, ///< Skip when the vehicle requires service
	Unconditionally = 5, ///< Always skip
	RemainingLifetime = 6, ///< Skip based on the remaining lifetime
	MaxReliability = 7, ///< Skip based on the maximum reliability
	End,
};

/**
 * Comparator for the skip reasoning.
 */
enum class OrderConditionComparator : uint8_t {
	Equal = 0, ///< Skip if both values are equal
	NotEqual = 1, ///< Skip if both values are not equal
	LessThan = 2, ///< Skip if the value is less than the limit
	LessThanOrEqual = 3, ///< Skip if the value is less or equal to the limit
	MoreThan = 4, ///< Skip if the value is more than the limit
	MoreThanOrEqual = 5, ///< Skip if the value is more or equal to the limit
	IsTrue = 6, ///< Skip if the variable is true
	IsFalse = 7, ///< Skip if the variable is false
	End,
};


/**
 * Enumeration for the data to set in #CmdModifyOrder.
 */
enum ModifyOrderFlags : uint8_t {
	MOF_NON_STOP,        ///< Passes an OrderNonStopFlags.
	MOF_STOP_LOCATION,   ///< Passes an OrderStopLocation.
	MOF_UNLOAD,          ///< Passes an OrderUnloadType.
	MOF_LOAD,            ///< Passes an OrderLoadType
	MOF_DEPOT_ACTION,    ///< Selects the OrderDepotAction
	MOF_COND_VARIABLE,   ///< A conditional variable changes.
	MOF_COND_COMPARATOR, ///< A comparator changes.
	MOF_COND_VALUE,      ///< The value to set the condition to.
	MOF_COND_DESTINATION,///< Change the destination of a conditional order.
	MOF_END
};

/**
 * Depot action to switch to when doing a #MOF_DEPOT_ACTION.
 */
enum class OrderDepotAction : uint8_t {
	AlwaysGo = 0, ///< Always go to the depot
	Service = 1, ///< Service only if needed
	Stop = 2, ///< Go to the depot and stop there
	Unbunch = 3, ///< Go to the depot and unbunch
	End
};

/**
 * Enumeration for the data to set in #CmdChangeTimetable.
 */
enum ModifyTimetableFlags : uint8_t {
	MTF_WAIT_TIME,    ///< Set wait time.
	MTF_TRAVEL_TIME,  ///< Set travel time.
	MTF_TRAVEL_SPEED, ///< Set max travel speed.
	MTF_END
};

/** Clone actions. */
enum CloneOptions : uint8_t {
	CO_SHARE   = 0,
	CO_COPY    = 1,
	CO_UNSHARE = 2
};

struct Order;
struct OrderList;

#endif /* ORDER_TYPE_H */
