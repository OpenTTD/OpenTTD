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
#include "rail.h"
#include "road.h"
#include "strings_func.h"
#include "window_func.h"
#include "autoreplace_func.h"
#include "company_func.h"
#include "engine_base.h"
#include "window_gui.h"
#include "engine_gui.h"
#include "settings_func.h"
#include "core/geometry_func.hpp"
#include "rail_gui.h"
#include "road_gui.h"
#include "widgets/dropdown_func.h"

#include "widgets/autoreplace_widget.h"

#include "safeguards.h"

void DrawEngineList(VehicleType type, int x, int r, int y, const GUIEngineList *eng_list, uint16 min, uint16 max, EngineID selected_id, bool show_count, GroupID selected_group);

static int CDECL EngineNumberSorter(const EngineID *a, const EngineID *b)
{
	int r = Engine::Get(*a)->list_position - Engine::Get(*b)->list_position;

	return r;
}

/**
 * Rebuild the left autoreplace list if an engine is removed or added
 * @param e Engine to check if it is removed or added
 * @param id_g The group the engine belongs to
 *  Note: this function only works if it is called either
 *   - when a new vehicle is build, but before it's counted in num_engines
 *   - when a vehicle is deleted and after it's subtracted from num_engines
 *   - when not changing the count (used when changing replace orders)
 */
void InvalidateAutoreplaceWindow(EngineID e, GroupID id_g)
{
	if (GetGroupNumEngines(_local_company, id_g, e) == 0 || GetGroupNumEngines(_local_company, ALL_GROUP, e) == 0) {
		/* We don't have any of this engine type.
		 * Either we just sold the last one, we build a new one or we stopped replacing it.
		 * In all cases, we need to update the left list */
		InvalidateWindowData(WC_REPLACE_VEHICLE, Engine::Get(e)->type, 1);
	}
}

/**
 * When an engine is made buildable or is removed from being buildable, add/remove it from the build/autoreplace lists
 * @param type The type of engine
 */
void AddRemoveEngineFromAutoreplaceAndBuildWindows(VehicleType type)
{
	InvalidateWindowData(WC_REPLACE_VEHICLE, type, 0); // Update the autoreplace window
	InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
}

static const StringID _start_replace_dropdown[] = {
	STR_REPLACE_VEHICLES_NOW,
	STR_REPLACE_VEHICLES_WHEN_OLD,
	INVALID_STRING_ID
};

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
	byte sort_criteria;           ///< Criteria of sorting vehicles.
	bool descending_sort_order;   ///< Order of sorting vehicles.
	bool show_hidden_engines;     ///< Whether to show the hidden engines.
	RailType sel_railtype;        ///< Type of rail tracks selected. #INVALID_RAILTYPE to show all.
	RoadTypeIdentifier sel_rtid;  ///< Type of road selected. Invalid to show all.
	Scrollbar *vscroll[2];

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
			if (!draw_left && !this->show_hidden_engines && e->IsHidden(_local_company)) continue;
			EngineID eid = e->index;
			switch (type) {
				case VEH_TRAIN:
					/* Ensure that the wagon/engine selection fits the engine. */
					if ((e->u.rail.railveh_type == RAILVEH_WAGON) == this->replace_engines) continue;

					if (draw_left && this->sel_railtype != INVALID_RAILTYPE) {
						/* Ensure that the railtype is specific to the selected one */
						if (e->u.rail.railtype != this->sel_railtype) continue;
					}
					break;

				case VEH_ROAD:
					if (draw_left && this->sel_rtid.IsValid()) {
						/* Ensure that the railtype is specific to the selected one */
						if (e->GetRoadType() != this->sel_rtid) continue;
					}
					break;

				default:
					break;
			}

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
		if (draw_left) {
			EngList_Sort(list, &EngineNumberSorter);
		} else {
			_engine_sort_direction = this->descending_sort_order;
			EngList_Sort(list, _engine_sort_functions[this->window_number][this->sort_criteria]);
		}
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
				if (this->reset_sel_engine && this->sel_engine[0] != INVALID_ENGINE) {
					/* Select the current replacement for sel_engine[0]. */
					const Company *c = Company::Get(_local_company);
					this->sel_engine[1] = EngineReplacementForCompany(c, this->sel_engine[0], this->sel_group);
				}
				/* Regenerate the list on the right. Note: This resets sel_engine[1] to INVALID_ENGINE, if it is no longer available. */
				this->GenerateReplaceVehList(false);
				this->vscroll[1]->SetCount(this->engines[1].Length());
				if (this->reset_sel_engine && this->sel_engine[1] != INVALID_ENGINE) {
					int position = 0;
					for (EngineID *it = this->engines[1].Begin(); it != this->engines[1].End(); ++it) {
						if (*it == this->sel_engine[1]) break;
						++position;
					}
					this->vscroll[1]->ScrollTowards(position);
				}
			}
		}
		/* Reset the flags about needed updates */
		this->engines[0].RebuildDone();
		this->engines[1].RebuildDone();
		this->reset_sel_engine = false;
	}

	/**
	 * Handle click on the start replace button.
	 * @param replace_when_old Replace now or only when old?
	 */
	void ReplaceClick_StartReplace(bool replace_when_old)
	{
		EngineID veh_from = this->sel_engine[0];
		EngineID veh_to = this->sel_engine[1];
		DoCommandP(0, (replace_when_old ? 1 : 0) | (this->sel_group << 16), veh_from + (veh_to << 16), CMD_SET_AUTOREPLACE);
	}

public:
	ReplaceVehicleWindow(WindowDesc *desc, VehicleType vehicletype, GroupID id_g) : Window(desc)
	{
		this->sel_railtype = INVALID_RAILTYPE;
		this->replace_engines  = true; // start with locomotives (all other vehicles will not read this bool)
		this->engines[0].ForceRebuild();
		this->engines[1].ForceRebuild();
		this->reset_sel_engine = true;
		this->details_height   = ((vehicletype == VEH_TRAIN) ? 10 : 9) * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		this->sel_engine[0] = INVALID_ENGINE;
		this->sel_engine[1] = INVALID_ENGINE;
		this->show_hidden_engines = _engine_sort_show_hidden_engines[vehicletype];

		this->CreateNestedTree();
		this->vscroll[0] = this->GetScrollbar(WID_RV_LEFT_SCROLLBAR);
		this->vscroll[1] = this->GetScrollbar(WID_RV_RIGHT_SCROLLBAR);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_RV_SHOW_HIDDEN_ENGINES);
		widget->widget_data = STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN + vehicletype;
		widget->tool_tip    = STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP + vehicletype;
		widget->SetLowered(this->show_hidden_engines);
		this->FinishInitNested(vehicletype);

		this->sort_criteria = _engine_sort_last_criteria[vehicletype];
		this->descending_sort_order = _engine_sort_last_order[vehicletype];
		this->owner = _local_company;
		this->sel_group = id_g;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_RV_SORT_ASCENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_RV_LEFT_MATRIX:
			case WID_RV_RIGHT_MATRIX:
				resize->height = GetEngineListHeight((VehicleType)this->window_number);
				size->height = (this->window_number <= VEH_ROAD ? 8 : 4) * resize->height;
				break;

			case WID_RV_LEFT_DETAILS:
			case WID_RV_RIGHT_DETAILS:
				size->height = this->details_height;
				break;

			case WID_RV_TRAIN_WAGONREMOVE_TOGGLE: {
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

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN: {
				Dimension d = GetStringBoundingBox(STR_REPLACE_ENGINES);
				d = maxdim(d, GetStringBoundingBox(STR_REPLACE_WAGONS));
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_RV_INFO_TAB: {
				Dimension d = GetStringBoundingBox(STR_REPLACE_NOT_REPLACING);
				d = maxdim(d, GetStringBoundingBox(STR_REPLACE_NOT_REPLACING_VEHICLE_SELECTED));
				d.width += WD_FRAMETEXT_LEFT +  WD_FRAMETEXT_RIGHT;
				d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
				break;
			}

			case WID_RV_RAIL_ROAD_TYPE_DROPDOWN: {
				Dimension d = {0, 0};
				switch (this->window_number) {
					case VEH_TRAIN:
						for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
							const RailtypeInfo *rti = GetRailTypeInfo(rt);
							/* Skip rail type if it has no label */
							if (rti->label == 0) continue;
							d = maxdim(d, GetStringBoundingBox(rti->strings.replace_text));
						}
						break;
					case VEH_ROAD:
						for (RoadType rt = ROADTYPE_BEGIN; rt < ROADTYPE_END; rt++) {
							RoadTypeIdentifier rtid;
							FOR_ALL_SORTED_ROADTYPES(rtid, rt) {
								const RoadtypeInfo *rti = GetRoadTypeInfo(rtid);
								d = maxdim(d, GetStringBoundingBox(rti->strings.replace_text));
							}
						}
						break;
					default: NOT_REACHED();
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_RV_START_REPLACE: {
				Dimension d = GetStringBoundingBox(STR_REPLACE_VEHICLES_START);
				for (int i = 0; _start_replace_dropdown[i] != INVALID_STRING_ID; i++) {
					d = maxdim(d, GetStringBoundingBox(_start_replace_dropdown[i]));
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
			case WID_RV_CAPTION:
				SetDParam(0, STR_REPLACE_VEHICLE_TRAIN + this->window_number);
				switch (this->sel_group) {
					case ALL_GROUP:
						SetDParam(1, STR_GROUP_ALL_TRAINS + this->window_number);
						break;

					case DEFAULT_GROUP:
						SetDParam(1, STR_GROUP_DEFAULT_TRAINS + this->window_number);
						break;

					default:
						SetDParam(1, STR_GROUP_NAME);
						SetDParam(2, sel_group);
						break;
				}
				break;

			case WID_RV_SORT_DROPDOWN:
				SetDParam(0, _engine_sort_listing[this->window_number][this->sort_criteria]);
				break;

			case WID_RV_TRAIN_WAGONREMOVE_TOGGLE: {
				const Company *c = Company::Get(_local_company);
				SetDParam(0, c->settings.renew_keep_length ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
				break;
			}

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN:
				SetDParam(0, this->replace_engines ? STR_REPLACE_ENGINES : STR_REPLACE_WAGONS);
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_RV_SORT_ASCENDING_DESCENDING:
				this->DrawSortButtonState(WID_RV_SORT_ASCENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;

			case WID_RV_INFO_TAB: {
				const Company *c = Company::Get(_local_company);
				StringID str;
				if (this->sel_engine[0] != INVALID_ENGINE) {
					if (!EngineHasReplacementForCompany(c, this->sel_engine[0], this->sel_group)) {
						str = STR_REPLACE_NOT_REPLACING;
					} else {
						bool when_old = false;
						EngineID e = EngineReplacementForCompany(c, this->sel_engine[0], this->sel_group, &when_old);
						str = when_old ? STR_REPLACE_REPLACING_WHEN_OLD : STR_ENGINE_NAME;
						SetDParam(0, e);
					}
				} else {
					str = STR_REPLACE_NOT_REPLACING_VEHICLE_SELECTED;
				}

				DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, r.top + WD_FRAMERECT_TOP, str, TC_BLACK, SA_HOR_CENTER);
				break;
			}

			case WID_RV_LEFT_MATRIX:
			case WID_RV_RIGHT_MATRIX: {
				int side = (widget == WID_RV_LEFT_MATRIX) ? 0 : 1;
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
		 * or The selected replacement engine has a replacement (to prevent loops). */
		this->SetWidgetDisabledState(WID_RV_START_REPLACE,
				this->sel_engine[0] == INVALID_ENGINE || this->sel_engine[1] == INVALID_ENGINE || EngineReplacementForCompany(c, this->sel_engine[1], this->sel_group) != INVALID_ENGINE);

		/* Disable the "Stop Replacing" button if:
		 *   The left engines list (existing vehicle) is empty
		 *   or The selected vehicle has no replacement set up */
		this->SetWidgetDisabledState(WID_RV_STOP_REPLACE, this->sel_engine[0] == INVALID_ENGINE || !EngineHasReplacementForCompany(c, this->sel_engine[0], this->sel_group));

		switch (this->window_number) {
			case VEH_TRAIN:
				/* Show the selected railtype in the pulldown menu */
				this->GetWidget<NWidgetCore>(WID_RV_RAIL_ROAD_TYPE_DROPDOWN)->widget_data = sel_railtype == INVALID_RAILTYPE ? STR_REPLACE_ALL_RAILTYPE : GetRailTypeInfo(sel_railtype)->strings.replace_text;
				break;

			case VEH_ROAD:
				/* Show the selected roadtype in the pulldown menu */
				this->GetWidget<NWidgetCore>(WID_RV_RAIL_ROAD_TYPE_DROPDOWN)->widget_data = sel_rtid.IsValid() ? GetRoadTypeInfo(sel_rtid)->strings.replace_text : STR_REPLACE_ALL_ROADTYPE;
				break;

			default:
				break;
		}

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			for (int side = 0; side < 2; side++) {
				if (this->sel_engine[side] != INVALID_ENGINE) {
					NWidgetBase *nwi = this->GetWidget<NWidgetBase>(side == 0 ? WID_RV_LEFT_DETAILS : WID_RV_RIGHT_DETAILS);
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
			case WID_RV_SORT_ASCENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_engine_sort_last_order[this->window_number] = this->descending_sort_order;
				this->engines[1].ForceRebuild();
				this->SetDirty();
				break;

			case WID_RV_SHOW_HIDDEN_ENGINES:
				this->show_hidden_engines ^= true;
				_engine_sort_show_hidden_engines[this->window_number] = this->show_hidden_engines;
				this->engines[1].ForceRebuild();
				this->SetWidgetLoweredState(widget, this->show_hidden_engines);
				this->SetDirty();
				break;

			case WID_RV_SORT_DROPDOWN:
				DisplayVehicleSortDropDown(this, static_cast<VehicleType>(this->window_number), this->sort_criteria, WID_RV_SORT_DROPDOWN);
				break;

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN: {
				DropDownList *list = new DropDownList();
				*list->Append() = new DropDownListStringItem(STR_REPLACE_ENGINES, 1, false);
				*list->Append() = new DropDownListStringItem(STR_REPLACE_WAGONS, 0, false);
				ShowDropDownList(this, list, this->replace_engines ? 1 : 0, WID_RV_TRAIN_ENGINEWAGON_DROPDOWN);
				break;
			}

			case WID_RV_RAIL_ROAD_TYPE_DROPDOWN: // Rail/roadtype selection dropdown menu
				switch (this->window_number) {
					case VEH_TRAIN:
						ShowDropDownList(this, GetRailTypeDropDownList(true, true), sel_railtype, WID_RV_RAIL_ROAD_TYPE_DROPDOWN);
						break;

					case VEH_ROAD:
						ShowDropDownList(this, GetRoadTypeDropDownList(ROADTYPES_ALL, true, true), sel_rtid.IsValid() ? sel_rtid.Pack() : -1, WID_RV_RAIL_ROAD_TYPE_DROPDOWN);
						break;
				}
				break;

			case WID_RV_TRAIN_WAGONREMOVE_TOGGLE: // toggle renew_keep_length
				DoCommandP(0, GetCompanySettingIndex("company.renew_keep_length"), Company::Get(_local_company)->settings.renew_keep_length ? 0 : 1, CMD_CHANGE_COMPANY_SETTING);
				break;

			case WID_RV_START_REPLACE: { // Start replacing
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->HandleButtonClick(WID_RV_START_REPLACE);
					ReplaceClick_StartReplace(false);
				} else {
					bool replacment_when_old = EngineHasReplacementWhenOldForCompany(Company::Get(_local_company), this->sel_engine[0], this->sel_group);
					ShowDropDownMenu(this, _start_replace_dropdown, replacment_when_old ? 1 : 0, WID_RV_START_REPLACE, !this->replace_engines ? 1 << 1 : 0, 0);
				}
				break;
			}

			case WID_RV_STOP_REPLACE: { // Stop replacing
				EngineID veh_from = this->sel_engine[0];
				DoCommandP(0, this->sel_group << 16, veh_from + (INVALID_ENGINE << 16), CMD_SET_AUTOREPLACE);
				break;
			}

			case WID_RV_LEFT_MATRIX:
			case WID_RV_RIGHT_MATRIX: {
				byte click_side;
				if (widget == WID_RV_LEFT_MATRIX) {
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
		switch (widget) {
			case WID_RV_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_engine_sort_last_criteria[this->window_number] = this->sort_criteria;
					this->engines[1].ForceRebuild();
					this->SetDirty();
				}
				break;

			case WID_RV_RAIL_ROAD_TYPE_DROPDOWN:
				switch (this->window_number) {
					case VEH_TRAIN: {
						RailType temp = (RailType)index;
						if (temp == sel_railtype) return; // we didn't select a new one. No need to change anything
						sel_railtype = temp;
						break;
					}
					case VEH_ROAD: {
						RoadTypeIdentifier temp;
						if (index != -1) temp = RoadTypeIdentifier::Unpack(index);
						if (temp == sel_rtid) return; // we didn't select a new one. No need to change anything
						sel_rtid = temp;
						break;
					}
					default: NOT_REACHED();
				}

				/* Reset scrollbar positions */
				this->vscroll[0]->SetPosition(0);
				this->vscroll[1]->SetPosition(0);
				/* Rebuild the lists */
				this->engines[0].ForceRebuild();
				this->engines[1].ForceRebuild();
				this->reset_sel_engine = true;
				this->SetDirty();
				break;

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN: {
				this->replace_engines = index != 0;
				this->engines[0].ForceRebuild();
				this->reset_sel_engine = true;
				this->SetDirty();
				break;
			}

			case WID_RV_START_REPLACE:
				this->ReplaceClick_StartReplace(index != 0);
				break;
		}
	}

	virtual void OnResize()
	{
		this->vscroll[0]->SetCapacityFromWidget(this, WID_RV_LEFT_MATRIX);
		this->vscroll[1]->SetCapacityFromWidget(this, WID_RV_RIGHT_MATRIX);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (data != 0) {
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->engines[0].ForceRebuild();
		} else {
			this->engines[1].ForceRebuild();
		}
	}
};

static const NWidgetPart _nested_replace_rail_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_RV_CAPTION), SetDataTip(STR_REPLACE_VEHICLES_WHITE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_REPLACE_VEHICLE_VEHICLES_IN_USE, STR_REPLACE_VEHICLE_VEHICLES_IN_USE_TOOLTIP), SetFill(1, 1), SetMinimalSize(0, 12), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES, STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES_TOOLTIP), SetFill(1, 1), SetMinimalSize(0, 12), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_RAIL_ROAD_TYPE_DROPDOWN), SetMinimalSize(136, 12), SetDataTip(0x0, STR_REPLACE_HELP_RAILTYPE), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_TRAIN_ENGINEWAGON_DROPDOWN), SetDataTip(STR_BLACK_STRING, STR_REPLACE_ENGINE_WAGON_SELECT_HELP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_SORT_ASCENDING_DESCENDING), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER), SetFill(1, 1),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_RV_SHOW_HIDDEN_ENGINES), SetDataTip(STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN, STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_HELP_LEFT_ARRAY), SetResize(1, 1), SetScrollbar(WID_RV_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_HELP_RIGHT_ARRAY), SetResize(1, 1), SetScrollbar(WID_RV_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_LEFT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_RIGHT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_TRAIN_WAGONREMOVE_TOGGLE), SetMinimalSize(138, 12), SetDataTip(STR_REPLACE_REMOVE_WAGON, STR_REPLACE_REMOVE_WAGON_HELP), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_PUSHBUTTON_DROPDOWN, COLOUR_GREY, WID_RV_START_REPLACE), SetMinimalSize(139, 12), SetDataTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_INFO_TAB), SetMinimalSize(167, 12), SetDataTip(0x0, STR_REPLACE_HELP_REPLACE_INFO_TAB), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_STOP_REPLACE), SetMinimalSize(150, 12), SetDataTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_HELP_STOP_BUTTON),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _replace_rail_vehicle_desc(
	WDP_AUTO, "replace_vehicle_train", 500, 140,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_replace_rail_vehicle_widgets, lengthof(_nested_replace_rail_vehicle_widgets)
);

static const NWidgetPart _nested_replace_road_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_RV_CAPTION), SetDataTip(STR_REPLACE_VEHICLES_WHITE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_REPLACE_VEHICLE_VEHICLES_IN_USE, STR_REPLACE_VEHICLE_VEHICLES_IN_USE_TOOLTIP), SetFill(1, 1), SetMinimalSize(0, 12), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES, STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES_TOOLTIP), SetFill(1, 1), SetMinimalSize(0, 12), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_RAIL_ROAD_TYPE_DROPDOWN), SetMinimalSize(136, 12), SetDataTip(0x0, STR_REPLACE_HELP_RAILTYPE), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_SORT_ASCENDING_DESCENDING), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER), SetFill(1, 1),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_RV_SHOW_HIDDEN_ENGINES), SetDataTip(STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN, STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_HELP_LEFT_ARRAY), SetResize(1, 1), SetScrollbar(WID_RV_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_HELP_RIGHT_ARRAY), SetResize(1, 1), SetScrollbar(WID_RV_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_LEFT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_RIGHT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_PUSHBUTTON_DROPDOWN, COLOUR_GREY, WID_RV_START_REPLACE), SetMinimalSize(139, 12), SetDataTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_INFO_TAB), SetMinimalSize(167, 12), SetDataTip(0x0, STR_REPLACE_HELP_REPLACE_INFO_TAB), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_STOP_REPLACE), SetMinimalSize(150, 12), SetDataTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_HELP_STOP_BUTTON),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _replace_road_vehicle_desc(
	WDP_AUTO, "replace_vehicle_road", 500, 140,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_replace_road_vehicle_widgets, lengthof(_nested_replace_road_vehicle_widgets)
);

static const NWidgetPart _nested_replace_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_RV_CAPTION), SetMinimalSize(433, 14), SetDataTip(STR_REPLACE_VEHICLES_WHITE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_REPLACE_VEHICLE_VEHICLES_IN_USE, STR_REPLACE_VEHICLE_VEHICLES_IN_USE_TOOLTIP), SetFill(1, 1), SetMinimalSize(0, 12), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES, STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES_TOOLTIP), SetFill(1, 1), SetMinimalSize(0, 12), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_SORT_ASCENDING_DESCENDING), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_RV_SHOW_HIDDEN_ENGINES), SetDataTip(STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN, STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_HELP_LEFT_ARRAY), SetResize(1, 1), SetScrollbar(WID_RV_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_HELP_RIGHT_ARRAY), SetResize(1, 1), SetScrollbar(WID_RV_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_LEFT_DETAILS), SetMinimalSize(228, 92), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_RIGHT_DETAILS), SetMinimalSize(228, 92), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_PUSHBUTTON_DROPDOWN, COLOUR_GREY, WID_RV_START_REPLACE), SetMinimalSize(139, 12), SetDataTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_INFO_TAB), SetMinimalSize(167, 12), SetDataTip(0x0, STR_REPLACE_HELP_REPLACE_INFO_TAB), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_STOP_REPLACE), SetMinimalSize(138, 12), SetDataTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_HELP_STOP_BUTTON),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _replace_vehicle_desc(
	WDP_AUTO, "replace_vehicle", 456, 118,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_replace_vehicle_widgets, lengthof(_nested_replace_vehicle_widgets)
);

/**
 * Show the autoreplace configuration window for a particular group.
 * @param id_g The group to replace the vehicles for.
 * @param vehicletype The type of vehicles in the group.
 */
void ShowReplaceGroupVehicleWindow(GroupID id_g, VehicleType vehicletype)
{
	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype);
	WindowDesc *desc;
	switch (vehicletype) {
		case VEH_TRAIN: desc = &_replace_rail_vehicle_desc; break;
		case VEH_ROAD:  desc = &_replace_road_vehicle_desc; break;
		default:        desc = &_replace_vehicle_desc;      break;
	}
	new ReplaceVehicleWindow(desc, vehicletype, id_g);
}
