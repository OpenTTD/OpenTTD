/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_gui_base.h Functions/classes shared between the different vehicle list GUIs. */

#ifndef VEHICLE_GUI_BASE_H
#define VEHICLE_GUI_BASE_H

#include "sortlist_type.h"
#include "vehiclelist.h"
#include "window_gui.h"
#include "widgets/dropdown_type.h"

typedef GUIList<const Vehicle*> GUIVehicleList;

struct BaseVehicleListWindow : public Window {
	GUIVehicleList vehicles;  ///< The list of vehicles
	Listing *sorting;         ///< Pointer to the vehicle type related sorting.
	byte unitnumber_digits;   ///< The number of digits of the highest unit number
	Scrollbar *vscroll;
	VehicleListIdentifier vli; ///< Identifier of the vehicle list we want to currently show.

	enum ActionDropdownItem {
		ADI_REPLACE,
		ADI_SERVICE,
		ADI_DEPOT,
		ADI_ADD_SHARED,
		ADI_REMOVE_ALL,
	};

	static const StringID vehicle_depot_name[];
	static const StringID vehicle_sorter_names[];
	static GUIVehicleList::SortFunction * const vehicle_sorter_funcs[];

	BaseVehicleListWindow(WindowDesc *desc, WindowNumber wno) : Window(desc), vli(VehicleListIdentifier::UnPack(wno))
	{
		this->vehicles.SetSortFuncs(this->vehicle_sorter_funcs);
	}

	void DrawVehicleListItems(VehicleID selected_vehicle, int line_height, const Rect &r) const;
	void SortVehicleList();
	void BuildVehicleList();
	Dimension GetActionDropdownSize(bool show_autoreplace, bool show_group);
	DropDownList *BuildActionDropdownList(bool show_autoreplace, bool show_group);
};

uint GetVehicleListHeight(VehicleType type, uint divisor = 1);

struct Sorting {
	Listing aircraft;
	Listing roadveh;
	Listing ship;
	Listing train;
};

extern Sorting _sorting;

#endif /* VEHICLE_GUI_BASE_H */
