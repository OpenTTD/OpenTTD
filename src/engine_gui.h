/* $Id$ */

/** @file engine_gui.h Engine GUI functions, used by build_vehicle_gui and autoreplace_gui */

#ifndef ENGINE_GUI_H
#define ENGINE_GUI_H

#include "sortlist_type.h"

typedef GUIList<EngineID> GUIEngineList;

typedef int CDECL EngList_SortTypeFunction(const void*, const void*); ///< argument type for EngList_Sort()
void EngList_Sort(GUIEngineList *el, EngList_SortTypeFunction compare);  ///< qsort of the engine list
void EngList_SortPartial(GUIEngineList *el, EngList_SortTypeFunction compare, uint begin, uint num_items); ///< qsort of specified portion of the engine list

#endif /* ENGINE_GUI_H */
