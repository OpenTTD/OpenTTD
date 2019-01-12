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
#include "sortlist_type.h"
#include "gfx_type.h"
#include "vehicle_type.h"

typedef GUIList<EngineID, CargoID> GUIEngineList;

StringID GetEngineCategoryName(EngineID engine);
StringID GetEngineInfoString(EngineID engine);

void DrawVehicleEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawTrainEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawRoadVehEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawShipEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);
void DrawAircraftEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type);

extern Listing _engine_sort_last_sorting[VEH_COMPANY_END];
extern bool _engine_sort_show_hidden_engines[VEH_COMPANY_END];
extern const StringID _engine_sort_listing[VEH_COMPANY_END][12];
extern GUIEngineList::SortFunction * const _engine_sort_functions[VEH_COMPANY_END][11];

uint GetEngineListHeight(VehicleType type);
void DisplayVehicleSortDropDown(Window *w, VehicleType vehicle_type, int selected, int button);

#endif /* ENGINE_GUI_H */
