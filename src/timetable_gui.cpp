/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "date_func.h"

#include "table/strings.h"

enum TimetableViewWindowWidgets {
	TTV_CAPTION,
	TTV_ORDER_VIEW,
	TTV_TIMETABLE_PANEL,
	TTV_FAKE_SCROLLBAR,               ///< So the timetable panel 'sees' the scrollbar too
	TTV_ARRIVAL_DEPARTURE_PANEL,      ///< Panel with the expected/scheduled arrivals
	TTV_SCROLLBAR,
	TTV_SUMMARY_PANEL,
	TTV_CHANGE_TIME,
	TTV_CLEAR_TIME,
	TTV_RESET_LATENESS,
	TTV_AUTOFILL,
	TTV_EXPECTED,                    ///< Toggle between expected and scheduled arrivals
	TTV_ARRIVAL_DEPARTURE_SELECTION, ///< Disable/hide the arrival departure panel
	TTV_EXPECTED_SELECTION,          ///< Disable/hide the expected selection button
};

/** Container for the arrival/departure dates of a vehicle */
struct TimetableArrivalDeparture {
	Ticks arrival;   ///< The arrival time
	Ticks departure; ///< The departure time
};

/**
 * Set the timetable parameters in the format as described by the setting.
 * @param param1 the first DParam to fill
 * @param param2 the second DParam to fill
 * @param ticks  the number of ticks to 'draw'
 */
void SetTimetableParams(int param1, int param2, Ticks ticks)
{
	if (_settings_client.gui.timetable_in_ticks) {
		SetDParam(param1, STR_TIMETABLE_TICKS);
		SetDParam(param2, ticks);
	} else {
		SetDParam(param1, STR_TIMETABLE_DAYS);
		SetDParam(param2, ticks / DAY_TICKS);
	}
}

/**
 * Sets the arrival or departure string and parameters.
 * @param param1 the first DParam to fill
 * @param param2 the second DParam to fill
 * @param ticks  the number of ticks to 'draw'
 */
static void SetArrivalDepartParams(int param1, int param2, Ticks ticks)
{
	SetDParam(param1, STR_JUST_DATE_TINY);
	SetDParam(param2, _date + (ticks / DAY_TICKS));
}

/**
 * Check whether it is possible to determine how long the order takes.
 * @param order the order to check.
 * @param travelling whether we are interested in the travel or the wait part.
 * @return true if the travel/wait time can be used.
 */
static bool CanDetermineTimeTaken(const Order *order, bool travelling)
{
	/* Current order is conditional */
	if (order->IsType(OT_CONDITIONAL)) return false;
	/* No travel time and we have not already finished travelling */
	if (travelling && order->travel_time == 0) return false;
	/* No wait time but we are loading at this timetabled station */
	if (!travelling && order->wait_time == 0 && order->IsType(OT_GOTO_STATION) && !(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) return false;

	return true;
}


/**
 * Fill the table with arrivals and departures
 * @param v Vehicle which must have at least 2 orders.
 * @param start order index to start at
 * @param travelling Are we still in the travelling part of the start order
 * @param table Fill in arrival and departures including intermediate orders
 * @param offset Add this value to result and all arrivals and departures
 */
static void FillTimetableArrivalDepartureTable(const Vehicle *v, VehicleOrderID start, bool travelling, TimetableArrivalDeparture *table, Ticks offset)
{
	assert(table != NULL);
	assert(v->GetNumOrders() >= 2);
	assert(start < v->GetNumOrders());

	Ticks sum = offset;
	VehicleOrderID i = start;
	const Order *order = v->GetOrder(i);

	/* Pre-initialize with unknown time */
	for (int i = 0; i < v->GetNumOrders(); ++i) {
		table[i].arrival = table[i].departure = INVALID_TICKS;
	}

	/* Cyclically loop over all orders until we reach the current one again.
	 * As we may start at the current order, do a post-checking loop */
	do {
		if (travelling || i != start) {
			if (!CanDetermineTimeTaken(order, true)) return;
			sum += order->travel_time;
			table[i].arrival = sum;
		}

		if (!CanDetermineTimeTaken(order, false)) return;
		sum += order->wait_time;
		table[i].departure = sum;

		++i;
		order = order->next;
		if (i >= v->GetNumOrders()) {
			i = 0;
			assert(order == NULL);
			order = v->orders.list->GetFirstOrder();
		}
	} while (i != start);

	/* When loading at a scheduled station we still have to treat the
	 * travelling part of the first order. */
	if (!travelling) {
		if (!CanDetermineTimeTaken(order, true)) return;
		sum += order->travel_time;
		table[i].arrival = sum;
	}
}


struct TimetableWindow : Window {
	int sel_index;
	const Vehicle *vehicle; ///< Vehicle monitored by the window.
	bool show_expected;     ///< Whether we show expected arrival or scheduled

	TimetableWindow(const WindowDesc *desc, WindowNumber window_number) :
			Window(),
			sel_index(-1),
			vehicle(Vehicle::Get(window_number)),
			show_expected(true)
	{
		this->CreateNestedTree(desc);
		this->UpdateSelectionStates();
		this->FinishInitNested(desc, window_number);

		this->owner = this->vehicle->owner;
	}

	/**
	 * Build the arrival-departure list for a given vehicle
	 * @param v the vehicle to make the list for
	 * @param table the table to fill
	 * @return if next arrival will be early
	 */
	static bool BuildArrivalDepartureList(const Vehicle *v, TimetableArrivalDeparture *table)
	{
		assert(HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED));

		bool travelling = (!v->current_order.IsType(OT_LOADING) || v->current_order.GetNonStopType() == ONSF_STOP_EVERYWHERE);
		Ticks start_time = _date_fract - v->current_order_time;

		FillTimetableArrivalDepartureTable(v, v->cur_order_index % v->GetNumOrders(), travelling, table, start_time);

		return (travelling && v->lateness_counter < 0);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case TTV_ARRIVAL_DEPARTURE_PANEL:
				SetDParam(0, STR_JUST_DATE_TINY);
				SetDParam(1, MAX_YEAR * DAYS_IN_YEAR);
				size->width = GetStringBoundingBox(STR_TIMETABLE_ARRIVAL).width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				/* fall through */
			case TTV_ARRIVAL_DEPARTURE_SELECTION:
			case TTV_TIMETABLE_PANEL:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 8 * resize->height + WD_FRAMERECT_BOTTOM;
				break;

			case TTV_SUMMARY_PANEL:
				size->height = WD_FRAMERECT_TOP + 2 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				break;
		}
	}

	int GetOrderFromTimetableWndPt(int y, const Vehicle *v)
	{
		int sel = (y - this->GetWidget<NWidgetBase>(TTV_TIMETABLE_PANEL)->pos_y - WD_FRAMERECT_TOP) / FONT_HEIGHT_NORMAL;

		if ((uint)sel >= this->vscroll.GetCapacity()) return INVALID_ORDER;

		sel += this->vscroll.GetPosition();

		return (sel < v->GetNumOrders() * 2 && sel >= 0) ? sel : INVALID_ORDER;
	}

	virtual void OnInvalidateData(int data)
	{
		switch (data) {
			case 0:
				/* Autoreplace replaced the vehicle */
				this->vehicle = Vehicle::Get(this->window_number);
				break;

			case -1:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->sel_index == -1) break;

				this->DeleteChildWindows();
				this->sel_index = -1;
				break;

			case -2:
				this->UpdateSelectionStates();
				this->ReInit();
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

		this->vscroll.SetCount(v->GetNumOrders() * 2);

		if (v->owner == _local_company) {
			bool disable = true;
			if (selected != -1) {
				const Order *order = v->GetOrder(((selected + 1) / 2) % v->GetNumOrders());
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

		this->DrawWidgets();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case TTV_CAPTION: SetDParam(0, this->vehicle->index); break;
			case TTV_EXPECTED: SetDParam(0, this->show_expected ? STR_TIMETABLE_EXPECTED : STR_TIMETABLE_SCHEDULED); break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		const Vehicle *v = this->vehicle;
		int selected = this->sel_index;

		switch (widget) {
			case TTV_TIMETABLE_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;
				int i = this->vscroll.GetPosition();
				VehicleOrderID order_id = (i + 1) / 2;
				bool final_order = false;

				const Order *order = v->GetOrder(order_id);
				while (order != NULL) {
					/* Don't draw anything if it extends past the end of the window. */
					if (!this->vscroll.IsVisible(i)) break;

					if (i % 2 == 0) {
						DrawOrderString(v, order, order_id, y, i == selected, true, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT);

						order_id++;

						if (order_id >= v->GetNumOrders()) {
							order = v->GetOrder(0);
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

						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, string, (i == selected) ? TC_WHITE : TC_BLACK);

						if (final_order) break;
					}

					i++;
					y += FONT_HEIGHT_NORMAL;
				}
				break;
			}

			case TTV_ARRIVAL_DEPARTURE_PANEL: {
				/* Arrival and departure times are handled in an all-or-nothing approach,
				 * i.e. are only shown if we can calculate all times.
				 * Excluding order lists with only one order makes some things easier.
				 */
				Ticks total_time = v->orders.list != NULL ? v->orders.list->GetTimetableDurationIncomplete() : 0;
				if (total_time <= 0 || v->GetNumOrders() <= 1 || !HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) break;

				TimetableArrivalDeparture *arr_dep = AllocaM(TimetableArrivalDeparture, v->GetNumOrders());
				const VehicleOrderID cur_order = v->cur_order_index % v->GetNumOrders();

				VehicleOrderID earlyID = BuildArrivalDepartureList(v, arr_dep) ? cur_order : (VehicleOrderID)INVALID_VEH_ORDER_ID;

				int y = r.top + WD_FRAMERECT_TOP;

				Ticks offset;
				StringID str_offset;
				if (this->show_expected && v->lateness_counter > DAY_TICKS) {
					offset = 0;
					str_offset = STR_TIMETABLE_ARRIVAL_LATE - STR_TIMETABLE_ARRIVAL;
				} else {
					offset = -v->lateness_counter;
					str_offset = 0;
				}

				for (int i = this->vscroll.GetPosition(); i / 2 < v->GetNumOrders(); ++i) { // note: i is also incremented in the loop
					/* Don't draw anything if it extends past the end of the window. */
					if (!this->vscroll.IsVisible(i)) break;

					if (i % 2 == 0) {
						if (arr_dep[i / 2].arrival != INVALID_TICKS) {
							if (this->show_expected && i / 2 == earlyID) {
								SetArrivalDepartParams(0, 1, arr_dep[i / 2].arrival);
								DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_ARRIVAL_EARLY, i == selected ? TC_WHITE : TC_BLACK);
							} else {
								SetArrivalDepartParams(0, 1, arr_dep[i / 2].arrival + offset);
								DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_ARRIVAL + str_offset, i == selected ? TC_WHITE : TC_BLACK);
							}
						}
					} else {
						if (arr_dep[i / 2].departure != INVALID_TICKS) {
							SetArrivalDepartParams(0, 1, arr_dep[i/2].departure + offset);
							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_DEPARTURE + str_offset, i == selected ? TC_WHITE : TC_BLACK);
						}
					}
					y += FONT_HEIGHT_NORMAL;
				}
			} break;

			case TTV_SUMMARY_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;

				Ticks total_time = v->orders.list != NULL ? v->orders.list->GetTimetableDurationIncomplete() : 0;
				if (total_time != 0) {
					SetTimetableParams(0, 1, total_time);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, v->orders.list->IsCompleteTimetable() ? STR_TIMETABLE_TOTAL_TIME : STR_TIMETABLE_TOTAL_TIME_INCOMPLETE);
				}
				y += FONT_HEIGHT_NORMAL;

				if (v->lateness_counter == 0 || (!_settings_client.gui.timetable_in_ticks && v->lateness_counter / DAY_TICKS == 0)) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_STATUS_ON_TIME);
				} else {
					SetTimetableParams(0, 1, abs(v->lateness_counter));
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, v->lateness_counter < 0 ? STR_TIMETABLE_STATUS_EARLY : STR_TIMETABLE_STATUS_LATE);
				}
				break;
			}
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

				const Order *order = v->GetOrder(real);
				StringID current = STR_EMPTY;

				if (order != NULL) {
					uint time = (selected % 2 == 1) ? order->travel_time : order->wait_time;
					if (!_settings_client.gui.timetable_in_ticks) time /= DAY_TICKS;

					if (time != 0) {
						SetDParam(0, time);
						current = STR_JUST_INT;
					}
				}

				ShowQueryString(current, STR_TIMETABLE_CHANGE_TIME, 31, 150, this, CS_NUMERAL, QSF_NONE);
			} break;

			case TTV_CLEAR_TIME: { // Clear waiting time button.
				uint32 p1 = PackTimetableArgs(v, this->sel_index);
				DoCommandP(0, p1, 0, CMD_CHANGE_TIMETABLE | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
			} break;

			case TTV_RESET_LATENESS: // Reset the vehicle's late counter.
				DoCommandP(0, v->index, 0, CMD_SET_VEHICLE_ON_TIME | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
				break;

			case TTV_AUTOFILL: { // Autofill the timetable.
				uint32 p2 = 0;
				if (!HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE)) SetBit(p2, 0);
				if (_ctrl_pressed) SetBit(p2, 1);
				DoCommandP(0, v->index, p2, CMD_AUTOFILL_TIMETABLE | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
			} break;

			case TTV_EXPECTED:
				this->show_expected = !this->show_expected;
				break;
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

		DoCommandP(0, p1, p2, CMD_CHANGE_TIMETABLE | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
	}

	virtual void OnResize()
	{
		/* Update the scroll bar */
		this->vscroll.SetCapacity((this->GetWidget<NWidgetBase>(TTV_TIMETABLE_PANEL)->current_y - WD_FRAMERECT_TOP - WD_FRAMERECT_BOTTOM) / this->resize.step_height);
	}

	/**
	 * Update the selection state of the arrival/departure data
	 */
	void UpdateSelectionStates()
	{
		int plane = _settings_client.gui.timetable_arrival_departure ? 0 : STACKED_SELECTION_ZERO_SIZE;
		this->GetWidget<NWidgetStacked>(TTV_ARRIVAL_DEPARTURE_SELECTION)->SetDisplayedPlane(plane);
		this->GetWidget<NWidgetStacked>(TTV_EXPECTED_SELECTION)->SetDisplayedPlane(plane);
	}
};

static const NWidgetPart _nested_timetable_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, TTV_CAPTION), SetDataTip(STR_TIMETABLE_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, TTV_ORDER_VIEW), SetMinimalSize(61, 14), SetDataTip( STR_TIMETABLE_ORDER_VIEW, STR_TIMETABLE_ORDER_VIEW_TOOLTIP),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, TTV_TIMETABLE_PANEL), SetMinimalSize(388, 82), SetResize(1, 10), SetDataTip(STR_NULL, STR_TIMETABLE_TOOLTIP), EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, TTV_FAKE_SCROLLBAR), SetMinimalSize(0, 0), // Hack so the timetable panel can 'use' the scrollbar too
		NWidget(NWID_SELECTION, INVALID_COLOUR, TTV_ARRIVAL_DEPARTURE_SELECTION),
			NWidget(WWT_PANEL, COLOUR_GREY, TTV_ARRIVAL_DEPARTURE_PANEL), SetMinimalSize(110, 0), SetFill(0, 1), SetDataTip(STR_NULL, STR_TIMETABLE_TOOLTIP), EndContainer(),
		EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, TTV_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, TTV_SUMMARY_PANEL), SetMinimalSize(400, 22), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, TTV_CHANGE_TIME), SetMinimalSize(110, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_TIMETABLE_CHANGE_TIME, STR_TIMETABLE_WAIT_TIME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, TTV_CLEAR_TIME), SetMinimalSize(110, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_TIMETABLE_CLEAR_TIME, STR_TIMETABLE_CLEAR_TIME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, TTV_RESET_LATENESS), SetMinimalSize(118, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_TIMETABLE_RESET_LATENESS, STR_TIMETABLE_RESET_LATENESS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, TTV_AUTOFILL), SetMinimalSize(50, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_TIMETABLE_AUTOFILL, STR_TIMETABLE_AUTOFILL_TOOLTIP),
		NWidget(NWID_SELECTION, INVALID_COLOUR, TTV_EXPECTED_SELECTION),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, TTV_EXPECTED), SetMinimalSize(50, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_BLACK_STRING, STR_TIMETABLE_EXPECTED_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX,COLOUR_GREY),
	EndContainer(),
};

static const WindowDesc _timetable_desc(
	WDP_AUTO, WDP_AUTO, 400, 130,
	WC_VEHICLE_TIMETABLE, WC_VEHICLE_VIEW,
	WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_nested_timetable_widgets, lengthof(_nested_timetable_widgets)
);

void ShowTimetableWindow(const Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_DETAILS, v->index, false);
	DeleteWindowById(WC_VEHICLE_ORDERS, v->index, false);
	AllocateWindowDescFront<TimetableWindow>(&_timetable_desc, v->index);
}
