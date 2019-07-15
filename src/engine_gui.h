/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_gui.h %Engine GUI functions, used by build_vehicle_gui and autoreplace_gui */

#ifndef ENGINE_GUI_H
#define ENGINE_GUI_H

#include "engine_type.h"
#include "group_type.h"
#include "sortlist_type.h"
#include "gfx_type.h"
#include "vehicle_type.h"

typedef GUIList<EngineID, CargoID> GUIEngineList;

typedef bool EngList_SortTypeFunction(const EngineID&, const EngineID&); ///< argument type for #EngList_Sort.
void EngList_Sort(GUIEngineList *el, EngList_SortTypeFunction compare);
void EngList_SortPartial(GUIEngineList *el, EngList_SortTypeFunction compare, uint begin, uint num_items);

StringID GetEngineCategoryName(EngineID engine);
StringID GetEngineInfoString(EngineID engine);

void DrawVehicleEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawTrainEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawRoadVehEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawShipEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawAircraftEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);

extern bool _engine_sort_direction;
extern byte _engine_sort_last_criteria[];
extern bool _engine_sort_last_order[];
extern bool _engine_sort_show_hidden_engines[];
extern const StringID _engine_sort_listing[][15];
extern EngList_SortTypeFunction * const _engine_sort_functions[][14];

uint GetEngineListHeight(VehicleType type);
void DrawEngineList(VehicleType type, int l, int r, int y, const GUIEngineList* eng_list, uint16 min, uint16 max, EngineID selected_id, bool show_count, GroupID selected_group);
void DisplayVehicleSortDropDown(Window *w, VehicleType vehicle_type, int selected, int button);

#endif /* ENGINE_GUI_H */
