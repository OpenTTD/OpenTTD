/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_gui_base.h Functions/classes shared between the different vehicle list GUIs. */

#ifndef VEHICLE_GUI_BASE_H
#define VEHICLE_GUI_BASE_H

#include "cargo_type.h"
#include "timer/timer_game_calendar.h"
#include "economy_type.h"
#include "sortlist_type.h"
#include "vehicle_base.h"
#include "vehiclelist.h"
#include "window_gui.h"
#include "widgets/dropdown_type.h"

typedef GUIList<const Vehicle*, std::nullptr_t, CargoID> GUIVehicleList;

struct GUIVehicleGroup {
	VehicleList::const_iterator vehicles_begin;    ///< Pointer to beginning element of this vehicle group.
	VehicleList::const_iterator vehicles_end;      ///< Pointer to past-the-end element of this vehicle group.

	GUIVehicleGroup(VehicleList::const_iterator vehicles_begin, VehicleList::const_iterator vehicles_end)
		: vehicles_begin(vehicles_begin), vehicles_end(vehicles_end) {}

	std::ptrdiff_t NumVehicles() const
	{
		return std::distance(this->vehicles_begin, this->vehicles_end);
	}

	const Vehicle *GetSingleVehicle() const
	{
		assert(this->NumVehicles() == 1);
		return this->vehicles_begin[0];
	}

	Money GetDisplayProfitThisYear() const
	{
		return std::accumulate(this->vehicles_begin, this->vehicles_end, (Money)0, [](Money acc, const Vehicle *v) {
			return acc + v->GetDisplayProfitThisYear();
		});
	}

	Money GetDisplayProfitLastYear() const
	{
		return std::accumulate(this->vehicles_begin, this->vehicles_end, (Money)0, [](Money acc, const Vehicle *v) {
			return acc + v->GetDisplayProfitLastYear();
		});
	}

	TimerGameCalendar::Date GetOldestVehicleAge() const
	{
		const Vehicle *oldest = *std::max_element(this->vehicles_begin, this->vehicles_end, [](const Vehicle *v_a, const Vehicle *v_b) {
			return v_a->age < v_b->age;
		});
		return oldest->age;
	}
};

typedef GUIList<GUIVehicleGroup, std::nullptr_t, CargoID> GUIVehicleGroupList;

struct BaseVehicleListWindow : public Window {

	enum GroupBy : byte {
		GB_NONE,
		GB_SHARED_ORDERS,

		GB_END,
	};

	/** Special cargo filter criteria */
	enum CargoFilterSpecialType {
		CF_NONE = CT_INVALID,       ///< Show only vehicles which do not carry cargo (e.g. train engines)
		CF_ANY = CT_NO_REFIT,       ///< Show all vehicles independent of carried cargo (i.e. no filtering)
		CF_FREIGHT = CT_AUTO_REFIT, ///< Show only vehicles which carry any freight (non-passenger) cargo
	};

	GroupBy grouping;                           ///< How we want to group the list.
	VehicleList vehicles;                       ///< List of vehicles.  This is the buffer for `vehgroups` to point into; if this is structurally modified, `vehgroups` must be rebuilt.
	GUIVehicleGroupList vehgroups;              ///< List of (groups of) vehicles.  This stores iterators of `vehicles`, and should be rebuilt if `vehicles` is structurally changed.
	Listing *sorting;                           ///< Pointer to the vehicle type related sorting.
	byte unitnumber_digits;                     ///< The number of digits of the highest unit number.
	Scrollbar *vscroll;
	VehicleListIdentifier vli;                  ///< Identifier of the vehicle list we want to currently show.
	VehicleID vehicle_sel;                      ///< Selected vehicle
	CargoID cargo_filter_criteria;              ///< Selected cargo filter index
	uint order_arrow_width;                     ///< Width of the arrow in the small order list.
	CargoTypes used_cargoes;

	typedef GUIVehicleGroupList::SortFunction VehicleGroupSortFunction;
	typedef GUIVehicleList::SortFunction VehicleIndividualSortFunction;

	enum ActionDropdownItem {
		ADI_REPLACE,
		ADI_SERVICE,
		ADI_DEPOT,
		ADI_ADD_SHARED,
		ADI_REMOVE_ALL,
		ADI_CREATE_GROUP,
	};

	static const StringID vehicle_depot_name[];
	static const StringID vehicle_group_by_names[];
	static const StringID vehicle_group_none_sorter_names[];
	static const StringID vehicle_group_shared_orders_sorter_names[];
	static VehicleGroupSortFunction * const vehicle_group_none_sorter_funcs[];
	static VehicleGroupSortFunction * const vehicle_group_shared_orders_sorter_funcs[];

	BaseVehicleListWindow(WindowDesc *desc, WindowNumber wno);

	void OnInit() override;

	void UpdateSortingFromGrouping();

	void DrawVehicleListItems(VehicleID selected_vehicle, int line_height, const Rect &r) const;
	void UpdateVehicleGroupBy(GroupBy group_by);
	void SortVehicleList();
	void BuildVehicleList();
	void SetCargoFilter(byte index);
	void SetCargoFilterArray();
	void FilterVehicleList();
	StringID GetCargoFilterLabel(CargoID cid) const;
	DropDownList BuildCargoDropDownList(bool full) const;
	Dimension GetActionDropdownSize(bool show_autoreplace, bool show_group, bool show_create);
	DropDownList BuildActionDropdownList(bool show_autoreplace, bool show_group, bool show_create);

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
