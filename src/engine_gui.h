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
#include "engine_base.h"

struct GUIEngineListItem {
	EngineID engine_id;       ///< Engine to display in build purchase list
	EngineID variant_id;      ///< Variant group of the engine.
	EngineDisplayFlags flags; ///< Flags for toggling/drawing (un)folded status and controlling indentation.
	int8_t indent;              ///< Display indentation level.

	GUIEngineListItem(EngineID engine_id, EngineID variant_id, EngineDisplayFlags flags, int indent) : engine_id(engine_id), variant_id(variant_id), flags(flags), indent(indent) {}

	/* Used when searching list only by engine_id. */
	bool operator == (const EngineID &other) const { return this->engine_id == other; }
};

typedef GUIList<GUIEngineListItem, std::nullptr_t, CargoID> GUIEngineList;

typedef bool EngList_SortTypeFunction(const GUIEngineListItem&, const GUIEngineListItem&); ///< argument type for #EngList_Sort.
void EngList_Sort(GUIEngineList &el, EngList_SortTypeFunction compare);
void EngList_SortPartial(GUIEngineList &el, EngList_SortTypeFunction compare, size_t begin, size_t num_items);

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
extern const StringID _engine_sort_listing[][12];
extern EngList_SortTypeFunction * const _engine_sort_functions[][11];

uint GetEngineListHeight(VehicleType type);
void DisplayVehicleSortDropDown(Window *w, VehicleType vehicle_type, int selected, int button);

#endif /* ENGINE_GUI_H */
