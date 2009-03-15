/* $Id$ */

/** @file timetable_gui.cpp GUI for time tabling. */

#include "stdafx.h"
#include "command_func.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "strings_func.h"
#include "vehicle_base.h"
#include "string_func.h"
#include "gfx_func.h"
#include "company_func.h"
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

void SetTimetableParams(int param1, int param2, uint32 time)
{
	if (_settings_client.gui.timetable_in_ticks) {
		SetDParam(param1, STR_TIMETABLE_TICKS);
		SetDParam(param2, time);
	} else {
		SetDParam(param1, STR_TIMETABLE_DAYS);
		SetDParam(param2, time / DAY_TICKS);
	}
}

struct TimetableWindow : Window {
	int sel_index;
	const Vehicle *vehicle;

	TimetableWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->vehicle = GetVehicle(window_number);
		this->owner = this->vehicle->owner;
		this->vscroll.cap = 8;
		this->resize.step_height = 10;
		this->sel_index = -1;

		this->FindWindowPlacementAndResize(desc);
	}

	int GetOrderFromTimetableWndPt(int y, const Vehicle *v)
	{
		/*
		 * Calculation description:
		 * 15 = 14 (this->widget[TTV_ORDER_VIEW].top) + 1 (frame-line)
		 * 10 = order text hight
		 */
		int sel = (y - 15) / 10;

		if ((uint)sel >= this->vscroll.cap) return INVALID_ORDER;

		sel += this->vscroll.pos;

		return (sel < v->GetNumOrders() * 2 && sel >= 0) ? sel : INVALID_ORDER;
	}

	virtual void OnInvalidateData(int data)
	{
		switch (data) {
			case 0:
				/* Autoreplace replaced the vehicle */
				this->vehicle = GetVehicle(this->window_number);
				break;

			case -1:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->sel_index == -1) break;

				this->DeleteChildWindows();
				this->sel_index = -1;
				break;

			default: {
				/* Moving an order. If one of these is INVALID_VEH_ORDER_ID, then
				 * the order is being created / removed */
				if (this->sel_index == -1) break;

				VehicleOrderID from = GB(data, 0, 8);
				VehicleOrderID to   = GB(data, 8, 8);

				if (from == to) break; // no need to change anything

				/* if from == INVALID_VEH_ORDER_ID, one order was added; if to == INVALID_VEH_ORDER_ID, one order was removed */
				uint old_num_orders = this->vehicle->GetNumOrders() - (uint)(from == INVALID_VEH_ORDER_ID) + (uint)(to == INVALID_VEH_ORDER_ID);

				VehicleOrderID selected_order = (this->sel_index + 1) / 2;
				if (selected_order == old_num_orders) selected_order = 0; // when last travel time is selected, it belongs to order 0

				bool travel = HasBit(this->sel_index, 0);

				if (from != selected_order) {
					/* Moving from preceeding order? */
					selected_order -= (int)(from <= selected_order);
					/* Moving to   preceeding order? */
					selected_order += (int)(to   <= selected_order);
				} else {
					/* Now we are modifying the selected order */
					if (to == INVALID_VEH_ORDER_ID) {
						/* Deleting selected order */
						this->DeleteChildWindows();
						this->sel_index = -1;
						break;
					} else {
						/* Moving selected order */
						selected_order = to;
					}
				}

				/* recompute new sel_index */
				this->sel_index = 2 * selected_order - (int)travel;
				/* travel time of first order needs special handling */
				if (this->sel_index == -1) this->sel_index = this->vehicle->GetNumOrders() * 2 - 1;
			} break;
		}
	}


	virtual void OnPaint()
	{
		const Vehicle *v = this->vehicle;
		int selected = this->sel_index;

		SetVScrollCount(this, v->GetNumOrders() * 2);

		if (v->owner == _local_company) {
			bool disable = true;
			if (selected != -1) {
				const Order *order = GetVehicleOrder(v, ((selected + 1) / 2) % v->GetNumOrders());
				if (selected % 2 == 1) {
					disable = order != NULL && order->IsType(OT_CONDITIONAL);
				} else {
					disable = order == NULL || ((!order->IsType(OT_GOTO_STATION) || (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) && !order->IsType(OT_CONDITIONAL));
				}
			}

			this->SetWidgetDisabledState(TTV_CHANGE_TIME, disable);
			this->SetWidgetDisabledState(TTV_CLEAR_TIME, disable);

			this->EnableWidget(TTV_RESET_LATENESS);
			this->EnableWidget(TTV_AUTOFILL);
		} else {
			this->DisableWidget(TTV_CHANGE_TIME);
			this->DisableWidget(TTV_CLEAR_TIME);
			this->DisableWidget(TTV_RESET_LATENESS);
			this->DisableWidget(TTV_AUTOFILL);
		}

		this->SetWidgetLoweredState(TTV_AUTOFILL, HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE));

		SetDParam(0, v->index);
		this->DrawWidgets();

		int y = 15;
		int i = this->vscroll.pos;
		VehicleOrderID order_id = (i + 1) / 2;
		bool final_order = false;

		const Order *order = GetVehicleOrder(v, order_id);

		while (order != NULL) {
			/* Don't draw anything if it extends past the end of the window. */
			if (i - this->vscroll.pos >= this->vscroll.cap) break;

			if (i % 2 == 0) {
				DrawOrderString(v, order, order_id, y, i == selected, true, this->widget[TTV_TIMETABLE_PANEL].right - 4);

				order_id++;

				if (order_id >= v->GetNumOrders()) {
					order = GetVehicleOrder(v, 0);
					final_order = true;
				} else {
					order = order->next;
				}
			} else {
				StringID string;

				if (order->IsType(OT_CONDITIONAL)) {
					string = STR_TIMETABLE_NO_TRAVEL;
				} else if (order->travel_time == 0) {
					string = STR_TIMETABLE_TRAVEL_NOT_TIMETABLED;
				} else {
					SetTimetableParams(0, 1, order->travel_time);
					string = STR_TIMETABLE_TRAVEL_FOR;
				}

				DrawStringTruncated(2, y, string, (i == selected) ? TC_WHITE : TC_BLACK, this->widget[TTV_TIMETABLE_PANEL].right - 4);

				if (final_order) break;
			}

			i++;
			y += 10;
		}

		y = this->widget[TTV_SUMMARY_PANEL].top + 1;

		{
			uint total_time = 0;
			bool complete = true;

			for (const Order *order = GetVehicleOrder(v, 0); order != NULL; order = order->next) {
				total_time += order->travel_time + order->wait_time;
				if (order->travel_time == 0 && !order->IsType(OT_CONDITIONAL)) complete = false;
				if (order->wait_time == 0 && order->IsType(OT_GOTO_STATION) && !(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) complete = false;
			}

			if (total_time != 0) {
				SetTimetableParams(0, 1, total_time);
				DrawString(2, y, complete ? STR_TIMETABLE_TOTAL_TIME : STR_TIMETABLE_TOTAL_TIME_INCOMPLETE, TC_BLACK);
			}
		}
		y += 10;

		if (v->lateness_counter == 0 || (!_settings_client.gui.timetable_in_ticks && v->lateness_counter / DAY_TICKS == 0)) {
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

		if (order_number >= v->GetNumOrders()) order_number = 0;

		return v->index | (order_number << 16) | (is_journey << 24);
	}

	virtual void OnClick(Point pt, int widget)
	{
		const Vehicle *v = this->vehicle;

		switch (widget) {
			case TTV_ORDER_VIEW: // Order view button
				ShowOrdersWindow(v);
				break;

			case TTV_TIMETABLE_PANEL: { // Main panel.
				int selected = GetOrderFromTimetableWndPt(pt.y, v);

				this->DeleteChildWindows();
				this->sel_index = (selected == INVALID_ORDER || selected == this->sel_index) ? -1 : selected;
			} break;

			case TTV_CHANGE_TIME: { // "Wait For" button.
				int selected = this->sel_index;
				VehicleOrderID real = (selected + 1) / 2;

				if (real >= v->GetNumOrders()) real = 0;

				const Order *order = GetVehicleOrder(v, real);
				StringID current = STR_EMPTY;

				if (order != NULL) {
					uint time = (selected % 2 == 1) ? order->travel_time : order->wait_time;
					if (!_settings_client.gui.timetable_in_ticks) time /= DAY_TICKS;

					if (time != 0) {
						SetDParam(0, time);
						current = STR_CONFIG_SETTING_INT32;
					}
				}

				ShowQueryString(current, STR_TIMETABLE_CHANGE_TIME, 31, 150, this, CS_NUMERAL, QSF_NONE);
			} break;

			case TTV_CLEAR_TIME: { // Clear waiting time button.
				uint32 p1 = PackTimetableArgs(v, this->sel_index);
				DoCommandP(0, p1, 0, CMD_CHANGE_TIMETABLE | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
			} break;

			case TTV_RESET_LATENESS: // Reset the vehicle's late counter.
				DoCommandP(0, v->index, 0, CMD_SET_VEHICLE_ON_TIME | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
				break;

			case TTV_AUTOFILL: { // Autofill the timetable.
				uint32 p2 = 0;
				if (!HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE)) SetBit(p2, 0);
				if (_ctrl_pressed) SetBit(p2, 1);
				DoCommandP(0, v->index, p2, CMD_AUTOFILL_TIMETABLE | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
			} break;
		}

		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		const Vehicle *v = this->vehicle;

		uint32 p1 = PackTimetableArgs(v, this->sel_index);

		uint64 time = StrEmpty(str) ? 0 : strtoul(str, NULL, 10);
		if (!_settings_client.gui.timetable_in_ticks) time *= DAY_TICKS;

		uint32 p2 = minu(time, UINT16_MAX);

		DoCommandP(0, p1, p2, CMD_CHANGE_TIMETABLE | CMD_MSG(STR_CAN_T_TIMETABLE_VEHICLE));
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		/* Update the scroll + matrix */
		this->vscroll.cap = (this->widget[TTV_TIMETABLE_PANEL].bottom - this->widget[TTV_TIMETABLE_PANEL].top) / 10;
	}
};

static const Widget _timetable_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,   COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},                // TTV_WIDGET_CLOSEBOX
	{    WWT_CAPTION,   RESIZE_RIGHT,  COLOUR_GREY,    11,   326,     0,    13, STR_TIMETABLE_TITLE,        STR_018C_WINDOW_TITLE_DRAG_THIS},      // TTV_CAPTION
	{ WWT_PUSHTXTBTN,   RESIZE_LR,     COLOUR_GREY,   327,   387,     0,    13, STR_ORDER_VIEW,             STR_ORDER_VIEW_TOOLTIP},               // TTV_ORDER_VIEW
	{  WWT_STICKYBOX,   RESIZE_LR,     COLOUR_GREY,   388,   399,     0,    13, STR_NULL,                   STR_STICKY_BUTTON},                    // TTV_STICKY

	{      WWT_PANEL,   RESIZE_RB,     COLOUR_GREY,     0,   387,    14,    95, STR_NULL,                   STR_TIMETABLE_TOOLTIP},                // TTV_TIMETABLE_PANEL
	{  WWT_SCROLLBAR,   RESIZE_LRB,    COLOUR_GREY,   388,   399,    14,    95, STR_NULL,                   STR_0190_SCROLL_BAR_SCROLLS_LIST},     // TTV_SCROLLBAR

	{      WWT_PANEL,   RESIZE_RTB,    COLOUR_GREY,     0,   399,    96,   117, STR_NULL,                   STR_NULL},                             // TTV_SUMMARY_PANEL

	{ WWT_PUSHTXTBTN,   RESIZE_TB,     COLOUR_GREY,     0,   109,   118,   129, STR_TIMETABLE_CHANGE_TIME,  STR_TIMETABLE_WAIT_TIME_TOOLTIP},      // TTV_CHANGE_TIME
	{ WWT_PUSHTXTBTN,   RESIZE_TB,     COLOUR_GREY,   110,   219,   118,   129, STR_CLEAR_TIME,             STR_TIMETABLE_CLEAR_TIME_TOOLTIP},     // TTV_CLEAR_TIME
	{ WWT_PUSHTXTBTN,   RESIZE_TB,     COLOUR_GREY,   220,   337,   118,   129, STR_RESET_LATENESS,         STR_TIMETABLE_RESET_LATENESS_TOOLTIP}, // TTV_RESET_LATENESS
	{ WWT_PUSHTXTBTN,   RESIZE_TB,     COLOUR_GREY,   338,   387,   118,   129, STR_TIMETABLE_AUTOFILL,     STR_TIMETABLE_AUTOFILL_TOOLTIP},       // TTV_AUTOFILL

	{      WWT_PANEL,   RESIZE_RTB,    COLOUR_GREY,   388,   387,   118,   129, STR_NULL,                   STR_NULL},                             // TTV_EMPTY
	{  WWT_RESIZEBOX,   RESIZE_LRTB,   COLOUR_GREY,   388,   399,   118,   129, STR_NULL,                   STR_RESIZE_BUTTON},                    // TTV_RESIZE

	{    WIDGETS_END }
};

static const WindowDesc _timetable_desc(
	WDP_AUTO, WDP_AUTO, 400, 130, 400, 130,
	WC_VEHICLE_TIMETABLE, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE | WDF_CONSTRUCTION,
	_timetable_widgets
);

void ShowTimetableWindow(const Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_DETAILS, v->index, false);
	DeleteWindowById(WC_VEHICLE_ORDERS, v->index, false);
	AllocateWindowDescFront<TimetableWindow>(&_timetable_desc, v->index);
}
