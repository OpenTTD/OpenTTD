/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "command.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"


static RailType _railtype_selected_in_replace_gui;

static const StringID _rail_types_list[] = {
	STR_RAIL_VEHICLES,
	STR_ELRAIL_VEHICLES,
	STR_MONORAIL_VEHICLES,
	STR_MAGLEV_VEHICLES,
	INVALID_STRING_ID
};

/* General Vehicle GUI based procedures that are independent of vehicle types */
void InitializeVehiclesGuiList(void)
{
	_railtype_selected_in_replace_gui = RAILTYPE_RAIL;
}

// this define is to match engine.c, but engine.c keeps it to itself
// ENGINE_AVAILABLE is used in ReplaceVehicleWndProc
#define ENGINE_AVAILABLE ((e->flags & 1 && HASBIT(info->climates, _opt.landscape)) || HASBIT(e->player_avail, _local_player))

/*  if show_outdated is selected, it do not sort psudo engines properly but it draws all engines
 * if used compined with show_cars set to false, it will work as intended. Replace window do it like that
 *  this was a big hack even before show_outdated was added. Stupid newgrf :p
 */
static void train_engine_drawing_loop(int *x, int *y, int *pos, int *sel, EngineID *selected_id, RailType railtype,
	uint8 lines_drawn, bool is_engine, bool show_cars, bool show_outdated, bool show_compatible)
{
	EngineID j;
	byte colour;
	const Player *p = GetPlayer(_local_player);

	for (j = 0; j < NUM_TRAIN_ENGINES; j++) {
		EngineID i = GetRailVehAtPosition(j);
		const Engine *e = GetEngine(i);
		const RailVehicleInfo *rvi = RailVehInfo(i);
		const EngineInfo *info = EngInfo(i);

		if (!EngineHasReplacementForPlayer(p, i) && p->num_engines[i] == 0 && show_outdated) continue;

		if ((rvi->power == 0 && !show_cars) || (rvi->power != 0 && show_cars))  // show wagons or engines (works since wagons do not have power)
			continue;

		if (*sel == 0) *selected_id = j;


		colour = *sel == 0 ? 0xC : 0x10;
		if (!(ENGINE_AVAILABLE && show_outdated && RailVehInfo(i)->power && IsCompatibleRail(e->railtype, railtype))) {
			if ((!IsCompatibleRail(e->railtype, railtype) && show_compatible)
				|| (e->railtype != railtype && !show_compatible)
				|| !(rvi->flags & RVI_WAGON) != is_engine ||
				!HASBIT(e->player_avail, _local_player))
				continue;
#if 0
		} else {
			// TODO find a nice red colour for vehicles being replaced
			if ( _autoreplace_array[i] != i )
				colour = *sel == 0 ? 0x44 : 0x45;
#endif
		}

		if (IS_INT_INSIDE(--*pos, -lines_drawn, 0)) {
			DrawString(*x + 59, *y + 2, GetCustomEngineName(i),
				colour);
			// show_outdated is true only for left side, which is where we show old replacements
			DrawTrainEngine(*x + 29, *y + 6, i, (p->num_engines[i] == 0 && show_outdated) ?
				PALETTE_CRASH : GetEnginePalette(i, _local_player));
			if ( show_outdated ) {
				SetDParam(0, p->num_engines[i]);
				DrawStringRightAligned(213, *y+5, STR_TINY_BLACK, 0);
			}
			*y += 14;
		}
		--*sel;
	}
}


static void SetupScrollStuffForReplaceWindow(Window *w)
{
	EngineID selected_id[2] = { INVALID_ENGINE, INVALID_ENGINE };
	const Player* p = GetPlayer(_local_player);
	uint sel[2];
	uint count = 0;
	uint count2 = 0;
	EngineID i;

	sel[0] = WP(w,replaceveh_d).sel_index[0];
	sel[1] = WP(w,replaceveh_d).sel_index[1];

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			RailType railtype = _railtype_selected_in_replace_gui;

			w->widget[13].color = _player_colors[_local_player]; // sets the colour of that art thing
			w->widget[16].color = _player_colors[_local_player]; // sets the colour of that art thing

			for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
				EngineID eid = GetRailVehAtPosition(i);
				const Engine* e = GetEngine(eid);
				const EngineInfo* info = EngInfo(eid);

				// left window contains compatible engines while right window only contains engines of the selected type
				if (ENGINE_AVAILABLE &&
						(RailVehInfo(eid)->power != 0) == (WP(w, replaceveh_d).wagon_btnstate != 0)) {
					if (IsCompatibleRail(e->railtype, railtype) && (p->num_engines[eid] > 0 || EngineHasReplacementForPlayer(p, eid))) {
						if (sel[0] == count) selected_id[0] = eid;
						count++;
					}
					if (e->railtype == railtype && HASBIT(e->player_avail, _local_player)) {
						if (sel[1] == count2) selected_id[1] = eid;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Road: {
			for (i = ROAD_ENGINES_INDEX; i < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; i++) {
				if (p->num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) { // only draw right array if we have anything in the left one
				CargoID cargo = RoadVehInfo(selected_id[0])->cargo_type;

				for (i = ROAD_ENGINES_INDEX; i < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; i++) {
					if (cargo == RoadVehInfo(i)->cargo_type &&
							HASBIT(GetEngine(i)->player_avail, _local_player)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Ship: {
			for (i = SHIP_ENGINES_INDEX; i < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; i++) {
				if (p->num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) {
				const ShipVehicleInfo* svi = ShipVehInfo(selected_id[0]);
				CargoID cargo = svi->cargo_type;
				bool refittable = svi->refittable;

				for (i = SHIP_ENGINES_INDEX; i < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; i++) {
					if (HASBIT(GetEngine(i)->player_avail, _local_player) && (
								ShipVehInfo(i)->cargo_type == cargo ||
								ShipVehInfo(i)->refittable && refittable
							)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Aircraft: {
			for (i = AIRCRAFT_ENGINES_INDEX; i < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; i++) {
				if (p->num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) {
				byte subtype = AircraftVehInfo(selected_id[0])->subtype;

				for (i = AIRCRAFT_ENGINES_INDEX; i < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; i++) {
					if (HASBIT(GetEngine(i)->player_avail, _local_player) &&
							(subtype & AIR_CTOL) == (AircraftVehInfo(i)->subtype & AIR_CTOL)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}
	}
	// sets up the number of items in each list
	SetVScrollCount(w, count);
	SetVScroll2Count(w, count2);
	WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
	WP(w,replaceveh_d).sel_engine[1] = selected_id[1];

	WP(w,replaceveh_d).count[0] = count;
	WP(w,replaceveh_d).count[1] = count2;
	return;
}


static void DrawEngineArrayInReplaceWindow(Window *w, int x, int y, int x2, int y2, int pos, int pos2,
	int sel1, int sel2, EngineID selected_id1, EngineID selected_id2)
{
	int sel[2];
	EngineID selected_id[2];
	const Player *p = GetPlayer(_local_player);

	sel[0] = sel1;
	sel[1] = sel2;

	selected_id[0] = selected_id1;
	selected_id[1] = selected_id2;

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			RailType railtype = _railtype_selected_in_replace_gui;
			DrawString(157, w->widget[14].top + 1, _rail_types_list[railtype], 0x10);
			/* draw sorting criteria string */

			/* Ensure that custom engines which substituted wagons
			 * are sorted correctly.
			 * XXX - DO NOT EVER DO THIS EVER AGAIN! GRRR hacking in wagons as
			 * engines to get more types.. Stays here until we have our own format
			 * then it is exit!!! */
			if (WP(w,replaceveh_d).wagon_btnstate) {
				train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, true, false, true, true); // True engines
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, true, false, false, false); // True engines
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, false, false, false); // Feeble wagons
			} else {
				train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, false, true, true, true);
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, true, false, true);
			}
			break;
		}

		case VEH_Road: {
			int num = NUM_ROAD_ENGINES;
			const Engine* e = GetEngine(ROAD_ENGINES_INDEX);
			EngineID engine_id = ROAD_ENGINES_INDEX;

			do {
				if (p->num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+59, y+2, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
						DrawRoadVehEngine(x+29, y+6, engine_id, p->num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
						SetDParam(0, p->num_engines[engine_id]);
						DrawStringRightAligned(213, y+5, STR_TINY_BLACK, 0);
						y += 14;
					}
				sel[0]--;
				}

				if (selected_id[0] != INVALID_ENGINE) {
					byte cargo = RoadVehInfo(selected_id[0])->cargo_type;

					if (RoadVehInfo(engine_id)->cargo_type == cargo && HASBIT(e->player_avail, _local_player)) {
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0) && RoadVehInfo(engine_id)->cargo_type == cargo) {
							DrawString(x2+59, y2+2, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawRoadVehEngine(x2+29, y2+6, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 14;
						}
						sel[1]--;
					}
				}
			} while (++engine_id, ++e,--num);
			break;
		}

		case VEH_Ship: {
			int num = NUM_SHIP_ENGINES;
			const Engine* e = GetEngine(SHIP_ENGINES_INDEX);
			EngineID engine_id = SHIP_ENGINES_INDEX;

			do {
				if (p->num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+75, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
						DrawShipEngine(x+35, y+10, engine_id, p->num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
						SetDParam(0, p->num_engines[engine_id]);
						DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
						y += 24;
					}
					sel[0]--;
				}

				if (selected_id[0] != INVALID_ENGINE) {
					CargoID cargo = ShipVehInfo(selected_id[0])->cargo_type;
					bool refittable = ShipVehInfo(selected_id[0])->refittable;

					if (HASBIT(e->player_avail, _local_player) && ( cargo == ShipVehInfo(engine_id)->cargo_type || refittable & ShipVehInfo(engine_id)->refittable)) {
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
							DrawString(x2+75, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawShipEngine(x2+35, y2+10, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 24;
						}
						sel[1]--;
					}
				}
			} while (++engine_id, ++e, --num);
			break;
		}   //end of ship

		case VEH_Aircraft: {
			int num = NUM_AIRCRAFT_ENGINES;
			const Engine* e = GetEngine(AIRCRAFT_ENGINES_INDEX);
			EngineID engine_id = AIRCRAFT_ENGINES_INDEX;

			do {
				if (p->num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
					if (sel[0] == 0) selected_id[0] = engine_id;
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+62, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
						DrawAircraftEngine(x+29, y+10, engine_id, p->num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
						SetDParam(0, p->num_engines[engine_id]);
						DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
						y += 24;
					}
					sel[0]--;
				}

				if (selected_id[0] != INVALID_ENGINE) {
					byte subtype = AircraftVehInfo(selected_id[0])->subtype;

					if ((subtype & AIR_CTOL) == (AircraftVehInfo(engine_id)->subtype & AIR_CTOL) &&
							HASBIT(e->player_avail, _local_player)) {
						if (sel[1] == 0) selected_id[1] = engine_id;
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
							DrawString(x2+62, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawAircraftEngine(x2+29, y2+10, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 24;
						}
						sel[1]--;
					}
				}
			} while (++engine_id, ++e,--num);
			break;
		}   // end of aircraft
	}
}

static void ReplaceVehicleWndProc(Window *w, WindowEvent *e)
{
	static const StringID _vehicle_type_names[] = {
		STR_019F_TRAIN,
		STR_019C_ROAD_VEHICLE,
		STR_019E_SHIP,
		STR_019D_AIRCRAFT
	};

	switch (e->event) {
		case WE_PAINT: {
				Player *p = GetPlayer(_local_player);
				int pos = w->vscroll.pos;
				EngineID selected_id[2] = { INVALID_ENGINE, INVALID_ENGINE };
				int x = 1;
				int y = 15;
				int pos2 = w->vscroll2.pos;
				int x2 = 1 + 228;
				int y2 = 15;
				int sel[2];
				byte i;
				sel[0] = WP(w,replaceveh_d).sel_index[0];
				sel[1] = WP(w,replaceveh_d).sel_index[1];

				SetupScrollStuffForReplaceWindow(w);

				selected_id[0] = WP(w,replaceveh_d).sel_engine[0];
				selected_id[1] = WP(w,replaceveh_d).sel_engine[1];

				// Disable the "Start Replacing" button if:
				//    Either list is empty
				// or Both lists have the same vehicle selected
				// or The selected replacement engine has a replacement (to prevent loops)
				// or The right list (new replacement) has the existing replacement vehicle selected
				SetWindowWidgetDisabledState(w, 4,
						selected_id[0] == INVALID_ENGINE ||
						selected_id[1] == INVALID_ENGINE ||
						selected_id[0] == selected_id[1] ||
						EngineReplacementForPlayer(p, selected_id[1]) != INVALID_ENGINE ||
						EngineReplacementForPlayer(p, selected_id[0]) == selected_id[1]);

				// Disable the "Stop Replacing" button if:
				//    The left list (existing vehicle) is empty
				// or The selected vehicle has no replacement set up
				SetWindowWidgetDisabledState(w, 6,
						selected_id[0] == INVALID_ENGINE ||
						!EngineHasReplacementForPlayer(p, selected_id[0]));

				// now the actual drawing of the window itself takes place
				SetDParam(0, _vehicle_type_names[WP(w, replaceveh_d).vehicletype - VEH_Train]);

				if (WP(w, replaceveh_d).vehicletype == VEH_Train) {
					// set on/off for renew_keep_length
					SetDParam(1, p->renew_keep_length ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);

					// set wagon/engine button
					SetDParam(2, WP(w, replaceveh_d).wagon_btnstate ? STR_ENGINES : STR_WAGONS);
				}

				DrawWindowWidgets(w);

				// sets up the string for the vehicle that is being replaced to
				if (selected_id[0] != INVALID_ENGINE) {
					if (!EngineHasReplacementForPlayer(p, selected_id[0])) {
						SetDParam(0, STR_NOT_REPLACING);
					} else {
						SetDParam(0, GetCustomEngineName(EngineReplacementForPlayer(p, selected_id[0])));
					}
				} else {
					SetDParam(0, STR_NOT_REPLACING_VEHICLE_SELECTED);
				}

				DrawString(145, w->widget[5].top + 1, STR_02BD, 0x10);

				/* now we draw the two arrays according to what we just counted */
				DrawEngineArrayInReplaceWindow(w, x, y, x2, y2, pos, pos2, sel[0], sel[1], selected_id[0], selected_id[1]);

				WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
				WP(w,replaceveh_d).sel_engine[1] = selected_id[1];
				/* now we draw the info about the vehicles we selected */
				for (i = 0 ; i < 2 ; i++) {
					if (selected_id[i] != INVALID_ENGINE) {
						const Widget *wi = &w->widget[i == 0 ? 3 : 11];
						DrawVehiclePurchaseInfo(wi->left + 2 , wi->top + 1, wi->right - wi->left - 2, selected_id[i]);
					}
				}
			} break;   // end of paint

		case WE_CLICK: {
			// these 3 variables is used if any of the lists is clicked
			uint16 click_scroll_pos = w->vscroll2.pos;
			uint16 click_scroll_cap = w->vscroll2.cap;
			byte click_side = 1;

			switch (e->we.click.widget) {
				case 12:
					WP(w, replaceveh_d).wagon_btnstate = !(WP(w, replaceveh_d).wagon_btnstate);
					SetWindowDirty(w);
					break;

				case 14:
				case 15: /* Railtype selection dropdown menu */
					ShowDropDownMenu(w, _rail_types_list, _railtype_selected_in_replace_gui, 15, 0, ~GetPlayer(_local_player)->avail_railtypes);
					break;

				case 17: /* toggle renew_keep_length */
					DoCommandP(0, 5, GetPlayer(_local_player)->renew_keep_length ? 0 : 1, NULL, CMD_SET_AUTOREPLACE);
					break;

				case 4: { /* Start replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					EngineID veh_to = WP(w, replaceveh_d).sel_engine[1];
					DoCommandP(0, 3, veh_from + (veh_to << 16), NULL, CMD_SET_AUTOREPLACE);
					break;
				}

				case 6: { /* Stop replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					DoCommandP(0, 3, veh_from + (INVALID_ENGINE << 16), NULL, CMD_SET_AUTOREPLACE);
					break;
				}

				case 7:
					// sets up that the left one was clicked. The default values are for the right one (9)
					// this way, the code for 9 handles both sides
					click_scroll_pos = w->vscroll.pos;
					click_scroll_cap = w->vscroll.cap;
					click_side = 0;
					/* FALL THROUGH */

				case 9: {
					uint i = (e->we.click.pt.y - 14) / w->resize.step_height;
					if (i < click_scroll_cap) {
						WP(w,replaceveh_d).sel_index[click_side] = i + click_scroll_pos;
						SetWindowDirty(w);
					}
					break;
				}
			}
			break;
		}

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			_railtype_selected_in_replace_gui = (RailType)e->we.dropdown.index;
			/* Reset scrollbar positions */
			w->vscroll.pos  = 0;
			w->vscroll2.pos = 0;
			SetWindowDirty(w);
			break;

		case WE_RESIZE:
			w->vscroll.cap  += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->vscroll2.cap += e->we.sizing.diff.y / (int)w->resize.step_height;

			w->widget[7].data = (w->vscroll.cap  << 8) + 1;
			w->widget[9].data = (w->vscroll2.cap << 8) + 1;
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
{      WWT_PANEL,     RESIZE_TB,    14,   154,   277,   240,   251, 0x0,            STR_REPLACE_HELP_RAILTYPE},
{    WWT_TEXTBTN,     RESIZE_TB,    14,   278,   289,   240,   251, STR_0225,       STR_REPLACE_HELP_RAILTYPE},
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
	WDP_AUTO, WDP_AUTO, 456, 252,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_rail_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_road_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 230,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_road_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_ship_aircraft_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 214,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_ship_aircraft_vehicle_widgets,
	ReplaceVehicleWndProc
};


void ShowReplaceVehicleWindow(byte vehicletype)
{
	Window *w;

	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype);

	switch (vehicletype) {
		case VEH_Train:
			w = AllocateWindowDescFront(&_replace_rail_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			WP(w, replaceveh_d).wagon_btnstate = true;
			break;
		case VEH_Road:
			w = AllocateWindowDescFront(&_replace_road_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			break;
		case VEH_Ship:
		case VEH_Aircraft:
			w = AllocateWindowDescFront(&_replace_ship_aircraft_vehicle_desc, vehicletype);
			w->vscroll.cap  = 4;
			w->resize.step_height = 24;
			break;
		default: return;
	}

	w->caption_color = _local_player;
	WP(w, replaceveh_d).vehicletype = vehicletype;
	w->vscroll2.cap = w->vscroll.cap;   // these two are always the same
}
