/* $Id$ */

/** @file autoreplace_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "gui.h"
#include "command_func.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "group.h"
#include "rail.h"
#include "strings_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "autoreplace_func.h"
#include "gfx_func.h"
#include "player_func.h"
#include "widgets/dropdown_func.h"

#include "table/sprites.h"
#include "table/strings.h"

static RailType _railtype_selected_in_replace_gui;

static bool _rebuild_left_list;
static bool _rebuild_right_list;

static const StringID _rail_types_list[] = {
	STR_RAIL_VEHICLES,
	STR_ELRAIL_VEHICLES,
	STR_MONORAIL_VEHICLES,
	STR_MAGLEV_VEHICLES,
	INVALID_STRING_ID
};

enum ReplaceVehicleWindowWidgets {
	RVW_WIDGET_LEFT_DETAILS = 3,
	RVW_WIDGET_START_REPLACE,
	RVW_WIDGET_INFO_TAB,
	RVW_WIDGET_STOP_REPLACE,
	RVW_WIDGET_LEFT_MATRIX,
	RVW_WIDGET_LEFT_SCROLLBAR,
	RVW_WIDGET_RIGHT_MATRIX,
	RVW_WIDGET_RIGHT_SCROLLBAR,
	RVW_WIDGET_RIGHT_DETAILS,

	RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE,
	RVW_WIDGET_TRAIN_FLUFF_LEFT,
	RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN,
	RVW_WIDGET_TRAIN_FLUFF_RIGHT,
	RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE,
};

static int CDECL TrainEngineNumberSorter(const void *a, const void *b)
{
	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r = ListPositionOfEngine(va) - ListPositionOfEngine(vb);

	return r;
}

/* General Vehicle GUI based procedures that are independent of vehicle types */
void InitializeVehiclesGuiList()
{
	_railtype_selected_in_replace_gui = RAILTYPE_RAIL;
}

/** Rebuild the left autoreplace list if an engine is removed or added
 * @param e Engine to check if it is removed or added
 * @param id_g The group the engine belongs to
 *  Note: this function only works if it is called either
 *   - when a new vehicle is build, but before it's counted in num_engines
 *   - when a vehicle is deleted and after it's substracted from num_engines
 *   - when not changing the count (used when changing replace orders)
 */
void InvalidateAutoreplaceWindow(EngineID e, GroupID id_g)
{
	Player *p = GetPlayer(_local_player);
	byte type = GetEngine(e)->type;
	uint num_engines = GetGroupNumEngines(_local_player, id_g, e);

	if (num_engines == 0 || p->num_engines[e] == 0) {
		/* We don't have any of this engine type.
		 * Either we just sold the last one, we build a new one or we stopped replacing it.
		 * In all cases, we need to update the left list */
		_rebuild_left_list = true;
	} else {
		_rebuild_left_list = false;
	}
	_rebuild_right_list = false;
	InvalidateWindowData(WC_REPLACE_VEHICLE, type);
}

/** When an engine is made buildable or is removed from being buildable, add/remove it from the build/autoreplace lists
 * @param type The type of engine
 */
void AddRemoveEngineFromAutoreplaceAndBuildWindows(VehicleType type)
{
	_rebuild_left_list = false; // left list is only for the vehicles the player owns and is not related to being buildable
	_rebuild_right_list = true;
	InvalidateWindowData(WC_REPLACE_VEHICLE, type); // Update the autoreplace window
	InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
}

/** Get the default cargo type for an engine
 * @param engine the EngineID to get the cargo for
 * @return the cargo type carried by the engine (CT_INVALID if engine got no cargo capacity)
 */
static CargoID EngineCargo(EngineID engine)
{
	if (engine == INVALID_ENGINE) return CT_INVALID; // surely INVALID_ENGINE can't carry anything but CT_INVALID

	switch (GetEngine(engine)->type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			if (RailVehInfo(engine)->capacity == 0) return CT_INVALID; // no capacity -> can't carry cargo
			return RailVehInfo(engine)->cargo_type;
		case VEH_ROAD:       return RoadVehInfo(engine)->cargo_type;
		case VEH_SHIP:       return ShipVehInfo(engine)->cargo_type;
		case VEH_AIRCRAFT:   return CT_PASSENGERS; // all planes are build with passengers by default
	}
}

/** Figure out if an engine should be added to a list
 * @param e The EngineID
 * @param draw_left If true, then the left list is drawn (the engines specific to the railtype you selected)
 * @param show_engines if truem then locomotives are drawn, else wagons (never both)
 * @return true if the engine should be in the list (based on this check)
 */
static bool GenerateReplaceRailList(EngineID e, bool draw_left, bool show_engines)
{
	const RailVehicleInfo *rvi = RailVehInfo(e);

	/* Ensure that the wagon/engine selection fits the engine. */
	if ((rvi->railveh_type == RAILVEH_WAGON) == show_engines) return false;

	if (draw_left && show_engines) {
		/* Ensure that the railtype is specific to the selected one */
		if (rvi->railtype != _railtype_selected_in_replace_gui) return false;
	} else {
		/* Ensure that it's a compatible railtype to the selected one (like electric <-> diesel)
		 * The vehicle do not have to have power on the railtype in question, only able to drive (pulled if needed) */
		if (!IsCompatibleRail(rvi->railtype, _railtype_selected_in_replace_gui)) return false;
	}
	return true;
}

/** Figure out if two engines got at least one type of cargo in common (refitting if needed)
 * @param engine_a one of the EngineIDs
 * @param engine_b the other EngineID
 * @return true if they can both carry the same type of cargo (or at least one of them got no capacity at all)
 */
static bool EnginesGotCargoInCommon(EngineID engine_a, EngineID engine_b)
{
	CargoID a = EngineCargo(engine_a);
	CargoID b = EngineCargo(engine_b);

	 /* we should always be able to refit to/from locomotives without capacity
	  * Because of that, CT_INVALID shoudl always return true */
	if (a == CT_INVALID || b == CT_INVALID || a == b) return true; // they carry no ro the same type by default
	if (EngInfo(engine_a)->refit_mask & EngInfo(engine_b)->refit_mask) return true; // both can refit to the same
	if (CanRefitTo(engine_a, b) || CanRefitTo(engine_b, a)) return true; // one can refit to what the other one carries
	return false;
}

/** Generate a list
 * @param w Window, that contains the list
 * @param draw_left true if generating the left list, otherwise false
 */
static void GenerateReplaceVehList(Window *w, bool draw_left)
{
	EngineID e;
	EngineID selected_engine = INVALID_ENGINE;
	byte type = w->window_number;
	byte i = draw_left ? 0 : 1;

	EngineList *list = &WP(w, replaceveh_d).list[i];
	EngList_RemoveAll(list);

	FOR_ALL_ENGINEIDS_OF_TYPE(e, type) {
		if (type == VEH_TRAIN && !GenerateReplaceRailList(e, draw_left, WP(w, replaceveh_d).wagon_btnstate)) continue; // special rules for trains

		if (draw_left) {
			const GroupID selected_group = WP(w, replaceveh_d).sel_group;
			const uint num_engines = GetGroupNumEngines(_local_player, selected_group, e);

			/* Skip drawing the engines we don't have any of and haven't set for replacement */
			if (num_engines == 0 && EngineReplacementForPlayer(GetPlayer(_local_player), e, selected_group) == INVALID_ENGINE) continue;
		} else {
			/* This is for engines we can replace to and they should depend on what we selected to replace from */
			if (!IsEngineBuildable(e, type, _local_player)) continue; // we need to be able to build the engine
			if (!EnginesGotCargoInCommon(e, WP(w, replaceveh_d).sel_engine[0])) continue; // the engines needs to be able to carry the same cargo

			/* Road vehicles can't be replaced by trams and vice-versa */
			if (type == VEH_ROAD && HasBit(EngInfo(WP(w, replaceveh_d).sel_engine[0])->misc_flags, EF_ROAD_TRAM) != HasBit(EngInfo(e)->misc_flags, EF_ROAD_TRAM)) continue;
			if (e == WP(w, replaceveh_d).sel_engine[0]) continue; // we can't replace an engine into itself (that would be autorenew)
		}

		EngList_Add(list, e);
		if (e == WP(w, replaceveh_d).sel_engine[i]) selected_engine = e; // The selected engine is still in the list
	}
	WP(w, replaceveh_d).sel_engine[i] = selected_engine; // update which engine we selected (the same or none, if it's not in the list anymore)
	if (type == VEH_TRAIN) EngList_Sort(list, &TrainEngineNumberSorter);
}

/** Generate the lists
 * @param w Window containing the lists
 */
static void GenerateLists(Window *w)
{
	EngineID e = WP(w, replaceveh_d).sel_engine[0];

	if (WP(w, replaceveh_d).update_left == true) {
		/* We need to rebuild the left list */
		GenerateReplaceVehList(w, true);
		SetVScrollCount(w, EngList_Count(&WP(w, replaceveh_d).list[0]));
		if (WP(w, replaceveh_d).init_lists && WP(w, replaceveh_d).sel_engine[0] == INVALID_ENGINE && EngList_Count(&WP(w, replaceveh_d).list[0]) != 0) {
			WP(w, replaceveh_d).sel_engine[0] = WP(w, replaceveh_d).list[0][0];
		}
	}

	if (WP(w, replaceveh_d).update_right || e != WP(w, replaceveh_d).sel_engine[0]) {
		/* Either we got a request to rebuild the right list or the left list selected a different engine */
		if (WP(w, replaceveh_d).sel_engine[0] == INVALID_ENGINE) {
			/* Always empty the right list when nothing is selected in the left list */
			EngList_RemoveAll(&WP(w, replaceveh_d).list[1]);
			WP(w, replaceveh_d).sel_engine[1] = INVALID_ENGINE;
		} else {
			GenerateReplaceVehList(w, false);
			SetVScroll2Count(w, EngList_Count(&WP(w, replaceveh_d).list[1]));
			if (WP(w, replaceveh_d).init_lists && WP(w, replaceveh_d).sel_engine[1] == INVALID_ENGINE && EngList_Count(&WP(w, replaceveh_d).list[1]) != 0) {
				WP(w, replaceveh_d).sel_engine[1] = WP(w, replaceveh_d).list[1][0];
			}
		}
	}
	/* Reset the flags about needed updates */
	WP(w, replaceveh_d).update_left  = false;
	WP(w, replaceveh_d).update_right = false;
	WP(w, replaceveh_d).init_lists   = false;
}


void DrawEngineList(VehicleType type, int x, int y, const EngineList eng_list, uint16 min, uint16 max, EngineID selected_id, bool show_count, GroupID selected_group);

static void ReplaceVehicleWndProc(Window *w, WindowEvent *e)
{
	/* Strings for the pulldown menu */
	static const StringID _vehicle_type_names[] = {
		STR_019F_TRAIN,
		STR_019C_ROAD_VEHICLE,
		STR_019E_SHIP,
		STR_019D_AIRCRAFT
	};

	switch (e->event) {
		case WE_CREATE:
			WP(w, replaceveh_d).wagon_btnstate = true; // start with locomotives (all other vehicles will not read this bool)
			EngList_Create(&WP(w, replaceveh_d).list[0]);
			EngList_Create(&WP(w, replaceveh_d).list[1]);
			WP(w, replaceveh_d).update_left   = true;
			WP(w, replaceveh_d).update_right  = true;
			WP(w, replaceveh_d).init_lists    = true;
			WP(w, replaceveh_d).sel_engine[0] = INVALID_ENGINE;
			WP(w, replaceveh_d).sel_engine[1] = INVALID_ENGINE;
			break;

		case WE_PAINT: {
			if (WP(w, replaceveh_d).update_left || WP(w, replaceveh_d).update_right) GenerateLists(w);

			Player *p = GetPlayer(_local_player);
			EngineID selected_id[2];
			const GroupID selected_group = WP(w, replaceveh_d).sel_group;

			selected_id[0] = WP(w, replaceveh_d).sel_engine[0];
			selected_id[1] = WP(w, replaceveh_d).sel_engine[1];

			/* Disable the "Start Replacing" button if:
			 *    Either list is empty
			 * or The selected replacement engine has a replacement (to prevent loops)
			 * or The right list (new replacement) has the existing replacement vehicle selected */
			w->SetWidgetDisabledState(RVW_WIDGET_START_REPLACE,
										 selected_id[0] == INVALID_ENGINE ||
										 selected_id[1] == INVALID_ENGINE ||
										 EngineReplacementForPlayer(p, selected_id[1], selected_group) != INVALID_ENGINE ||
										 EngineReplacementForPlayer(p, selected_id[0], selected_group) == selected_id[1]);

			/* Disable the "Stop Replacing" button if:
			 *   The left list (existing vehicle) is empty
			 *   or The selected vehicle has no replacement set up */
			w->SetWidgetDisabledState(RVW_WIDGET_STOP_REPLACE,
										 selected_id[0] == INVALID_ENGINE ||
										 !EngineHasReplacementForPlayer(p, selected_id[0], selected_group));

			/* now the actual drawing of the window itself takes place */
			SetDParam(0, _vehicle_type_names[w->window_number]);

			if (w->window_number == VEH_TRAIN) {
				/* set on/off for renew_keep_length */
				SetDParam(1, p->renew_keep_length ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);

				/* set wagon/engine button */
				SetDParam(2, WP(w, replaceveh_d).wagon_btnstate ? STR_ENGINES : STR_WAGONS);

				/* sets the colour of that art thing */
				w->widget[RVW_WIDGET_TRAIN_FLUFF_LEFT].color  = _player_colors[_local_player];
				w->widget[RVW_WIDGET_TRAIN_FLUFF_RIGHT].color = _player_colors[_local_player];
			}

			if (w->window_number == VEH_TRAIN) {
				/* Show the selected railtype in the pulldown menu */
				RailType railtype = _railtype_selected_in_replace_gui;
				w->widget[RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN].data = _rail_types_list[railtype];
			}

			DrawWindowWidgets(w);

			/* sets up the string for the vehicle that is being replaced to */
			if (selected_id[0] != INVALID_ENGINE) {
				if (!EngineHasReplacementForPlayer(p, selected_id[0], selected_group)) {
					SetDParam(0, STR_NOT_REPLACING);
				} else {
					SetDParam(0, STR_ENGINE_NAME);
					SetDParam(1, EngineReplacementForPlayer(p, selected_id[0], selected_group));
				}
			} else {
				SetDParam(0, STR_NOT_REPLACING_VEHICLE_SELECTED);
			}

			DrawString(145, w->widget[RVW_WIDGET_INFO_TAB].top + 1, STR_02BD, TC_BLACK);

			/* Draw the lists */
			for(byte i = 0; i < 2; i++) {
				uint widget     = (i == 0) ? RVW_WIDGET_LEFT_MATRIX : RVW_WIDGET_RIGHT_MATRIX;
				EngineList list = WP(w, replaceveh_d).list[i]; // which list to draw
				EngineID start  = i == 0 ? w->vscroll.pos : w->vscroll2.pos; // what is the offset for the start (scrolling)
				EngineID end    = min((i == 0 ? w->vscroll.cap : w->vscroll2.cap) + start, EngList_Count(&list));

				/* Do the actual drawing */
				DrawEngineList((VehicleType)w->window_number, w->widget[widget].left + 2, w->widget[widget].top + 1, list, start, end, WP(w, replaceveh_d).sel_engine[i], i == 0, selected_group);

				/* Also draw the details if an engine is selected */
				if (WP(w, replaceveh_d).sel_engine[i] != INVALID_ENGINE) {
					const Widget *wi = &w->widget[i == 0 ? RVW_WIDGET_LEFT_DETAILS : RVW_WIDGET_RIGHT_DETAILS];
					int text_end = DrawVehiclePurchaseInfo(wi->left + 2, wi->top + 1, wi->right - wi->left - 2, WP(w, replaceveh_d).sel_engine[i]);

					if (text_end > wi->bottom) {
						SetWindowDirty(w);
						ResizeWindowForWidget(w, i == 0 ? RVW_WIDGET_LEFT_DETAILS : RVW_WIDGET_RIGHT_DETAILS, 0, text_end - wi->bottom);
						SetWindowDirty(w);
					}
				}
			}

		} break;   // end of paint

		case WE_CLICK: {
			switch (e->we.click.widget) {
				case RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE:
					WP(w, replaceveh_d).wagon_btnstate = !(WP(w, replaceveh_d).wagon_btnstate);
					WP(w, replaceveh_d).update_left = true;
					WP(w, replaceveh_d).init_lists  = true;
					SetWindowDirty(w);
					break;

				case RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN: /* Railtype selection dropdown menu */
					ShowDropDownMenu(w, _rail_types_list, _railtype_selected_in_replace_gui, RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN, 0, ~GetPlayer(_local_player)->avail_railtypes);
					break;

				case RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE: /* toggle renew_keep_length */
					DoCommandP(0, 5, GetPlayer(_local_player)->renew_keep_length ? 0 : 1, NULL, CMD_SET_AUTOREPLACE);
					break;

				case RVW_WIDGET_START_REPLACE: { /* Start replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					EngineID veh_to = WP(w, replaceveh_d).sel_engine[1];
					DoCommandP(0, 3 + (WP(w, replaceveh_d).sel_group << 16) , veh_from + (veh_to << 16), NULL, CMD_SET_AUTOREPLACE);
				} break;

				case RVW_WIDGET_STOP_REPLACE: { /* Stop replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					DoCommandP(0, 3 + (WP(w, replaceveh_d).sel_group << 16), veh_from + (INVALID_ENGINE << 16), NULL, CMD_SET_AUTOREPLACE);
				} break;

				case RVW_WIDGET_LEFT_MATRIX:
				case RVW_WIDGET_RIGHT_MATRIX: {
					uint i = (e->we.click.pt.y - 14) / w->resize.step_height;
					uint16 click_scroll_pos = e->we.click.widget == RVW_WIDGET_LEFT_MATRIX ? w->vscroll.pos : w->vscroll2.pos;
					uint16 click_scroll_cap = e->we.click.widget == RVW_WIDGET_LEFT_MATRIX ? w->vscroll.cap : w->vscroll2.cap;
					byte click_side         = e->we.click.widget == RVW_WIDGET_LEFT_MATRIX ? 0 : 1;
					uint16 engine_count     = EngList_Count(&WP(w, replaceveh_d).list[click_side]);

					if (i < click_scroll_cap) {
						i += click_scroll_pos;
						EngineID e = engine_count > i ? WP(w, replaceveh_d).list[click_side][i] : INVALID_ENGINE;
						if (e == WP(w, replaceveh_d).sel_engine[click_side]) break; // we clicked the one we already selected
						WP(w, replaceveh_d).sel_engine[click_side] = e;
						if (click_side == 0) {
							WP(w, replaceveh_d).update_right = true;
							WP(w, replaceveh_d).init_lists   = true;
						}
						SetWindowDirty(w);
						}
					break;
					}
			}
			break;
		}

		case WE_DROPDOWN_SELECT: { /* we have selected a dropdown item in the list */
			RailType temp = (RailType)e->we.dropdown.index;
			if (temp == _railtype_selected_in_replace_gui) break; // we didn't select a new one. No need to change anything
			_railtype_selected_in_replace_gui = temp;
			/* Reset scrollbar positions */
			w->vscroll.pos  = 0;
			w->vscroll2.pos = 0;
			/* Rebuild the lists */
			WP(w, replaceveh_d).update_left  = true;
			WP(w, replaceveh_d).update_right = true;
			WP(w, replaceveh_d).init_lists   = true;
			SetWindowDirty(w);
		} break;

		case WE_RESIZE:
			w->vscroll.cap  += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->vscroll2.cap += e->we.sizing.diff.y / (int)w->resize.step_height;

			w->widget[RVW_WIDGET_LEFT_MATRIX].data  = (w->vscroll.cap  << 8) + 1;
			w->widget[RVW_WIDGET_RIGHT_MATRIX].data = (w->vscroll2.cap << 8) + 1;
			break;

		case WE_INVALIDATE_DATA:
			if (_rebuild_left_list) WP(w, replaceveh_d).update_left = true;
			if (_rebuild_right_list) WP(w, replaceveh_d).update_right = true;
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			EngList_RemoveAll(&WP(w, replaceveh_d).list[0]);
			EngList_RemoveAll(&WP(w, replaceveh_d).list[1]);
		break;
	}
}

static const Widget _replace_rail_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,       STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   227, 0x0,            STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   240,   251, STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   316,   228,   239, 0x0,            STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   240,   251, STR_REPLACE_VEHICLES_STOP,  STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,          STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,          STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   227, 0x0,            STR_NULL},
// train specific stuff
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   228,   239, STR_REPLACE_ENGINE_WAGON_SELECT,       STR_REPLACE_ENGINE_WAGON_SELECT_HELP},  // widget 12
{      WWT_PANEL,     RESIZE_TB,    14,   139,   153,   240,   251, 0x0,            STR_NULL},
{   WWT_DROPDOWN,     RESIZE_TB,    14,   154,   289,   240,   251, 0x0,            STR_REPLACE_HELP_RAILTYPE},
{      WWT_PANEL,     RESIZE_TB,    14,   290,   305,   240,   251, 0x0,            STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   317,   455,   228,   239, STR_REPLACE_REMOVE_WAGON,       STR_REPLACE_REMOVE_WAGON_HELP},
// end of train specific stuff
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   240,   251, STR_NULL,       STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_road_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,                    STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   217, 0x0,                         STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   218,   229, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   218,   229, 0x0,                         STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   218,   229, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,                       STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,                       STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   217, 0x0,                         STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   218,   229, STR_NULL,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_ship_aircraft_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,                    STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   110,   201, 0x0,                         STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   202,   213, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   202,   213, 0x0,                         STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   202,   213, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   109, 0x401,                       STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   109, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   109, 0x401,                       STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   109, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   110,   201, 0x0,                         STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   202,   213, STR_NULL,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _replace_rail_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 252, 456, 252,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_rail_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_road_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 230, 456, 230,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_road_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_ship_aircraft_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 214, 456, 214,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_ship_aircraft_vehicle_widgets,
	ReplaceVehicleWndProc
};


void ShowReplaceGroupVehicleWindow(GroupID id_g, VehicleType vehicletype)
{
	Window *w;

	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype);

	switch (vehicletype) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			w = AllocateWindowDescFront(&_replace_rail_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			WP(w, replaceveh_d).wagon_btnstate = true;
			break;
		case VEH_ROAD:
			w = AllocateWindowDescFront(&_replace_road_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			break;
		case VEH_SHIP:
		case VEH_AIRCRAFT:
			w = AllocateWindowDescFront(&_replace_ship_aircraft_vehicle_desc, vehicletype);
			w->vscroll.cap  = 4;
			w->resize.step_height = 24;
			break;
	}

	w->caption_color = _local_player;
	WP(w, replaceveh_d).sel_group = id_g;
	w->vscroll2.cap = w->vscroll.cap;   // these two are always the same
}
