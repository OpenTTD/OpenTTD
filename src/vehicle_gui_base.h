/* $Id$ */

/** @file vehicle_gui_base.h Functions/classes shared between the different vehicle list GUIs. */

#ifndef VEHICLE_GUI_BASE_H
#define VEHICLE_GUI_BASE_H

#include "sortlist_type.h"

/** Start of functions regarding vehicle list windows */
enum {
	PLY_WND_PRC__OFFSET_TOP_WIDGET = 26,
	PLY_WND_PRC__SIZE_OF_ROW_TINY  = 13,
	PLY_WND_PRC__SIZE_OF_ROW_SMALL = 26,
	PLY_WND_PRC__SIZE_OF_ROW_BIG   = 39,
};

typedef GUIList<const Vehicle*> GUIVehicleList;

struct BaseVehicleListWindow: public Window {
	GUIVehicleList vehicles;  ///< The list of vehicles
	Listing *sorting;         ///< Pointer to the vehicle type related sorting.
	VehicleType vehicle_type; ///< The vehicle type that is sorted

	static const StringID vehicle_sorter_names[];
	static GUIVehicleList::SortFunction * const vehicle_sorter_funcs[];

	BaseVehicleListWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->vehicles.SetSortFuncs(this->vehicle_sorter_funcs);
	}

	void DrawVehicleListItems(int x, VehicleID selected_vehicle);
	void SortVehicleList();
	void BuildVehicleList(Owner owner, uint16 index, uint16 window_type);
};

struct Sorting {
	Listing aircraft;
	Listing roadveh;
	Listing ship;
	Listing train;
};

extern Sorting _sorting;

#endif /* VEHICLE_GUI_BASE_H */
