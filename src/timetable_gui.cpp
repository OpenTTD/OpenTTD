/* $Id$ */

/** @file timetable_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "command_func.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "cargotype.h"
#include "depot.h"
#include "strings_func.h"
#include "vehicle_base.h"
#include "string_func.h"
#include "gfx_func.h"
#include "player_func.h"
#include "settings_type.h"

#include "table/strings.h"

enum TimetableViewWindowWidgets {
	TTV_WIDGET_CLOSEBOX = 0,
	TTV_CAPTION,
	TTV_ORDER_VIEW,
	TTV_STICKY,
	TTV_TIMETABLE_PANEL,
	TTV_SCROLLBAR,
	TTV_SUMMARY_PANEL,
	TTV_CHANGE_TIME,
	TTV_CLEAR_TIME,
	TTV_RESET_LATENESS,
	TTV_AUTOFILL,
	TTV_EMPTY,
	TTV_RESIZE,
};

struct timetable_d {
	int sel;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(timetable_d));

static int GetOrderFromTimetableWndPt(Window *w, int y, const Vehicle *v)
{
	/*
	 * Calculation description:
	 * 15 = 14 (w->widget[TTV_ORDER_VIEW].top) + 1 (frame-line)
	 * 10 = order text hight
	 */
	int sel = (y - 15) / 10;

	if ((uint)sel >= w->vscroll.cap) return INVALID_ORDER;

	sel += w->vscroll.pos;

	return (sel <= v->num_orders * 2 && sel >= 0) ? sel : INVALID_ORDER;
}

static inline void SetTimetableParams(int param1, int param2, uint32 time)
{
	if (_patches.timetable_in_ticks) {
		SetDParam(param1, STR_TIMETABLE_TICKS);
		SetDParam(param2, time);
	} else {
		SetDParam(param1, STR_TIMETABLE_DAYS);
		SetDParam(param2, time / DAY_TICKS);
	}
}

static void DrawTimetableWindow(Window *w)
{
	const Vehicle *v = GetVehicle(w->window_number);
	int selected = WP(w, timetable_d).sel;

	SetVScrollCount(w, v->num_orders * 2);

	if (v->owner == _local_player) {
		if (selected == -1) {
			w->DisableWidget(TTV_CHANGE_TIME);
			w->DisableWidget(TTV_CLEAR_TIME);
		} else if (selected % 2 == 1) {
			w->EnableWidget(TTV_CHANGE_TIME);
			w->EnableWidget(TTV_CLEAR_TIME);
		} else {
			const Order *order = GetVehicleOrder(v, (selected + 1) / 2);
			bool disable = order == NULL || !order->IsType(OT_GOTO_STATION) || (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION);

			w->SetWidgetDisabledState(TTV_CHANGE_TIME, disable);
			w->SetWidgetDisabledState(TTV_CLEAR_TIME, disable);
		}

		w->EnableWidget(TTV_RESET_LATENESS);
		w->EnableWidget(TTV_AUTOFILL);
	} else {
		w->DisableWidget(TTV_CHANGE_TIME);
		w->DisableWidget(TTV_CLEAR_TIME);
		w->DisableWidget(TTV_RESET_LATENESS);
		w->DisableWidget(TTV_AUTOFILL);
	}

	w->SetWidgetLoweredState(TTV_AUTOFILL, HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE));

	SetDParam(0, v->index);
	DrawWindowWidgets(w);

	int y = 15;
	int i = w->vscroll.pos;
	VehicleOrderID order_id = (i + 1) / 2;
	bool final_order = false;

	const Order *order = GetVehicleOrder(v, order_id);

	while (order != NULL) {
		/* Don't draw anything if it extends past the end of the window. */
		if (i - w->vscroll.pos >= w->vscroll.cap) break;

		if (i % 2 == 0) {
			SetDParam(5, STR_EMPTY);

			switch (order->GetType()) {
				case OT_DUMMY:
					SetDParam(0, STR_INVALID_ORDER);
					break;

				case OT_GOTO_STATION:
					SetDParam(0, STR_GO_TO_STATION);
					SetDParam(1, STR_ORDER_GO_TO + order->GetNonStopType());
					SetDParam(2, order->GetDestination());
					SetDParam(3, STR_EMPTY);

					if (order->wait_time > 0) {
						SetDParam(5, STR_TIMETABLE_STAY_FOR);
						SetTimetableParams(6, 7, order->wait_time);
					} else {
						SetDParam(4, STR_EMPTY);
					}

					break;

				case OT_GOTO_DEPOT:
					SetDParam(4, STR_EMPTY);
					if (v->type == VEH_AIRCRAFT) {
						if (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) {
							SetDParam(0, STR_GO_TO_NEAREST_DEPOT);
							SetDParam(2, STR_ORDER_NEAREST_HANGAR);
						} else {
							SetDParam(0, STR_GO_TO_HANGAR);
							SetDParam(2, order->GetDestination());
						}
						SetDParam(3, STR_EMPTY);
					} else {
						if (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) {
							SetDParam(0, STR_GO_TO_NEAREST_DEPOT);
							SetDParam(2, STR_ORDER_NEAREST_DEPOT);
						} else {
							SetDParam(0, STR_GO_TO_DEPOT);
							SetDParam(2, GetDepot(order->GetDestination())->town_index);
						}

						switch (v->type) {
							case VEH_TRAIN: SetDParam(3, STR_ORDER_TRAIN_DEPOT); break;
							case VEH_ROAD:  SetDParam(3, STR_ORDER_ROAD_DEPOT); break;
							case VEH_SHIP:  SetDParam(3, STR_ORDER_SHIP_DEPOT); break;
							default: NOT_REACHED();
						}
					}

					if (order->GetDepotOrderType() & ODTFB_SERVICE) {
						SetDParam(1, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_SERVICE_NON_STOP_AT : STR_ORDER_SERVICE_AT);
					} else {
						SetDParam(1, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_GO_NON_STOP_TO : STR_ORDER_GO_TO);
					}
					break;

				case OT_GOTO_WAYPOINT:
					SetDParam(0, (order->GetNonStopType() != ONSF_STOP_EVERYWHERE) ? STR_GO_NON_STOP_TO_WAYPOINT : STR_GO_TO_WAYPOINT);
					SetDParam(1, order->GetDestination());
					break;


				case OT_CONDITIONAL: {
					extern uint ConvertSpeedToDisplaySpeed(uint speed);
					OrderConditionComparator occ = order->GetConditionComparator();
					SetDParam(0, (occ == OCC_IS_TRUE || occ == OCC_IS_FALSE) ? STR_CONDITIONAL_TRUE_FALSE : STR_CONDITIONAL_NUM);
					SetDParam(1, order->GetConditionSkipToOrder() + 1);
					SetDParam(2, STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + order->GetConditionVariable());
					SetDParam(3, STR_ORDER_CONDITIONAL_COMPARATOR_EQUALS + occ);

					uint value = order->GetConditionValue();
					if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
					SetDParam(4, value);
				} break;

				default: break;
			}

			DrawString(2, y, STR_TIMETABLE_GO_TO, (i == selected) ? TC_WHITE : TC_BLACK);

			order_id++;

			if (order_id >= v->num_orders) {
				order = GetVehicleOrder(v, 0);
				final_order = true;
			} else {
				order = order->next;
			}
		} else {
			StringID string;

			if (order->travel_time == 0) {
				string = STR_TIMETABLE_TRAVEL_NOT_TIMETABLED;
			} else {
				SetTimetableParams(0, 1, order->travel_time);
				string = STR_TIMETABLE_TRAVEL_FOR;
			}

			DrawString(12, y, string, (i == selected) ? TC_WHITE : TC_BLACK);

			if (final_order) break;
		}

		i++;
		y += 10;
	}

	y = w->widget[TTV_SUMMARY_PANEL].top + 1;

	{
		uint total_time = 0;
		bool complete = true;

		for (const Order *order = GetVehicleOrder(v, 0); order != NULL; order = order->next) {
			total_time += order->travel_time + order->wait_time;
			if (order->travel_time == 0) complete = false;
			if (order->wait_time == 0 && order->IsType(OT_GOTO_STATION) && !(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) complete = false;
		}

		if (total_time != 0) {
			SetTimetableParams(0, 1, total_time);
			DrawString(2, y, complete ? STR_TIMETABLE_TOTAL_TIME : STR_TIMETABLE_TOTAL_TIME_INCOMPLETE, TC_BLACK);
		}
	}
	y += 10;

	if (v->lateness_counter == 0 || (!_patches.timetable_in_ticks && v->lateness_counter / DAY_TICKS == 0)) {
		DrawString(2, y, STR_TIMETABLE_STATUS_ON_TIME, TC_BLACK);
	} else {
		SetTimetableParams(0, 1, abs(v->lateness_counter));
		DrawString(2, y, v->lateness_counter < 0 ? STR_TIMETABLE_STATUS_EARLY : STR_TIMETABLE_STATUS_LATE, TC_BLACK);
	}
}

static inline uint32 PackTimetableArgs(const Vehicle *v, uint selected)
{
	uint order_number = (selected + 1) / 2;
	uint is_journey   = (selected % 2 == 1) ? 1 : 0;

	if (order_number >= v->num_orders) order_number = 0;

	return v->index | (order_number << 16) | (is_journey << 24);
}

static void TimetableWndProc(Window *w, WindowEvent *we)
{
	switch (we->event) {
		case WE_PAINT:
			DrawTimetableWindow(w);
			break;

		case WE_CLICK: {
			const Vehicle *v = GetVehicle(w->window_number);

			switch (we->we.click.widget) {
				case TTV_ORDER_VIEW: /* Order view button */
					ShowOrdersWindow(v);
					break;

				case TTV_TIMETABLE_PANEL: { /* Main panel. */
					int selected = GetOrderFromTimetableWndPt(w, we->we.click.pt.y, v);

					if (selected == INVALID_ORDER || selected == WP(w, timetable_d).sel) {
						/* Deselect clicked order */
						WP(w, timetable_d).sel = -1;
					} else {
						/* Select clicked order */
						WP(w, timetable_d).sel = selected;
					}
				} break;

				case TTV_CHANGE_TIME: { /* "Wait For" button. */
					int selected = WP(w, timetable_d).sel;
					VehicleOrderID real = (selected + 1) / 2;

					if (real >= v->num_orders) real = 0;

					const Order *order = GetVehicleOrder(v, real);
					StringID current = STR_EMPTY;

					if (order != NULL) {
						uint time = (selected % 2 == 1) ? order->travel_time : order->wait_time;
						if (!_patches.timetable_in_ticks) time /= DAY_TICKS;

						if (time != 0) {
							SetDParam(0, time);
							current = STR_CONFIG_PATCHES_INT32;
						}
					}

					ShowQueryString(current, STR_TIMETABLE_CHANGE_TIME, 31, 150, w, CS_NUMERAL);
				} break;

				case TTV_CLEAR_TIME: { /* Clear waiting time button. */
					uint32 p1 = PackTimetableArgs(v, WP(w, timetable_d).sel);
					DoCommandP(0, p1, 0, NULL, CMD_CHANGE_TIMETABLE | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
				} break;

				case TTV_RESET_LATENESS: /* Reset the vehicle's late counter. */
					DoCommandP(0, v->index, 0, NULL, CMD_SET_VEHICLE_ON_TIME | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
					break;

				case TTV_AUTOFILL: /* Autofill the timetable. */
					DoCommandP(0, v->index, HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE) ? 0 : 1, NULL, CMD_AUTOFILL_TIMETABLE | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
					break;
			}

			SetWindowDirty(w);
		} break;

		case WE_ON_EDIT_TEXT: {
			const Vehicle *v = GetVehicle(w->window_number);

			uint32 p1 = PackTimetableArgs(v, WP(w, timetable_d).sel);

			uint64 time = StrEmpty(we->we.edittext.str) ? 0 : strtoul(we->we.edittext.str, NULL, 10);
			if (!_patches.timetable_in_ticks) time *= DAY_TICKS;

			uint32 p2 = minu(time, MAX_UVALUE(uint16));

			DoCommandP(0, p1, p2, NULL, CMD_CHANGE_TIMETABLE | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
		} break;

		case WE_RESIZE:
			/* Update the scroll + matrix */
			w->vscroll.cap = (w->widget[TTV_TIMETABLE_PANEL].bottom - w->widget[TTV_TIMETABLE_PANEL].top) / 10;
			break;

	}
}

static const Widget _timetable_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},                // TTV_WIDGET_CLOSEBOX
	{    WWT_CAPTION,   RESIZE_RIGHT,   14,    11,   326,     0,    13, STR_TIMETABLE_TITLE,        STR_018C_WINDOW_TITLE_DRAG_THIS},      // TTV_CAPTION
	{ WWT_PUSHTXTBTN,   RESIZE_LR,      14,   327,   387,     0,    13, STR_ORDER_VIEW,             STR_ORDER_VIEW_TOOLTIP},               // TTV_ORDER_VIEW
	{  WWT_STICKYBOX,   RESIZE_LR,      14,   388,   399,     0,    13, STR_NULL,                   STR_STICKY_BUTTON},                    // TTV_STICKY

	{      WWT_PANEL,   RESIZE_RB,      14,     0,   387,    14,    95, STR_NULL,                   STR_TIMETABLE_TOOLTIP},                // TTV_TIMETABLE_PANEL
	{  WWT_SCROLLBAR,   RESIZE_LRB,     14,   388,   399,    14,    95, STR_NULL,                   STR_0190_SCROLL_BAR_SCROLLS_LIST},     // TTV_SCROLLBAR

	{      WWT_PANEL,   RESIZE_RTB,     14,     0,   399,    96,   117, STR_NULL,                   STR_NULL},                             // TTV_SUMMARY_PANEL

	{ WWT_PUSHTXTBTN,   RESIZE_TB,      14,     0,   109,   118,   129, STR_TIMETABLE_CHANGE_TIME,  STR_TIMETABLE_WAIT_TIME_TOOLTIP},      // TTV_CHANGE_TIME
	{ WWT_PUSHTXTBTN,   RESIZE_TB,      14,   110,   219,   118,   129, STR_CLEAR_TIME,             STR_TIMETABLE_CLEAR_TIME_TOOLTIP},     // TTV_CLEAR_TIME
	{ WWT_PUSHTXTBTN,   RESIZE_TB,      14,   220,   337,   118,   129, STR_RESET_LATENESS,         STR_TIMETABLE_RESET_LATENESS_TOOLTIP}, // TTV_RESET_LATENESS
	{ WWT_PUSHTXTBTN,   RESIZE_TB,      14,   338,   387,   118,   129, STR_TIMETABLE_AUTOFILL,     STR_TIMETABLE_AUTOFILL_TOOLTIP},       // TTV_AUTOFILL

	{      WWT_PANEL,   RESIZE_RTB,     14,   388,   387,   118,   129, STR_NULL,                   STR_NULL},                             // TTV_EMPTY
	{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   388,   399,   118,   129, STR_NULL,                   STR_RESIZE_BUTTON},                    // TTV_RESIZE

	{    WIDGETS_END }
};

static const WindowDesc _timetable_desc = {
	WDP_AUTO, WDP_AUTO, 400, 130, 400, 130,
	WC_VEHICLE_TIMETABLE, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_timetable_widgets,
	TimetableWndProc
};

void ShowTimetableWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront(&_timetable_desc, v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		w->vscroll.cap = 8;
		w->resize.step_height = 10;
		WP(w, timetable_d).sel = -1;
	}
}
