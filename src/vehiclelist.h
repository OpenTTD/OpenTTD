/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehiclelist.h Functions and type for generating vehicle lists. */

#ifndef VEHICLELIST_H
#define VEHICLELIST_H

#include "company_type.h"
#include "group_type.h"
#include "order_type.h"
#include "station_type.h"
#include "tile_type.h"
#include "vehicle_type.h"
#include "window_type.h"

/** Vehicle List type flags */
enum VehicleListType : uint8_t {
	VL_STANDARD, ///< Index is the company.
	VL_SHARED_ORDERS, ///< Index is the first vehicle of the shared orders.
	VL_STATION_LIST, ///< Index is the station.
	VL_DEPOT_LIST, ///< Index is the destination (station for hangar of aircraft, depot for others)
	VL_GROUP_LIST, ///< Index is the group.
	VLT_END
};

/** The information about a vehicle list. */
struct VehicleListIdentifier {
	VehicleListType type; ///< The type of vehicle list.
	VehicleType vtype;    ///< The vehicle type associated with this list.
	CompanyID company;    ///< The company associated with this list.
	uint32_t index;         ///< A vehicle list type specific index.

	WindowNumber ToWindowNumber() const;

	bool Valid() const { return this->type < VLT_END; }

	constexpr CompanyID ToCompanyID() const { assert(this->type == VL_STANDARD); return CompanyID(this->index); }
	constexpr DestinationID ToDestinationID() const { assert(this->type == VL_DEPOT_LIST); return DestinationID(this->index); }
	constexpr GroupID ToGroupID() const { assert(this->type == VL_GROUP_LIST); return GroupID(this->index); }
	constexpr StationID ToStationID() const { assert(this->type == VL_STATION_LIST); return StationID(this->index); }
	constexpr VehicleID ToVehicleID() const { assert(this->type == VL_SHARED_ORDERS); return VehicleID(this->index); }

	constexpr void SetIndex(uint32_t index) { this->index = index; }
	constexpr void SetIndex(ConvertibleThroughBase auto index) { this->index = index.base(); }

	/**
	 * Create a simple vehicle list.
	 * @param type    List type.
	 * @param vtype   Vehicle type associated with this list.
	 * @param company Company associated with this list.
	 * @param index   Optional type specific index.
	 */
	VehicleListIdentifier(VehicleListType type, VehicleType vtype, CompanyID company, uint index = 0) :
		type(type), vtype(vtype), company(company), index(index) {}

	VehicleListIdentifier(VehicleListType type, VehicleType vtype, CompanyID company, ConvertibleThroughBase auto index) :
		type(type), vtype(vtype), company(company), index(index.base()) {}

	VehicleListIdentifier() = default;
};

/** A list of vehicles. */
typedef std::vector<const Vehicle *> VehicleList;

bool GenerateVehicleSortList(VehicleList *list, const VehicleListIdentifier &identifier);
void BuildDepotVehicleList(VehicleType type, TileIndex tile, VehicleList *engine_list, VehicleList *wagon_list, bool individual_wagons = false);
uint GetUnitNumberDigits(VehicleList &vehicles);

#endif /* VEHICLELIST_H */
