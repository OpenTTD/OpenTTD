/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_gui.cpp The base GUI for all vehicles. */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "company_func.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "vehicle_gui_base.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "waypoint_base.h"
#include "roadveh.h"
#include "train.h"
#include "aircraft.h"
#include "depot_base.h"
#include "group_gui.h"
#include "strings_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "autoreplace_gui.h"
#include "string_func.h"
#include "widgets/dropdown_func.h"
#include "timetable.h"
#include "vehiclelist.h"
#include "articulated_vehicles.h"
#include "cargotype.h"
#include "spritecache.h"

#include "table/sprites.h"
#include "table/strings.h"

Sorting _sorting;

static GUIVehicleList::SortFunction VehicleNumberSorter;
static GUIVehicleList::SortFunction VehicleNameSorter;
static GUIVehicleList::SortFunction VehicleAgeSorter;
static GUIVehicleList::SortFunction VehicleProfitThisYearSorter;
static GUIVehicleList::SortFunction VehicleProfitLastYearSorter;
static GUIVehicleList::SortFunction VehicleCargoSorter;
static GUIVehicleList::SortFunction VehicleReliabilitySorter;
static GUIVehicleList::SortFunction VehicleMaxSpeedSorter;
static GUIVehicleList::SortFunction VehicleModelSorter;
static GUIVehicleList::SortFunction VehicleValueSorter;
static GUIVehicleList::SortFunction VehicleLengthSorter;
static GUIVehicleList::SortFunction VehicleTimeToLiveSorter;
static GUIVehicleList::SortFunction VehicleTimetableDelaySorter;

GUIVehicleList::SortFunction * const BaseVehicleListWindow::vehicle_sorter_funcs[] = {
	&VehicleNumberSorter,
	&VehicleNameSorter,
	&VehicleAgeSorter,
	&VehicleProfitThisYearSorter,
	&VehicleProfitLastYearSorter,
	&VehicleCargoSorter,
	&VehicleReliabilitySorter,
	&VehicleMaxSpeedSorter,
	&VehicleModelSorter,
	&VehicleValueSorter,
	&VehicleLengthSorter,
	&VehicleTimeToLiveSorter,
	&VehicleTimetableDelaySorter,
};

const StringID BaseVehicleListWindow::vehicle_sorter_names[] = {
	STR_SORT_BY_NUMBER,
	STR_SORT_BY_NAME,
	STR_SORT_BY_AGE,
	STR_SORT_BY_PROFIT_THIS_YEAR,
	STR_SORT_BY_PROFIT_LAST_YEAR,
	STR_SORT_BY_TOTAL_CAPACITY_PER_CARGOTYPE,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_MODEL,
	STR_SORT_BY_VALUE,
	STR_SORT_BY_LENGTH,
	STR_SORT_BY_LIFE_TIME,
	STR_SORT_BY_TIMETABLE_DELAY,
	INVALID_STRING_ID
};

void BaseVehicleListWindow::BuildVehicleList(Owner owner, uint16 index, uint16 window_type)
{
	if (!this->vehicles.NeedRebuild()) return;

	DEBUG(misc, 3, "Building vehicle list for company %d at station %d", owner, index);

	GenerateVehicleSortList(&this->vehicles, this->vehicle_type, owner, index, window_type);

	uint unitnumber = 0;
	for (const Vehicle **v = this->vehicles.Begin(); v != this->vehicles.End(); v++) {
		unitnumber = max<uint>(unitnumber, (*v)->unitnumber);
	}

	/* Because 111 is much less wide than e.g. 999 we use the
	 * wider numbers to determine the width instead of just
	 * the random number that it seems to be. */
	if (unitnumber >= 1000) {
		this->max_unitnumber = 9999;
	} else if (unitnumber >= 100) {
		this->max_unitnumber = 999;
	} else {
		this->max_unitnumber = 99;
	}

	this->vehicles.RebuildDone();
	this->vscroll.SetCount(this->vehicles.Length());
}

/* cached values for VehicleNameSorter to spare many GetString() calls */
static const Vehicle *_last_vehicle[2] = { NULL, NULL };

void BaseVehicleListWindow::SortVehicleList()
{
	if (this->vehicles.Sort()) return;

	/* invalidate cached values for name sorter - vehicle names could change */
	_last_vehicle[0] = _last_vehicle[1] = NULL;
}

void DepotSortList(VehicleList *list)
{
	if (list->Length() < 2) return;
	QSortT(list->Begin(), list->Length(), &VehicleNumberSorter);
}

/** draw the vehicle profit button in the vehicle list window. */
static void DrawVehicleProfitButton(const Vehicle *v, int x, int y)
{
	SpriteID pal;

	/* draw profit-based coloured icons */
	if (v->age <= DAYS_IN_YEAR * 2) {
		pal = PALETTE_TO_GREY;
	} else if (v->GetDisplayProfitLastYear() < 0) {
		pal = PALETTE_TO_RED;
	} else if (v->GetDisplayProfitLastYear() < 10000) {
		pal = PALETTE_TO_YELLOW;
	} else {
		pal = PALETTE_TO_GREEN;
	}
	DrawSprite(SPR_BLOT, pal, x, y);
}

struct RefitOption {
	CargoID cargo;
	byte subtype;
	uint16 value;
	EngineID engine;
};

struct RefitList {
	uint num_lines;     ///< Number of #items.
	RefitOption *items;
};

static RefitList *BuildRefitList(const Vehicle *v)
{
	uint max_lines = 256;
	RefitOption *refit = CallocT<RefitOption>(max_lines);
	RefitList *list = CallocT<RefitList>(1);
	Vehicle *u = const_cast<Vehicle *>(v);
	uint num_lines = 0;
	uint i;

	do {
		const Engine *e = Engine::Get(u->engine_type);
		uint32 cmask = e->info.refit_mask;
		byte callback_mask = e->info.callback_mask;

		/* Skip this engine if it has no capacity */
		if (u->cargo_cap == 0) continue;

		/* Loop through all cargos in the refit mask */
		for (CargoID cid = 0; cid < NUM_CARGO && num_lines < max_lines; cid++) {
			/* Skip cargo type if it's not listed */
			if (!HasBit(cmask, cid)) continue;

			/* Check the vehicle's callback mask for cargo suffixes */
			if (HasBit(callback_mask, CBM_VEHICLE_CARGO_SUFFIX)) {
				/* Make a note of the original cargo type. It has to be
				 * changed to test the cargo & subtype... */
				CargoID temp_cargo = u->cargo_type;
				byte temp_subtype  = u->cargo_subtype;
				byte refit_cyc;

				u->cargo_type = cid;

				for (refit_cyc = 0; refit_cyc < 16 && num_lines < max_lines; refit_cyc++) {
					bool duplicate = false;
					uint16 callback;

					u->cargo_subtype = refit_cyc;
					callback = GetVehicleCallback(CBID_VEHICLE_CARGO_SUFFIX, 0, 0, u->engine_type, u);

					if (callback == 0xFF) callback = CALLBACK_FAILED;
					if (refit_cyc != 0 && callback == CALLBACK_FAILED) break;

					/* Check if this cargo and subtype combination are listed */
					for (i = 0; i < num_lines && !duplicate; i++) {
						if (refit[i].cargo == cid && refit[i].value == callback) duplicate = true;
					}

					if (duplicate) continue;

					refit[num_lines].cargo   = cid;
					refit[num_lines].subtype = refit_cyc;
					refit[num_lines].value   = callback;
					refit[num_lines].engine  = u->engine_type;
					num_lines++;
				}

				/* Reset the vehicle's cargo type */
				u->cargo_type    = temp_cargo;
				u->cargo_subtype = temp_subtype;
			} else {
				/* No cargo suffix callback -- use no subtype */
				bool duplicate = false;

				for (i = 0; i < num_lines && !duplicate; i++) {
					if (refit[i].cargo == cid && refit[i].value == CALLBACK_FAILED) duplicate = true;
				}

				if (!duplicate) {
					refit[num_lines].cargo   = cid;
					refit[num_lines].subtype = 0;
					refit[num_lines].value   = CALLBACK_FAILED;
					refit[num_lines].engine  = INVALID_ENGINE;
					num_lines++;
				}
			}
		}
	} while ((v->type == VEH_TRAIN || v->type == VEH_ROAD) && (u = u->Next()) != NULL && num_lines < max_lines);

	list->num_lines = num_lines;
	list->items = refit;

	return list;
}

/** Draw the list of available refit options for a consist and highlight the selected refit option (if any).
 * @param *list First vehicle in consist to get the refit-options of
 * @param sel   Selected refit cargo-type in the window
 * @param pos   Position of the selected item in caller widow
 * @param rows  Number of rows(capacity) in caller window
 * @param delta Step height in caller window
 * @param r     Rectangle of the matrix widget.
 */
static void DrawVehicleRefitWindow(const RefitList *list, int sel, uint pos, uint rows, uint delta, const Rect &r)
{
	uint y = r.top + WD_MATRIX_TOP;
	/* Draw the list, and find the selected cargo (by its position in list) */
	for (uint i = pos; i < pos + rows && i < list->num_lines; i++) {
		TextColour colour = (sel == (int)i) ? TC_WHITE : TC_BLACK;
		RefitOption *refit = &list->items[i];

		/* Get the cargo name */
		SetDParam(0, CargoSpec::Get(refit->cargo)->name);

		/* If the callback succeeded, draw the cargo suffix */
		if (refit->value != CALLBACK_FAILED) {
			SetDParam(1, GetGRFStringID(GetEngineGRFID(refit->engine), 0xD000 + refit->value));
			DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y, STR_JUST_STRING_SPACE_STRING, colour);
		} else {
			DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y, STR_JUST_STRING, colour);
		}

		y += delta;
	}
}

/** Widget numbers of the vehicle refit window. */
enum VehicleRefitWidgets {
	VRW_CLOSEBOX,
	VRW_CAPTION,
	VRW_SELECTHEADER,
	VRW_MATRIX,
	VRW_SCROLLBAR,
	VRW_INFOPANEL,
	VRW_REFITBUTTON,
	VRW_RESIZEBOX,
};

/** Refit cargo window. */
struct RefitWindow : public Window {
	int sel;              ///< Index in refit options, \c -1 if nothing is selected.
	RefitOption *cargo;   ///< Refit option selected by \v sel.
	RefitList *list;      ///< List of cargo types available for refitting.
	uint length;          ///< For trains, the number of vehicles.
	VehicleOrderID order; ///< If not #INVALID_VEH_ORDER_ID, selection is part of a refit order (rather than execute directly).

	RefitWindow(const WindowDesc *desc, const Vehicle *v, VehicleOrderID order) : Window()
	{
		this->CreateNestedTree(desc);

		this->GetWidget<NWidgetCore>(VRW_SELECTHEADER)->tool_tip = STR_REFIT_TRAIN_LIST_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VRW_MATRIX)->tool_tip       = STR_REFIT_TRAIN_LIST_TOOLTIP + v->type;
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(VRW_REFITBUTTON);
		nwi->widget_data = STR_REFIT_TRAIN_REFIT_BUTTON + v->type;
		nwi->tool_tip    = STR_REFIT_TRAIN_REFIT_TOOLTIP + v->type;

		this->FinishInitNested(desc, v->index);
		this->owner = v->owner;

		this->order = order;
		this->sel  = -1;
		this->list = BuildRefitList(v);
		if (v->type == VEH_TRAIN) this->length = CountVehiclesInChain(v);
		this->vscroll.SetCount(this->list->num_lines);
	}

	~RefitWindow()
	{
		free(this->list->items);
		free(this->list);
	}

	virtual void OnPaint()
	{
		Vehicle *v = Vehicle::Get(this->window_number);

		if (v->type == VEH_TRAIN) {
			uint length = CountVehiclesInChain(v);

			if (length != this->length) {
				/* Consist length has changed, so rebuild the refit list */
				free(this->list->items);
				free(this->list);
				this->list = BuildRefitList(v);
				this->length = length;
			}
		}

		this->vscroll.SetCount(this->list->num_lines);

		this->cargo = (this->sel >= 0 && this->sel < (int)this->list->num_lines) ? &this->list->items[this->sel] : NULL;
		this->DrawWidgets();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case VRW_MATRIX:
				resize->height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
				size->height = resize->height * 8;
				break;
			case VRW_INFOPANEL:
				size->height = max(size->height, (uint)(WD_FRAMERECT_TOP + 2 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM));
				break;
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == VRW_CAPTION) SetDParam(0, Vehicle::Get(this->window_number)->index);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case VRW_MATRIX:
				DrawVehicleRefitWindow(this->list, this->sel, this->vscroll.GetPosition(), this->vscroll.GetCapacity(), this->resize.step_height, r);
				break;

			case VRW_INFOPANEL:
				if (this->cargo != NULL) {
					Vehicle *v = Vehicle::Get(this->window_number);
					CommandCost cost = DoCommand(v->tile, v->index, this->cargo->cargo | this->cargo->subtype << 8, DC_QUERY_COST, GetCmdRefitVeh(v->type));
					if (CmdSucceeded(cost)) {
						SetDParam(0, this->cargo->cargo);
						SetDParam(1, _returned_refit_capacity);
						SetDParam(2, cost.GetCost());
						DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT,
								r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM, STR_REFIT_NEW_CAPACITY_COST_OF_REFIT);
					}
				}
				break;
		}
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		if (widget == VRW_MATRIX) this->OnClick(pt, VRW_REFITBUTTON);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case VRW_MATRIX: { // listbox
				int y = pt.y - this->GetWidget<NWidgetBase>(VRW_MATRIX)->pos_y;
				if (y >= 0) {
					this->sel = (y / (int)this->resize.step_height) + this->vscroll.GetPosition();
					this->SetDirty();
				}
				break;
			}

			case VRW_REFITBUTTON: // refit button
				if (this->cargo != NULL) {
					const Vehicle *v = Vehicle::Get(this->window_number);

					if (this->order == INVALID_VEH_ORDER_ID) {
						if (DoCommandP(v->tile, v->index, this->cargo->cargo | this->cargo->subtype << 8, GetCmdRefitVeh(v))) delete this;
					} else {
						if (DoCommandP(v->tile, v->index, this->cargo->cargo | this->cargo->subtype << 8 | this->order << 16, CMD_ORDER_REFIT)) delete this;
					}
				}
				break;
		}
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(VRW_MATRIX);
		this->vscroll.SetCapacity(nwi->current_y / this->resize.step_height);
		nwi->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}
};

static const NWidgetPart _nested_vehicle_refit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, VRW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, VRW_CAPTION), SetDataTip(STR_REFIT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_TEXTBTN, COLOUR_GREY, VRW_SELECTHEADER), SetMinimalSize(240, 14), SetDataTip(STR_REFIT_TITLE, STR_NULL),
	/* Matrix + scrollbar. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, VRW_MATRIX), SetMinimalSize(228, 112), SetResize(0, 14), SetDataTip(0x801, STR_NULL),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, VRW_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VRW_INFOPANEL), SetMinimalSize(240, 22), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VRW_REFITBUTTON), SetMinimalSize(228, 12),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY, VRW_RESIZEBOX),
	EndContainer(),
};

static const WindowDesc _vehicle_refit_desc(
	WDP_AUTO, WDP_AUTO, 240, 174,
	WC_VEHICLE_REFIT, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE | WDF_CONSTRUCTION,
	_nested_vehicle_refit_widgets, lengthof(_nested_vehicle_refit_widgets)
);

/** Show the refit window for a vehicle
 * @param *v The vehicle to show the refit window for
 * @param order of the vehicle ( ? )
 * @param parent the parent window of the refit window
 */
void ShowVehicleRefitWindow(const Vehicle *v, VehicleOrderID order, Window *parent)
{
	DeleteWindowById(WC_VEHICLE_REFIT, v->index);
	RefitWindow *w = new RefitWindow(&_vehicle_refit_desc, v, order);
	w->parent = parent;
}

/** Display list of cargo types of the engine, for the purchase information window */
uint ShowRefitOptionsList(int left, int right, int y, EngineID engine)
{
	/* List of cargo types of this engine */
	uint32 cmask = GetUnionOfArticulatedRefitMasks(engine, false);
	/* List of cargo types available in this climate */
	uint32 lmask = _cargo_mask;
	char string[512];
	char *b = string;

	/* Draw nothing if the engine is not refittable */
	if (CountBits(cmask) <= 1) return y;

	b = InlineString(b, STR_PURCHASE_INFO_REFITTABLE_TO);

	if (cmask == lmask) {
		/* Engine can be refitted to all types in this climate */
		b = InlineString(b, STR_PURCHASE_INFO_ALL_TYPES);
	} else {
		/* Check if we are able to refit to more cargo types and unable to. If
		 * so, invert the cargo types to list those that we can't refit to. */
		if (CountBits(cmask ^ lmask) < CountBits(cmask)) {
			cmask ^= lmask;
			b = InlineString(b, STR_PURCHASE_INFO_ALL_BUT);
		}

		bool first = true;

		/* Add each cargo type to the list */
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			if (!HasBit(cmask, cid)) continue;

			if (b >= lastof(string) - (2 + 2 * 4)) break; // ", " and two calls to Utf8Encode()

			if (!first) b = strecpy(b, ", ", lastof(string));
			first = false;

			b = InlineString(b, CargoSpec::Get(cid)->name);
		}
	}

	/* Terminate and display the completed string */
	*b = '\0';

	/* Make sure we detect any buffer overflow */
	assert(b < endof(string));

	SetDParamStr(0, string);
	return DrawStringMultiLine(left, right, y, INT32_MAX, STR_JUST_RAW_STRING);
}

/** Get the cargo subtype text from NewGRF for the vehicle details window. */
StringID GetCargoSubtypeText(const Vehicle *v)
{
	if (HasBit(EngInfo(v->engine_type)->callback_mask, CBM_VEHICLE_CARGO_SUFFIX)) {
		uint16 cb = GetVehicleCallback(CBID_VEHICLE_CARGO_SUFFIX, 0, 0, v->engine_type, v);
		if (cb != CALLBACK_FAILED) {
			return GetGRFStringID(GetEngineGRFID(v->engine_type), 0xD000 + cb);
		}
	}
	return STR_EMPTY;
}

/** Sort vehicles by their number */
static int CDECL VehicleNumberSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	return (*a)->unitnumber - (*b)->unitnumber;
}

/** Sort vehicles by their name */
static int CDECL VehicleNameSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	static char last_name[2][64];

	if (*a != _last_vehicle[0]) {
		_last_vehicle[0] = *a;
		SetDParam(0, (*a)->index);
		GetString(last_name[0], STR_VEHICLE_NAME, lastof(last_name[0]));
	}

	if (*b != _last_vehicle[1]) {
		_last_vehicle[1] = *b;
		SetDParam(0, (*b)->index);
		GetString(last_name[1], STR_VEHICLE_NAME, lastof(last_name[1]));
	}

	int r = strcmp(last_name[0], last_name[1]);
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by their age */
static int CDECL VehicleAgeSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = (*a)->age - (*b)->age;
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by this year profit */
static int CDECL VehicleProfitThisYearSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = ClampToI32((*a)->GetDisplayProfitThisYear() - (*b)->GetDisplayProfitThisYear());
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by last year profit */
static int CDECL VehicleProfitLastYearSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = ClampToI32((*a)->GetDisplayProfitLastYear() - (*b)->GetDisplayProfitLastYear());
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by their cargo */
static int CDECL VehicleCargoSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	const Vehicle *v;
	CargoArray diff;

	/* Append the cargo of the connected weagons */
	for (v = *a; v != NULL; v = v->Next()) diff[v->cargo_type] += v->cargo_cap;
	for (v = *b; v != NULL; v = v->Next()) diff[v->cargo_type] -= v->cargo_cap;

	int r = 0;
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		r = diff[i];
		if (r != 0) break;
	}

	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by their reliability */
static int CDECL VehicleReliabilitySorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = (*a)->reliability - (*b)->reliability;
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by their max speed */
static int CDECL VehicleMaxSpeedSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = 0;
	if ((*a)->type == VEH_TRAIN && (*b)->type == VEH_TRAIN) {
		r = Train::From(*a)->tcache.cached_max_speed - Train::From(*b)->tcache.cached_max_speed;
	} else {
		r = (*a)->max_speed - (*b)->max_speed;
	}
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by model */
static int CDECL VehicleModelSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = (*a)->engine_type - (*b)->engine_type;
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehciles by their value */
static int CDECL VehicleValueSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	const Vehicle *u;
	Money diff = 0;

	for (u = *a; u != NULL; u = u->Next()) diff += u->value;
	for (u = *b; u != NULL; u = u->Next()) diff -= u->value;

	int r = ClampToI32(diff);
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by their length */
static int CDECL VehicleLengthSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = 0;
	switch ((*a)->type) {
		case VEH_TRAIN:
			r = Train::From(*a)->tcache.cached_total_length - Train::From(*b)->tcache.cached_total_length;
			break;

		case VEH_ROAD: {
			const RoadVehicle *u;
			for (u = RoadVehicle::From(*a); u != NULL; u = u->Next()) r += u->rcache.cached_veh_length;
			for (u = RoadVehicle::From(*b); u != NULL; u = u->Next()) r -= u->rcache.cached_veh_length;
		} break;

		default: NOT_REACHED();
	}
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by the time they can still live */
static int CDECL VehicleTimeToLiveSorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = ClampToI32(((*a)->max_age - (*a)->age) - ((*b)->max_age - (*b)->age));
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

/** Sort vehicles by the timetable delay */
static int CDECL VehicleTimetableDelaySorter(const Vehicle * const *a, const Vehicle * const *b)
{
	int r = (*a)->lateness_counter - (*b)->lateness_counter;
	return (r != 0) ? r : VehicleNumberSorter(a, b);
}

void InitializeGUI()
{
	MemSetT(&_sorting, 0);
}

/**
 * Assign a vehicle window a new vehicle
 * @param window_class WindowClass to search for
 * @param from_index the old vehicle ID
 * @param to_index the new vehicle ID
 */
static inline void ChangeVehicleWindow(WindowClass window_class, VehicleID from_index, VehicleID to_index)
{
	Window *w = FindWindowById(window_class, from_index);
	if (w != NULL) {
		w->window_number = to_index;
		if (w->viewport != NULL) w->viewport->follow_vehicle = to_index;
		if (to_index != INVALID_VEHICLE) w->InvalidateData();
	}
}

/**
 * Report a change in vehicle IDs (due to autoreplace) to affected vehicle windows.
 * @param from_index the old vehicle ID
 * @param to_index the new vehicle ID
 */
void ChangeVehicleViewWindow(VehicleID from_index, VehicleID to_index)
{
	ChangeVehicleWindow(WC_VEHICLE_VIEW,      from_index, to_index);
	ChangeVehicleWindow(WC_VEHICLE_ORDERS,    from_index, to_index);
	ChangeVehicleWindow(WC_VEHICLE_REFIT,     from_index, to_index);
	ChangeVehicleWindow(WC_VEHICLE_DETAILS,   from_index, to_index);
	ChangeVehicleWindow(WC_VEHICLE_TIMETABLE, from_index, to_index);
}

enum VehicleListWindowWidgets {
	VLW_WIDGET_CLOSEBOX = 0,
	VLW_WIDGET_CAPTION,
	VLW_WIDGET_STICKY,
	VLW_WIDGET_SORT_ORDER,
	VLW_WIDGET_SORT_BY_PULLDOWN,
	VLW_WIDGET_EMPTY_TOP_RIGHT,
	VLW_WIDGET_LIST,
	VLW_WIDGET_SCROLLBAR,
	VLW_WIDGET_HIDE_BUTTONS,
	VLW_WIDGET_OTHER_COMPANY_FILLER,
	VLW_WIDGET_AVAILABLE_VEHICLES,
	VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
	VLW_WIDGET_STOP_ALL,
	VLW_WIDGET_START_ALL,
	VLW_WIDGET_EMPTY_BOTTOM_RIGHT,
	VLW_WIDGET_RESIZE,
};

static const NWidgetPart _nested_vehicle_list[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, VLW_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, VLW_WIDGET_CAPTION), SetMinimalSize(237, 14), SetDataTip(0x0, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, VLW_WIDGET_STICKY),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLW_WIDGET_SORT_ORDER), SetMinimalSize(81, 12), SetFill(false, true), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, VLW_WIDGET_SORT_BY_PULLDOWN), SetMinimalSize(167, 12), SetFill(false, true), SetDataTip(0x0, STR_TOOLTIP_SORT_CRITERIAP),
		NWidget(WWT_PANEL, COLOUR_GREY, VLW_WIDGET_EMPTY_TOP_RIGHT), SetMinimalSize(12, 12), SetFill(true, true), SetResize(1, 0),
		EndContainer(),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, VLW_WIDGET_LIST), SetMinimalSize(248, 0), SetFill(true, false),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, VLW_WIDGET_SCROLLBAR), SetMinimalSize(12, 0),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, VLW_WIDGET_HIDE_BUTTONS),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLW_WIDGET_AVAILABLE_VEHICLES), SetMinimalSize(106, 12), SetFill(false, true),
								SetDataTip(0x0, STR_VEHICLE_LIST_AVAILABLE_ENGINES_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN), SetMinimalSize(118, 12), SetFill(false, true),
								SetDataTip(STR_VEHICLE_LIST_MANAGE_LIST, STR_VEHICLE_LIST_MANAGE_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VLW_WIDGET_STOP_ALL), SetMinimalSize(12, 12), SetFill(false, true),
								SetDataTip(SPR_FLAG_VEH_STOPPED, STR_VEHICLE_LIST_MASS_STOP_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VLW_WIDGET_START_ALL), SetMinimalSize(12, 12), SetFill(false, true),
								SetDataTip(SPR_FLAG_VEH_RUNNING, STR_VEHICLE_LIST_MASS_START_LIST_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY, VLW_WIDGET_EMPTY_BOTTOM_RIGHT), SetMinimalSize(0, 12), SetResize(1, 0), SetFill(false, true), EndContainer(),
			EndContainer(),
			/* Widget to be shown for other companies hiding the previous 5 widgets. */
			NWidget(WWT_PANEL, COLOUR_GREY, VLW_WIDGET_OTHER_COMPANY_FILLER), SetFill(true, true), SetResize(1, 0), EndContainer(),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY, VLW_WIDGET_RESIZE),
	EndContainer(),
};

static void DrawSmallOrderList(const Vehicle *v, int left, int right, int y)
{
	const Order *order;
	int i = 0;

	int sel = v->cur_order_index;

	FOR_VEHICLE_ORDERS(v, order) {
		if (sel == 0) DrawString(left, right, y, STR_TINY_RIGHT_ARROW, TC_BLACK);
		sel--;

		if (order->IsType(OT_GOTO_STATION)) {
			SetDParam(0, order->GetDestination());
			DrawString(left + 6, right - 6, y, STR_TINY_BLACK_STATION);

			y += FONT_HEIGHT_SMALL;
			if (++i == 4) break;
		}
	}
}

/**
 * Draws an image of a vehicle chain
 * @param v         Front vehicle
 * @param left      The minimum horizontal position
 * @param right     The maximum horizontal position
 * @param y         Vertical position to draw at
 * @param selection Selected vehicle to draw a frame around
 * @param skip      Number of pixels to skip at the front (for scrolling)
 */
static void DrawVehicleImage(const Vehicle *v, int left, int right, int y, VehicleID selection, int skip)
{
	switch (v->type) {
		case VEH_TRAIN:    DrawTrainImage(Train::From(v), left, right, y, selection, skip); break;
		case VEH_ROAD:     DrawRoadVehImage(v, left, right, y, selection);  break;
		case VEH_SHIP:     DrawShipImage(v, left, right, y, selection);     break;
		case VEH_AIRCRAFT: DrawAircraftImage(v, left, right, y, selection); break;
		default: NOT_REACHED();
	}
}

/**
 * Get the height of a vehicle in the vehicle list GUIs.
 * @param type    the vehicle type to look at
 * @param divisor the resulting height must be dividable by this
 * @return the height
 */
uint GetVehicleListHeight(VehicleType type, uint divisor)
{
	/* Name + vehicle + profit */
	uint base = GetVehicleHeight(type) + 2 * FONT_HEIGHT_SMALL;
	/* Drawing of the 4 small orders + profit*/
	if (type >= VEH_SHIP) base = max(base, 5U * FONT_HEIGHT_SMALL);

	if (divisor == 1) return base;

	/* Make sure the height is dividable by divisor */
	uint rem = base % divisor;
	return base + (rem == 0 ? 0 : divisor - rem);
}

/**
 * Draw all the vehicle list items.
 * @param selected_vehicle The vehicle that is to be highlighted.
 * @param line_height      Height of a single item line.
 * @param r                Rectangle with edge positions of the matrix widget.
 */
void BaseVehicleListWindow::DrawVehicleListItems(VehicleID selected_vehicle, int line_height, const Rect &r) const
{
	int left = r.left + WD_MATRIX_LEFT;
	int right = r.right - WD_MATRIX_RIGHT;
	int width = right - left;
	bool rtl = _dynlang.text_dir == TD_RTL;

	SetDParam(0, this->max_unitnumber);
	int text_offset = GetStringBoundingBox(STR_JUST_INT).width + WD_FRAMERECT_RIGHT;
	int text_left  = left  + (rtl ?           0 : text_offset);
	int text_right = right - (rtl ? text_offset :           0);

	bool show_orderlist = vehicle_type >= VEH_SHIP;
	int orderlist_left  = left  + (rtl ? 0 : max(100 + text_offset, width / 2));
	int orderlist_right = right - (rtl ? max(100 + text_offset, width / 2) : 0);

	int image_left  = (rtl && show_orderlist) ? orderlist_right : text_left;
	int image_right = (!rtl && show_orderlist) ? orderlist_left : text_right;

	int vehicle_button_x = rtl ? right - 8 : left;

	int y = r.top;
	uint max = min(this->vscroll.GetPosition() + this->vscroll.GetCapacity(), this->vehicles.Length());
	for (uint i = this->vscroll.GetPosition(); i < max; ++i) {
		const Vehicle *v = this->vehicles[i];
		StringID str;

		SetDParam(0, v->GetDisplayProfitThisYear());
		SetDParam(1, v->GetDisplayProfitLastYear());

		DrawVehicleImage(v, image_left, image_right, y + FONT_HEIGHT_SMALL - 1, selected_vehicle, 0);
		DrawString(text_left, text_right, y + line_height - FONT_HEIGHT_SMALL - WD_FRAMERECT_BOTTOM - 1, STR_VEHICLE_LIST_PROFIT_THIS_YEAR_LAST_YEAR);

		if (v->name != NULL) {
			/* The vehicle got a name so we will print it */
			SetDParam(0, v->index);
			DrawString(text_left, text_right, y, STR_TINY_BLACK_VEHICLE);
		} else if (v->group_id != DEFAULT_GROUP) {
			/* The vehicle has no name, but is member of a group, so print group name */
			SetDParam(0, v->group_id);
			DrawString(text_left, text_right, y, STR_TINY_GROUP, TC_BLACK);
		}

		if (show_orderlist) DrawSmallOrderList(v, orderlist_left, orderlist_right, y);

		if (v->IsInDepot()) {
			str = STR_BLUE_COMMA;
		} else {
			str = (v->age > v->max_age - DAYS_IN_LEAP_YEAR) ? STR_RED_COMMA : STR_BLACK_COMMA;
		}

		SetDParam(0, v->unitnumber);
		DrawString(left, right, y + 2, str);

		DrawVehicleProfitButton(v, vehicle_button_x, y + FONT_HEIGHT_NORMAL + 3);

		y += line_height;
	}
}

/**
 * Window for the (old) vehicle listing.
 *
 * bitmask for w->window_number
 * 0-7 CompanyID (owner)
 * 8-10 window type (use flags in vehicle_gui.h)
 * 11-15 vehicle type (using VEH_, but can be compressed to fewer bytes if needed)
 * 16-31 StationID or OrderID depending on window type (bit 8-10)
 */
struct VehicleListWindow : public BaseVehicleListWindow {
private:
	/** Enumeration of planes of the button row at the bottom. */
	enum ButtonPlanes {
		BP_SHOW_BUTTONS, ///< Show the buttons.
		BP_HIDE_BUTTONS, ///< Show the empty panel.
	};

public:
	VehicleListWindow(const WindowDesc *desc, WindowNumber window_number) : BaseVehicleListWindow()
	{
		uint16 window_type = window_number & VLW_MASK;
		CompanyID company = (CompanyID)GB(window_number, 0, 8);

		this->vehicle_type = (VehicleType)GB(window_number, 11, 5);

		/* Set up sorting. Make the window-specific _sorting variable
		 * point to the correct global _sorting struct so we are freed
		 * from having conditionals during window operation */
		switch (this->vehicle_type) {
			case VEH_TRAIN:    this->sorting = &_sorting.train; break;
			case VEH_ROAD:     this->sorting = &_sorting.roadveh; break;
			case VEH_SHIP:     this->sorting = &_sorting.ship; break;
			case VEH_AIRCRAFT: this->sorting = &_sorting.aircraft; break;
			default: NOT_REACHED();
		}

		this->vehicles.SetListing(*this->sorting);
		this->vehicles.ForceRebuild();
		this->vehicles.NeedResort();
		this->BuildVehicleList(company, GB(window_number, 16, 16), window_type);
		this->SortVehicleList();

		this->CreateNestedTree(desc);

		/* Set up the window widgets */
		this->GetWidget<NWidgetCore>(VLW_WIDGET_LIST)->tool_tip                  = STR_VEHICLE_LIST_TRAIN_LIST_TOOLTIP + this->vehicle_type;
		this->GetWidget<NWidgetCore>(VLW_WIDGET_AVAILABLE_VEHICLES)->widget_data = STR_VEHICLE_LIST_AVAILABLE_TRAINS + this->vehicle_type;

		if (window_type == VLW_SHARED_ORDERS) {
			this->GetWidget<NWidgetCore>(VLW_WIDGET_CAPTION)->widget_data = STR_VEHICLE_LIST_SHARED_ORDERS_LIST_CAPTION;
		} else {
			this->GetWidget<NWidgetCore>(VLW_WIDGET_CAPTION)->widget_data = STR_VEHICLE_LIST_TRAIN_CAPTION + this->vehicle_type;
		}

		this->FinishInitNested(desc, window_number);
		this->owner = company;

		if (this->vehicle_type == VEH_TRAIN) ResizeWindow(this, 65, 0);
	}

	~VehicleListWindow()
	{
		*this->sorting = this->vehicles.GetListing();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget != VLW_WIDGET_LIST) return;

		resize->width = 0;
		resize->height = GetVehicleListHeight(this->vehicle_type, 1);

		switch (this->vehicle_type) {
			case VEH_TRAIN:
				resize->width = 1;
				/* Fallthrough */
			case VEH_ROAD:
				size->height = 6 * resize->height;
				break;
			case VEH_SHIP:
			case VEH_AIRCRAFT:
				size->height = 4 * resize->height;
				break;
			default: NOT_REACHED();
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget != VLW_WIDGET_CAPTION) return;

		const uint16 index = GB(this->window_number, 16, 16);
		switch (this->window_number & VLW_MASK) {
			case VLW_SHARED_ORDERS: // Shared Orders
				if (this->vehicles.Length() == 0) {
					/* We can't open this window without vehicles using this order
					 * and we should close the window when deleting the order      */
					NOT_REACHED();
				}
				SetDParam(0, this->vscroll.GetCount());
				break;

			case VLW_STANDARD: // Company Name
				SetDParam(0, STR_COMPANY_NAME);
				SetDParam(1, index);
				SetDParam(2, this->vscroll.GetCount());
				break;

			case VLW_WAYPOINT_LIST:
				SetDParam(0, STR_WAYPOINT_NAME);
				SetDParam(1, index);
				SetDParam(2, this->vscroll.GetCount());
				break;

			case VLW_STATION_LIST: // Station Name
				SetDParam(0, STR_STATION_NAME);
				SetDParam(1, index);
				SetDParam(2, this->vscroll.GetCount());
				break;

			case VLW_DEPOT_LIST:
				SetDParam(0, STR_DEPOT_TRAIN_CAPTION + this->vehicle_type);
				if (this->vehicle_type == VEH_AIRCRAFT) {
					SetDParam(1, index); // Airport name
				} else {
					SetDParam(1, Depot::Get(index)->town_index);
				}
				SetDParam(2, this->vscroll.GetCount());
				break;
			default: NOT_REACHED();
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case VLW_WIDGET_SORT_ORDER:
				/* draw arrow pointing up/down for ascending/descending sorting */
				this->DrawSortButtonState(widget, this->vehicles.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case VLW_WIDGET_LIST:
				this->DrawVehicleListItems(INVALID_VEHICLE, this->resize.step_height, r);
				break;
		}
	}

	virtual void OnPaint()
	{
		const uint16 window_type = this->window_number & VLW_MASK;

		this->BuildVehicleList(this->owner, GB(this->window_number, 16, 16), window_type);
		this->SortVehicleList();

		if (this->vehicles.Length() == 0) HideDropDownMenu(this);

		/* Hide the widgets that we will not use in this window
		 * Some windows contains actions only fit for the owner */
		int plane_to_show = (this->owner == _local_company) ? BP_SHOW_BUTTONS : BP_HIDE_BUTTONS;
		NWidgetStacked *nwi = this->GetWidget<NWidgetStacked>(VLW_WIDGET_HIDE_BUTTONS);
		if (plane_to_show != nwi->shown_plane) {
			nwi->SetDisplayedPlane(plane_to_show);
			nwi->SetDirty(this);
		}
		if (this->owner == _local_company) {
			this->SetWidgetDisabledState(VLW_WIDGET_AVAILABLE_VEHICLES, window_type != VLW_STANDARD);
			this->SetWidgetsDisabledState(this->vehicles.Length() == 0,
				VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
				VLW_WIDGET_STOP_ALL,
				VLW_WIDGET_START_ALL,
				WIDGET_LIST_END);
		}

		/* Set text of sort by dropdown widget. */
		this->GetWidget<NWidgetCore>(VLW_WIDGET_SORT_BY_PULLDOWN)->widget_data = this->vehicle_sorter_names[this->vehicles.SortType()];

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case VLW_WIDGET_SORT_ORDER: // Flip sorting method ascending/descending
				this->vehicles.ToggleSortOrder();
				this->SetDirty();
				break;

			case VLW_WIDGET_SORT_BY_PULLDOWN:// Select sorting criteria dropdown menu
				ShowDropDownMenu(this, this->vehicle_sorter_names, this->vehicles.SortType(), VLW_WIDGET_SORT_BY_PULLDOWN, 0,
						(this->vehicle_type == VEH_TRAIN || this->vehicle_type == VEH_ROAD) ? 0 : (1 << 10));
				return;

			case VLW_WIDGET_LIST: { // Matrix to show vehicles
				uint32 id_v = (pt.y - this->GetWidget<NWidgetBase>(VLW_WIDGET_LIST)->pos_y) / this->resize.step_height;
				const Vehicle *v;

				if (id_v >= this->vscroll.GetCapacity()) return; // click out of bounds

				id_v += this->vscroll.GetPosition();

				if (id_v >= this->vehicles.Length()) return; // click out of list bound

				v = this->vehicles[id_v];

				ShowVehicleViewWindow(v);
			} break;

			case VLW_WIDGET_AVAILABLE_VEHICLES:
				ShowBuildVehicleWindow(INVALID_TILE, this->vehicle_type);
				break;

			case VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN: {
				static StringID action_str[] = {
					STR_VEHICLE_LIST_REPLACE_VEHICLES,
					STR_VEHICLE_LIST_SEND_FOR_SERVICING,
					STR_NULL,
					INVALID_STRING_ID
				};

				static const StringID depot_name[] = {
					STR_VEHICLE_LIST_SEND_TRAIN_TO_DEPOT,
					STR_VEHICLE_LIST_SEND_ROAD_VEHICLE_TO_DEPOT,
					STR_VEHICLE_LIST_SEND_SHIP_TO_DEPOT,
					STR_VEHICLE_LIST_SEND_AIRCRAFT_TO_HANGAR
				};

				/* XXX - Substite string since the dropdown cannot handle dynamic strings */
				action_str[2] = depot_name[this->vehicle_type];
				ShowDropDownMenu(this, action_str, 0, VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN, 0, (this->window_number & VLW_MASK) == VLW_STANDARD ? 0 : 1);
				break;
			}

			case VLW_WIDGET_STOP_ALL:
			case VLW_WIDGET_START_ALL:
				DoCommandP(0, GB(this->window_number, 16, 16),
						(this->window_number & VLW_MASK) | (1 << 6) | (widget == VLW_WIDGET_START_ALL ? (1 << 5) : 0) | this->vehicle_type, CMD_MASS_START_STOP);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case VLW_WIDGET_SORT_BY_PULLDOWN:
				this->vehicles.SetSortType(index);
				break;
			case VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN:
				assert(this->vehicles.Length() != 0);

				switch (index) {
					case 0: // Replace window
						ShowReplaceGroupVehicleWindow(DEFAULT_GROUP, this->vehicle_type);
						break;
					case 1: // Send for servicing
						DoCommandP(0, GB(this->window_number, 16, 16) /* StationID or OrderID (depending on VLW) */,
							(this->window_number & VLW_MASK) | DEPOT_MASS_SEND | DEPOT_SERVICE, GetCmdSendToDepot(this->vehicle_type));
						break;
					case 2: // Send to Depots
						DoCommandP(0, GB(this->window_number, 16, 16) /* StationID or OrderID (depending on VLW) */,
							(this->window_number & VLW_MASK) | DEPOT_MASS_SEND, GetCmdSendToDepot(this->vehicle_type));
						break;

					default: NOT_REACHED();
				}
				break;
			default: NOT_REACHED();
		}
		this->SetDirty();
	}

	virtual void OnTick()
	{
		if (_pause_mode != PM_UNPAUSED) return;
		if (this->vehicles.NeedResort()) {
			StationID station = ((this->window_number & VLW_MASK) == VLW_STATION_LIST) ? GB(this->window_number, 16, 16) : INVALID_STATION;

			DEBUG(misc, 3, "Periodic resort %d list company %d at station %d", this->vehicle_type, this->owner, station);
			this->SetDirty();
		}
	}

	virtual void OnResize()
	{
		this->vscroll.SetCapacity(this->GetWidget<NWidgetBase>(VLW_WIDGET_LIST)->current_y / this->resize.step_height);
		this->GetWidget<NWidgetCore>(VLW_WIDGET_LIST)->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnInvalidateData(int data)
	{
		if (HasBit(data, 15) && (this->window_number & VLW_MASK) == VLW_SHARED_ORDERS) {
			SB(this->window_number, 16, 16, GB(data, 16, 16));
			this->vehicles.ForceRebuild();
			return;
		}

		if (data == 0) {
			this->vehicles.ForceRebuild();
		} else {
			this->vehicles.ForceResort();
		}
	}
};

static WindowDesc _vehicle_list_desc(
	WDP_AUTO, WDP_AUTO, 260, 246,
	WC_INVALID, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_vehicle_list, lengthof(_nested_vehicle_list)
);

static void ShowVehicleListWindowLocal(CompanyID company, uint16 VLW_flag, VehicleType vehicle_type, uint16 unique_number)
{
	if (!Company::IsValidID(company)) return;

	_vehicle_list_desc.cls = GetWindowClassForVehicleType(vehicle_type);
	WindowNumber num = (unique_number << 16) | (vehicle_type << 11) | VLW_flag | company;
	AllocateWindowDescFront<VehicleListWindow>(&_vehicle_list_desc, num);
}

void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type)
{
	/* If _settings_client.gui.advanced_vehicle_list > 1, display the Advanced list
	 * if _settings_client.gui.advanced_vehicle_list == 1, display Advanced list only for local company
	 * if _ctrl_pressed, do the opposite action (Advanced list x Normal list)
	 */

	if ((_settings_client.gui.advanced_vehicle_list > (uint)(company != _local_company)) != _ctrl_pressed) {
		ShowCompanyGroup(company, vehicle_type);
	} else {
		ShowVehicleListWindowLocal(company, VLW_STANDARD, vehicle_type, company);
	}
}

void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, const Waypoint *wp)
{
	if (wp == NULL) return;
	ShowVehicleListWindowLocal(company, VLW_WAYPOINT_LIST, vehicle_type, wp->index);
}

void ShowVehicleListWindow(const Vehicle *v)
{
	ShowVehicleListWindowLocal(v->owner, VLW_SHARED_ORDERS, v->type, v->FirstShared()->index);
}

void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, StationID station)
{
	ShowVehicleListWindowLocal(company, VLW_STATION_LIST, vehicle_type, station);
}

void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, TileIndex depot_tile)
{
	uint16 depot_airport_index;

	if (vehicle_type == VEH_AIRCRAFT) {
		depot_airport_index = GetStationIndex(depot_tile);
	} else {
		depot_airport_index = GetDepotIndex(depot_tile);
	}
	ShowVehicleListWindowLocal(company, VLW_DEPOT_LIST, vehicle_type, depot_airport_index);
}


/* Unified vehicle GUI - Vehicle Details Window */

/** Constants of vehicle details widget indices */
enum VehicleDetailsWindowWidgets {
	VLD_WIDGET_CLOSEBOX = 0,
	VLD_WIDGET_CAPTION,
	VLD_WIDGET_RENAME_VEHICLE,
	VLD_WIDGET_STICKY,
	VLD_WIDGET_TOP_DETAILS,
	VLD_WIDGET_INCREASE_SERVICING_INTERVAL,
	VLD_WIDGET_DECREASE_SERVICING_INTERVAL,
	VLD_WIDGET_SERVICING_INTERVAL,
	VLD_WIDGET_BOTTOM_RIGHT,
	VLD_WIDGET_MIDDLE_DETAILS,
	VLD_WIDGET_MATRIX,
	VLD_WIDGET_SCROLLBAR,
	VLD_WIDGET_DETAILS_CARGO_CARRIED,
	VLD_WIDGET_DETAILS_TRAIN_VEHICLES,
	VLD_WIDGET_DETAILS_CAPACITY_OF_EACH,
	VLD_WIDGET_DETAILS_TOTAL_CARGO,
	VLD_WIDGET_RESIZE,
};

assert_compile(VLD_WIDGET_DETAILS_CARGO_CARRIED    == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_CARGO   );
assert_compile(VLD_WIDGET_DETAILS_TRAIN_VEHICLES   == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_INFO    );
assert_compile(VLD_WIDGET_DETAILS_CAPACITY_OF_EACH == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_CAPACITY);
assert_compile(VLD_WIDGET_DETAILS_TOTAL_CARGO      == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_TOTALS  );

/** Vehicle details widgets (other than train). */
static const NWidgetPart _nested_nontrain_vehicle_details_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, VLD_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, VLD_WIDGET_CAPTION), SetDataTip(STR_VEHICLE_DETAILS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_RENAME_VEHICLE), SetMinimalSize(40, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_VEHICLE_NAME_BUTTON, STR_NULL /* filled in later */),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, VLD_WIDGET_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_TOP_DETAILS), SetMinimalSize(405, 42), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_MIDDLE_DETAILS), SetMinimalSize(405, 45), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_BOTTOM_RIGHT),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HORIZONTAL_LTR),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DECREASE_SERVICING_INTERVAL), SetFill(false, true),
						SetDataTip(STR_BLACK_SMALL_ARROW_LEFT, STR_VEHICLE_DETAILS_DECREASE_SERVICING_INTERVAL_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_INCREASE_SERVICING_INTERVAL), SetFill(false, true),
						SetDataTip(STR_BLACK_SMALL_ARROW_RIGHT, STR_VEHICLE_DETAILS_DECREASE_SERVICING_INTERVAL_TOOLTIP),
			EndContainer(),
			NWidget(WWT_EMPTY, COLOUR_GREY, VLD_WIDGET_SERVICING_INTERVAL), SetFill(true, true),
		EndContainer(),
	EndContainer(),
};

/** Train details widgets. */
static const NWidgetPart _nested_train_vehicle_details_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, VLD_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, VLD_WIDGET_CAPTION), SetDataTip(STR_VEHICLE_DETAILS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_RENAME_VEHICLE), SetMinimalSize(40, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_VEHICLE_NAME_BUTTON, STR_NULL /* filled in later */),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, VLD_WIDGET_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_TOP_DETAILS), SetResize(1, 0), SetMinimalSize(405, 42), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, VLD_WIDGET_MATRIX), SetResize(1, 1), SetMinimalSize(393, 45), SetDataTip(0x701, STR_NULL), SetFill(true, false),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, VLD_WIDGET_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_BOTTOM_RIGHT),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HORIZONTAL_LTR),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DECREASE_SERVICING_INTERVAL), SetFill(false, true),
						SetDataTip(STR_BLACK_SMALL_ARROW_LEFT, STR_VEHICLE_DETAILS_DECREASE_SERVICING_INTERVAL_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_INCREASE_SERVICING_INTERVAL), SetFill(false, true),
						SetDataTip(STR_BLACK_SMALL_ARROW_RIGHT, STR_VEHICLE_DETAILS_DECREASE_SERVICING_INTERVAL_TOOLTIP),
			EndContainer(),
			NWidget(WWT_EMPTY, COLOUR_GREY, VLD_WIDGET_SERVICING_INTERVAL), SetFill(true, true), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_CARGO_CARRIED), SetMinimalSize(96, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_CARGO, STR_VEHICLE_DETAILS_TRAIN_CARGO_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_TRAIN_VEHICLES), SetMinimalSize(99, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_INFORMATION, STR_VEHICLE_DETAILS_TRAIN_INFORMATION_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_CAPACITY_OF_EACH), SetMinimalSize(99, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_CAPACITIES, STR_VEHICLE_DETAILS_TRAIN_CAPACITIES_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_TOTAL_CARGO), SetMinimalSize(99, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_TOTAL_CARGO, STR_VEHICLE_DETAILS_TRAIN_TOTAL_CARGO_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY, VLD_WIDGET_RESIZE),
	EndContainer(),
};


extern int GetTrainDetailsWndVScroll(VehicleID veh_id, TrainDetailsWindowTabs det_tab);
extern void DrawTrainDetails(const Train *v, int left, int right, int y, int vscroll_pos, uint16 vscroll_cap, TrainDetailsWindowTabs det_tab);
extern void DrawRoadVehDetails(const Vehicle *v, int left, int right, int y);
extern void DrawShipDetails(const Vehicle *v, int left, int right, int y);
extern void DrawAircraftDetails(const Aircraft *v, int left, int right, int y);

/** Class for managing the vehicle details window. */
struct VehicleDetailsWindow : Window {
	TrainDetailsWindowTabs tab; ///< For train vehicles: which tab is displayed.

	/** Initialize a newly created vehicle details window */
	VehicleDetailsWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);

		const Vehicle *v = Vehicle::Get(this->window_number);

		this->GetWidget<NWidgetCore>(VLD_WIDGET_RENAME_VEHICLE)->tool_tip = STR_VEHICLE_DETAILS_TRAIN_RENAME + v->type;

		this->owner = v->owner;
		this->tab = TDW_TAB_CARGO;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case VLD_WIDGET_TOP_DETAILS:
				size->height = WD_FRAMERECT_TOP + 4 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				break;

			case VLD_WIDGET_MIDDLE_DETAILS: {
				const Vehicle *v = Vehicle::Get(this->window_number);
				switch (v->type) {
					case VEH_ROAD:
						if (RoadVehicle::From(v)->HasArticulatedPart()) {
							/* An articulated RV has its text drawn under the sprite instead of after it, hence 15 pixels extra. */
							size->height = WD_FRAMERECT_TOP + 15 + 3 * FONT_HEIGHT_NORMAL + 2 + WD_FRAMERECT_BOTTOM;
							/* Add space for the cargo amount for each part. */
							for (const Vehicle *u = v; u != NULL; u = u->Next()) {
								if (u->cargo_cap != 0) size->height += FONT_HEIGHT_NORMAL + 1;
							}
						} else {
							size->height = WD_FRAMERECT_TOP + 4 * FONT_HEIGHT_NORMAL + 3 + WD_FRAMERECT_BOTTOM;
						}
						break;

					case VEH_SHIP:
						size->height = WD_FRAMERECT_TOP + 4 * FONT_HEIGHT_NORMAL + 3 + WD_FRAMERECT_BOTTOM;
						break;

					case VEH_AIRCRAFT:
						size->height = WD_FRAMERECT_TOP + 5 * FONT_HEIGHT_NORMAL + 4 + WD_FRAMERECT_BOTTOM;
						break;

					default:
						NOT_REACHED(); // Train uses VLD_WIDGET_MATRIX instead.
				}
				break;
			}

			case VLD_WIDGET_MATRIX:
				resize->height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
				size->height = 4 * resize->height;
				break;

			case VLD_WIDGET_SERVICING_INTERVAL:
				size->height = WD_FRAMERECT_TOP + FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				break;
		}
	}

	/** Checks whether service interval is enabled for the vehicle. */
	static bool IsVehicleServiceIntervalEnabled(const VehicleType vehicle_type, CompanyID company_id)
	{
		const VehicleDefaultSettings *vds = &Company::Get(company_id)->settings.vehicle;
		switch (vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:    return vds->servint_trains   != 0;
			case VEH_ROAD:     return vds->servint_roadveh  != 0;
			case VEH_SHIP:     return vds->servint_ships    != 0;
			case VEH_AIRCRAFT: return vds->servint_aircraft != 0;
		}
	}

	/**
	 * Draw the details for the given vehicle at the position of the Details windows
	 *
	 * @param v     current vehicle
	 * @param left  The left most coordinate to draw
	 * @param right The right most coordinate to draw
	 * @param y     The y coordinate
	 * @param vscroll_pos Position of scrollbar (train only)
	 * @param vscroll_cap Number of lines currently displayed (train only)
	 * @param det_tab Selected details tab (train only)
	 */
	static void DrawVehicleDetails(const Vehicle *v, int left, int right, int y, int vscroll_pos, uint vscroll_cap, TrainDetailsWindowTabs det_tab)
	{
		switch (v->type) {
			case VEH_TRAIN:    DrawTrainDetails(Train::From(v), left, right, y, vscroll_pos, vscroll_cap, det_tab);  break;
			case VEH_ROAD:     DrawRoadVehDetails(v, left, right, y);  break;
			case VEH_SHIP:     DrawShipDetails(v, left, right, y);     break;
			case VEH_AIRCRAFT: DrawAircraftDetails(Aircraft::From(v), left, right, y); break;
			default: NOT_REACHED();
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == VLD_WIDGET_CAPTION) SetDParam(0, Vehicle::Get(this->window_number)->index);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		const Vehicle *v = Vehicle::Get(this->window_number);

		switch (widget) {
			case VLD_WIDGET_TOP_DETAILS: {
				int y = r.top + WD_FRAMERECT_TOP;

				/* Draw running cost */
				SetDParam(1, v->age / DAYS_IN_LEAP_YEAR);
				SetDParam(0, (v->age + DAYS_IN_YEAR < v->max_age) ? STR_VEHICLE_INFO_AGE : STR_VEHICLE_INFO_AGE_RED);
				SetDParam(2, v->max_age / DAYS_IN_LEAP_YEAR);
				SetDParam(3, v->GetDisplayRunningCost());
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_VEHICLE_INFO_AGE_RUNNING_COST_YR);
				y += FONT_HEIGHT_NORMAL;

				/* Draw max speed */
				switch (v->type) {
					case VEH_TRAIN:
						SetDParam(2, v->GetDisplayMaxSpeed());
						SetDParam(1, Train::From(v)->tcache.cached_power);
						SetDParam(0, Train::From(v)->tcache.cached_weight);
						SetDParam(3, Train::From(v)->tcache.cached_max_te / 1000);
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, (_settings_game.vehicle.train_acceleration_model != TAM_ORIGINAL && Train::From(v)->railtype != RAILTYPE_MAGLEV) ?
								STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE : STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED);
						break;

					case VEH_ROAD:
					case VEH_SHIP:
					case VEH_AIRCRAFT:
						SetDParam(0, v->GetDisplayMaxSpeed());
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_VEHICLE_INFO_MAX_SPEED);
						break;

					default: NOT_REACHED();
				}
				y += FONT_HEIGHT_NORMAL;

				/* Draw profit */
				SetDParam(0, v->GetDisplayProfitThisYear());
				SetDParam(1, v->GetDisplayProfitLastYear());
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_VEHICLE_INFO_PROFIT_THIS_YEAR_LAST_YEAR);
				y += FONT_HEIGHT_NORMAL;

				/* Draw breakdown & reliability */
				SetDParam(0, ToPercent16(v->reliability));
				SetDParam(1, v->breakdowns_since_last_service);
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_VEHICLE_INFO_RELIABILITY_BREAKDOWNS);
				break;
			}

			case VLD_WIDGET_MATRIX:
				/* For trains only. */
				DrawVehicleDetails(v, r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, r.top + WD_MATRIX_TOP, this->vscroll.GetPosition(), this->vscroll.GetCapacity(), this->tab);
				break;

			case VLD_WIDGET_MIDDLE_DETAILS: {
				/* For other vehicles, at the place of the matrix. */
				bool rtl = _dynlang.text_dir == TD_RTL;
				uint sprite_width = max<uint>(GetSprite(v->GetImage(rtl ? DIR_E : DIR_W), ST_NORMAL)->width, 70U) + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;

				uint text_left  = r.left  + (rtl ? 0 : sprite_width);
				uint text_right = r.right - (rtl ? sprite_width : 0);

				uint sprite_left  = rtl ? text_right : r.left;
				uint sprite_right = rtl ? r.right : text_left;

				DrawVehicleImage(v, sprite_left + WD_FRAMERECT_LEFT, sprite_right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, INVALID_VEHICLE, 0);
				DrawVehicleDetails(v, text_left + WD_FRAMERECT_LEFT, text_right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, this->vscroll.GetPosition(), this->vscroll.GetCapacity(), this->tab);
			} break;

			case VLD_WIDGET_SERVICING_INTERVAL:
				/* Draw service interval text */
				SetDParam(0, v->service_interval);
				SetDParam(1, v->date_of_last_service);
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + (r.bottom - r.top + 1 - FONT_HEIGHT_NORMAL) / 2,
						Company::Get(v->owner)->settings.vehicle.servint_ispercent ? STR_VEHICLE_DETAILS_SERVICING_INTERVAL_PERCENT : STR_VEHICLE_DETAILS_SERVICING_INTERVAL_DAYS);
				break;
		}
	}

	/** Repaint vehicle details window. */
	virtual void OnPaint()
	{
		const Vehicle *v = Vehicle::Get(this->window_number);

		this->SetWidgetDisabledState(VLD_WIDGET_RENAME_VEHICLE, v->owner != _local_company);

		if (v->type == VEH_TRAIN) {
			this->DisableWidget(this->tab + VLD_WIDGET_DETAILS_CARGO_CARRIED);
			this->vscroll.SetCount(GetTrainDetailsWndVScroll(v->index, this->tab));
		}

		/* Disable service-scroller when interval is set to disabled */
		this->SetWidgetsDisabledState(!IsVehicleServiceIntervalEnabled(v->type, v->owner),
			VLD_WIDGET_INCREASE_SERVICING_INTERVAL,
			VLD_WIDGET_DECREASE_SERVICING_INTERVAL,
			WIDGET_LIST_END);

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case VLD_WIDGET_RENAME_VEHICLE: { // rename
				const Vehicle *v = Vehicle::Get(this->window_number);
				SetDParam(0, v->index);
				ShowQueryString(STR_VEHICLE_NAME, STR_QUERY_RENAME_TRAIN_CAPTION + v->type,
						MAX_LENGTH_VEHICLE_NAME_BYTES, MAX_LENGTH_VEHICLE_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
			} break;

			case VLD_WIDGET_INCREASE_SERVICING_INTERVAL:   // increase int
			case VLD_WIDGET_DECREASE_SERVICING_INTERVAL: { // decrease int
				int mod = _ctrl_pressed ? 5 : 10;
				const Vehicle *v = Vehicle::Get(this->window_number);

				mod = (widget == VLD_WIDGET_DECREASE_SERVICING_INTERVAL) ? -mod : mod;
				mod = GetServiceIntervalClamped(mod + v->service_interval, v->owner);
				if (mod == v->service_interval) return;

				DoCommandP(v->tile, v->index, mod, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_ERROR_CAN_T_CHANGE_SERVICING));
			} break;

			case VLD_WIDGET_DETAILS_CARGO_CARRIED:
			case VLD_WIDGET_DETAILS_TRAIN_VEHICLES:
			case VLD_WIDGET_DETAILS_CAPACITY_OF_EACH:
			case VLD_WIDGET_DETAILS_TOTAL_CARGO:
				this->SetWidgetsDisabledState(false,
					VLD_WIDGET_DETAILS_CARGO_CARRIED,
					VLD_WIDGET_DETAILS_TRAIN_VEHICLES,
					VLD_WIDGET_DETAILS_CAPACITY_OF_EACH,
					VLD_WIDGET_DETAILS_TOTAL_CARGO,
					widget,
					WIDGET_LIST_END);

				this->tab = (TrainDetailsWindowTabs)(widget - VLD_WIDGET_DETAILS_CARGO_CARRIED);
				this->SetDirty();
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_RENAME_TRAIN + Vehicle::Get(this->window_number)->type), NULL, str);
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(VLD_WIDGET_MATRIX);
		if (nwi != NULL) {
			this->vscroll.SetCapacity(nwi->current_y / this->resize.step_height);
			nwi->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
		}
	}
};

/** Vehicle details window descriptor. */
static const WindowDesc _train_vehicle_details_desc(
	WDP_AUTO, WDP_AUTO, 405, 178,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_train_vehicle_details_widgets, lengthof(_nested_train_vehicle_details_widgets)
);

/** Vehicle details window descriptor for other vehicles than a train. */
static const WindowDesc _nontrain_vehicle_details_desc(
	WDP_AUTO, WDP_AUTO, 405, 113,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_nontrain_vehicle_details_widgets, lengthof(_nested_nontrain_vehicle_details_widgets)
);

/** Shows the vehicle details window of the given vehicle. */
static void ShowVehicleDetailsWindow(const Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_ORDERS, v->index, false);
	DeleteWindowById(WC_VEHICLE_TIMETABLE, v->index, false);
	AllocateWindowDescFront<VehicleDetailsWindow>((v->type == VEH_TRAIN) ? &_train_vehicle_details_desc : &_nontrain_vehicle_details_desc, v->index);
}


/* Unified vehicle GUI - Vehicle View Window */

/** Vehicle view widgets. */
static const NWidgetPart _nested_vehicle_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, VVW_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, VVW_WIDGET_CAPTION), SetDataTip(STR_VEHICLE_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, VVW_WIDGET_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, VVW_WIDGET_PANEL),
			NWidget(WWT_INSET, COLOUR_GREY, VVW_WIDGET_INSET), SetPadding(2, 2, 2, 2),
				NWidget(NWID_VIEWPORT, INVALID_COLOUR, VVW_WIDGET_VIEWPORT), SetMinimalSize(226, 84), SetResize(1, 1), SetPadding(1, 1, 1, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_CENTER_MAIN_VIEH), SetMinimalSize(18, 18), SetFill(true, true), SetDataTip(SPR_CENTRE_VIEW_VEHICLE, 0x0 /* filled later */),
			NWidget(NWID_SELECTION, INVALID_COLOUR, VVW_WIDGET_SELECT_DEPOT_CLONE),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_GOTO_DEPOT), SetMinimalSize(18, 18), SetFill(true, true), SetDataTip(0x0 /* filled later */, 0x0 /* filled later */),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_CLONE_VEH), SetMinimalSize(18, 18), SetFill(true, true), SetDataTip(0x0 /* filled later */, 0x0 /* filled later */),
			EndContainer(),
			/* For trains only, 'ignore signal' button. */
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_FORCE_PROCEED), SetMinimalSize(18, 18), SetFill(true, true),
											SetDataTip(SPR_IGNORE_SIGNALS, STR_VEHICLE_VIEW_TRAIN_IGNORE_SIGNAL_TOOLTIP),
			NWidget(NWID_SELECTION, INVALID_COLOUR, VVW_WIDGET_SELECT_REFIT_TURN),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_REFIT_VEH), SetMinimalSize(18, 18), SetFill(true, true), SetDataTip(SPR_REFIT_VEHICLE, 0x0 /* filled later */),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_TURN_AROUND), SetMinimalSize(18, 18), SetFill(true, true),
												SetDataTip(SPR_FORCE_VEHICLE_TURN, STR_VEHICLE_VIEW_ROAD_VEHICLE_REVERSE_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_SHOW_ORDERS), SetFill(true, true), SetMinimalSize(18, 18), SetDataTip(SPR_SHOW_ORDERS, 0x0 /* filled later */),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_SHOW_DETAILS), SetFill(true, true), SetMinimalSize(18, 18), SetDataTip(SPR_SHOW_VEHICLE_DETAILS, 0x0 /* filled later */),
			NWidget(WWT_PANEL, COLOUR_GREY, VVW_WIDGET_EMPTY_BOTTOM_RIGHT), SetFill(true, true), SetMinimalSize(18, 0), SetResize(0, 1), EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, VVW_WIDGET_START_STOP_VEH), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM), SetResize(1, 0), SetFill(true, false),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY, VVW_WIDGET_RESIZE),
	EndContainer(),
};

/** Vehicle view window descriptor for all vehicles but trains. */
static const WindowDesc _vehicle_view_desc(
	WDP_AUTO, WDP_AUTO, 250, 116,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_vehicle_view_widgets, lengthof(_nested_vehicle_view_widgets)
);

/** Vehicle view window descriptor for trains. Only minimum_height and
 *  default_height are different for train view.
 */
static const WindowDesc _train_view_desc(
	WDP_AUTO, WDP_AUTO, 250, 134,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_vehicle_view_widgets, lengthof(_nested_vehicle_view_widgets)
);


/* Just to make sure, nobody has changed the vehicle type constants, as we are
	 using them for array indexing in a number of places here. */
assert_compile(VEH_TRAIN == 0);
assert_compile(VEH_ROAD == 1);
assert_compile(VEH_SHIP == 2);
assert_compile(VEH_AIRCRAFT == 3);

/** Zoom levels for vehicle views indexed by vehicle type. */
static const ZoomLevel _vehicle_view_zoom_levels[] = {
	ZOOM_LVL_TRAIN,
	ZOOM_LVL_ROADVEH,
	ZOOM_LVL_SHIP,
	ZOOM_LVL_AIRCRAFT,
};

/* Constants for geometry of vehicle view viewport */
static const int VV_INITIAL_VIEWPORT_WIDTH = 226;
static const int VV_INITIAL_VIEWPORT_HEIGHT = 84;
static const int VV_INITIAL_VIEWPORT_HEIGHT_TRAIN = 102;

/** Command indices for the _vehicle_command_translation_table. */
enum VehicleCommandTranslation {
	VCT_CMD_START_STOP = 0,
	VCT_CMD_CLONE_VEH,
	VCT_CMD_TURN_AROUND,
};

/** Command codes for the shared buttons indexed by VehicleCommandTranslation and vehicle type. */
static const uint32 _vehicle_command_translation_table[][4] = {
	{ // VCT_CMD_START_STOP
		CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_TRAIN),
		CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_ROAD_VEHICLE),
		CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_SHIP),
		CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_AIRCRAFT)
	},
	{ // VCT_CMD_CLONE_VEH
		CMD_CLONE_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_BUY_TRAIN),
		CMD_CLONE_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_BUY_ROAD_VEHICLE),
		CMD_CLONE_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_BUY_SHIP),
		CMD_CLONE_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_BUY_AIRCRAFT)
	},
	{ // VCT_CMD_TURN_AROUND
		CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_ERROR_CAN_T_REVERSE_DIRECTION_TRAIN),
		CMD_TURN_ROADVEH            | CMD_MSG(STR_ERROR_CAN_T_MAKE_ROAD_VEHICLE_TURN),
		0xffffffff, // invalid for ships
		0xffffffff  // invalid for aircrafts
	},
};

/** Checks whether the vehicle may be refitted at the moment.*/
static bool IsVehicleRefitable(const Vehicle *v)
{
	if (!v->IsStoppedInDepot()) return false;

	do {
		if (IsEngineRefittable(v->engine_type)) return true;
	} while ((v->type == VEH_TRAIN || v->type == VEH_ROAD) && (v = v->Next()) != NULL);

	return false;
}

/** Window manager class for viewing a vehicle. */
struct VehicleViewWindow : Window {
private:
	/** Display planes available in the vehicle view window. */
	enum PlaneSelections {
		SEL_DC_GOTO_DEPOT,  ///< Display 'goto depot' button in #VVW_WIDGET_SELECT_DEPOT_CLONE stacked widget.
		SEL_DC_CLONE,       ///< Display 'clone vehicle' button in #VVW_WIDGET_SELECT_DEPOT_CLONE stacked widget.

		SEL_RT_REFIT,       ///< Display 'refit' button in #VVW_WIDGET_SELECT_REFIT_TURN stacked widget.
		SEL_RT_TURN_AROUND, ///< Display 'turn around' button in #VVW_WIDGET_SELECT_REFIT_TURN stacked widget.

		SEL_DC_BASEPLANE = SEL_DC_GOTO_DEPOT, ///< First plane of the #VVW_WIDGET_SELECT_DEPOT_CLONE stacked widget.
		SEL_RT_BASEPLANE = SEL_RT_REFIT,      ///< First plane of the #VVW_WIDGET_SELECT_REFIT_TURN stacked widget.
	};

	/** Display a plane in the window.
	 * @param plane Plane to show.
	 */
	void SelectPlane(PlaneSelections plane)
	{
		switch (plane) {
			case SEL_DC_GOTO_DEPOT:
			case SEL_DC_CLONE:
				this->GetWidget<NWidgetStacked>(VVW_WIDGET_SELECT_DEPOT_CLONE)->SetDisplayedPlane(plane - SEL_DC_BASEPLANE);
				break;

			case SEL_RT_REFIT:
			case SEL_RT_TURN_AROUND:
				this->GetWidget<NWidgetStacked>(VVW_WIDGET_SELECT_REFIT_TURN)->SetDisplayedPlane(plane - SEL_RT_BASEPLANE);
				break;

			default:
				NOT_REACHED();
		}
	}

public:
	VehicleViewWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->CreateNestedTree(desc);

		/* Sprites for the 'send to depot' button indexed by vehicle type. */
		static const SpriteID vehicle_view_goto_depot_sprites[] = {
			SPR_SEND_TRAIN_TODEPOT,
			SPR_SEND_ROADVEH_TODEPOT,
			SPR_SEND_SHIP_TODEPOT,
			SPR_SEND_AIRCRAFT_TODEPOT,
		};
		const Vehicle *v = Vehicle::Get(window_number);
		this->GetWidget<NWidgetCore>(VVW_WIDGET_GOTO_DEPOT)->widget_data = vehicle_view_goto_depot_sprites[v->type];

		/* Sprites for the 'clone vehicle' button indexed by vehicle type. */
		static const SpriteID vehicle_view_clone_sprites[] = {
			SPR_CLONE_TRAIN,
			SPR_CLONE_ROADVEH,
			SPR_CLONE_SHIP,
			SPR_CLONE_AIRCRAFT,
		};
		this->GetWidget<NWidgetCore>(VVW_WIDGET_CLONE_VEH)->widget_data = vehicle_view_clone_sprites[v->type];

		switch (v->type) {
			case VEH_TRAIN:
				this->GetWidget<NWidgetCore>(VVW_WIDGET_TURN_AROUND)->tool_tip = STR_VEHICLE_VIEW_TRAIN_REVERSE_TOOLTIP;
				break;

			case VEH_ROAD:
				break;

			case VEH_SHIP:
			case VEH_AIRCRAFT:
				this->SelectPlane(SEL_RT_REFIT);
				break;

			default: NOT_REACHED();
		}
		this->FinishInitNested(desc, window_number);
		this->owner = v->owner;
		this->GetWidget<NWidgetViewport>(VVW_WIDGET_VIEWPORT)->InitializeViewport(this, this->window_number | (1 << 31), _vehicle_view_zoom_levels[v->type]);

		this->GetWidget<NWidgetCore>(VVW_WIDGET_START_STOP_VEH)->tool_tip   = STR_VEHICLE_VIEW_TRAIN_STATE_START_STOP_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VVW_WIDGET_CENTER_MAIN_VIEH)->tool_tip = STR_VEHICLE_VIEW_TRAIN_LOCATION_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VVW_WIDGET_REFIT_VEH)->tool_tip        = STR_VEHICLE_VIEW_TRAIN_REFIT_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VVW_WIDGET_GOTO_DEPOT)->tool_tip       = STR_VEHICLE_VIEW_TRAIN_SEND_TO_DEPOT_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VVW_WIDGET_SHOW_ORDERS)->tool_tip      = STR_VEHICLE_VIEW_TRAIN_ORDERS_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VVW_WIDGET_SHOW_DETAILS)->tool_tip     = STR_VEHICLE_VIEW_TRAIN_SHOW_DETAILS_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VVW_WIDGET_CLONE_VEH)->tool_tip        = STR_VEHICLE_VIEW_CLONE_TRAIN_INFO + v->type;
	}

	~VehicleViewWindow()
	{
		DeleteWindowById(WC_VEHICLE_ORDERS, this->window_number, false);
		DeleteWindowById(WC_VEHICLE_REFIT, this->window_number, false);
		DeleteWindowById(WC_VEHICLE_DETAILS, this->window_number, false);
		DeleteWindowById(WC_VEHICLE_TIMETABLE, this->window_number, false);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		switch (widget) {
			case VVW_WIDGET_FORCE_PROCEED:
				if (v->type != VEH_TRAIN) {
					size->height = 0;
					size->width = 0;
				} break;

			case VVW_WIDGET_VIEWPORT:
				size->width = VV_INITIAL_VIEWPORT_WIDTH;
				size->height = (v->type == VEH_TRAIN) ? VV_INITIAL_VIEWPORT_HEIGHT_TRAIN : VV_INITIAL_VIEWPORT_HEIGHT;
				break;
		}
	}

	virtual void OnPaint()
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		bool is_localcompany = v->owner == _local_company;
		bool refitable_and_stopped_in_depot = IsVehicleRefitable(v);

		this->SetWidgetDisabledState(VVW_WIDGET_GOTO_DEPOT, !is_localcompany);
		this->SetWidgetDisabledState(VVW_WIDGET_REFIT_VEH, !refitable_and_stopped_in_depot || !is_localcompany);
		this->SetWidgetDisabledState(VVW_WIDGET_CLONE_VEH, !is_localcompany);

		if (v->type == VEH_TRAIN) {
			this->SetWidgetDisabledState(VVW_WIDGET_FORCE_PROCEED, !is_localcompany);
			this->SetWidgetDisabledState(VVW_WIDGET_TURN_AROUND, !is_localcompany);
		}

		this->DrawWidgets();
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget != VVW_WIDGET_CAPTION) return;

		const Vehicle *v = Vehicle::Get(this->window_number);
		SetDParam(0, v->index);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != VVW_WIDGET_START_STOP_VEH) return;

		const Vehicle *v = Vehicle::Get(this->window_number);
		StringID str;
		if (v->vehstatus & VS_CRASHED) {
			str = STR_VEHICLE_STATUS_CRASHED;
		} else if (v->type != VEH_AIRCRAFT && v->breakdown_ctr == 1) { // check for aircraft necessary?
			str = STR_VEHICLE_STATUS_BROKEN_DOWN;
		} else if (v->vehstatus & VS_STOPPED) {
			if (v->type == VEH_TRAIN) {
				if (v->cur_speed == 0) {
					if (Train::From(v)->tcache.cached_power == 0) {
						str = STR_VEHICLE_STATUS_TRAIN_NO_POWER;
					} else {
						str = STR_VEHICLE_STATUS_STOPPED;
					}
				} else {
					SetDParam(0, v->GetDisplaySpeed());
					str = STR_VEHICLE_STATUS_TRAIN_STOPPING + _settings_client.gui.vehicle_speed;
				}
			} else { // no train
				str = STR_VEHICLE_STATUS_STOPPED;
			}
		} else if (v->type == VEH_TRAIN && HasBit(Train::From(v)->flags, VRF_TRAIN_STUCK) && !v->current_order.IsType(OT_LOADING)) {
			str = STR_VEHICLE_STATUS_TRAIN_STUCK;
		} else { // vehicle is in a "normal" state, show current order
			switch (v->current_order.GetType()) {
				case OT_GOTO_STATION: {
					SetDParam(0, v->current_order.GetDestination());
					SetDParam(1, v->GetDisplaySpeed());
					str = STR_VEHICLE_STATUS_HEADING_FOR_STATION + _settings_client.gui.vehicle_speed;
				} break;

				case OT_GOTO_DEPOT: {
					if (v->type == VEH_AIRCRAFT) {
						/* Aircrafts always go to a station, even if you say depot */
						SetDParam(0, v->current_order.GetDestination());
						SetDParam(1, v->GetDisplaySpeed());
					} else {
						Depot *depot = Depot::Get(v->current_order.GetDestination());
						SetDParam(0, depot->town_index);
						SetDParam(1, v->GetDisplaySpeed());
					}
					if (v->current_order.GetDepotActionType() & ODATFB_HALT) {
						str = STR_VEHICLE_STATUS_HEADING_FOR_TRAIN_DEPOT + 2 * v->type + _settings_client.gui.vehicle_speed;
					} else {
						str = STR_VEHICLE_STATUS_HEADING_FOR_TRAIN_DEPOT_SERVICE + 2 * v->type + _settings_client.gui.vehicle_speed;
					}
				} break;

				case OT_LOADING:
					str = STR_VEHICLE_STATUS_LOADING_UNLOADING;
					break;

				case OT_GOTO_WAYPOINT: {
					assert(v->type == VEH_TRAIN || v->type == VEH_SHIP);
					SetDParam(0, v->current_order.GetDestination());
					str = STR_VEHICLE_STATUS_HEADING_FOR_WAYPOINT + _settings_client.gui.vehicle_speed;
					SetDParam(1, v->GetDisplaySpeed());
					break;
				}

				case OT_LEAVESTATION:
					if (v->type != VEH_AIRCRAFT) {
						str = STR_VEHICLE_STATUS_LEAVING;
						break;
					}
					/* fall-through if aircraft. Does this even happen? */

				default:
					if (v->GetNumOrders() == 0) {
						str = STR_VEHICLE_STATUS_NO_ORDERS + _settings_client.gui.vehicle_speed;
						SetDParam(0, v->GetDisplaySpeed());
					} else {
						str = STR_EMPTY;
					}
					break;
			}
		}

		/* draw the flag plus orders */
		DrawSprite(v->vehstatus & VS_STOPPED ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, PAL_NONE, WD_FRAMERECT_LEFT, r.top + WD_FRAMERECT_TOP);
		DrawString(r.left + WD_FRAMERECT_LEFT + 6, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, str, TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnClick(Point pt, int widget)
	{
		const Vehicle *v = Vehicle::Get(this->window_number);

		switch (widget) {
			case VVW_WIDGET_START_STOP_VEH: // start stop
				DoCommandP(v->tile, v->index, 0,
										_vehicle_command_translation_table[VCT_CMD_START_STOP][v->type]);
				break;
			case VVW_WIDGET_CENTER_MAIN_VIEH: {// center main view
				const Window *mainwindow = FindWindowById(WC_MAIN_WINDOW, 0);
				/* code to allow the main window to 'follow' the vehicle if the ctrl key is pressed */
				if (_ctrl_pressed && mainwindow->viewport->zoom == ZOOM_LVL_NORMAL) {
					mainwindow->viewport->follow_vehicle = v->index;
				} else {
					ScrollMainWindowTo(v->x_pos, v->y_pos, v->z_pos);
				}
			} break;

			case VVW_WIDGET_GOTO_DEPOT: // goto hangar
				DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, GetCmdSendToDepot(v));
				break;
			case VVW_WIDGET_REFIT_VEH: // refit
				ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID, this);
				break;
			case VVW_WIDGET_SHOW_ORDERS: // show orders
				if (_ctrl_pressed) {
					ShowTimetableWindow(v);
				} else {
					ShowOrdersWindow(v);
				}
				break;
			case VVW_WIDGET_SHOW_DETAILS: // show details
				ShowVehicleDetailsWindow(v);
				break;
			case VVW_WIDGET_CLONE_VEH: // clone vehicle
				DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0,
										_vehicle_command_translation_table[VCT_CMD_CLONE_VEH][v->type],
										CcCloneVehicle);
				break;
			case VVW_WIDGET_TURN_AROUND: // turn around
				assert(v->type == VEH_TRAIN || v->type == VEH_ROAD);
				DoCommandP(v->tile, v->index, 0,
										_vehicle_command_translation_table[VCT_CMD_TURN_AROUND][v->type]);
				break;
			case VVW_WIDGET_FORCE_PROCEED: // force proceed
				assert(v->type == VEH_TRAIN);
				DoCommandP(v->tile, v->index, 0, CMD_FORCE_TRAIN_PROCEED | CMD_MSG(STR_ERROR_CAN_T_MAKE_TRAIN_PASS_SIGNAL));
				break;
		}
	}

	virtual void OnResize()
	{
		if (this->viewport != NULL) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(VVW_WIDGET_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);
		}
	}

	virtual void OnTick()
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		bool veh_stopped = v->IsStoppedInDepot();

		/* Widget VVW_WIDGET_GOTO_DEPOT must be hidden if the vehicle is already stopped in depot.
		 * Widget VVW_WIDGET_CLONE_VEH should then be shown, since cloning is allowed only while in depot and stopped.
		 */
		PlaneSelections plane = veh_stopped ? SEL_DC_CLONE : SEL_DC_GOTO_DEPOT;
		NWidgetStacked *nwi = this->GetWidget<NWidgetStacked>(VVW_WIDGET_SELECT_DEPOT_CLONE); // Selection widget 'send to depot' / 'clone'.
		if (nwi->shown_plane + SEL_DC_BASEPLANE != plane) {
			this->SelectPlane(plane);
			this->SetWidgetDirty(VVW_WIDGET_SELECT_DEPOT_CLONE);
		}
		/* The same system applies to widget VVW_WIDGET_REFIT_VEH and VVW_WIDGET_TURN_AROUND.*/
		if (v->type == VEH_ROAD || v->type == VEH_TRAIN) {
			PlaneSelections plane = veh_stopped ? SEL_RT_REFIT : SEL_RT_TURN_AROUND;
			NWidgetStacked *nwi = this->GetWidget<NWidgetStacked>(VVW_WIDGET_SELECT_REFIT_TURN);
			if (nwi->shown_plane + SEL_RT_BASEPLANE != plane) {
				this->SelectPlane(plane);
				this->SetWidgetDirty(VVW_WIDGET_SELECT_REFIT_TURN);
			}
		}
	}
};


/** Shows the vehicle view window of the given vehicle. */
void ShowVehicleViewWindow(const Vehicle *v)
{
	AllocateWindowDescFront<VehicleViewWindow>((v->type == VEH_TRAIN) ? &_train_view_desc : &_vehicle_view_desc, v->index);
}

void StopGlobalFollowVehicle(const Vehicle *v)
{
	Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
	if (w != NULL && w->viewport->follow_vehicle == v->index) {
		ScrollMainWindowTo(v->x_pos, v->y_pos, v->z_pos, true); // lock the main view on the vehicle's last position
		w->viewport->follow_vehicle = INVALID_VEHICLE;
	}
}
