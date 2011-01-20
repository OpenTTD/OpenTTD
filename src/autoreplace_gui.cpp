/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_gui.cpp GUI for autoreplace handling. */

#include "stdafx.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "group.h"
#include "rail.h"
#include "strings_func.h"
#include "window_func.h"
#include "autoreplace_func.h"
#include "company_func.h"
#include "widgets/dropdown_type.h"
#include "engine_base.h"
#include "window_gui.h"
#include "engine_gui.h"
#include "settings_func.h"
#include "core/geometry_func.hpp"
#include "rail_gui.h"

#include "table/strings.h"

uint GetEngineListHeight(VehicleType type);
void DrawEngineList(VehicleType type, int x, int r, int y, const GUIEngineList *eng_list, uint16 min, uint16 max, EngineID selected_id, bool show_count, GroupID selected_group);

/** Widget numbers of the autoreplace GUI. */
enum ReplaceVehicleWindowWidgets {
	RVW_WIDGET_CAPTION,

	/* Left and right matrix + details. */
	RVW_WIDGET_LEFT_MATRIX,
	RVW_WIDGET_LEFT_SCROLLBAR,
	RVW_WIDGET_RIGHT_MATRIX,
	RVW_WIDGET_RIGHT_SCROLLBAR,
	RVW_WIDGET_LEFT_DETAILS,
	RVW_WIDGET_RIGHT_DETAILS,

	/* Button row. */
	RVW_WIDGET_START_REPLACE,
	RVW_WIDGET_INFO_TAB,
	RVW_WIDGET_STOP_REPLACE,

	/* Train only widgets. */
	RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE,
	RVW_WIDGET_TRAIN_FLUFF_LEFT,
	RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN,
	RVW_WIDGET_TRAIN_FLUFF_RIGHT,
	RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE,
};

static int CDECL EngineNumberSorter(const EngineID *a, const EngineID *b)
{
	int r = ListPositionOfEngine(*a) - ListPositionOfEngine(*b);

	return r;
}

/**
 * Rebuild the left autoreplace list if an engine is removed or added
 * @param e Engine to check if it is removed or added
 * @param id_g The group the engine belongs to
 *  Note: this function only works if it is called either
 *   - when a new vehicle is build, but before it's counted in num_engines
 *   - when a vehicle is deleted and after it's substracted from num_engines
 *   - when not changing the count (used when changing replace orders)
 */
void InvalidateAutoreplaceWindow(EngineID e, GroupID id_g)
{
	Company *c = Company::Get(_local_company);
	uint num_engines = GetGroupNumEngines(_local_company, id_g, e);

	if (num_engines == 0 || c->num_engines[e] == 0) {
		/* We don't have any of this engine type.
		 * Either we just sold the last one, we build a new one or we stopped replacing it.
		 * In all cases, we need to update the left list */
		InvalidateWindowData(WC_REPLACE_VEHICLE, Engine::Get(e)->type, true);
	}
}

/**
 * When an engine is made buildable or is removed from being buildable, add/remove it from the build/autoreplace lists
 * @param type The type of engine
 */
void AddRemoveEngineFromAutoreplaceAndBuildWindows(VehicleType type)
{
	InvalidateWindowData(WC_REPLACE_VEHICLE, type, false); // Update the autoreplace window
	InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
}

/**
 * Window for the autoreplacing of vehicles.
 */
class ReplaceVehicleWindow : public Window {
	EngineID sel_engine[2];       ///< Selected engine left and right.
	GUIEngineList engines[2];     ///< Left and right list of engines.
	bool replace_engines;         ///< If \c true, engines are replaced, if \c false, wagons are replaced (only for trains).
	bool reset_sel_engine;        ///< Also reset #sel_engine while updating left and/or right (#update_left and/or #update_right) and no valid engine selected.
	GroupID sel_group;            ///< Group selected to replace.
	int details_height;           ///< Minimal needed height of the details panels (found so far).
	RailType sel_railtype;        ///< Type of rail tracks selected.
	Scrollbar *vscroll[2];

	/**
	 * Figure out if an engine should be added to a list.
	 * @param e            The EngineID.
	 * @param draw_left    If \c true, the left list is drawn (the engines specific to the railtype you selected).
	 * @param show_engines If \c true, the locomotives are drawn, else the wagons are drawn (never both).
	 * @return \c true if the engine should be in the list (based on this check), else \c false.
	 */
	bool GenerateReplaceRailList(EngineID e, bool draw_left, bool show_engines)
	{
		const RailVehicleInfo *rvi = RailVehInfo(e);

		/* Ensure that the wagon/engine selection fits the engine. */
		if ((rvi->railveh_type == RAILVEH_WAGON) == show_engines) return false;

		if (draw_left && show_engines) {
			/* Ensure that the railtype is specific to the selected one */
			if (rvi->railtype != this->sel_railtype) return false;
		}
		return true;
	}


	/**
	 * Generate an engines list
	 * @param draw_left true if generating the left list, otherwise false
	 */
	void GenerateReplaceVehList(bool draw_left)
	{
		EngineID selected_engine = INVALID_ENGINE;
		VehicleType type = (VehicleType)this->window_number;
		byte side = draw_left ? 0 : 1;

		GUIEngineList *list = &this->engines[side];
		list->Clear();

		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, type) {
			EngineID eid = e->index;
			if (type == VEH_TRAIN && !this->GenerateReplaceRailList(eid, draw_left, this->replace_engines)) continue; // special rules for trains

			if (draw_left) {
				const uint num_engines = GetGroupNumEngines(_local_company, this->sel_group, eid);

				/* Skip drawing the engines we don't have any of and haven't set for replacement */
				if (num_engines == 0 && EngineReplacementForCompany(Company::Get(_local_company), eid, this->sel_group) == INVALID_ENGINE) continue;
			} else {
				if (!CheckAutoreplaceValidity(this->sel_engine[0], eid, _local_company)) continue;
			}

			*list->Append() = eid;
			if (eid == this->sel_engine[side]) selected_engine = eid; // The selected engine is still in the list
		}
		this->sel_engine[side] = selected_engine; // update which engine we selected (the same or none, if it's not in the list anymore)
		EngList_Sort(list, &EngineNumberSorter);
	}

	/** Generate the lists */
	void GenerateLists()
	{
		EngineID e = this->sel_engine[0];

		if (this->engines[0].NeedRebuild()) {
			/* We need to rebuild the left engines list */
			this->GenerateReplaceVehList(true);
			this->vscroll[0]->SetCount(this->engines[0].Length());
			if (this->reset_sel_engine && this->sel_engine[0] == INVALID_ENGINE && this->engines[0].Length() != 0) {
				this->sel_engine[0] = this->engines[0][0];
			}
		}

		if (this->engines[1].NeedRebuild() || e != this->sel_engine[0]) {
			/* Either we got a request to rebuild the right engines list, or the left engines list selected a different engine */
			if (this->sel_engine[0] == INVALID_ENGINE) {
				/* Always empty the right engines list when nothing is selected in the left engines list */
				this->engines[1].Clear();
				this->sel_engine[1] = INVALID_ENGINE;
			} else {
				this->GenerateReplaceVehList(false);
				this->vscroll[1]->SetCount(this->engines[1].Length());
				if (this->reset_sel_engine && this->sel_engine[1] == INVALID_ENGINE && this->engines[1].Length() != 0) {
					this->sel_engine[1] = this->engines[1][0];
				}
			}
		}
		/* Reset the flags about needed updates */
		this->engines[0].RebuildDone();
		this->engines[1].RebuildDone();
		this->reset_sel_engine = false;
	}

public:
	ReplaceVehicleWindow(const WindowDesc *desc, VehicleType vehicletype, GroupID id_g) : Window()
	{
		if (vehicletype == VEH_TRAIN) {
			/* For rail vehicles find the most used vehicle type, which is usually
			 * better than 'just' the first/previous vehicle type. */
			uint type_count[RAILTYPE_END];
			memset(type_count, 0, sizeof(type_count));

			const Engine *e;
			FOR_ALL_ENGINES_OF_TYPE(e, VEH_TRAIN) {
				if (e->u.rail.railveh_type == RAILVEH_WAGON) continue;
				type_count[e->u.rail.railtype] += GetGroupNumEngines(_local_company, id_g, e->index);
			}

			this->sel_railtype = RAILTYPE_BEGIN;
			for (RailType rt = RAILTYPE_BEGIN; rt < RAILTYPE_END; rt++) {
				if (type_count[this->sel_railtype] < type_count[rt]) this->sel_railtype = rt;
			}
		}

		this->replace_engines  = true; // start with locomotives (all other vehicles will not read this bool)
		this->engines[0].ForceRebuild();
		this->engines[1].ForceRebuild();
		this->reset_sel_engine = true;
		this->details_height   = ((vehicletype == VEH_TRAIN) ? 10 : 9) * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		this->sel_engine[0] = INVALID_ENGINE;
		this->sel_engine[1] = INVALID_ENGINE;

		this->CreateNestedTree(desc);
		this->vscroll[0] = this->GetScrollbar(RVW_WIDGET_LEFT_SCROLLBAR);
		this->vscroll[1] = this->GetScrollbar(RVW_WIDGET_RIGHT_SCROLLBAR);
		this->FinishInitNested(desc, vehicletype);

		this->owner = _local_company;
		this->sel_group = id_g;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case RVW_WIDGET_LEFT_MATRIX:
			case RVW_WIDGET_RIGHT_MATRIX:
				resize->height = GetEngineListHeight((VehicleType)this->window_number);
				size->height = (this->window_number <= VEH_ROAD ? 8 : 4) * resize->height;
				break;

			case RVW_WIDGET_LEFT_DETAILS:
			case RVW_WIDGET_RIGHT_DETAILS:
				size->height = this->details_height;
				break;

			case RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE: {
				StringID str = this->GetWidget<NWidgetCore>(widget)->widget_data;
				SetDParam(0, STR_CONFIG_SETTING_ON);
				Dimension d = GetStringBoundingBox(str);
				SetDParam(0, STR_CONFIG_SETTING_OFF);
				d = maxdim(d, GetStringBoundingBox(str));
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE: {
				StringID str = this->GetWidget<NWidgetCore>(widget)->widget_data;
				SetDParam(0, STR_REPLACE_ENGINES);
				Dimension d = GetStringBoundingBox(str);
				SetDParam(0, STR_REPLACE_WAGONS);
				d = maxdim(d, GetStringBoundingBox(str));
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case RVW_WIDGET_INFO_TAB: {
				SetDParam(0, STR_REPLACE_NOT_REPLACING);
				Dimension d = GetStringBoundingBox(STR_BLACK_STRING);
				SetDParam(0, STR_REPLACE_NOT_REPLACING_VEHICLE_SELECTED);
				d = maxdim(d, GetStringBoundingBox(STR_BLACK_STRING));
				d.width += WD_FRAMETEXT_LEFT +  WD_FRAMETEXT_RIGHT;
				d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
				break;
			}

			case RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN: {
				Dimension d = {0, 0};
				for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
					const RailtypeInfo *rti = GetRailTypeInfo(rt);
					/* Skip rail type if it has no label */
					if (rti->label == 0) continue;
					d = maxdim(d, GetStringBoundingBox(rti->strings.replace_text));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case RVW_WIDGET_CAPTION:
				SetDParam(0, STR_REPLACE_VEHICLE_TRAIN + this->window_number);
				break;

			case RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE: {
				const Company *c = Company::Get(_local_company);
				SetDParam(0, c->settings.renew_keep_length ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
				break;
			}

			case RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE:
				SetDParam(0, this->replace_engines ? STR_REPLACE_ENGINES : STR_REPLACE_WAGONS);
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case RVW_WIDGET_INFO_TAB: {
				const Company *c = Company::Get(_local_company);
				if (this->sel_engine[0] != INVALID_ENGINE) {
					if (!EngineHasReplacementForCompany(c, this->sel_engine[0], this->sel_group)) {
						SetDParam(0, STR_REPLACE_NOT_REPLACING);
					} else {
						SetDParam(0, STR_ENGINE_NAME);
						SetDParam(1, EngineReplacementForCompany(c, this->sel_engine[0], this->sel_group));
					}
				} else {
					SetDParam(0, STR_REPLACE_NOT_REPLACING_VEHICLE_SELECTED);
				}

				DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, r.top + WD_FRAMERECT_TOP, STR_BLACK_STRING, TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case RVW_WIDGET_LEFT_MATRIX:
			case RVW_WIDGET_RIGHT_MATRIX: {
				int side = (widget == RVW_WIDGET_LEFT_MATRIX) ? 0 : 1;
				EngineID start  = this->vscroll[side]->GetPosition(); // what is the offset for the start (scrolling)
				EngineID end    = min(this->vscroll[side]->GetCapacity() + start, this->engines[side].Length());

				/* Do the actual drawing */
				DrawEngineList((VehicleType)this->window_number, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP,
						&this->engines[side], start, end, this->sel_engine[side], side == 0, this->sel_group);
				break;
			}
		}
	}

	virtual void OnPaint()
	{
		if (this->engines[0].NeedRebuild() || this->engines[1].NeedRebuild()) this->GenerateLists();

		Company *c = Company::Get(_local_company);

		/* Disable the "Start Replacing" button if:
		 *    Either engines list is empty
		 * or The selected replacement engine has a replacement (to prevent loops)
		 * or The right engines list (new replacement) has the existing replacement vehicle selected */
		this->SetWidgetDisabledState(RVW_WIDGET_START_REPLACE,
										this->sel_engine[0] == INVALID_ENGINE ||
										this->sel_engine[1] == INVALID_ENGINE ||
										EngineReplacementForCompany(c, this->sel_engine[1], this->sel_group) != INVALID_ENGINE ||
										EngineReplacementForCompany(c, this->sel_engine[0], this->sel_group) == this->sel_engine[1]);

		/* Disable the "Stop Replacing" button if:
		 *   The left engines list (existing vehicle) is empty
		 *   or The selected vehicle has no replacement set up */
		this->SetWidgetDisabledState(RVW_WIDGET_STOP_REPLACE,
										this->sel_engine[0] == INVALID_ENGINE ||
										!EngineHasReplacementForCompany(c, this->sel_engine[0], this->sel_group));

		/* now the actual drawing of the window itself takes place */
		SetDParam(0, STR_REPLACE_VEHICLE_TRAIN + this->window_number);

		if (this->window_number == VEH_TRAIN) {
			/* sets the colour of that art thing */
			this->GetWidget<NWidgetCore>(RVW_WIDGET_TRAIN_FLUFF_LEFT)->colour  = _company_colours[_local_company];
			this->GetWidget<NWidgetCore>(RVW_WIDGET_TRAIN_FLUFF_RIGHT)->colour = _company_colours[_local_company];

			/* Show the selected railtype in the pulldown menu */
			this->GetWidget<NWidgetCore>(RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN)->widget_data = GetRailTypeInfo(sel_railtype)->strings.replace_text;
		}

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			for (int side = 0; side < 2; side++) {
				if (this->sel_engine[side] != INVALID_ENGINE) {
					NWidgetBase *nwi = this->GetWidget<NWidgetBase>(side == 0 ? RVW_WIDGET_LEFT_DETAILS : RVW_WIDGET_RIGHT_DETAILS);
					int text_end = DrawVehiclePurchaseInfo(nwi->pos_x + WD_FRAMETEXT_LEFT, nwi->pos_x + nwi->current_x - WD_FRAMETEXT_RIGHT,
							nwi->pos_y + WD_FRAMERECT_TOP, this->sel_engine[side]);
					needed_height = max(needed_height, text_end - (int)nwi->pos_y + WD_FRAMERECT_BOTTOM);
				}
			}
			if (needed_height != this->details_height) { // Details window are not high enough, enlarge them.
				this->details_height = needed_height;
				this->ReInit();
				return;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE:
				this->replace_engines  = !(this->replace_engines);
				this->engines[0].ForceRebuild();
				this->reset_sel_engine = true;
				this->SetDirty();
				break;

			case RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN: // Railtype selection dropdown menu
				ShowDropDownList(this, GetRailTypeDropDownList(true), sel_railtype, RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN);
				break;

			case RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE: // toggle renew_keep_length
				DoCommandP(0, GetCompanySettingIndex("company.renew_keep_length"), Company::Get(_local_company)->settings.renew_keep_length ? 0 : 1, CMD_CHANGE_COMPANY_SETTING);
				break;

			case RVW_WIDGET_START_REPLACE: { // Start replacing
				EngineID veh_from = this->sel_engine[0];
				EngineID veh_to = this->sel_engine[1];
				DoCommandP(0, this->sel_group << 16, veh_from + (veh_to << 16), CMD_SET_AUTOREPLACE);
				this->SetDirty();
				break;
			}

			case RVW_WIDGET_STOP_REPLACE: { // Stop replacing
				EngineID veh_from = this->sel_engine[0];
				DoCommandP(0, this->sel_group << 16, veh_from + (INVALID_ENGINE << 16), CMD_SET_AUTOREPLACE);
				this->SetDirty();
				break;
			}

			case RVW_WIDGET_LEFT_MATRIX:
			case RVW_WIDGET_RIGHT_MATRIX: {
				byte click_side;
				if (widget == RVW_WIDGET_LEFT_MATRIX) {
					click_side = 0;
				} else {
					click_side = 1;
				}
				uint i = this->vscroll[click_side]->GetScrolledRowFromWidget(pt.y, this, widget);
				size_t engine_count = this->engines[click_side].Length();

				EngineID e = engine_count > i ? this->engines[click_side][i] : INVALID_ENGINE;
				if (e == this->sel_engine[click_side]) break; // we clicked the one we already selected
				this->sel_engine[click_side] = e;
				if (click_side == 0) {
					this->engines[1].ForceRebuild();
					this->reset_sel_engine = true;
				}
				this->SetDirty();
				break;
			}
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		RailType temp = (RailType)index;
		if (temp == sel_railtype) return; // we didn't select a new one. No need to change anything
		sel_railtype = temp;
		/* Reset scrollbar positions */
		this->vscroll[0]->SetPosition(0);
		this->vscroll[1]->SetPosition(0);
		/* Rebuild the lists */
		this->engines[0].ForceRebuild();
		this->engines[1].ForceRebuild();
		this->reset_sel_engine = true;
		this->SetDirty();
	}

	virtual void OnResize()
	{
		this->vscroll[0]->SetCapacityFromWidget(this, RVW_WIDGET_LEFT_MATRIX);
		this->vscroll[1]->SetCapacityFromWidget(this, RVW_WIDGET_RIGHT_MATRIX);

		this->GetWidget<NWidgetCore>(RVW_WIDGET_LEFT_MATRIX)->widget_data =
				this->GetWidget<NWidgetCore>(RVW_WIDGET_RIGHT_MATRIX)->widget_data = (this->vscroll[0]->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnInvalidateData(int data)
	{
		if (data != 0) {
			this->engines[0].ForceRebuild();
		} else {
			this->engines[1].ForceRebuild();
		}
	}
};

static const NWidgetPart _nested_replace_rail_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, RVW_WIDGET_CAPTION), SetDataTip(STR_REPLACE_VEHICLES_WHITE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_MATRIX, COLOUR_GREY, RVW_WIDGET_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetDataTip(0x1, STR_REPLACE_HELP_LEFT_ARRAY), SetResize(1, 1), SetScrollbar(RVW_WIDGET_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, RVW_WIDGET_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, RVW_WIDGET_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetDataTip(0x1, STR_REPLACE_HELP_RIGHT_ARRAY), SetResize(1, 1), SetScrollbar(RVW_WIDGET_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, RVW_WIDGET_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_LEFT_DETAILS), SetMinimalSize(228, 102), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_RIGHT_DETAILS), SetMinimalSize(228, 102), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, RVW_WIDGET_START_REPLACE), SetMinimalSize(139, 12), SetDataTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_INFO_TAB), SetMinimalSize(167, 12), SetDataTip(0x0, STR_REPLACE_HELP_REPLACE_INFO_TAB), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, RVW_WIDGET_STOP_REPLACE), SetMinimalSize(150, 12), SetDataTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_HELP_STOP_BUTTON),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE), SetMinimalSize(139, 12), SetDataTip(STR_REPLACE_ENGINE_WAGON_SELECT, STR_REPLACE_ENGINE_WAGON_SELECT_HELP),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_TRAIN_FLUFF_LEFT), SetMinimalSize(15, 12), EndContainer(),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN), SetMinimalSize(136, 12), SetDataTip(0x0, STR_REPLACE_HELP_RAILTYPE), SetResize(1, 0),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_TRAIN_FLUFF_RIGHT), SetMinimalSize(16, 12), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE), SetMinimalSize(138, 12), SetDataTip(STR_REPLACE_REMOVE_WAGON, STR_REPLACE_REMOVE_WAGON_HELP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static const WindowDesc _replace_rail_vehicle_desc(
	WDP_AUTO, 456, 140,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_nested_replace_rail_vehicle_widgets, lengthof(_nested_replace_rail_vehicle_widgets)
);

static const NWidgetPart _nested_replace_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, RVW_WIDGET_CAPTION), SetMinimalSize(433, 14), SetDataTip(STR_REPLACE_VEHICLES_WHITE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_MATRIX, COLOUR_GREY, RVW_WIDGET_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetDataTip(0x1, STR_REPLACE_HELP_LEFT_ARRAY), SetResize(1, 1), SetScrollbar(RVW_WIDGET_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, RVW_WIDGET_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, RVW_WIDGET_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetDataTip(0x1, STR_REPLACE_HELP_RIGHT_ARRAY), SetResize(1, 1), SetScrollbar(RVW_WIDGET_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, RVW_WIDGET_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_LEFT_DETAILS), SetMinimalSize(228, 92), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_RIGHT_DETAILS), SetMinimalSize(228, 92), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, RVW_WIDGET_START_REPLACE), SetMinimalSize(139, 12), SetDataTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON),
		NWidget(WWT_PANEL, COLOUR_GREY, RVW_WIDGET_INFO_TAB), SetMinimalSize(167, 12), SetDataTip(0x0, STR_REPLACE_HELP_REPLACE_INFO_TAB), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, RVW_WIDGET_STOP_REPLACE), SetMinimalSize(138, 12), SetDataTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_HELP_STOP_BUTTON),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static const WindowDesc _replace_vehicle_desc(
	WDP_AUTO, 456, 118,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_nested_replace_vehicle_widgets, lengthof(_nested_replace_vehicle_widgets)
);

void ShowReplaceGroupVehicleWindow(GroupID id_g, VehicleType vehicletype)
{
	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype);
	new ReplaceVehicleWindow(vehicletype == VEH_TRAIN ? &_replace_rail_vehicle_desc : &_replace_vehicle_desc, vehicletype, id_g);
}
