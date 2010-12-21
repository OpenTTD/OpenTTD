/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_gui.cpp The base GUI for all vehicles. */

#include "stdafx.h"
#include "debug.h"
#include "company_func.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "vehicle_gui_base.h"
#include "viewport_func.h"
#include "newgrf_text.h"
#include "newgrf_debug.h"
#include "roadveh.h"
#include "train.h"
#include "aircraft.h"
#include "depot_map.h"
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
#include "spritecache.h"
#include "core/geometry_func.hpp"
#include "company_base.h"
#include "engine_func.h"
#include "station_base.h"
#include "tilehighlight_func.h"

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

const StringID BaseVehicleListWindow::vehicle_depot_name[] = {
	STR_VEHICLE_LIST_SEND_TRAIN_TO_DEPOT,
	STR_VEHICLE_LIST_SEND_ROAD_VEHICLE_TO_DEPOT,
	STR_VEHICLE_LIST_SEND_SHIP_TO_DEPOT,
	STR_VEHICLE_LIST_SEND_AIRCRAFT_TO_HANGAR
};

void BaseVehicleListWindow::BuildVehicleList()
{
	if (!this->vehicles.NeedRebuild()) return;

	DEBUG(misc, 3, "Building vehicle list type %d for company %d given index %d", this->vli.type, this->vli.company, this->vli.index);

	GenerateVehicleSortList(&this->vehicles, this->vli);

	uint unitnumber = 0;
	for (const Vehicle **v = this->vehicles.Begin(); v != this->vehicles.End(); v++) {
		unitnumber = max<uint>(unitnumber, (*v)->unitnumber);
	}

	/* Because 111 is much less wide than e.g. 999 we use the
	 * wider numbers to determine the width instead of just
	 * the random number that it seems to be. */
	if (unitnumber >= 1000) {
		this->unitnumber_digits = 4;
	} else if (unitnumber >= 100) {
		this->unitnumber_digits = 3;
	} else {
		this->unitnumber_digits = 2;
	}

	this->vehicles.RebuildDone();
	this->vscroll->SetCount(this->vehicles.Length());
}

/**
 * Compute the size for the Action dropdown.
 * @param show_autoreplace If true include the autoreplace item.
 * @param show_group If true include group-related stuff.
 * @return Required size.
 */
Dimension BaseVehicleListWindow::GetActionDropdownSize(bool show_autoreplace, bool show_group)
{
	Dimension d = {0, 0};

	if (show_autoreplace) d = maxdim(d, GetStringBoundingBox(STR_VEHICLE_LIST_REPLACE_VEHICLES));
	d = maxdim(d, GetStringBoundingBox(STR_VEHICLE_LIST_SEND_FOR_SERVICING));
	d = maxdim(d, GetStringBoundingBox(this->vehicle_depot_name[this->vli.vtype]));

	if (show_group) {
		d = maxdim(d, GetStringBoundingBox(STR_GROUP_ADD_SHARED_VEHICLE));
		d = maxdim(d, GetStringBoundingBox(STR_GROUP_REMOVE_ALL_VEHICLES));
	}

	return d;
}

/**
 * Display the Action dropdown window.
 * @param show_autoreplace If true include the autoreplace item.
 * @param show_group If true include group-related stuff.
 * @return Itemlist for dropdown
 */
DropDownList *BaseVehicleListWindow::BuildActionDropdownList(bool show_autoreplace, bool show_group)
{
	DropDownList *list = new DropDownList();

	if (show_autoreplace) list->push_back(new DropDownListStringItem(STR_VEHICLE_LIST_REPLACE_VEHICLES, ADI_REPLACE, false));
	list->push_back(new DropDownListStringItem(STR_VEHICLE_LIST_SEND_FOR_SERVICING, ADI_SERVICE, false));
	list->push_back(new DropDownListStringItem(this->vehicle_depot_name[this->vli.vtype], ADI_DEPOT, false));

	if (show_group) {
		list->push_back(new DropDownListStringItem(STR_GROUP_ADD_SHARED_VEHICLE, ADI_ADD_SHARED, false));
		list->push_back(new DropDownListStringItem(STR_GROUP_REMOVE_ALL_VEHICLES, ADI_REMOVE_ALL, false));
	}

	return list;
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
	SpriteID spr;

	/* draw profit-based coloured icons */
	if (v->age <= DAYS_IN_YEAR * 2) {
		spr = SPR_PROFIT_NA;
	} else if (v->GetDisplayProfitLastYear() < 0) {
		spr = SPR_PROFIT_NEGATIVE;
	} else if (v->GetDisplayProfitLastYear() < 10000) {
		spr = SPR_PROFIT_SOME;
	} else {
		spr = SPR_PROFIT_LOT;
	}
	DrawSprite(spr, PAL_NONE, x, y);
}

/** Maximum number of refit cycles we try, to prevent infinite loops. And we store only a byte anyway */
static const uint MAX_REFIT_CYCLE = 256;

/**
 * Get the best fitting subtype when 'cloning'/'replacing' v_from with v_for.
 * Assuming they are going to carry the same cargo ofcourse!
 * @param v_from the vehicle to match the subtype from
 * @param v_for  the vehicle to get the subtype for
 * @return the best sub type
 */
byte GetBestFittingSubType(Vehicle *v_from, Vehicle *v_for)
{
	const Engine *e_from = Engine::Get(v_from->engine_type);
	const Engine *e_for  = Engine::Get(v_for->engine_type);

	/* If one them doesn't carry cargo, there's no need to find a sub type */
	if (!e_from->CanCarryCargo() || !e_for->CanCarryCargo()) return 0;

	if (!HasBit(e_from->info.callback_mask, CBM_VEHICLE_CARGO_SUFFIX) ||
			!HasBit(e_for->info.callback_mask,  CBM_VEHICLE_CARGO_SUFFIX)) {
		/* One of the engines doesn't have cargo suffixes, i.e. sub types. */
		return 0;
	}

	/* It has to be possible for v_for to carry the cargo of v_from. */
	if (!HasBit(e_for->info.refit_mask, v_from->cargo_type)) return 0;

	StringID expected_string = GetCargoSubtypeText(v_from);

	CargoID old_cargo_type = v_for->cargo_type;
	byte old_cargo_subtype = v_for->cargo_subtype;
	byte ret_refit_cyc = 0;

	/* Set the 'destination' cargo */
	v_for->cargo_type = v_from->cargo_type;

	/* Cycle through the refits */
	for (uint refit_cyc = 0; refit_cyc < MAX_REFIT_CYCLE; refit_cyc++) {
		v_for->cargo_subtype = refit_cyc;

		/* Make sure we don't pick up anything cached. */
		v_for->First()->InvalidateNewGRFCache();
		v_for->InvalidateNewGRFCache();
		uint16 callback = GetVehicleCallback(CBID_VEHICLE_CARGO_SUFFIX, 0, 0, v_for->engine_type, v_for);

		if (callback == 0xFF) callback = CALLBACK_FAILED;
		if (callback == CALLBACK_FAILED) break;

		if (GetCargoSubtypeText(v_for) != expected_string) continue;

		/* We found something matching. */
		ret_refit_cyc = refit_cyc;
		break;
	}

	/* Reset the vehicle's cargo type */
	v_for->cargo_type    = old_cargo_type;
	v_for->cargo_subtype = old_cargo_subtype;

	/* Make sure we don't taint the vehicle. */
	v_for->First()->InvalidateNewGRFCache();
	v_for->InvalidateNewGRFCache();

	return ret_refit_cyc;
}

/** Option to refit a vehicle chain */
struct RefitOption {
	CargoID cargo;    ///< Cargo to refit to
	byte subtype;     ///< Subcargo to use
	uint16 value;     ///< GRF-local String to display for the cargo
	EngineID engine;  ///< Engine for which to resolve #value

	/**
	 * Inequality operator for #RefitOption.
	 * @param other Compare to this #RefitOption.
	 * @return True if both #RefitOption are different.
	 */
	FORCEINLINE bool operator != (const RefitOption &other) const
	{
		return other.cargo != this->cargo || other.value != this->value;
	}

	/**
	 * Equality operator for #RefitOption.
	 * @param other Compare to this #RefitOption.
	 * @return True if both #RefitOption are equal.
	 */
	FORCEINLINE bool operator == (const RefitOption &other) const
	{
		return other.cargo == this->cargo && other.value == this->value;
	}
};

typedef SmallVector<RefitOption, 32> SubtypeList; ///< List of refit subtypes associated to a cargo.

/**
 * Draw the list of available refit options for a consist and highlight the selected refit option (if any).
 * @param list  List of subtype options for each (sorted) cargo.
 * @param sel   Selected refit cargo-type in the window
 * @param pos   Position of the selected item in caller widow
 * @param rows  Number of rows(capacity) in caller window
 * @param delta Step height in caller window
 * @param r     Rectangle of the matrix widget.
 */
static void DrawVehicleRefitWindow(const SubtypeList list[NUM_CARGO], int sel, uint pos, uint rows, uint delta, const Rect &r)
{
	uint y = r.top + WD_MATRIX_TOP;
	uint current = 0;

	/* Draw the list of subtypes for each cargo, and find the selected refit option (by its position). */
	for (uint i = 0; current < pos + rows && i < NUM_CARGO; i++) {
		for (uint j = 0; current < pos + rows && j < list[i].Length(); j++) {
			/* Refit options with a position smaller than pos don't have to be drawn. */
			if (current < pos) {
				current++;
				continue;
			}

			TextColour colour = (sel == (int)current) ? TC_WHITE : TC_BLACK;
			const RefitOption refit = list[i][j];
			/* Get the cargo name. */
			SetDParam(0, CargoSpec::Get(refit.cargo)->name);
			/* If the callback succeeded, draw the cargo suffix. */
			if (refit.value != CALLBACK_FAILED) {
				SetDParam(1, GetGRFStringID(GetEngineGRFID(refit.engine), 0xD000 + refit.value));
				DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y, STR_JUST_STRING_SPACE_STRING, colour);
			} else {
				DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y, STR_JUST_STRING, colour);
			}

			y += delta;
			current++;
		}
	}
}

/** Widget numbers of the vehicle refit window. */
enum VehicleRefitWidgets {
	VRW_CAPTION,
	VRW_VEHICLE_PANEL_DISPLAY,
	VRW_SHOW_HSCROLLBAR,
	VRW_HSCROLLBAR,
	VRW_SELECTHEADER,
	VRW_MATRIX,
	VRW_SCROLLBAR,
	VRW_INFOPANEL,
	VRW_REFITBUTTON,
};

/** Refit cargo window. */
struct RefitWindow : public Window {
	int sel;                     ///< Index in refit options, \c -1 if nothing is selected.
	RefitOption *cargo;          ///< Refit option selected by \v sel.
	SubtypeList list[NUM_CARGO]; ///< List of refit subtypes available for each sorted cargo.
	VehicleOrderID order;        ///< If not #INVALID_VEH_ORDER_ID, selection is part of a refit order (rather than execute directly).
	uint information_width;      ///< Width required for correctly displaying all cargos in the information panel.
	Scrollbar *vscroll;          ///< The main scrollbar.
	Scrollbar *hscroll;          ///< Only used for long vehicles.
	int vehicle_width;           ///< Width of the vehicle being drawn.
	int sprite_left;             ///< Left position of the vehicle sprite.
	int sprite_right;            ///< Right position of the vehicle sprite.
	uint vehicle_margin;         ///< Margin to use while selecting vehicles when the vehicle image is centered.
	int click_x;                 ///< Position of the first click while dragging.
	VehicleID selected_vehicle;  ///< First vehicle in the current selection.
	uint8 num_vehicles;          ///< Number of selected vehicles.

	/**
	 * Collects all (cargo, subcargo) refit options of a vehicle chain.
	 */
	void BuildRefitList()
	{
		for (uint i = 0; i < NUM_CARGO; i++) this->list[i].Clear();
		Vehicle *v = Vehicle::Get(this->window_number);

		/* Check only the selected vehicles. */
		VehicleSet vehicles_to_refit;
		GetVehicleSet(vehicles_to_refit, Vehicle::Get(this->selected_vehicle), this->num_vehicles);

		do {
			if (v->type == VEH_TRAIN && !vehicles_to_refit.Contains(v->index)) continue;
			const Engine *e = Engine::Get(v->engine_type);
			uint32 cmask = e->info.refit_mask;
			byte callback_mask = e->info.callback_mask;

			/* Skip this engine if it does not carry anything */
			if (!e->CanCarryCargo()) continue;

			/* Loop through all cargos in the refit mask */
			int current_index = 0;
			const CargoSpec *cs;
			FOR_ALL_SORTED_CARGOSPECS(cs) {
				CargoID cid = cs->Index();
				/* Skip cargo type if it's not listed */
				if (!HasBit(cmask, cid)) {
					current_index++;
					continue;
				}

				/* Check the vehicle's callback mask for cargo suffixes */
				if (HasBit(callback_mask, CBM_VEHICLE_CARGO_SUFFIX)) {
					/* Make a note of the original cargo type. It has to be
					 * changed to test the cargo & subtype... */
					CargoID temp_cargo = v->cargo_type;
					byte temp_subtype  = v->cargo_subtype;

					v->cargo_type = cid;

					for (uint refit_cyc = 0; refit_cyc < MAX_REFIT_CYCLE; refit_cyc++) {
						v->cargo_subtype = refit_cyc;

						/* Make sure we don't pick up anything cached. */
						v->First()->InvalidateNewGRFCache();
						v->InvalidateNewGRFCache();
						uint16 callback = GetVehicleCallback(CBID_VEHICLE_CARGO_SUFFIX, 0, 0, v->engine_type, v);

						if (callback == 0xFF) callback = CALLBACK_FAILED;
						if (refit_cyc != 0 && callback == CALLBACK_FAILED) break;

						RefitOption option;
						option.cargo   = cid;
						option.subtype = refit_cyc;
						option.value   = callback;
						option.engine  = v->engine_type;
						this->list[current_index].Include(option);
					}

					/* Reset the vehicle's cargo type */
					v->cargo_type    = temp_cargo;
					v->cargo_subtype = temp_subtype;

					/* And make sure we haven't tainted the cache */
					v->First()->InvalidateNewGRFCache();
					v->InvalidateNewGRFCache();
				} else {
					/* No cargo suffix callback -- use no subtype */
					RefitOption option;
					option.cargo   = cid;
					option.subtype = 0;
					option.value   = CALLBACK_FAILED;
					option.engine  = INVALID_ENGINE;
					this->list[current_index].Include(option);
				}
				current_index++;
			}
		} while (v->IsGroundVehicle() && (v = v->Next()) != NULL);

		int scroll_size = 0;
		for (uint i = 0; i < NUM_CARGO; i++) {
			scroll_size += (this->list[i].Length());
		}
		this->vscroll->SetCount(scroll_size);
	}

	/**
	 * Gets the #RefitOption placed in the selected index.
	 * @return Pointer to the #RefitOption currently in use.
	 */
	RefitOption *GetRefitOption()
	{
		if (this->sel < 0) return NULL;
		int subtype = 0;
		for (uint i = 0; subtype <= this->sel && i < NUM_CARGO; i++) {
			for (uint j = 0; subtype <= this->sel && j < this->list[i].Length(); j++) {
				if (subtype == this->sel) {
					return &this->list[i][j];
				}
				subtype++;
			}
		}

		return NULL;
	}

	RefitWindow(const WindowDesc *desc, const Vehicle *v, VehicleOrderID order) : Window()
	{
		this->sel = -1;
		this->CreateNestedTree(desc);

		this->vscroll = this->GetScrollbar(VRW_SCROLLBAR);
		this->hscroll = (v->IsGroundVehicle() ? this->GetScrollbar(VRW_HSCROLLBAR) : NULL);
		this->GetWidget<NWidgetCore>(VRW_SELECTHEADER)->tool_tip = STR_REFIT_TRAIN_LIST_TOOLTIP + v->type;
		this->GetWidget<NWidgetCore>(VRW_MATRIX)->tool_tip       = STR_REFIT_TRAIN_LIST_TOOLTIP + v->type;
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(VRW_REFITBUTTON);
		nwi->widget_data = STR_REFIT_TRAIN_REFIT_BUTTON + v->type;
		nwi->tool_tip    = STR_REFIT_TRAIN_REFIT_TOOLTIP + v->type;
		this->GetWidget<NWidgetStacked>(VRW_SHOW_HSCROLLBAR)->SetDisplayedPlane(v->IsGroundVehicle() ? 0 : SZSP_HORIZONTAL);
		this->GetWidget<NWidgetCore>(VRW_VEHICLE_PANEL_DISPLAY)->tool_tip = (v->type == VEH_TRAIN) ? STR_REFIT_SELECT_VEHICLES_TOOLTIP : STR_NULL;

		this->FinishInitNested(desc, v->index);
		this->owner = v->owner;

		this->order = order;
		this->SetWidgetDisabledState(VRW_REFITBUTTON, this->sel == -1);
	}

	virtual void OnInit()
	{
		if (this->cargo != NULL) {
			/* Store the RefitOption currently in use. */
			RefitOption current_refit_option = *(this->cargo);

			/* Rebuild the refit list */
			this->BuildRefitList();
			this->sel = -1;
			this->cargo = NULL;
			int current = 0;
			for (uint i = 0; this->cargo == NULL && i < NUM_CARGO; i++) {
				for (uint j = 0; j < list[i].Length(); j++) {
					if (list[i][j] == current_refit_option) {
						this->sel = current;
						this->cargo = &list[i][j];
						this->vscroll->ScrollTowards(current);
						break;
					}
					current++;
				}
			}

			this->SetWidgetDisabledState(VRW_REFITBUTTON, this->sel == -1);
			/* If the selected refit option was not found, scroll the window to the initial position. */
			if (this->sel == -1) this->vscroll->ScrollTowards(0);
		} else {
			/* Rebuild the refit list */
			this->OnInvalidateData(0);
		}
	}

	virtual void OnPaint()
	{
		/* Determine amount of items for scroller. */
		if (this->hscroll != NULL) this->hscroll->SetCount(this->vehicle_width);

		/* Calculate sprite position. */
		NWidgetCore *vehicle_panel_display = this->GetWidget<NWidgetCore>(VRW_VEHICLE_PANEL_DISPLAY);
		int sprite_width = max(0, ((int)vehicle_panel_display->current_x - this->vehicle_width) / 2);
		this->sprite_left = vehicle_panel_display->pos_x;
		this->sprite_right = vehicle_panel_display->pos_x + vehicle_panel_display->current_x - 1;
		if (_current_text_dir == TD_RTL) {
			this->sprite_right -= sprite_width;
			this->vehicle_margin = vehicle_panel_display->current_x - sprite_right;
		} else {
			this->sprite_left += sprite_width;
			this->vehicle_margin = sprite_left;
		}

		this->DrawWidgets();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case VRW_MATRIX:
				resize->height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
				size->height = resize->height * 8;
				break;

			case VRW_VEHICLE_PANEL_DISPLAY:
				size->height = GetVehicleHeight(Vehicle::Get(this->window_number)->type);
				break;

			case VRW_INFOPANEL:
				size->width = WD_FRAMERECT_LEFT + this->information_width + WD_FRAMERECT_RIGHT;
				break;
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == VRW_CAPTION) SetDParam(0, Vehicle::Get(this->window_number)->index);
	}

	/**
	 * Gets the #StringID to use for displaying capacity.
	 * @param Cargo and cargo subtype to check for capacity.
	 * @return INVALID_STRING_ID if there is no capacity. StringID to use in any other case.
	 * @post String parameters have been set.
	 */
	StringID GetCapacityString(RefitOption *option) const
	{
		Vehicle *v = Vehicle::Get(this->window_number);
		CommandCost cost = DoCommand(v->tile, this->selected_vehicle, option->cargo | option->subtype << 8 |
				this->num_vehicles << 17, DC_QUERY_COST, GetCmdRefitVeh(v->type));

		if (cost.Failed()) return INVALID_STRING_ID;

		SetDParam(0, option->cargo);
		SetDParam(1, _returned_refit_capacity);

		if (_returned_mail_refit_capacity > 0) {
			SetDParam(2, CT_MAIL);
			SetDParam(3, _returned_mail_refit_capacity);
			SetDParam(4, cost.GetCost());
			return STR_REFIT_NEW_CAPACITY_COST_OF_AIRCRAFT_REFIT;
		} else {
			SetDParam(2, cost.GetCost());
			return STR_REFIT_NEW_CAPACITY_COST_OF_REFIT;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case VRW_VEHICLE_PANEL_DISPLAY: {
				Vehicle *v = Vehicle::Get(this->window_number);
				DrawVehicleImage(v, this->sprite_left + WD_FRAMERECT_LEFT, this->sprite_right - WD_FRAMERECT_RIGHT,
					r.top + WD_FRAMERECT_TOP, INVALID_VEHICLE, this->hscroll != NULL ? this->hscroll->GetPosition() : 0);

				/* Highlight selected vehicles. */
				int x = 0;
				switch (v->type) {
					case VEH_TRAIN: {
						VehicleSet vehicles_to_refit;
						GetVehicleSet(vehicles_to_refit, Vehicle::Get(this->selected_vehicle), this->num_vehicles);

						int left = INT32_MIN;
						int width = 0;

						for (Train *u = Train::From(v); u != NULL; u = u->Next()) {
							/* Start checking. */
							if (vehicles_to_refit.Contains(u->index) && left == INT32_MIN) {
								left = x - this->hscroll->GetPosition() + r.left + this->vehicle_margin;
								width = 0;
							}

							/* Draw a selection. */
							if ((!vehicles_to_refit.Contains(u->index) || u->Next() == NULL) && left != INT32_MIN) {
								if (u->Next() == NULL && vehicles_to_refit.Contains(u->index)) {
									int current_width = u->GetDisplayImageWidth();
									width += current_width;
									x += current_width;
								}

								int right = Clamp(left + width, 0, r.right);
								left = max(0, left);

								if (_current_text_dir == TD_RTL) {
									right = this->GetWidget<NWidgetCore>(VRW_VEHICLE_PANEL_DISPLAY)->current_x - left;
									left = right - width;
								}

								if (left != right) {
									DrawFrameRect(left, r.top + WD_FRAMERECT_TOP, right, r.top + WD_FRAMERECT_TOP + 13, COLOUR_WHITE, FR_BORDERONLY);
								}

								left = INT32_MIN;
							}

							int current_width = u->GetDisplayImageWidth();
							width += current_width;
							x += current_width;
						}
						break;
					}

					default: break;
				}
				break;
			}

			case VRW_MATRIX:
				DrawVehicleRefitWindow(this->list, this->sel, this->vscroll->GetPosition(), this->vscroll->GetCapacity(), this->resize.step_height, r);
				break;

			case VRW_INFOPANEL:
				if (this->cargo != NULL) {
					StringID string = this->GetCapacityString(this->cargo);
					if (string != INVALID_STRING_ID) {
						DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT,
								r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM, string);
					}
				}
				break;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		switch (data) {
			case 0: { // The consist has changed; rebuild the entire list.
				/* Clear the selection. */
				Vehicle *v = Vehicle::Get(this->window_number);
				this->selected_vehicle = v->index;
				this->num_vehicles = UINT8_MAX;
				/* FALL THROUGH */
			}

			case 2: { // The vehicle selection has changed; rebuild the entire list.
				this->BuildRefitList();

				/* The vehicle width has changed too. */
				this->vehicle_width = GetVehicleWidth(Vehicle::Get(this->window_number));
				uint max_width = 0;

				/* Check the width of all cargo information strings. */
				for (uint i = 0; i < NUM_CARGO; i++) {
					for (uint j = 0; j < this->list[i].Length(); j++) {
						StringID string = this->GetCapacityString(&list[i][j]);
						if (string != INVALID_STRING_ID) {
							Dimension dim = GetStringBoundingBox(string);
							max_width = max(dim.width, max_width);
						}
					}
				}

				if (this->information_width < max_width) {
					this->information_width = max_width;
					this->ReInit();
				}
				/* FALL THROUGH */
			}

			case 1: // A new cargo has been selected.
				this->cargo = GetRefitOption();
				break;
		}
	}

	int GetClickPosition(int click_x)
	{
		const NWidgetCore *matrix_widget = this->GetWidget<NWidgetCore>(VRW_VEHICLE_PANEL_DISPLAY);
		if (_current_text_dir == TD_RTL) click_x = matrix_widget->current_x - click_x;
		click_x -= this->vehicle_margin;
		if (this->hscroll != NULL) click_x += this->hscroll->GetPosition();

		return click_x;
	}

	void SetSelectedVehicles(int drag_x)
	{
		drag_x = GetClickPosition(drag_x);

		int left_x  = min(this->click_x, drag_x);
		int right_x = max(this->click_x, drag_x);
		this->num_vehicles = 0;

		Vehicle *v = Vehicle::Get(this->window_number);
		/* Find the vehicle part that was clicked. */
		switch (v->type) {
			case VEH_TRAIN: {
				/* Don't select anything if we are not clicking in the vehicle. */
				if (left_x >= 0) {
					const Train *u = Train::From(v);
					bool start_counting = false;
					for (; u != NULL; u = u->Next()) {
						int current_width = u->GetDisplayImageWidth();
						left_x  -= current_width;
						right_x -= current_width;

						if (left_x < 0 && !start_counting) {
							this->selected_vehicle = u->index;
							start_counting = true;
						}

						if (start_counting) this->num_vehicles++;
						if (right_x < 0) break;
					}
				}

				/* If the selection is not correct, clear it. */
				if (this->num_vehicles != 0) {
					if (_ctrl_pressed) this->num_vehicles = UINT8_MAX;
					break;
				}
				/* FALL THROUGH */
			}

			default:
				/* Clear the selection. */
				this->selected_vehicle = v->index;
				this->num_vehicles = UINT8_MAX;
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case VRW_VEHICLE_PANEL_DISPLAY: { // Vehicle image.
				if (this->order != INVALID_VEH_ORDER_ID) break;
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(VRW_VEHICLE_PANEL_DISPLAY);
				this->click_x = GetClickPosition(pt.x - nwi->pos_x);
				this->SetSelectedVehicles(pt.x - nwi->pos_x);
				this->SetWidgetDirty(VRW_VEHICLE_PANEL_DISPLAY);
				if (!_ctrl_pressed) SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);
				break;
			}

			case VRW_MATRIX: { // listbox
				this->sel = this->vscroll->GetScrolledRowFromWidget(pt.y, this, VRW_MATRIX);
				if (this->sel == INT_MAX) this->sel = -1;
				this->SetWidgetDisabledState(VRW_REFITBUTTON, this->sel == -1);
				this->InvalidateData(1);

				if (click_count == 1) break;
				/* FALL THROUGH */
			}

			case VRW_REFITBUTTON: // refit button
				if (this->cargo != NULL) {
					const Vehicle *v = Vehicle::Get(this->window_number);

					if (this->order == INVALID_VEH_ORDER_ID) {
						bool delete_window = this->selected_vehicle == v->index && this->num_vehicles == UINT8_MAX;
						if (DoCommandP(v->tile, this->selected_vehicle, this->cargo->cargo | this->cargo->subtype << 8 | this->num_vehicles << 17, GetCmdRefitVeh(v)) && delete_window) delete this;
					} else {
						if (DoCommandP(v->tile, v->index, this->cargo->cargo | this->cargo->subtype << 8 | this->order << 16, CMD_ORDER_REFIT)) delete this;
					}
				}
				break;
		}
	}

	virtual void OnMouseDrag(Point pt, int widget)
	{
		switch (widget) {
			case VRW_VEHICLE_PANEL_DISPLAY: { // Vehicle image.
				if (this->order != INVALID_VEH_ORDER_ID) break;
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(VRW_VEHICLE_PANEL_DISPLAY);
				this->SetSelectedVehicles(pt.x - nwi->pos_x);
				this->SetWidgetDirty(VRW_VEHICLE_PANEL_DISPLAY);
				break;
			}
		}
	}

	virtual void OnDragDrop(Point pt, int widget)
	{
		switch (widget) {
			case VRW_VEHICLE_PANEL_DISPLAY: { // Vehicle image.
				if (this->order != INVALID_VEH_ORDER_ID) break;
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(VRW_VEHICLE_PANEL_DISPLAY);
				this->SetSelectedVehicles(pt.x - nwi->pos_x);
				this->InvalidateData(2);
				break;
			}
		}
	}

	virtual void OnResize()
	{
		this->vehicle_width = GetVehicleWidth(Vehicle::Get(this->window_number));
		this->vscroll->SetCapacityFromWidget(this, VRW_MATRIX);
		if (this->hscroll != NULL) this->hscroll->SetCapacityFromWidget(this, VRW_VEHICLE_PANEL_DISPLAY);
		this->GetWidget<NWidgetCore>(VRW_MATRIX)->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}
};

static const NWidgetPart _nested_vehicle_refit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, VRW_CAPTION), SetDataTip(STR_REFIT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	/* Vehicle display + scrollbar. */
	NWidget(NWID_VERTICAL),
		NWidget(WWT_PANEL, COLOUR_GREY, VRW_VEHICLE_PANEL_DISPLAY), SetMinimalSize(228, 14), SetResize(1, 0), SetScrollbar(VRW_HSCROLLBAR), EndContainer(),
		NWidget(NWID_SELECTION, INVALID_COLOUR, VRW_SHOW_HSCROLLBAR),
			NWidget(NWID_HSCROLLBAR, COLOUR_GREY, VRW_HSCROLLBAR),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_TEXTBTN, COLOUR_GREY, VRW_SELECTHEADER), SetDataTip(STR_REFIT_TITLE, STR_NULL), SetResize(1, 0),
	/* Matrix + scrollbar. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, VRW_MATRIX), SetMinimalSize(228, 112), SetResize(1, 14), SetFill(1, 1), SetDataTip(0x801, STR_NULL), SetScrollbar(VRW_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, VRW_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VRW_INFOPANEL), SetMinimalTextLines(2, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VRW_REFITBUTTON), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static const WindowDesc _vehicle_refit_desc(
	WDP_AUTO, 240, 174,
	WC_VEHICLE_REFIT, WC_VEHICLE_VIEW,
	WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_nested_vehicle_refit_widgets, lengthof(_nested_vehicle_refit_widgets)
);

/**
 * Show the refit window for a vehicle
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
	if (HasAtMostOneBit(cmask)) return y;

	b = InlineString(b, STR_PURCHASE_INFO_REFITTABLE_TO);

	if (cmask == lmask) {
		/* Engine can be refitted to all types in this climate */
		b = InlineString(b, STR_PURCHASE_INFO_ALL_TYPES);
	} else {
		/* Check if we are able to refit to more cargo types and unable to. If
		 * so, invert the cargo types to list those that we can't refit to. */
		if (CountBits(cmask ^ lmask) < CountBits(cmask) && CountBits(cmask ^ lmask) <= 7) {
			cmask ^= lmask;
			b = InlineString(b, STR_PURCHASE_INFO_ALL_BUT);
		}

		bool first = true;

		/* Add each cargo type to the list */
		const CargoSpec *cs;
		FOR_ALL_SORTED_CARGOSPECS(cs) {
			if (!HasBit(cmask, cs->Index())) continue;

			if (b >= lastof(string) - (2 + 2 * 4)) break; // ", " and two calls to Utf8Encode()

			if (!first) b = strecpy(b, ", ", lastof(string));
			first = false;

			b = InlineString(b, cs->name);
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

	int r = strnatcmp(last_name[0], last_name[1]); // Sort by name (natural sorting).
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
	int r = (*a)->vcache.cached_max_speed - (*b)->vcache.cached_max_speed;
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
	int r = (*a)->GetGroundVehicleCache()->cached_total_length - (*b)->GetGroundVehicleCache()->cached_total_length;
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
	VLW_WIDGET_CAPTION,
	VLW_WIDGET_SORT_ORDER,
	VLW_WIDGET_SORT_BY_PULLDOWN,
	VLW_WIDGET_LIST,
	VLW_WIDGET_SCROLLBAR,
	VLW_WIDGET_HIDE_BUTTONS,
	VLW_WIDGET_AVAILABLE_VEHICLES,
	VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
	VLW_WIDGET_STOP_ALL,
	VLW_WIDGET_START_ALL,
};

static const NWidgetPart _nested_vehicle_list[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, VLW_WIDGET_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLW_WIDGET_SORT_ORDER), SetMinimalSize(81, 12), SetFill(0, 1), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, VLW_WIDGET_SORT_BY_PULLDOWN), SetMinimalSize(167, 12), SetFill(0, 1), SetDataTip(0x0, STR_TOOLTIP_SORT_CRITERIA),
		NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(12, 12), SetFill(1, 1), SetResize(1, 0),
		EndContainer(),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, VLW_WIDGET_LIST), SetMinimalSize(248, 0), SetFill(1, 0), SetResize(1, 1), SetScrollbar(VLW_WIDGET_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, VLW_WIDGET_SCROLLBAR),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, VLW_WIDGET_HIDE_BUTTONS),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLW_WIDGET_AVAILABLE_VEHICLES), SetMinimalSize(106, 12), SetFill(0, 1),
								SetDataTip(STR_BLACK_STRING, STR_VEHICLE_LIST_AVAILABLE_ENGINES_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN), SetMinimalSize(118, 12), SetFill(0, 1),
								SetDataTip(STR_VEHICLE_LIST_MANAGE_LIST, STR_VEHICLE_LIST_MANAGE_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VLW_WIDGET_STOP_ALL), SetMinimalSize(12, 12), SetFill(0, 1),
								SetDataTip(SPR_FLAG_VEH_STOPPED, STR_VEHICLE_LIST_MASS_STOP_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VLW_WIDGET_START_ALL), SetMinimalSize(12, 12), SetFill(0, 1),
								SetDataTip(SPR_FLAG_VEH_RUNNING, STR_VEHICLE_LIST_MASS_START_LIST_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(0, 12), SetResize(1, 0), SetFill(1, 1), EndContainer(),
			EndContainer(),
			/* Widget to be shown for other companies hiding the previous 5 widgets. */
			NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static void DrawSmallOrderList(const Vehicle *v, int left, int right, int y, VehicleOrderID start = 0)
{
	const Order *order = v->GetOrder(start);
	if (order == NULL) return;

	int i = 0;
	VehicleOrderID oid = start;

	do {
		if (oid == v->cur_order_index) DrawString(left, right, y, STR_TINY_RIGHT_ARROW, TC_BLACK);

		if (order->IsType(OT_GOTO_STATION)) {
			SetDParam(0, order->GetDestination());
			DrawString(left + 6, right - 6, y, STR_TINY_BLACK_STATION);

			y += FONT_HEIGHT_SMALL;
			if (++i == 4) break;
		}

		oid++;
		order = order->next;
		if (order == NULL) {
			order = v->orders.list->GetFirstOrder();
			oid = 0;
		}
	} while (oid != start);
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
void DrawVehicleImage(const Vehicle *v, int left, int right, int y, VehicleID selection, int skip)
{
	switch (v->type) {
		case VEH_TRAIN:    DrawTrainImage(Train::From(v), left, right, y, selection, skip); break;
		case VEH_ROAD:     DrawRoadVehImage(v, left, right, y, selection, skip);  break;
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
	bool rtl = _current_text_dir == TD_RTL;

	int text_offset = GetDigitWidth() * this->unitnumber_digits + WD_FRAMERECT_RIGHT;
	int text_left  = left  + (rtl ?           0 : text_offset);
	int text_right = right - (rtl ? text_offset :           0);

	bool show_orderlist = this->vli.vtype >= VEH_SHIP;
	int orderlist_left  = left  + (rtl ? 0 : max(100 + text_offset, width / 2));
	int orderlist_right = right - (rtl ? max(100 + text_offset, width / 2) : 0);

	int image_left  = (rtl && show_orderlist) ? orderlist_right : text_left;
	int image_right = (!rtl && show_orderlist) ? orderlist_left : text_right;

	int vehicle_button_x = rtl ? right - GetSpriteSize(SPR_PROFIT_LOT).width : left;

	int y = r.top;
	uint max = min(this->vscroll->GetPosition() + this->vscroll->GetCapacity(), this->vehicles.Length());
	for (uint i = this->vscroll->GetPosition(); i < max; ++i) {
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

		if (show_orderlist) DrawSmallOrderList(v, orderlist_left, orderlist_right, y, v->cur_order_index);

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
	VehicleListWindow(const WindowDesc *desc, WindowNumber window_number) : BaseVehicleListWindow(window_number)
	{
		/* Set up sorting. Make the window-specific _sorting variable
		 * point to the correct global _sorting struct so we are freed
		 * from having conditionals during window operation */
		switch (this->vli.vtype) {
			case VEH_TRAIN:    this->sorting = &_sorting.train; break;
			case VEH_ROAD:     this->sorting = &_sorting.roadveh; break;
			case VEH_SHIP:     this->sorting = &_sorting.ship; break;
			case VEH_AIRCRAFT: this->sorting = &_sorting.aircraft; break;
			default: NOT_REACHED();
		}

		this->CreateNestedTree(desc);

		this->vscroll = this->GetScrollbar(VLW_WIDGET_SCROLLBAR);

		this->vehicles.SetListing(*this->sorting);
		this->vehicles.ForceRebuild();
		this->vehicles.NeedResort();
		this->BuildVehicleList();
		this->SortVehicleList();

		/* Set up the window widgets */
		this->GetWidget<NWidgetCore>(VLW_WIDGET_LIST)->tool_tip = STR_VEHICLE_LIST_TRAIN_LIST_TOOLTIP + this->vli.vtype;

		if (this->vli.type == VL_SHARED_ORDERS) {
			this->GetWidget<NWidgetCore>(VLW_WIDGET_CAPTION)->widget_data = STR_VEHICLE_LIST_SHARED_ORDERS_LIST_CAPTION;
		} else {
			this->GetWidget<NWidgetCore>(VLW_WIDGET_CAPTION)->widget_data = STR_VEHICLE_LIST_TRAIN_CAPTION + this->vli.vtype;
		}

		this->FinishInitNested(desc, window_number);
		this->owner = this->vli.company;

		if (this->vli.vtype == VEH_TRAIN) ResizeWindow(this, 65, 0);
	}

	~VehicleListWindow()
	{
		*this->sorting = this->vehicles.GetListing();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case VLW_WIDGET_LIST:
				resize->height = GetVehicleListHeight(this->vli.vtype, 1);

				switch (this->vli.vtype) {
					case VEH_TRAIN:
					case VEH_ROAD:
						size->height = 6 * resize->height;
						break;
					case VEH_SHIP:
					case VEH_AIRCRAFT:
						size->height = 4 * resize->height;
						break;
					default: NOT_REACHED();
				}
				break;

			case VLW_WIDGET_SORT_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + WD_SORTBUTTON_ARROW_WIDTH * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN: {
				Dimension d = this->GetActionDropdownSize(this->vli.type == VL_STANDARD, false);
				d.height += padding.height;
				d.width  += padding.width;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case VLW_WIDGET_AVAILABLE_VEHICLES:
				SetDParam(0, STR_VEHICLE_LIST_AVAILABLE_TRAINS + this->vli.vtype);
				break;

			case VLW_WIDGET_CAPTION: {
				switch (this->vli.type) {
					case VL_SHARED_ORDERS: // Shared Orders
						if (this->vehicles.Length() == 0) {
							/* We can't open this window without vehicles using this order
							* and we should close the window when deleting the order      */
							NOT_REACHED();
						}
						SetDParam(0, this->vscroll->GetCount());
						break;

					case VL_STANDARD: // Company Name
						SetDParam(0, STR_COMPANY_NAME);
						SetDParam(1, this->vli.index);
						SetDParam(3, this->vscroll->GetCount());
						break;

					case VL_STATION_LIST: // Station/Waypoint Name
						SetDParam(0, Station::IsExpected(BaseStation::Get(this->vli.index)) ? STR_STATION_NAME : STR_WAYPOINT_NAME);
						SetDParam(1, this->vli.index);
						SetDParam(3, this->vscroll->GetCount());
						break;

					case VL_DEPOT_LIST:
						SetDParam(0, STR_DEPOT_CAPTION);
						SetDParam(1, this->vli.vtype);
						SetDParam(2, this->vli.index);
						SetDParam(3, this->vscroll->GetCount());
						break;
					default: NOT_REACHED();
				}
				break;
			}
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
		this->BuildVehicleList();
		this->SortVehicleList();

		if (this->vehicles.Length() == 0 && this->IsWidgetLowered(VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN)) {
			HideDropDownMenu(this);
		}

		/* Hide the widgets that we will not use in this window
		 * Some windows contains actions only fit for the owner */
		int plane_to_show = (this->owner == _local_company) ? BP_SHOW_BUTTONS : BP_HIDE_BUTTONS;
		NWidgetStacked *nwi = this->GetWidget<NWidgetStacked>(VLW_WIDGET_HIDE_BUTTONS);
		if (plane_to_show != nwi->shown_plane) {
			nwi->SetDisplayedPlane(plane_to_show);
			nwi->SetDirty(this);
		}
		if (this->owner == _local_company) {
			this->SetWidgetDisabledState(VLW_WIDGET_AVAILABLE_VEHICLES, this->vli.type != VL_STANDARD);
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

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case VLW_WIDGET_SORT_ORDER: // Flip sorting method ascending/descending
				this->vehicles.ToggleSortOrder();
				this->SetDirty();
				break;

			case VLW_WIDGET_SORT_BY_PULLDOWN:// Select sorting criteria dropdown menu
				ShowDropDownMenu(this, this->vehicle_sorter_names, this->vehicles.SortType(), VLW_WIDGET_SORT_BY_PULLDOWN, 0,
						(this->vli.vtype == VEH_TRAIN || this->vli.vtype == VEH_ROAD) ? 0 : (1 << 10));
				return;

			case VLW_WIDGET_LIST: { // Matrix to show vehicles
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, VLW_WIDGET_LIST);
				if (id_v >= this->vehicles.Length()) return; // click out of list bound

				const Vehicle *v = this->vehicles[id_v];
				if (!VehicleClicked(v)) ShowVehicleViewWindow(v);
				break;
			}

			case VLW_WIDGET_AVAILABLE_VEHICLES:
				ShowBuildVehicleWindow(INVALID_TILE, this->vli.vtype);
				break;

			case VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN: {
				DropDownList *list = this->BuildActionDropdownList(VehicleListIdentifier(this->window_number).type == VL_STANDARD, false);
				ShowDropDownList(this, list, 0, VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN);
				break;
			}

			case VLW_WIDGET_STOP_ALL:
			case VLW_WIDGET_START_ALL:
				DoCommandP(0, (1 << 1) | (widget == VLW_WIDGET_START_ALL ? (1 << 0) : 0), this->window_number, CMD_MASS_START_STOP);
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
					case ADI_REPLACE: // Replace window
						ShowReplaceGroupVehicleWindow(DEFAULT_GROUP, this->vli.vtype);
						break;
					case ADI_SERVICE: // Send for servicing
					case ADI_DEPOT: // Send to Depots
						DoCommandP(0, DEPOT_MASS_SEND | (index == ADI_SERVICE ? DEPOT_SERVICE : (DepotCommand)0), this->window_number, GetCmdSendToDepot(this->vli.vtype));
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
			StationID station = (this->vli.type == VL_STATION_LIST) ? this->vli.index : INVALID_STATION;

			DEBUG(misc, 3, "Periodic resort %d list company %d at station %d", this->vli.vtype, this->owner, station);
			this->SetDirty();
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, VLW_WIDGET_LIST);
		this->GetWidget<NWidgetCore>(VLW_WIDGET_LIST)->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnInvalidateData(int data)
	{
		if (HasBit(data, 31) && this->vli.type == VL_SHARED_ORDERS) {
			this->vli.index = GB(data, 0, 20);
			this->window_number = this->vli.Pack();
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
	WDP_AUTO, 260, 246,
	WC_INVALID, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_vehicle_list, lengthof(_nested_vehicle_list)
);

static void ShowVehicleListWindowLocal(CompanyID company, VehicleListType vlt, VehicleType vehicle_type, uint16 unique_number)
{
	if (!Company::IsValidID(company)) {
		company = _local_company;
		/* This can happen when opening the vehicle list as a spectator.
		 * While it would be cleaner to check this somewhere else, having
		 * it here reduces code duplication */
		if (!Company::IsValidID(company)) return;
		_vehicle_list_desc.flags |= WDF_CONSTRUCTION;
	} else {
		_vehicle_list_desc.flags &= ~WDF_CONSTRUCTION;
	}

	_vehicle_list_desc.cls = GetWindowClassForVehicleType(vehicle_type);
	AllocateWindowDescFront<VehicleListWindow>(&_vehicle_list_desc, VehicleListIdentifier(vlt, vehicle_type, company, unique_number).Pack());
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
		ShowVehicleListWindowLocal(company, VL_STANDARD, vehicle_type, company);
	}
}

void ShowVehicleListWindow(const Vehicle *v)
{
	ShowVehicleListWindowLocal(v->owner, VL_SHARED_ORDERS, v->type, v->FirstShared()->index);
}

void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, StationID station)
{
	ShowVehicleListWindowLocal(company, VL_STATION_LIST, vehicle_type, station);
}

void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, TileIndex depot_tile)
{
	uint16 depot_airport_index;

	if (vehicle_type == VEH_AIRCRAFT) {
		depot_airport_index = GetStationIndex(depot_tile);
	} else {
		depot_airport_index = GetDepotIndex(depot_tile);
	}
	ShowVehicleListWindowLocal(company, VL_DEPOT_LIST, vehicle_type, depot_airport_index);
}


/* Unified vehicle GUI - Vehicle Details Window */

/** Constants of vehicle details widget indices */
enum VehicleDetailsWindowWidgets {
	VLD_WIDGET_CAPTION,
	VLD_WIDGET_RENAME_VEHICLE,
	VLD_WIDGET_TOP_DETAILS,
	VLD_WIDGET_INCREASE_SERVICING_INTERVAL,
	VLD_WIDGET_DECREASE_SERVICING_INTERVAL,
	VLD_WIDGET_SERVICING_INTERVAL,
	VLD_WIDGET_MIDDLE_DETAILS,
	VLD_WIDGET_MATRIX,
	VLD_WIDGET_SCROLLBAR,
	VLD_WIDGET_DETAILS_CARGO_CARRIED,
	VLD_WIDGET_DETAILS_TRAIN_VEHICLES,
	VLD_WIDGET_DETAILS_CAPACITY_OF_EACH,
	VLD_WIDGET_DETAILS_TOTAL_CARGO,
};

assert_compile(VLD_WIDGET_DETAILS_CARGO_CARRIED    == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_CARGO   );
assert_compile(VLD_WIDGET_DETAILS_TRAIN_VEHICLES   == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_INFO    );
assert_compile(VLD_WIDGET_DETAILS_CAPACITY_OF_EACH == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_CAPACITY);
assert_compile(VLD_WIDGET_DETAILS_TOTAL_CARGO      == VLD_WIDGET_DETAILS_CARGO_CARRIED + TDW_TAB_TOTALS  );

/** Vehicle details widgets (other than train). */
static const NWidgetPart _nested_nontrain_vehicle_details_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, VLD_WIDGET_CAPTION), SetDataTip(STR_VEHICLE_DETAILS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_RENAME_VEHICLE), SetMinimalSize(40, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_VEHICLE_NAME_BUTTON, STR_NULL /* filled in later */),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_TOP_DETAILS), SetMinimalSize(405, 42), SetResize(1, 0), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_MIDDLE_DETAILS), SetMinimalSize(405, 45), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, VLD_WIDGET_DECREASE_SERVICING_INTERVAL), SetFill(0, 1),
				SetDataTip(AWV_DECREASE, STR_VEHICLE_DETAILS_DECREASE_SERVICING_INTERVAL_TOOLTIP),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, VLD_WIDGET_INCREASE_SERVICING_INTERVAL), SetFill(0, 1),
				SetDataTip(AWV_INCREASE, STR_VEHICLE_DETAILS_INCREASE_SERVICING_INTERVAL_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_SERVICING_INTERVAL), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** Train details widgets. */
static const NWidgetPart _nested_train_vehicle_details_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, VLD_WIDGET_CAPTION), SetDataTip(STR_VEHICLE_DETAILS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_RENAME_VEHICLE), SetMinimalSize(40, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_VEHICLE_NAME_BUTTON, STR_NULL /* filled in later */),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_TOP_DETAILS), SetResize(1, 0), SetMinimalSize(405, 42), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, VLD_WIDGET_MATRIX), SetResize(1, 1), SetMinimalSize(393, 45), SetDataTip(0x701, STR_NULL), SetFill(1, 0), SetScrollbar(VLD_WIDGET_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, VLD_WIDGET_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, VLD_WIDGET_DECREASE_SERVICING_INTERVAL), SetFill(0, 1),
				SetDataTip(AWV_DECREASE, STR_VEHICLE_DETAILS_DECREASE_SERVICING_INTERVAL_TOOLTIP),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, VLD_WIDGET_INCREASE_SERVICING_INTERVAL), SetFill(0, 1),
				SetDataTip(AWV_INCREASE, STR_VEHICLE_DETAILS_DECREASE_SERVICING_INTERVAL_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY, VLD_WIDGET_SERVICING_INTERVAL), SetFill(1, 1), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_CARGO_CARRIED), SetMinimalSize(96, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_CARGO, STR_VEHICLE_DETAILS_TRAIN_CARGO_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_TRAIN_VEHICLES), SetMinimalSize(99, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_INFORMATION, STR_VEHICLE_DETAILS_TRAIN_INFORMATION_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_CAPACITY_OF_EACH), SetMinimalSize(99, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_CAPACITIES, STR_VEHICLE_DETAILS_TRAIN_CAPACITIES_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, VLD_WIDGET_DETAILS_TOTAL_CARGO), SetMinimalSize(99, 12),
				SetDataTip(STR_VEHICLE_DETAIL_TAB_TOTAL_CARGO, STR_VEHICLE_DETAILS_TRAIN_TOTAL_CARGO_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
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
	Scrollbar *vscroll;

	/** Initialize a newly created vehicle details window */
	VehicleDetailsWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		const Vehicle *v = Vehicle::Get(window_number);

		this->CreateNestedTree(desc);
		this->vscroll = (v->type == VEH_TRAIN ? this->GetScrollbar(VLD_WIDGET_SCROLLBAR) : NULL);
		this->FinishInitNested(desc, window_number);

		this->GetWidget<NWidgetCore>(VLD_WIDGET_RENAME_VEHICLE)->tool_tip = STR_VEHICLE_DETAILS_TRAIN_RENAME + v->type;

		this->owner = v->owner;
		this->tab = TDW_TAB_CARGO;
	}

	virtual void OnInvalidateData(int data)
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		if (v->type == VEH_ROAD) {
			const NWidgetBase *nwid_info = this->GetWidget<NWidgetBase>(VLD_WIDGET_MIDDLE_DETAILS);
			uint aimed_height = this->GetRoadVehDetailsHeight(v);
			/* If the number of articulated parts changes, the size of the window must change too. */
			if (aimed_height != nwid_info->current_y) {
				this->ReInit();
			}
		}
	}

	/**
	 * Gets the desired height for the road vehicle details panel.
	 * @param v Road vehicle being shown.
	 * @return Desired height in pixels.
	 */
	uint GetRoadVehDetailsHeight(const Vehicle *v)
	{
		uint desired_height;
		if (RoadVehicle::From(v)->HasArticulatedPart()) {
			/* An articulated RV has its text drawn under the sprite instead of after it, hence 15 pixels extra. */
			desired_height = WD_FRAMERECT_TOP + 15 + 3 * FONT_HEIGHT_NORMAL + 2 + WD_FRAMERECT_BOTTOM;
			/* Add space for the cargo amount for each part. */
			for (const Vehicle *u = v; u != NULL; u = u->Next()) {
				if (u->cargo_cap != 0) desired_height += FONT_HEIGHT_NORMAL + 1;
			}
		} else {
			desired_height = WD_FRAMERECT_TOP + 4 * FONT_HEIGHT_NORMAL + 3 + WD_FRAMERECT_BOTTOM;
		}
		return desired_height;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case VLD_WIDGET_TOP_DETAILS: {
				Dimension dim = { 0, 0 };
				size->height = WD_FRAMERECT_TOP + 4 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;

				for (uint i = 0; i < 4; i++) SetDParam(i, INT16_MAX);
				static const StringID info_strings[] = {
					STR_VEHICLE_INFO_MAX_SPEED,
					STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED,
					STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE,
					STR_VEHICLE_INFO_PROFIT_THIS_YEAR_LAST_YEAR,
					STR_VEHICLE_INFO_RELIABILITY_BREAKDOWNS
				};
				for (uint i = 0; i < lengthof(info_strings); i++) {
					dim = maxdim(dim, GetStringBoundingBox(info_strings[i]));
				}
				SetDParam(0, STR_VEHICLE_INFO_AGE);
				dim = maxdim(dim, GetStringBoundingBox(STR_VEHICLE_INFO_AGE_RUNNING_COST_YR));
				size->width = dim.width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				break;
			}

			case VLD_WIDGET_MIDDLE_DETAILS: {
				const Vehicle *v = Vehicle::Get(this->window_number);
				switch (v->type) {
					case VEH_ROAD:
						size->height = this->GetRoadVehDetailsHeight(v);
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
				SetDParam(0, 9999); // Roughly the maximum interval
				SetDParam(1, MAX_YEAR * DAYS_IN_YEAR); // Roughly the maximum year
				size->width = max(GetStringBoundingBox(STR_VEHICLE_DETAILS_SERVICING_INTERVAL_PERCENT).width, GetStringBoundingBox(STR_VEHICLE_DETAILS_SERVICING_INTERVAL_DAYS).width) + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
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
				StringID string;
				if (v->type == VEH_TRAIN ||
						(v->type == VEH_ROAD && _settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL)) {
					const GroundVehicleCache *gcache = v->GetGroundVehicleCache();
					SetDParam(2, v->GetDisplayMaxSpeed());
					SetDParam(1, gcache->cached_power);
					SetDParam(0, gcache->cached_weight);
					SetDParam(3, gcache->cached_max_te / 1000);
					if (v->type == VEH_TRAIN && (_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL ||
							GetRailTypeInfo(Train::From(v)->railtype)->acceleration_type == 2)) {
						string = STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED;
					} else {
						string = STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE;
					}
				} else {
					SetDParam(0, v->GetDisplayMaxSpeed());
					string = STR_VEHICLE_INFO_MAX_SPEED;
				}
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, string);
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
				DrawVehicleDetails(v, r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, r.top + WD_MATRIX_TOP, this->vscroll->GetPosition(), this->vscroll->GetCapacity(), this->tab);
				break;

			case VLD_WIDGET_MIDDLE_DETAILS: {
				/* For other vehicles, at the place of the matrix. */
				bool rtl = _current_text_dir == TD_RTL;
				uint sprite_width = max<uint>(GetSprite(v->GetImage(rtl ? DIR_E : DIR_W), ST_NORMAL)->width, 70U) + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;

				uint text_left  = r.left  + (rtl ? 0 : sprite_width);
				uint text_right = r.right - (rtl ? sprite_width : 0);

				/* Articulated road vehicles use a complete line. */
				if (v->type == VEH_ROAD && RoadVehicle::From(v)->HasArticulatedPart()) {
					DrawVehicleImage(v, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, INVALID_VEHICLE, 0);
				} else {
					uint sprite_left  = rtl ? text_right : r.left;
					uint sprite_right = rtl ? r.right : text_left;

					DrawVehicleImage(v, sprite_left + WD_FRAMERECT_LEFT, sprite_right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, INVALID_VEHICLE, 0);
				}
				DrawVehicleDetails(v, text_left + WD_FRAMERECT_LEFT, text_right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, 0, 0, this->tab);
				break;
			}

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
			this->vscroll->SetCount(GetTrainDetailsWndVScroll(v->index, this->tab));
		}

		/* Disable service-scroller when interval is set to disabled */
		this->SetWidgetsDisabledState(!IsVehicleServiceIntervalEnabled(v->type, v->owner),
			VLD_WIDGET_INCREASE_SERVICING_INTERVAL,
			VLD_WIDGET_DECREASE_SERVICING_INTERVAL,
			WIDGET_LIST_END);

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case VLD_WIDGET_RENAME_VEHICLE: { // rename
				const Vehicle *v = Vehicle::Get(this->window_number);
				SetDParam(0, v->index);
				ShowQueryString(STR_VEHICLE_NAME, STR_QUERY_RENAME_TRAIN_CAPTION + v->type,
						MAX_LENGTH_VEHICLE_NAME_CHARS, MAX_LENGTH_VEHICLE_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;
			}

			case VLD_WIDGET_INCREASE_SERVICING_INTERVAL:   // increase int
			case VLD_WIDGET_DECREASE_SERVICING_INTERVAL: { // decrease int
				int mod = _ctrl_pressed ? 5 : 10;
				const Vehicle *v = Vehicle::Get(this->window_number);

				mod = (widget == VLD_WIDGET_DECREASE_SERVICING_INTERVAL) ? -mod : mod;
				mod = GetServiceIntervalClamped(mod + v->service_interval, v->owner);
				if (mod == v->service_interval) return;

				DoCommandP(v->tile, v->index, mod, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_ERROR_CAN_T_CHANGE_SERVICING));
				break;
			}

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
			this->vscroll->SetCapacityFromWidget(this, VLD_WIDGET_MATRIX);
			nwi->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
		}
	}
};

/** Vehicle details window descriptor. */
static const WindowDesc _train_vehicle_details_desc(
	WDP_AUTO, 405, 178,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_UNCLICK_BUTTONS,
	_nested_train_vehicle_details_widgets, lengthof(_nested_train_vehicle_details_widgets)
);

/** Vehicle details window descriptor for other vehicles than a train. */
static const WindowDesc _nontrain_vehicle_details_desc(
	WDP_AUTO, 405, 113,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_UNCLICK_BUTTONS,
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
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, VVW_WIDGET_CAPTION), SetDataTip(STR_VEHICLE_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEBUGBOX, COLOUR_GREY),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_INSET, COLOUR_GREY), SetPadding(2, 2, 2, 2),
				NWidget(NWID_VIEWPORT, INVALID_COLOUR, VVW_WIDGET_VIEWPORT), SetMinimalSize(226, 84), SetResize(1, 1), SetPadding(1, 1, 1, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_CENTER_MAIN_VIEH), SetMinimalSize(18, 18), SetFill(1, 1), SetDataTip(SPR_CENTRE_VIEW_VEHICLE, 0x0 /* filled later */),
			NWidget(NWID_SELECTION, INVALID_COLOUR, VVW_WIDGET_SELECT_DEPOT_CLONE),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_GOTO_DEPOT), SetMinimalSize(18, 18), SetFill(1, 1), SetDataTip(0x0 /* filled later */, 0x0 /* filled later */),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_CLONE_VEH), SetMinimalSize(18, 18), SetFill(1, 1), SetDataTip(0x0 /* filled later */, 0x0 /* filled later */),
			EndContainer(),
			/* For trains only, 'ignore signal' button. */
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_FORCE_PROCEED), SetMinimalSize(18, 18), SetFill(1, 1),
											SetDataTip(SPR_IGNORE_SIGNALS, STR_VEHICLE_VIEW_TRAIN_IGNORE_SIGNAL_TOOLTIP),
			NWidget(NWID_SELECTION, INVALID_COLOUR, VVW_WIDGET_SELECT_REFIT_TURN),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_REFIT_VEH), SetMinimalSize(18, 18), SetFill(1, 1), SetDataTip(SPR_REFIT_VEHICLE, 0x0 /* filled later */),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_TURN_AROUND), SetMinimalSize(18, 18), SetFill(1, 1),
												SetDataTip(SPR_FORCE_VEHICLE_TURN, STR_VEHICLE_VIEW_ROAD_VEHICLE_REVERSE_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_SHOW_ORDERS), SetFill(1, 1), SetMinimalSize(18, 18), SetDataTip(SPR_SHOW_ORDERS, 0x0 /* filled later */),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, VVW_WIDGET_SHOW_DETAILS), SetFill(1, 1), SetMinimalSize(18, 18), SetDataTip(SPR_SHOW_VEHICLE_DETAILS, 0x0 /* filled later */),
			NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetMinimalSize(18, 0), SetResize(0, 1), EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, VVW_WIDGET_START_STOP_VEH), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** Vehicle view window descriptor for all vehicles but trains. */
static const WindowDesc _vehicle_view_desc(
	WDP_AUTO, 250, 116,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_vehicle_view_widgets, lengthof(_nested_vehicle_view_widgets)
);

/**
 * Vehicle view window descriptor for trains. Only minimum_height and
 *  default_height are different for train view.
 */
static const WindowDesc _train_view_desc(
	WDP_AUTO, 250, 134,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_UNCLICK_BUTTONS,
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

/**
 * This is the Callback method after the cloning attempt of a vehicle
 * @param result the result of the cloning command
 * @param tile unused
 * @param p1 vehicle ID
 * @param p2 unused
 */
void CcStartStopVehicle(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	const Vehicle *v = Vehicle::GetIfValid(p1);
	if (v == NULL || !v->IsPrimaryVehicle() || v->owner != _local_company) return;

	StringID msg = (v->vehstatus & VS_STOPPED) ? STR_VEHICLE_COMMAND_STOPPED : STR_VEHICLE_COMMAND_STARTED;
	Point pt = RemapCoords(v->x_pos, v->y_pos, v->z_pos);
	AddTextEffect(msg, pt.x, pt.y, DAY_TICKS, TE_RISING);
}

/**
 * Executes #CMD_START_STOP_VEHICLE for given vehicle.
 * @param v Vehicle to start/stop
 * @param texteffect Should a texteffect be shown?
 */
void StartStopVehicle(const Vehicle *v, bool texteffect)
{
	assert(v->IsPrimaryVehicle());
	DoCommandP(v->tile, v->index, 0, _vehicle_command_translation_table[VCT_CMD_START_STOP][v->type], texteffect ? CcStartStopVehicle : NULL);
}

/** Checks whether the vehicle may be refitted at the moment.*/
static bool IsVehicleRefitable(const Vehicle *v)
{
	if (!v->IsStoppedInDepot()) return false;

	do {
		if (IsEngineRefittable(v->engine_type)) return true;
	} while (v->IsGroundVehicle() && (v = v->Next()) != NULL);

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

	/**
	 * Display a plane in the window.
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

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		switch (widget) {
			case VVW_WIDGET_FORCE_PROCEED:
				if (v->type != VEH_TRAIN) {
					size->height = 0;
					size->width = 0;
				}
				break;

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
			this->SetWidgetLoweredState(VVW_WIDGET_FORCE_PROCEED, Train::From(v)->force_proceed == TFP_SIGNAL);
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
					if (Train::From(v)->gcache.cached_power == 0) {
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
					break;
				}

				case OT_GOTO_DEPOT: {
					SetDParam(0, v->type);
					SetDParam(1, v->current_order.GetDestination());
					SetDParam(2, v->GetDisplaySpeed());
					if (v->current_order.GetDepotActionType() & ODATFB_HALT) {
						str = STR_VEHICLE_STATUS_HEADING_FOR_DEPOT + _settings_client.gui.vehicle_speed;
					} else {
						str = STR_VEHICLE_STATUS_HEADING_FOR_DEPOT_SERVICE + _settings_client.gui.vehicle_speed;
					}
					break;
				}

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
					/* FALL THROUGH, if aircraft. Does this even happen? */

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
		DrawString(r.left + WD_FRAMERECT_LEFT + 6, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, str, TC_FROMSTRING, SA_HOR_CENTER);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		const Vehicle *v = Vehicle::Get(this->window_number);

		switch (widget) {
			case VVW_WIDGET_START_STOP_VEH: // start stop
				if (_ctrl_pressed) {
					/* Scroll to current order destination */
					TileIndex tile = v->current_order.GetLocation(v);
					if (tile != INVALID_TILE) ScrollMainWindowToTile(tile);
				} else {
					/* Start/Stop */
					StartStopVehicle(v, false);
				}
				break;
			case VVW_WIDGET_CENTER_MAIN_VIEH: {// center main view
				const Window *mainwindow = FindWindowById(WC_MAIN_WINDOW, 0);
				/* code to allow the main window to 'follow' the vehicle if the ctrl key is pressed */
				if (_ctrl_pressed && mainwindow->viewport->zoom == ZOOM_LVL_NORMAL) {
					mainwindow->viewport->follow_vehicle = v->index;
				} else {
					ScrollMainWindowTo(v->x_pos, v->y_pos, v->z_pos);
				}
				break;
			}

			case VVW_WIDGET_GOTO_DEPOT: // goto hangar
				DoCommandP(v->tile, v->index | (_ctrl_pressed ? DEPOT_SERVICE : 0U), 0, GetCmdSendToDepot(v));
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
				assert(v->IsGroundVehicle());
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
		if (v->IsGroundVehicle()) {
			PlaneSelections plane = veh_stopped ? SEL_RT_REFIT : SEL_RT_TURN_AROUND;
			NWidgetStacked *nwi = this->GetWidget<NWidgetStacked>(VVW_WIDGET_SELECT_REFIT_TURN);
			if (nwi->shown_plane + SEL_RT_BASEPLANE != plane) {
				this->SelectPlane(plane);
				this->SetWidgetDirty(VVW_WIDGET_SELECT_REFIT_TURN);
			}
		}
	}

	virtual bool IsNewGRFInspectable() const
	{
		return ::IsNewGRFInspectable(GetGrfSpecFeature(Vehicle::Get(this->window_number)->type), this->window_number);
	}

	virtual void ShowNewGRFInspectWindow() const
	{
		::ShowNewGRFInspectWindow(GetGrfSpecFeature(Vehicle::Get(this->window_number)->type), this->window_number);
	}
};


/** Shows the vehicle view window of the given vehicle. */
void ShowVehicleViewWindow(const Vehicle *v)
{
	AllocateWindowDescFront<VehicleViewWindow>((v->type == VEH_TRAIN) ? &_train_view_desc : &_vehicle_view_desc, v->index);
}

/**
 * Dispatch a "vehicle selected" event if any window waits for it.
 * @param v selected vehicle;
 * @return did any window accept vehicle selection?
 */
bool VehicleClicked(const Vehicle *v)
{
	assert(v != NULL);
	if (!(_thd.place_mode & HT_VEHICLE)) return false;

	v = v->First();
	if (!v->IsPrimaryVehicle()) return false;

	FindWindowById(_thd.window_class, _thd.window_number)->OnVehicleSelect(v);
	return true;
}

void StopGlobalFollowVehicle(const Vehicle *v)
{
	Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
	if (w != NULL && w->viewport->follow_vehicle == v->index) {
		ScrollMainWindowTo(v->x_pos, v->y_pos, v->z_pos, true); // lock the main view on the vehicle's last position
		w->viewport->follow_vehicle = INVALID_VEHICLE;
	}
}


/**
 * This is the Callback method after the construction attempt of a primary vehicle
 * @param result indicates completion (or not) of the operation
 * @param tile unused
 * @param p1 unused
 * @param p2 unused
 */
void CcBuildPrimaryVehicle(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	const Vehicle *v = Vehicle::Get(_new_vehicle_id);
	ShowVehicleViewWindow(v);
}

/**
 * Get the width of a vehicle (including all parts of the consist) in pixels.
 * @param v Vehicle to get the width for.
 * @return Width of the vehicle.
 */
int GetVehicleWidth(Vehicle *v)
{
	int vehicle_width = 0;

	switch (v->type) {
		case VEH_TRAIN:
			for (const Train *u = Train::From(v); u != NULL; u = u->Next()) {
				vehicle_width += u->GetDisplayImageWidth();
			}
			break;

		case VEH_ROAD:
			for (const RoadVehicle *u = RoadVehicle::From(v); u != NULL; u = u->Next()) {
				vehicle_width += u->GetDisplayImageWidth();
			}
			break;

		default:
			bool rtl = _current_text_dir == TD_RTL;
			SpriteID sprite = v->GetImage(rtl ? DIR_E : DIR_W);
			const Sprite *real_sprite = GetSprite(sprite, ST_NORMAL);
			vehicle_width = real_sprite->width;

			break;
	}

	return vehicle_width;
}
