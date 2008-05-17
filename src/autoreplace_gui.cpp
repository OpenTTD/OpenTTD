/* $Id$ */

/** @file autoreplace_gui.cpp GUI for autoreplace handling. */

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
#include "engine_func.h"
#include "engine_base.h"
#include "window_gui.h"

#include "table/sprites.h"
#include "table/strings.h"

void DrawEngineList(VehicleType type, int x, int y, const EngineList eng_list, uint16 min, uint16 max, EngineID selected_id, int count_location, GroupID selected_group);

static const StringID _rail_types_list[] = {
	STR_RAIL_VEHICLES,
	STR_ELRAIL_VEHICLES,
	STR_MONORAIL_VEHICLES,
	STR_MAGLEV_VEHICLES,
	INVALID_STRING_ID
};

enum ReplaceVehicleWindowWidgets {
	RVW_WIDGET_LEFT_MATRIX = 3,
	RVW_WIDGET_LEFT_SCROLLBAR,
	RVW_WIDGET_RIGHT_MATRIX,
	RVW_WIDGET_RIGHT_SCROLLBAR,
	RVW_WIDGET_LEFT_DETAILS,
	RVW_WIDGET_RIGHT_DETAILS,

	/* Button row */
	RVW_WIDGET_START_REPLACE,
	RVW_WIDGET_INFO_TAB,
	RVW_WIDGET_STOP_REPLACE,
	RVW_WIDGET_RESIZE,

	/* Train only widgets */
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
	uint num_engines = GetGroupNumEngines(_local_player, id_g, e);

	if (num_engines == 0 || p->num_engines[e] == 0) {
		/* We don't have any of this engine type.
		 * Either we just sold the last one, we build a new one or we stopped replacing it.
		 * In all cases, we need to update the left list */
		InvalidateWindowData(WC_REPLACE_VEHICLE, GetEngine(e)->type, true);
	}
}

/** When an engine is made buildable or is removed from being buildable, add/remove it from the build/autoreplace lists
 * @param type The type of engine
 */
void AddRemoveEngineFromAutoreplaceAndBuildWindows(VehicleType type)
{
	InvalidateWindowData(WC_REPLACE_VEHICLE, type, false); // Update the autoreplace window
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

/**
 * Window for the autoreplacing of vehicles.
 */
class ReplaceVehicleWindow : public Window {
	byte sel_index[2];
	EngineID sel_engine[2];
	uint16 count[2];
	bool wagon_btnstate; ///< true means engine is selected
	EngineList list[2];
	bool update_left;
	bool update_right;
	bool init_lists;
	GroupID sel_group;
	static RailType sel_railtype;

	/** Figure out if an engine should be added to a list
	 * @param e The EngineID
	 * @param draw_left If true, then the left list is drawn (the engines specific to the railtype you selected)
	 * @param show_engines if truem then locomotives are drawn, else wagons (never both)
	 * @return true if the engine should be in the list (based on this check)
	 */
	bool GenerateReplaceRailList(EngineID e, bool draw_left, bool show_engines)
	{
		const RailVehicleInfo *rvi = RailVehInfo(e);

		/* Ensure that the wagon/engine selection fits the engine. */
		if ((rvi->railveh_type == RAILVEH_WAGON) == show_engines) return false;

		if (draw_left && show_engines) {
			/* Ensure that the railtype is specific to the selected one */
			if (rvi->railtype != this->sel_railtype) return false;
		} else {
			/* Ensure that it's a compatible railtype to the selected one (like electric <-> diesel)
			 * The vehicle do not have to have power on the railtype in question, only able to drive (pulled if needed) */
			if (!IsCompatibleRail(rvi->railtype, this->sel_railtype)) return false;
		}
		return true;
	}


	/** Generate a list
	 * @param w Window, that contains the list
	 * @param draw_left true if generating the left list, otherwise false
	 */
	void GenerateReplaceVehList(Window *w, bool draw_left)
	{
		EngineID selected_engine = INVALID_ENGINE;
		VehicleType type = (VehicleType)this->window_number;
		byte i = draw_left ? 0 : 1;

		EngineList *list = &this->list[i];
		list->clear();

		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, type) {
			EngineID eid = e->index;
			if (type == VEH_TRAIN && !GenerateReplaceRailList(eid, draw_left, this->wagon_btnstate)) continue; // special rules for trains

			if (draw_left) {
				const GroupID selected_group = this->sel_group;
				const uint num_engines = GetGroupNumEngines(_local_player, selected_group, eid);

				/* Skip drawing the engines we don't have any of and haven't set for replacement */
				if (num_engines == 0 && EngineReplacementForPlayer(GetPlayer(_local_player), eid, selected_group) == INVALID_ENGINE) continue;
			} else {
				/* This is for engines we can replace to and they should depend on what we selected to replace from */
				if (!IsEngineBuildable(eid, type, _local_player)) continue; // we need to be able to build the engine
				if (!EnginesGotCargoInCommon(eid, this->sel_engine[0])) continue; // the engines needs to be able to carry the same cargo

				/* Road vehicles can't be replaced by trams and vice-versa */
				if (type == VEH_ROAD && HasBit(EngInfo(this->sel_engine[0])->misc_flags, EF_ROAD_TRAM) != HasBit(e->info.misc_flags, EF_ROAD_TRAM)) continue;
				if (eid == this->sel_engine[0]) continue; // we can't replace an engine into itself (that would be autorenew)
			}

			list->push_back(eid);
			if (eid == this->sel_engine[i]) selected_engine = eid; // The selected engine is still in the list
		}
		this->sel_engine[i] = selected_engine; // update which engine we selected (the same or none, if it's not in the list anymore)
		if (type == VEH_TRAIN) EngList_Sort(list, &TrainEngineNumberSorter);
	}

	/** Generate the lists */
	void GenerateLists()
	{
		EngineID e = this->sel_engine[0];

		if (this->update_left == true) {
			/* We need to rebuild the left list */
			GenerateReplaceVehList(this, true);
			SetVScrollCount(this, this->list[0].size());
			if (this->init_lists && this->sel_engine[0] == INVALID_ENGINE && this->list[0].size() != 0) {
				this->sel_engine[0] = this->list[0][0];
			}
		}

		if (this->update_right || e != this->sel_engine[0]) {
			/* Either we got a request to rebuild the right list or the left list selected a different engine */
			if (this->sel_engine[0] == INVALID_ENGINE) {
				/* Always empty the right list when nothing is selected in the left list */
				this->list[1].clear();
				this->sel_engine[1] = INVALID_ENGINE;
			} else {
				GenerateReplaceVehList(this, false);
				SetVScroll2Count(this, this->list[1].size());
				if (this->init_lists && this->sel_engine[1] == INVALID_ENGINE && this->list[1].size() != 0) {
					this->sel_engine[1] = this->list[1][0];
				}
			}
		}
		/* Reset the flags about needed updates */
		this->update_left  = false;
		this->update_right = false;
		this->init_lists   = false;
	}

public:
	ReplaceVehicleWindow(const WindowDesc *desc, VehicleType vehicletype, GroupID id_g) : Window(desc, vehicletype)
	{
		this->wagon_btnstate = true; // start with locomotives (all other vehicles will not read this bool)
		new (&this->list[0]) EngineList();
		new (&this->list[1]) EngineList();
		this->update_left   = true;
		this->update_right  = true;
		this->init_lists    = true;
		this->sel_engine[0] = INVALID_ENGINE;
		this->sel_engine[1] = INVALID_ENGINE;

		this->resize.step_height = GetVehicleListHeight(vehicletype);
		this->vscroll.cap = this->resize.step_height == 14 ? 8 : 4;

		Widget *widget = this->widget;
		widget[RVW_WIDGET_LEFT_MATRIX].data = widget[RVW_WIDGET_RIGHT_MATRIX].data = (this->vscroll.cap << 8) + 1;

		if (vehicletype == VEH_TRAIN) {
			this->wagon_btnstate = true;
			/* The train window is bigger so we will move some of the widgets to fit the new size.
			* We will start by moving the resize button to the lower right corner.                 */
			widget[RVW_WIDGET_RESIZE].top         = widget[RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE].top;
			widget[RVW_WIDGET_RESIZE].bottom      = widget[RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE].bottom;
			widget[RVW_WIDGET_STOP_REPLACE].right = widget[RVW_WIDGET_RESIZE].right;

			/* The detail panel is one line taller for trains so we will move some of the widgets one line (10 pixels) down. */
			widget[RVW_WIDGET_LEFT_DETAILS].bottom  += 10;
			widget[RVW_WIDGET_RIGHT_DETAILS].bottom += 10;
			for (int i = RVW_WIDGET_START_REPLACE; i < RVW_WIDGET_RESIZE; i++) {
				widget[i].top    += 10;
				widget[i].bottom += 10;
			}
		} else {
			/* Since it's not a train we will hide the train only widgets. */
			this->SetWidgetsHiddenState(true,
									RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE,
									RVW_WIDGET_TRAIN_FLUFF_LEFT,
									RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN,
									RVW_WIDGET_TRAIN_FLUFF_RIGHT,
									RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE,
									WIDGET_LIST_END);
		}

		ResizeWindow(this, 0, this->resize.step_height * this->vscroll.cap);

		/* Set the minimum window size to the current window size */
		this->resize.width  = this->width;
		this->resize.height = this->height;

		this->caption_color = _local_player;
		this->sel_group = id_g;
		this->vscroll2.cap = this->vscroll.cap;   // these two are always the same

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		static const StringID _vehicle_type_names[] = {
			STR_019F_TRAIN,
			STR_019C_ROAD_VEHICLE,
			STR_019E_SHIP,
			STR_019D_AIRCRAFT
		};

		if (this->update_left || this->update_right) this->GenerateLists();

		Player *p = GetPlayer(_local_player);
		EngineID selected_id[2];
		const GroupID selected_group = this->sel_group;

		selected_id[0] = this->sel_engine[0];
		selected_id[1] = this->sel_engine[1];

		/* Disable the "Start Replacing" button if:
		 *    Either list is empty
		 * or The selected replacement engine has a replacement (to prevent loops)
		 * or The right list (new replacement) has the existing replacement vehicle selected */
		this->SetWidgetDisabledState(RVW_WIDGET_START_REPLACE,
										selected_id[0] == INVALID_ENGINE ||
										selected_id[1] == INVALID_ENGINE ||
										EngineReplacementForPlayer(p, selected_id[1], selected_group) != INVALID_ENGINE ||
										EngineReplacementForPlayer(p, selected_id[0], selected_group) == selected_id[1]);

		/* Disable the "Stop Replacing" button if:
		 *   The left list (existing vehicle) is empty
		 *   or The selected vehicle has no replacement set up */
		this->SetWidgetDisabledState(RVW_WIDGET_STOP_REPLACE,
										selected_id[0] == INVALID_ENGINE ||
										!EngineHasReplacementForPlayer(p, selected_id[0], selected_group));

		/* now the actual drawing of the window itself takes place */
		SetDParam(0, _vehicle_type_names[this->window_number]);

		if (this->window_number == VEH_TRAIN) {
			/* set on/off for renew_keep_length */
			SetDParam(1, p->renew_keep_length ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);

			/* set wagon/engine button */
			SetDParam(2, this->wagon_btnstate ? STR_ENGINES : STR_WAGONS);

			/* sets the colour of that art thing */
			this->widget[RVW_WIDGET_TRAIN_FLUFF_LEFT].color  = _player_colors[_local_player];
			this->widget[RVW_WIDGET_TRAIN_FLUFF_RIGHT].color = _player_colors[_local_player];
		}

		if (this->window_number == VEH_TRAIN) {
			/* Show the selected railtype in the pulldown menu */
			this->widget[RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN].data = _rail_types_list[sel_railtype];
		}

		this->DrawWidgets();

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

		DrawString(145, this->widget[RVW_WIDGET_INFO_TAB].top + 1, STR_02BD, TC_BLACK);

		/* Draw the lists */
		for (byte i = 0; i < 2; i++) {
			uint widget     = (i == 0) ? RVW_WIDGET_LEFT_MATRIX : RVW_WIDGET_RIGHT_MATRIX;
			EngineList list = this->list[i]; // which list to draw
			EngineID start  = i == 0 ? this->vscroll.pos : this->vscroll2.pos; // what is the offset for the start (scrolling)
			EngineID end    = min((i == 0 ? this->vscroll.cap : this->vscroll2.cap) + start, list.size());

			/* Do the actual drawing */
			DrawEngineList((VehicleType)this->window_number, this->widget[widget].left + 2, this->widget[widget].top + 1, list, start, end, this->sel_engine[i], i == 0 ? this->widget[RVW_WIDGET_LEFT_MATRIX].right - 2 : 0, selected_group);

			/* Also draw the details if an engine is selected */
			if (this->sel_engine[i] != INVALID_ENGINE) {
				const Widget *wi = &this->widget[i == 0 ? RVW_WIDGET_LEFT_DETAILS : RVW_WIDGET_RIGHT_DETAILS];
				int text_end = DrawVehiclePurchaseInfo(wi->left + 2, wi->top + 1, wi->right - wi->left - 2, this->sel_engine[i]);

				if (text_end > wi->bottom) {
					this->SetDirty();
					ResizeWindowForWidget(this, i == 0 ? RVW_WIDGET_LEFT_DETAILS : RVW_WIDGET_RIGHT_DETAILS, 0, text_end - wi->bottom);
					this->SetDirty();
				}
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE:
				this->wagon_btnstate = !(this->wagon_btnstate);
				this->update_left = true;
				this->init_lists  = true;
				this->SetDirty();
				break;

			case RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN: /* Railtype selection dropdown menu */
				ShowDropDownMenu(this, _rail_types_list, sel_railtype, RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN, 0, ~GetPlayer(_local_player)->avail_railtypes);
				break;

			case RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE: /* toggle renew_keep_length */
				DoCommandP(0, 5, GetPlayer(_local_player)->renew_keep_length ? 0 : 1, NULL, CMD_SET_AUTOREPLACE);
				break;

			case RVW_WIDGET_START_REPLACE: { /* Start replacing */
				EngineID veh_from = this->sel_engine[0];
				EngineID veh_to = this->sel_engine[1];
				DoCommandP(0, 3 + (this->sel_group << 16) , veh_from + (veh_to << 16), NULL, CMD_SET_AUTOREPLACE);
			} break;

			case RVW_WIDGET_STOP_REPLACE: { /* Stop replacing */
				EngineID veh_from = this->sel_engine[0];
				DoCommandP(0, 3 + (this->sel_group << 16), veh_from + (INVALID_ENGINE << 16), NULL, CMD_SET_AUTOREPLACE);
			} break;

			case RVW_WIDGET_LEFT_MATRIX:
			case RVW_WIDGET_RIGHT_MATRIX: {
				uint i = (pt.y - 14) / this->resize.step_height;
				uint16 click_scroll_pos = widget == RVW_WIDGET_LEFT_MATRIX ? this->vscroll.pos : this->vscroll2.pos;
				uint16 click_scroll_cap = widget == RVW_WIDGET_LEFT_MATRIX ? this->vscroll.cap : this->vscroll2.cap;
				byte click_side         = widget == RVW_WIDGET_LEFT_MATRIX ? 0 : 1;
				size_t engine_count     = this->list[click_side].size();

				if (i < click_scroll_cap) {
					i += click_scroll_pos;
					EngineID e = engine_count > i ? this->list[click_side][i] : INVALID_ENGINE;
					if (e == this->sel_engine[click_side]) break; // we clicked the one we already selected
					this->sel_engine[click_side] = e;
					if (click_side == 0) {
						this->update_right = true;
						this->init_lists   = true;
					}
					this->SetDirty();
					}
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
		this->vscroll.pos  = 0;
		this->vscroll2.pos = 0;
		/* Rebuild the lists */
		this->update_left  = true;
		this->update_right = true;
		this->init_lists   = true;
		this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta) {
		this->vscroll.cap  += delta.y / (int)this->resize.step_height;
		this->vscroll2.cap += delta.y / (int)this->resize.step_height;

		Widget *widget = this->widget;

		widget[RVW_WIDGET_LEFT_MATRIX].data = widget[RVW_WIDGET_RIGHT_MATRIX].data = (this->vscroll2.cap << 8) + 1;

		if (delta.x != 0) {
			/* We changed the width of the window so we have to resize the lists.
				* Because ResizeButtons() makes each widget the same size it can't be used on the lists
				* because then the lists would have the same size as the scrollbars.
				* Instead we use it on the detail panels.
				* Afterwards we use the new location of the detail panels (the middle of the window)
				* to place the lists.
				* This way the lists will have equal size while keeping the width of the scrollbars unchanged. */
			ResizeButtons(this, RVW_WIDGET_LEFT_DETAILS, RVW_WIDGET_RIGHT_DETAILS);
			widget[RVW_WIDGET_RIGHT_MATRIX].left    = widget[RVW_WIDGET_RIGHT_DETAILS].left;
			widget[RVW_WIDGET_LEFT_SCROLLBAR].right = widget[RVW_WIDGET_LEFT_DETAILS].right;
			widget[RVW_WIDGET_LEFT_SCROLLBAR].left  = widget[RVW_WIDGET_LEFT_SCROLLBAR].right - 11;
			widget[RVW_WIDGET_LEFT_MATRIX].right    = widget[RVW_WIDGET_LEFT_SCROLLBAR].left - 1;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (data != 0) {
			this->update_left = true;
		} else {
			this->update_right = true;
		}
	}
};

static const Widget _replace_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   444,   455,     0,    13, STR_NULL,                        STR_STICKY_BUTTON},

{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,    13, 0x1,                             STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,    13, STR_NULL,                        STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX,    RESIZE_LRB,    14,   228,   443,    14,    13, 0x1,                             STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR,    RESIZE_LRB,    14,   444,   455,    14,    13, STR_NULL,                        STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,    14,   105, 0x0,                             STR_NULL},
{      WWT_PANEL,    RESIZE_RTB,    14,   228,   455,    14,   105, 0x0,                             STR_NULL},

{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   106,   117, STR_REPLACE_VEHICLES_START,      STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,    RESIZE_RTB,    14,   139,   305,   106,   117, 0x0,                             STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,    14,   306,   443,   106,   117, STR_REPLACE_VEHICLES_STOP,       STR_REPLACE_HELP_STOP_BUTTON},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   444,   455,   106,   117, STR_NULL,                        STR_RESIZE_BUTTON},

{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   128,   139, STR_REPLACE_ENGINE_WAGON_SELECT, STR_REPLACE_ENGINE_WAGON_SELECT_HELP},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   153,   128,   139, 0x0,                             STR_NULL},
{   WWT_DROPDOWN,    RESIZE_RTB,    14,   154,   289,   128,   139, 0x0,                             STR_REPLACE_HELP_RAILTYPE},
{      WWT_PANEL,   RESIZE_LRTB,    14,   290,   305,   128,   139, 0x0,                             STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,    14,   306,   443,   128,   139, STR_REPLACE_REMOVE_WAGON,        STR_REPLACE_REMOVE_WAGON_HELP},
{   WIDGETS_END},
};

static const WindowDesc _replace_rail_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 140, 456, 140,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_vehicle_widgets,
	NULL
};

static const WindowDesc _replace_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 118, 456, 118,
	WC_REPLACE_VEHICLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_vehicle_widgets,
	NULL
};

RailType ReplaceVehicleWindow::sel_railtype = RAILTYPE_RAIL;

void ShowReplaceGroupVehicleWindow(GroupID id_g, VehicleType vehicletype)
{
	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype);
	new ReplaceVehicleWindow(vehicletype == VEH_TRAIN ? &_replace_rail_vehicle_desc : &_replace_vehicle_desc, vehicletype, id_g);
}
