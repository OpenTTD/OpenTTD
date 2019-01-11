/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_gui_base.h Functions/classes shared between the different vehicle list GUIs. */

#ifndef VEHICLE_GUI_BASE_H
#define VEHICLE_GUI_BASE_H

#include "core/smallvec_type.hpp"
#include "date_type.h"
#include "economy_type.h"
#include "sortlist_type.h"
#include "vehiclelist.h"
#include "window_gui.h"
#include "widgets/dropdown_type.h"

#include <iterator>

typedef GUIList<const Vehicle*> GUIVehicleList;

struct GUIVehicleGroup {
	VehicleList::const_iterator vehicles_begin;    ///< Pointer to beginning element of this vehicle group.
	VehicleList::const_iterator vehicles_end;      ///< Pointer to past-the-end element of this vehicle group.
	Money display_profit_this_year;                ///< Total profit for the vehicle group this year.
	Money display_profit_last_year;                ///< Total profit for the vehicle group laste year.
	Date age;                                      ///< Age in days of oldest vehicle in the group.

	GUIVehicleGroup(VehicleList::const_iterator vehicles_begin, VehicleList::const_iterator vehicles_end, Money display_profit_this_year, Money display_profit_last_year, Date age)
		: vehicles_begin(vehicles_begin), vehicles_end(vehicles_end), display_profit_this_year(display_profit_this_year), display_profit_last_year(display_profit_last_year), age(age) {}

	std::ptrdiff_t NumVehicles() const
	{
		return std::distance(vehicles_begin, vehicles_end);
	}
	const Vehicle *GetSingleVehicle() const
	{
		assert(NumVehicles() == 1);
		return vehicles_begin[0];
	}
};

typedef GUIList<GUIVehicleGroup> GUIVehicleGroupList;

struct BaseVehicleListWindow : public Window {

	enum GroupBy : byte {
		GB_NONE,
		GB_SHARED_ORDERS,

		GB_END,
	};

	GroupBy grouping;                         ///< How we want to group the list.
	VehicleList vehicles;                     ///< List of vehicles.  This is the buffer for `vehgroups` to point into; if this is structurally modified, `vehgroups` must be rebuilt.
	GUIVehicleGroupList vehgroups;            ///< List of (groups of) vehicles.  This stores iterators of `vehicles`, and should be rebuilt if `vehicles` is structurally changed.
	Listing *sorting;                         ///< Pointer to the vehicle type related sorting.
	byte unitnumber_digits;                   ///< The number of digits of the highest unit number.
	Scrollbar *vscroll;
	VehicleListIdentifier vli;                ///< Identifier of the vehicle list we want to currently show.

	typedef GUIVehicleGroupList::SortFunction VehicleGroupSortFunction;
	typedef GUIVehicleList::SortFunction VehicleIndividualSortFunction;

	enum ActionDropdownItem {
		ADI_REPLACE,
		ADI_SERVICE,
		ADI_DEPOT,
		ADI_ADD_SHARED,
		ADI_REMOVE_ALL,
	};

	static const StringID vehicle_depot_name[];
	static const StringID vehicle_group_by_names[];
	static const StringID vehicle_group_none_sorter_names[];
	static const StringID vehicle_group_shared_orders_sorter_names[];
	static VehicleGroupSortFunction * const vehicle_group_none_sorter_funcs[];
	static VehicleGroupSortFunction * const vehicle_group_shared_orders_sorter_funcs[];

	BaseVehicleListWindow(WindowDesc *desc, WindowNumber wno);

	void UpdateSortingFromGrouping();

	void DrawVehicleListItems(VehicleID selected_vehicle, int line_height, const Rect &r) const;
	void UpdateVehicleGroupBy(GroupBy group_by);
	void SortVehicleList();
	void BuildVehicleList();
	Dimension GetActionDropdownSize(bool show_autoreplace, bool show_group);
	DropDownList BuildActionDropdownList(bool show_autoreplace, bool show_group);

	const StringID *GetVehicleSorterNames()
	{
		switch (this->grouping) {
			case GB_NONE:
				return vehicle_group_none_sorter_names;
			case GB_SHARED_ORDERS:
				return vehicle_group_shared_orders_sorter_names;
			default:
				NOT_REACHED();
		}
	}

	VehicleGroupSortFunction * const *GetVehicleSorterFuncs()
	{
		switch (this->grouping) {
			case GB_NONE:
				return vehicle_group_none_sorter_funcs;
			case GB_SHARED_ORDERS:
				return vehicle_group_shared_orders_sorter_funcs;
			default:
				NOT_REACHED();
		}
	}
};

uint GetVehicleListHeight(VehicleType type, uint divisor = 1);

struct Sorting {
	Listing aircraft;
	Listing roadveh;
	Listing ship;
	Listing train;
};

extern BaseVehicleListWindow::GroupBy _grouping[VLT_END][VEH_COMPANY_END];
extern Sorting _sorting[BaseVehicleListWindow::GB_END];

#endif /* VEHICLE_GUI_BASE_H */
