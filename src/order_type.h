/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_type.h Types related to orders. */

#ifndef ORDER_TYPE_H
#define ORDER_TYPE_H

#include "core/enum_type.hpp"
#include "depot_type.h"
#include "core/pool_type.hpp"
#include "station_type.h"

typedef uint8_t VehicleOrderID;  ///< The index of an order within its current vehicle (not pool related)
using OrderID = PoolID<uint32_t, struct OrderIDTag, 0xFF0000, 0xFFFFFF>;
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
 * Flags related to the unloading order.
 */
enum OrderUnloadFlags : uint8_t {
	OUF_UNLOAD_IF_POSSIBLE = 0,      ///< Unload all cargo that the station accepts.
	OUFB_UNLOAD            = 1 << 0, ///< Force unloading all cargo onto the platform, possibly not getting paid.
	OUFB_TRANSFER          = 1 << 1, ///< Transfer all cargo onto the platform.
	OUFB_NO_UNLOAD         = 1 << 2, ///< Totally no unloading will be done.
};

/**
 * Flags related to the loading order.
 */
enum OrderLoadFlags : uint8_t {
	OLF_LOAD_IF_POSSIBLE = 0,      ///< Load as long as there is cargo that fits in the train.
	OLFB_FULL_LOAD       = 1 << 1, ///< Full load all cargoes of the consist.
	OLF_FULL_LOAD_ANY    = 3,      ///< Full load a single cargo of the consist.
	OLFB_NO_LOAD         = 4,      ///< Do not load anything.
};

/**
 * Non-stop order flags.
 */
enum OrderNonStopFlags : uint8_t {
	ONSF_STOP_EVERYWHERE                  = 0, ///< The vehicle will stop at any station it passes and the destination.
	ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS = 1, ///< The vehicle will not stop at any stations it passes except the destination.
	ONSF_NO_STOP_AT_DESTINATION_STATION   = 2, ///< The vehicle will stop at any station it passes except the destination.
	ONSF_NO_STOP_AT_ANY_STATION           = 3, ///< The vehicle will not stop at any stations it passes including the destination.
	ONSF_END
};

/**
 * Where to stop the trains.
 */
enum OrderStopLocation : uint8_t {
	OSL_PLATFORM_NEAR_END = 0, ///< Stop at the near end of the platform
	OSL_PLATFORM_MIDDLE   = 1, ///< Stop at the middle of the platform
	OSL_PLATFORM_FAR_END  = 2, ///< Stop at the far end of the platform
	OSL_END
};

/**
 * Reasons that could cause us to go to the depot.
 */
enum OrderDepotTypeFlags : uint8_t {
	ODTF_MANUAL          = 0,      ///< Manually initiated order.
	ODTFB_SERVICE        = 1 << 0, ///< This depot order is because of the servicing limit.
	ODTFB_PART_OF_ORDERS = 1 << 1, ///< This depot order is because of a regular order.
};

/**
 * Actions that can be performed when the vehicle enters the depot.
 */
enum OrderDepotActionFlags : uint8_t {
	ODATF_SERVICE_ONLY   = 0,      ///< Only service the vehicle.
	ODATFB_HALT          = 1 << 0, ///< Service the vehicle and then halt it.
	ODATFB_NEAREST_DEPOT = 1 << 1, ///< Send the vehicle to the nearest depot.
	ODATFB_UNBUNCH       = 1 << 2, ///< Service the vehicle and then unbunch it.
};
DECLARE_ENUM_AS_BIT_SET(OrderDepotActionFlags)

/**
 * Variables (of a vehicle) to 'cause' skipping on.
 */
enum OrderConditionVariable : uint8_t {
	OCV_LOAD_PERCENTAGE,    ///< Skip based on the amount of load
	OCV_RELIABILITY,        ///< Skip based on the reliability
	OCV_MAX_SPEED,          ///< Skip based on the maximum speed
	OCV_AGE,                ///< Skip based on the age
	OCV_REQUIRES_SERVICE,   ///< Skip when the vehicle requires service
	OCV_UNCONDITIONALLY,    ///< Always skip
	OCV_REMAINING_LIFETIME, ///< Skip based on the remaining lifetime
	OCV_MAX_RELIABILITY,    ///< Skip based on the maximum reliability
	OCV_END
};

/**
 * Comparator for the skip reasoning.
 */
enum OrderConditionComparator : uint8_t {
	OCC_EQUALS,      ///< Skip if both values are equal
	OCC_NOT_EQUALS,  ///< Skip if both values are not equal
	OCC_LESS_THAN,   ///< Skip if the value is less than the limit
	OCC_LESS_EQUALS, ///< Skip if the value is less or equal to the limit
	OCC_MORE_THAN,   ///< Skip if the value is more than the limit
	OCC_MORE_EQUALS, ///< Skip if the value is more or equal to the limit
	OCC_IS_TRUE,     ///< Skip if the variable is true
	OCC_IS_FALSE,    ///< Skip if the variable is false
	OCC_END
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
enum OrderDepotAction : uint8_t {
	DA_ALWAYS_GO, ///< Always go to the depot
	DA_SERVICE,   ///< Service only if needed
	DA_STOP,      ///< Go to the depot and stop there
	DA_UNBUNCH,   ///< Go to the depot and unbunch
	DA_END
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
