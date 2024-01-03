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
#include "timer/timer.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_window.h"
#include "date_gui.h"
#include "vehicle_gui.h"
#include "settings_type.h"
#include "timetable_cmd.h"
#include "timetable.h"

#include "widgets/timetable_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

/** Container for the arrival/departure dates of a vehicle */
struct TimetableArrivalDeparture {
	TimerGameTick::Ticks arrival;   ///< The arrival time
	TimerGameTick::Ticks departure; ///< The departure time
};

/**
 * Set the timetable parameters in the format as described by the setting.
 * @param param1 the first DParam to fill
 * @param param2 the second DParam to fill
 * @param ticks  the number of ticks to 'draw'
 */
void SetTimetableParams(int param1, int param2, TimerGameTick::Ticks ticks)
{
	switch (_settings_client.gui.timetable_mode) {
		case TimetableMode::Days:
			SetDParam(param1, STR_UNITS_DAYS);
			SetDParam(param2, ticks / Ticks::DAY_TICKS);
			break;
		case TimetableMode::Seconds:
			SetDParam(param1, STR_UNITS_SECONDS);
			SetDParam(param2, ticks / Ticks::TICKS_PER_SECOND);
			break;
		case TimetableMode::Ticks:
			SetDParam(param1, STR_UNITS_TICKS);
			SetDParam(param2, ticks);
			break;
		default:
			NOT_REACHED();
	}
}

/**
 * Get the number of ticks in the current timetable display unit.
 * @return The number of ticks per day, second, or tick, to match the timetable display.
 */
static inline TimerGameTick::Ticks TicksPerTimetableUnit()
{
	switch (_settings_client.gui.timetable_mode) {
		case TimetableMode::Days:
			return Ticks::DAY_TICKS;
		case TimetableMode::Seconds:
			return Ticks::TICKS_PER_SECOND;
		case TimetableMode::Ticks:
			return 1;
		default:
			NOT_REACHED();
	}
}

/**
 * Determine if a vehicle should be shown as late, depending on the timetable display setting.
 * @param v The vehicle in question.
 * @param round_to_day When using ticks, if we should round up to the nearest day.
 * @return True if the vehicle is later than the threshold.
 */
bool VehicleIsAboveLatenessThreshold(const Vehicle *v, bool round_to_day)
{
	switch (_settings_client.gui.timetable_mode) {
		case TimetableMode::Days:
			return v->lateness_counter > Ticks::DAY_TICKS;
		case TimetableMode::Seconds:
			return v->lateness_counter > Ticks::TICKS_PER_SECOND;
		case TimetableMode::Ticks:
			return v->lateness_counter > (round_to_day ? Ticks::DAY_TICKS : 0);
		default:
			NOT_REACHED();
	}
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
	if (order->IsType(OT_CONDITIONAL) || order->IsType(OT_IMPLICIT)) return false;
	/* No travel time and we have not already finished travelling */
	if (travelling && !order->IsTravelTimetabled()) return false;
	/* No wait time but we are loading at this timetabled station */
	if (!travelling && !order->IsWaitTimetabled() && order->IsType(OT_GOTO_STATION) &&
			!(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) {
		return false;
	}

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
static void FillTimetableArrivalDepartureTable(const Vehicle *v, VehicleOrderID start, bool travelling, std::vector<TimetableArrivalDeparture> &table, TimerGameTick::Ticks offset)
{
	assert(!table.empty());
	assert(v->GetNumOrders() >= 2);
	assert(start < v->GetNumOrders());

	/* Pre-initialize with unknown time */
	for (int i = 0; i < v->GetNumOrders(); ++i) {
		table[i].arrival = table[i].departure = Ticks::INVALID_TICKS;
	}

	TimerGameTick::Ticks sum = offset;
	VehicleOrderID i = start;
	const Order *order = v->GetOrder(i);

	/* Cyclically loop over all orders until we reach the current one again.
	 * As we may start at the current order, do a post-checking loop */
	do {
		/* Automatic orders don't influence the overall timetable;
		 * they just add some untimetabled entries, but the time till
		 * the next non-implicit order can still be known. */
		if (!order->IsType(OT_IMPLICIT)) {
			if (travelling || i != start) {
				if (!CanDetermineTimeTaken(order, true)) return;
				sum += order->GetTimetabledTravel();
				table[i].arrival = sum;
			}

			if (!CanDetermineTimeTaken(order, false)) return;
			sum += order->GetTimetabledWait();
			table[i].departure = sum;
		}

		++i;
		order = order->next;
		if (i >= v->GetNumOrders()) {
			i = 0;
			assert(order == nullptr);
			order = v->orders->GetFirstOrder();
		}
	} while (i != start);

	/* When loading at a scheduled station we still have to treat the
	 * travelling part of the first order. */
	if (!travelling) {
		if (!CanDetermineTimeTaken(order, true)) return;
		sum += order->GetTimetabledTravel();
		table[i].arrival = sum;
	}
}


/**
 * Callback for when a time has been chosen to start the time table
 * @param w the window related to the setting of the date
 * @param date the actually chosen date
 */
static void ChangeTimetableStartCallback(const Window *w, TimerGameCalendar::Date date, void *data)
{
	Command<CMD_SET_TIMETABLE_START>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, (VehicleID)w->window_number, reinterpret_cast<std::uintptr_t>(data) != 0, GetStartTickFromDate(date));
}


struct TimetableWindow : Window {
	int sel_index;
	VehicleTimetableWidgets query_widget; ///< Which button was clicked to open the query text input?
	const Vehicle *vehicle;    ///< Vehicle monitored by the window.
	bool show_expected;        ///< Whether we show expected arrival or scheduled.
	Scrollbar *vscroll;        ///< The scrollbar.
	bool set_start_date_all;   ///< Set start date using minutes text entry for all timetable entries (ctrl-click) action.
	bool change_timetable_all; ///< Set wait time or speed for all timetable entries (ctrl-click) action.

	TimetableWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			sel_index(-1),
			vehicle(Vehicle::Get(window_number)),
			show_expected(true)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_VT_SCROLLBAR);
		this->UpdateSelectionStates();
		this->FinishInitNested(window_number);

		this->owner = this->vehicle->owner;
	}

	/**
	 * Build the arrival-departure list for a given vehicle
	 * @param v the vehicle to make the list for
	 * @param table the table to fill
	 * @return if next arrival will be early
	 */
	static bool BuildArrivalDepartureList(const Vehicle *v, std::vector<TimetableArrivalDeparture> &table)
	{
		assert(HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED));

		bool travelling = (!v->current_order.IsType(OT_LOADING) || v->current_order.GetNonStopType() == ONSF_STOP_EVERYWHERE);
		TimerGameTick::Ticks start_time = -v->current_order_time;

		/* If arrival and departure times are in days, compensate for the current date_fract. */
		if (_settings_client.gui.timetable_mode != TimetableMode::Seconds) start_time += TimerGameCalendar::date_fract;

		FillTimetableArrivalDepartureTable(v, v->cur_real_order_index % v->GetNumOrders(), travelling, table, start_time);

		return (travelling && v->lateness_counter < 0);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_VT_ARRIVAL_DEPARTURE_PANEL:
				/* We handle this differently depending on the timetable mode. */
				if (_settings_client.gui.timetable_mode == TimetableMode::Seconds) {
					/* A five-digit number would fit a timetable lasting 2.7 real-world hours, which should be plenty. */
					SetDParamMaxDigits(1, 4, FS_SMALL);
					size->width = std::max(GetStringBoundingBox(STR_TIMETABLE_ARRIVAL_SECONDS_IN_FUTURE).width, GetStringBoundingBox(STR_TIMETABLE_DEPARTURE_SECONDS_IN_FUTURE).width) + WidgetDimensions::scaled.hsep_wide + padding.width;
				} else {
					SetDParamMaxValue(1, TimerGameCalendar::DateAtStartOfYear(CalendarTime::MAX_YEAR), 0, FS_SMALL);
					size->width = std::max(GetStringBoundingBox(STR_TIMETABLE_ARRIVAL_DATE).width, GetStringBoundingBox(STR_TIMETABLE_DEPARTURE_DATE).width) + WidgetDimensions::scaled.hsep_wide + padding.width;
				}
				FALLTHROUGH;

			case WID_VT_ARRIVAL_DEPARTURE_SELECTION:
			case WID_VT_TIMETABLE_PANEL:
				resize->height = GetCharacterHeight(FS_NORMAL);
				size->height = 8 * resize->height + padding.height;
				break;

			case WID_VT_SUMMARY_PANEL:
				size->height = 2 * GetCharacterHeight(FS_NORMAL) + padding.height;
				break;
		}
	}

	int GetOrderFromTimetableWndPt(int y, [[maybe_unused]] const Vehicle *v)
	{
		int sel = this->vscroll->GetScrolledRowFromWidget(y, this, WID_VT_TIMETABLE_PANEL, WidgetDimensions::scaled.framerect.top);
		if (sel == INT_MAX) return INVALID_ORDER;
		assert(IsInsideBS(sel, 0, v->GetNumOrders() * 2));
		return sel;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		switch (data) {
			case VIWD_AUTOREPLACE:
				/* Autoreplace replaced the vehicle */
				this->vehicle = Vehicle::Get(this->window_number);
				break;

			case VIWD_REMOVE_ALL_ORDERS:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->sel_index == -1) break;

				this->CloseChildWindows();
				this->sel_index = -1;
				break;

			case VIWD_MODIFY_ORDERS:
				if (!gui_scope) break;
				this->UpdateSelectionStates();
				this->ReInit();
				break;

			default: {
				if (gui_scope) break; // only do this once; from command scope

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
					/* Moving from preceding order? */
					selected_order -= (int)(from <= selected_order);
					/* Moving to   preceding order? */
					selected_order += (int)(to   <= selected_order);
				} else {
					/* Now we are modifying the selected order */
					if (to == INVALID_VEH_ORDER_ID) {
						/* Deleting selected order */
						this->CloseChildWindows();
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
				break;
			}
		}
	}


	void OnPaint() override
	{
		const Vehicle *v = this->vehicle;
		int selected = this->sel_index;

		this->vscroll->SetCount(v->GetNumOrders() * 2);

		if (v->owner == _local_company) {
			bool disable = true;
			if (selected != -1) {
				const Order *order = v->GetOrder(((selected + 1) / 2) % v->GetNumOrders());
				if (selected % 2 != 0) {
					disable = order != nullptr && (order->IsType(OT_CONDITIONAL) || order->IsType(OT_IMPLICIT));
				} else {
					disable = order == nullptr || ((!order->IsType(OT_GOTO_STATION) || (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) && !order->IsType(OT_CONDITIONAL));
				}
			}
			bool disable_speed = disable || selected % 2 == 0 || v->type == VEH_AIRCRAFT;

			this->SetWidgetDisabledState(WID_VT_CHANGE_TIME, disable);
			this->SetWidgetDisabledState(WID_VT_CLEAR_TIME, disable);
			this->SetWidgetDisabledState(WID_VT_CHANGE_SPEED, disable_speed);
			this->SetWidgetDisabledState(WID_VT_CLEAR_SPEED, disable_speed);
			this->SetWidgetDisabledState(WID_VT_SHARED_ORDER_LIST, !v->IsOrderListShared());

			this->SetWidgetDisabledState(WID_VT_START_DATE, v->orders == nullptr);
			this->SetWidgetDisabledState(WID_VT_RESET_LATENESS, v->orders == nullptr);
			this->SetWidgetDisabledState(WID_VT_AUTOFILL, v->orders == nullptr);
		} else {
			this->DisableWidget(WID_VT_START_DATE);
			this->DisableWidget(WID_VT_CHANGE_TIME);
			this->DisableWidget(WID_VT_CLEAR_TIME);
			this->DisableWidget(WID_VT_CHANGE_SPEED);
			this->DisableWidget(WID_VT_CLEAR_SPEED);
			this->DisableWidget(WID_VT_RESET_LATENESS);
			this->DisableWidget(WID_VT_AUTOFILL);
			this->DisableWidget(WID_VT_SHARED_ORDER_LIST);
		}

		this->SetWidgetLoweredState(WID_VT_AUTOFILL, HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE));

		this->DrawWidgets();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_VT_CAPTION: SetDParam(0, this->vehicle->index); break;
			case WID_VT_EXPECTED: SetDParam(0, this->show_expected ? STR_TIMETABLE_EXPECTED : STR_TIMETABLE_SCHEDULED); break;
		}
	}

	/**
	 * Helper function to draw the timetable panel.
	 * @param r The rect to draw within.
	 */
	void DrawTimetablePanel(const Rect &r) const
	{
		const Vehicle *v = this->vehicle;
		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
		int i = this->vscroll->GetPosition();
		VehicleOrderID order_id = (i + 1) / 2;
		bool final_order = false;
		int selected = this->sel_index;

		bool rtl = _current_text_dir == TD_RTL;
		SetDParamMaxValue(0, v->GetNumOrders(), 2);
		int index_column_width = GetStringBoundingBox(STR_ORDER_INDEX).width + 2 * GetSpriteSize(rtl ? SPR_ARROW_RIGHT : SPR_ARROW_LEFT).width + WidgetDimensions::scaled.hsep_normal;
		int middle = rtl ? tr.right - index_column_width : tr.left + index_column_width;

		const Order *order = v->GetOrder(order_id);
		while (order != nullptr) {
			/* Don't draw anything if it extends past the end of the window. */
			if (!this->vscroll->IsVisible(i)) break;

			if (i % 2 == 0) {
				DrawOrderString(v, order, order_id, tr.top, i == selected, true, tr.left, middle, tr.right);

				order_id++;

				if (order_id >= v->GetNumOrders()) {
					order = v->GetOrder(0);
					final_order = true;
				} else {
					order = order->next;
				}
			} else {
				StringID string;
				TextColour colour = (i == selected) ? TC_WHITE : TC_BLACK;
				if (order->IsType(OT_CONDITIONAL)) {
					string = STR_TIMETABLE_NO_TRAVEL;
				} else if (order->IsType(OT_IMPLICIT)) {
					string = STR_TIMETABLE_NOT_TIMETABLEABLE;
					colour = ((i == selected) ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
				} else if (!order->IsTravelTimetabled()) {
					if (order->GetTravelTime() > 0) {
						SetTimetableParams(0, 1, order->GetTravelTime());
						string = order->GetMaxSpeed() != UINT16_MAX ?
							STR_TIMETABLE_TRAVEL_FOR_SPEED_ESTIMATED :
							STR_TIMETABLE_TRAVEL_FOR_ESTIMATED;
					} else {
						string = order->GetMaxSpeed() != UINT16_MAX ?
							STR_TIMETABLE_TRAVEL_NOT_TIMETABLED_SPEED :
							STR_TIMETABLE_TRAVEL_NOT_TIMETABLED;
					}
				} else {
					SetTimetableParams(0, 1, order->GetTimetabledTravel());
					string = order->GetMaxSpeed() != UINT16_MAX ?
						STR_TIMETABLE_TRAVEL_FOR_SPEED : STR_TIMETABLE_TRAVEL_FOR;
				}
				SetDParam(2, PackVelocity(order->GetMaxSpeed(), v->type));

				DrawString(rtl ? tr.left : middle, rtl ? middle : tr.right, tr.top, string, colour);

				if (final_order) break;
			}

			i++;
			tr.top += GetCharacterHeight(FS_NORMAL);
		}
	}

	/**
	 * Helper function to draw the arrival and departure panel.
	 * @param r The rect to draw within.
	 */
	void DrawArrivalDeparturePanel(const Rect &r) const
	{
		const Vehicle *v = this->vehicle;

		/* Arrival and departure times are handled in an all-or-nothing approach,
		 * i.e. are only shown if we can calculate all times.
		 * Excluding order lists with only one order makes some things easier. */
		TimerGameTick::Ticks total_time = v->orders != nullptr ? v->orders->GetTimetableDurationIncomplete() : 0;
		if (total_time <= 0 || v->GetNumOrders() <= 1 || !HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) return;

		std::vector<TimetableArrivalDeparture> arr_dep(v->GetNumOrders());
		const VehicleOrderID cur_order = v->cur_real_order_index % v->GetNumOrders();

		VehicleOrderID earlyID = BuildArrivalDepartureList(v, arr_dep) ? cur_order : (VehicleOrderID)INVALID_VEH_ORDER_ID;
		int selected = this->sel_index;

		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
		bool show_late = this->show_expected && VehicleIsAboveLatenessThreshold(v, true);
		TimerGameTick::Ticks offset = show_late ? 0 : -v->lateness_counter;

		for (int i = this->vscroll->GetPosition(); i / 2 < v->GetNumOrders(); ++i) { // note: i is also incremented in the loop
			/* Don't draw anything if it extends past the end of the window. */
			if (!this->vscroll->IsVisible(i)) break;

			/* TC_INVALID will skip the colour change. */
			SetDParam(0, show_late ? TC_RED : TC_INVALID);
			if (i % 2 == 0) {
				/* Draw an arrival time. */
				if (arr_dep[i / 2].arrival != Ticks::INVALID_TICKS) {
					/* First set the offset and text colour based on the expected/scheduled mode and some other things. */
					TimerGameTick::Ticks this_offset;
					if (this->show_expected && i / 2 == earlyID) {
						/* Show expected arrival. */
						this_offset = 0;
						SetDParam(0, TC_GREEN);
					} else {
						/* Show scheduled arrival. */
						this_offset = offset;
					}

					/* Now actually draw the arrival time. */
					if (_settings_client.gui.timetable_mode == TimetableMode::Seconds) {
						/* Display seconds from now. */
						SetDParam(1, ((arr_dep[i / 2].arrival + offset) / Ticks::TICKS_PER_SECOND));
						DrawString(tr.left, tr.right, tr.top, STR_TIMETABLE_ARRIVAL_SECONDS_IN_FUTURE, i == selected ? TC_WHITE : TC_BLACK);
					} else {
						/* Show a date. */
						SetDParam(1, TimerGameCalendar::date + (arr_dep[i / 2].arrival + this_offset) / Ticks::DAY_TICKS);
						DrawString(tr.left, tr.right, tr.top, STR_TIMETABLE_ARRIVAL_DATE, i == selected ? TC_WHITE : TC_BLACK);
					}
				}
			} else {
				/* Draw a departure time. */
				if (arr_dep[i / 2].departure != Ticks::INVALID_TICKS) {
					if (_settings_client.gui.timetable_mode == TimetableMode::Seconds) {
						/* Display seconds from now. */
						SetDParam(1, ((arr_dep[i / 2].departure + offset) / Ticks::TICKS_PER_SECOND));
						DrawString(tr.left, tr.right, tr.top, STR_TIMETABLE_DEPARTURE_SECONDS_IN_FUTURE, i == selected ? TC_WHITE : TC_BLACK);
					} else {
						/* Show a date. */
						SetDParam(1, TimerGameCalendar::date + (arr_dep[i / 2].departure + offset) / Ticks::DAY_TICKS);
						DrawString(tr.left, tr.right, tr.top, STR_TIMETABLE_DEPARTURE_DATE, i == selected ? TC_WHITE : TC_BLACK);
					}
				}
			}
			tr.top += GetCharacterHeight(FS_NORMAL);
		}
	}

	/**
	 * Helper function to draw the summary panel.
	 * @param r The rect to draw within.
	 */
	void DrawSummaryPanel(const Rect &r) const
	{
		const Vehicle *v = this->vehicle;
		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

		TimerGameTick::Ticks total_time = v->orders != nullptr ? v->orders->GetTimetableDurationIncomplete() : 0;
		if (total_time != 0) {
			SetTimetableParams(0, 1, total_time);
			DrawString(tr, v->orders->IsCompleteTimetable() ? STR_TIMETABLE_TOTAL_TIME : STR_TIMETABLE_TOTAL_TIME_INCOMPLETE);
		}
		tr.top += GetCharacterHeight(FS_NORMAL);

		/* Draw the lateness display, or indicate that the timetable has not started yet. */
		if (v->timetable_start != 0) {
			/* We are running towards the first station so we can start the
			 * timetable at the given time. */
			if (_settings_client.gui.timetable_mode == TimetableMode::Seconds) {
				/* Real time units use seconds relative to now. */
				SetDParam(0, (static_cast<TimerGameTick::Ticks>(v->timetable_start - TimerGameTick::counter) / Ticks::TICKS_PER_SECOND));
				DrawString(tr, STR_TIMETABLE_STATUS_START_IN_SECONDS);
			} else {
				/* Calendar units use dates. */
				SetDParam(0, STR_JUST_DATE_TINY);
				SetDParam(1, GetDateFromStartTick(v->timetable_start));
				DrawString(tr, STR_TIMETABLE_STATUS_START_AT_DATE);
			}
		} else if (!HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) {
			/* We aren't running on a timetable yet. */
			DrawString(tr, STR_TIMETABLE_STATUS_NOT_STARTED);
		} else if (!VehicleIsAboveLatenessThreshold(v, false)) {
			/* We are on time. */
			DrawString(tr, STR_TIMETABLE_STATUS_ON_TIME);
		} else {
			/* We are late. */
			SetTimetableParams(0, 1, abs(v->lateness_counter));
			DrawString(tr, v->lateness_counter < 0 ? STR_TIMETABLE_STATUS_EARLY : STR_TIMETABLE_STATUS_LATE);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				this->DrawTimetablePanel(r);
				break;
			}

			case WID_VT_ARRIVAL_DEPARTURE_PANEL: {
				this->DrawArrivalDeparturePanel(r);
				break;
			}

			case WID_VT_SUMMARY_PANEL: {
				this->DrawSummaryPanel(r);
				break;
			}
		}
	}

	static inline std::tuple<VehicleOrderID, ModifyTimetableFlags> PackTimetableArgs(const Vehicle *v, uint selected, bool speed)
	{
		uint order_number = (selected + 1) / 2;
		ModifyTimetableFlags mtf = (selected % 2 != 0) ? (speed ? MTF_TRAVEL_SPEED : MTF_TRAVEL_TIME) : MTF_WAIT_TIME;

		if (order_number >= v->GetNumOrders()) order_number = 0;

		return { order_number, mtf };
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		const Vehicle *v = this->vehicle;

		switch (widget) {
			case WID_VT_ORDER_VIEW: // Order view button
				ShowOrdersWindow(v);
				break;

			case WID_VT_TIMETABLE_PANEL: { // Main panel.
				int selected = GetOrderFromTimetableWndPt(pt.y, v);

				this->CloseChildWindows();
				this->sel_index = (selected == INVALID_ORDER || selected == this->sel_index) ? -1 : selected;
				break;
			}

			case WID_VT_START_DATE: // Change the date that the timetable starts.
				if (_settings_client.gui.timetable_mode == TimetableMode::Seconds) {
					this->query_widget = WID_VT_START_DATE;
					this->change_timetable_all = _ctrl_pressed;
					ShowQueryString(STR_EMPTY, STR_TIMETABLE_START_SECONDS_QUERY, 6, this, CS_NUMERAL, QSF_ACCEPT_UNCHANGED);
				} else {
					ShowSetDateWindow(this, v->index, TimerGameCalendar::date, TimerGameCalendar::year, TimerGameCalendar::year + MAX_TIMETABLE_START_YEARS, ChangeTimetableStartCallback, reinterpret_cast<void*>(static_cast<uintptr_t>(_ctrl_pressed)));
				}
				break;

			case WID_VT_CHANGE_TIME: { // "Wait For" button.
				this->query_widget = WID_VT_CHANGE_TIME;
				int selected = this->sel_index;
				VehicleOrderID real = (selected + 1) / 2;

				if (real >= v->GetNumOrders()) real = 0;

				const Order *order = v->GetOrder(real);
				StringID current = STR_EMPTY;

				if (order != nullptr) {
					uint time = (selected % 2 != 0) ? order->GetTravelTime() : order->GetWaitTime();
					time /= TicksPerTimetableUnit();

					if (time != 0) {
						SetDParam(0, time);
						current = STR_JUST_INT;
					}
				}

				this->change_timetable_all = _ctrl_pressed && (order != nullptr);
				ShowQueryString(current, STR_TIMETABLE_CHANGE_TIME, 31, this, CS_NUMERAL, QSF_ACCEPT_UNCHANGED);
				break;
			}

			case WID_VT_CHANGE_SPEED: { // Change max speed button.
				this->query_widget = WID_VT_CHANGE_SPEED;
				int selected = this->sel_index;
				VehicleOrderID real = (selected + 1) / 2;

				if (real >= v->GetNumOrders()) real = 0;

				StringID current = STR_EMPTY;
				const Order *order = v->GetOrder(real);
				if (order != nullptr) {
					if (order->GetMaxSpeed() != UINT16_MAX) {
						SetDParam(0, ConvertKmhishSpeedToDisplaySpeed(order->GetMaxSpeed(), v->type));
						current = STR_JUST_INT;
					}
				}

				this->change_timetable_all = _ctrl_pressed && (order != nullptr);
				ShowQueryString(current, STR_TIMETABLE_CHANGE_SPEED, 31, this, CS_NUMERAL, QSF_NONE);
				break;
			}

			case WID_VT_CLEAR_TIME: { // Clear waiting time.
				auto [order_id, mtf] = PackTimetableArgs(v, this->sel_index, false);
				if (_ctrl_pressed) {
					Command<CMD_BULK_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, mtf, 0);
				} else {
					Command<CMD_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, order_id, mtf, 0);
				}
				break;
			}

			case WID_VT_CLEAR_SPEED: { // Clear max speed button.
				auto [order_id, mtf] = PackTimetableArgs(v, this->sel_index, true);
				if (_ctrl_pressed) {
					Command<CMD_BULK_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, mtf, UINT16_MAX);
				} else {
					Command<CMD_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, order_id, mtf, UINT16_MAX);
				}
				break;
			}

			case WID_VT_RESET_LATENESS: // Reset the vehicle's late counter.
				Command<CMD_SET_VEHICLE_ON_TIME>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, _ctrl_pressed);
				break;

			case WID_VT_AUTOFILL: { // Autofill the timetable.
				Command<CMD_AUTOFILL_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, !HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE), _ctrl_pressed);
				break;
			}

			case WID_VT_EXPECTED:
				this->show_expected = !this->show_expected;
				break;

			case WID_VT_SHARED_ORDER_LIST:
				ShowVehicleListWindow(v);
				break;
		}

		this->SetDirty();
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		const Vehicle *v = this->vehicle;
		uint64_t val = StrEmpty(str) ? 0 : std::strtoul(str, nullptr, 10);
		auto [order_id, mtf] = PackTimetableArgs(v, this->sel_index, query_widget == WID_VT_CHANGE_SPEED);

		switch (query_widget) {
			case WID_VT_CHANGE_SPEED: {
				val = ConvertDisplaySpeedToKmhishSpeed(val, v->type);

				if (this->change_timetable_all) {
					Command<CMD_BULK_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, mtf, ClampTo<uint16_t>(val));
				} else {
					Command<CMD_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, order_id, mtf, ClampTo<uint16_t>(val));
				}
				break;
			}

			case WID_VT_CHANGE_TIME:
				val *= TicksPerTimetableUnit();

				if (this->change_timetable_all) {
					Command<CMD_BULK_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, mtf, ClampTo<uint16_t>(val));
				} else {
					Command<CMD_CHANGE_TIMETABLE>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, order_id, mtf, ClampTo<uint16_t>(val));
				}
				break;

			case WID_VT_START_DATE: {
				TimerGameTick::TickCounter start_tick = TimerGameTick::counter + (val * Ticks::TICKS_PER_SECOND);
				Command<CMD_SET_TIMETABLE_START>::Post(STR_ERROR_CAN_T_TIMETABLE_VEHICLE, v->index, this->change_timetable_all, start_tick);
				break;
			}

			default:
				NOT_REACHED();
		}
	}

	void OnResize() override
	{
		/* Update the scroll bar */
		this->vscroll->SetCapacityFromWidget(this, WID_VT_TIMETABLE_PANEL, WidgetDimensions::scaled.framerect.Vertical());
	}

	/**
	 * Update the selection state of the arrival/departure data
	 */
	void UpdateSelectionStates()
	{
		this->GetWidget<NWidgetStacked>(WID_VT_ARRIVAL_DEPARTURE_SELECTION)->SetDisplayedPlane(_settings_client.gui.timetable_arrival_departure ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_VT_EXPECTED_SELECTION)->SetDisplayedPlane(_settings_client.gui.timetable_arrival_departure ? 0 : 1);
	}

	/**
	 * In real-time mode, the timetable GUI shows relative times and needs to be redrawn every second.
	 */
	IntervalTimer<TimerGameTick> redraw_interval = {Ticks::TICKS_PER_SECOND, [this](auto) {
		if (_settings_client.gui.timetable_mode == TimetableMode::Seconds) {
			this->SetDirty();
		}
	}};
};

static const NWidgetPart _nested_timetable_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_VT_CAPTION), SetDataTip(STR_TIMETABLE_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_ORDER_VIEW), SetMinimalSize(61, 14), SetDataTip( STR_TIMETABLE_ORDER_VIEW, STR_TIMETABLE_ORDER_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_VT_TIMETABLE_PANEL), SetMinimalSize(388, 82), SetResize(1, 10), SetDataTip(STR_NULL, STR_TIMETABLE_TOOLTIP), SetScrollbar(WID_VT_SCROLLBAR), EndContainer(),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_VT_ARRIVAL_DEPARTURE_SELECTION),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_VT_ARRIVAL_DEPARTURE_PANEL), SetMinimalSize(110, 0), SetFill(0, 1), SetDataTip(STR_NULL, STR_TIMETABLE_TOOLTIP), SetScrollbar(WID_VT_SCROLLBAR), EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_VT_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_VT_SUMMARY_PANEL), SetMinimalSize(400, 22), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_CHANGE_TIME), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_TIMETABLE_CHANGE_TIME, STR_TIMETABLE_WAIT_TIME_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_CLEAR_TIME), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_TIMETABLE_CLEAR_TIME, STR_TIMETABLE_CLEAR_TIME_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_CHANGE_SPEED), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_TIMETABLE_CHANGE_SPEED, STR_TIMETABLE_CHANGE_SPEED_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_CLEAR_SPEED), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_TIMETABLE_CLEAR_SPEED, STR_TIMETABLE_CLEAR_SPEED_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_START_DATE), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_TIMETABLE_START, STR_TIMETABLE_START_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_RESET_LATENESS), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_TIMETABLE_RESET_LATENESS, STR_TIMETABLE_RESET_LATENESS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_AUTOFILL), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_TIMETABLE_AUTOFILL, STR_TIMETABLE_AUTOFILL_TOOLTIP),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_VT_EXPECTED_SELECTION),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_EXPECTED), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_JUST_STRING, STR_TIMETABLE_EXPECTED_TOOLTIP),
					NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_VT_SHARED_ORDER_LIST), SetFill(0, 1), SetDataTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY), SetFill(0, 1),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _timetable_desc(__FILE__, __LINE__,
	WDP_AUTO, "view_vehicle_timetable", 400, 130,
	WC_VEHICLE_TIMETABLE, WC_VEHICLE_VIEW,
	WDF_CONSTRUCTION,
	std::begin(_nested_timetable_widgets), std::end(_nested_timetable_widgets)
);

/**
 * Show the timetable for a given vehicle.
 * @param v The vehicle to show the timetable for.
 */
void ShowTimetableWindow(const Vehicle *v)
{
	CloseWindowById(WC_VEHICLE_DETAILS, v->index, false);
	CloseWindowById(WC_VEHICLE_ORDERS, v->index, false);
	AllocateWindowDescFront<TimetableWindow>(&_timetable_desc, v->index);
}
