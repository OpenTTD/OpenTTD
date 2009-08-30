/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_gui.h Engine GUI functions, used by build_vehicle_gui and autoreplace_gui */

#ifndef ENGINE_GUI_H
#define ENGINE_GUI_H

#include "sortlist_type.h"

typedef GUIList<EngineID, CargoID> GUIEngineList;

typedef int CDECL EngList_SortTypeFunction(const void*, const void*); ///< argument type for EngList_Sort()
void EngList_Sort(GUIEngineList *el, EngList_SortTypeFunction compare);  ///< qsort of the engine list
void EngList_SortPartial(GUIEngineList *el, EngList_SortTypeFunction compare, uint begin, uint num_items); ///< qsort of specified portion of the engine list

StringID GetEngineCategoryName(EngineID engine);
StringID GetEngineInfoString(EngineID engine);
void DrawVehicleEngine(int x, int y, EngineID engine, SpriteID pal);

#endif /* ENGINE_GUI_H */
