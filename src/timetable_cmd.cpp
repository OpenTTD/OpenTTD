/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_cmd.cpp Commands related to time tabling. */

#include "stdafx.h"
#include "command_func.h"
#include "company_func.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_game_calendar.h"
#include "window_func.h"
#include "vehicle_base.h"
#include "timetable_cmd.h"
#include "timetable.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Get the TimerGameTick::TickCounter tick of a given date.
 * @param start_date The date when the timetable starts.
 * @return The first tick of this date.
 */
TimerGameTick::TickCounter GetStartTickFromDate(TimerGameCalendar::Date start_date)
{
	/* Calculate the offset in ticks from the current date. */
	TimerGameTick::Ticks tick_offset = (start_date - TimerGameCalendar::date).base() * Ticks::DAY_TICKS;

	/* Compensate for the current date_fract. */
	tick_offset -= TimerGameCalendar::date_fract;

	/* Return the current tick plus the offset. */
	return TimerGameTick::counter + tick_offset;
}

/**
 * Get a date from a given start tick of timetable.
 * @param start_tick The TimerGameTick::TickCounter when the timetable starts.
 * @return The date when we reach this tick.
 */
TimerGameCalendar::Date GetDateFromStartTick(TimerGameTick::TickCounter start_tick)
{
	/* Calculate the offset in ticks from the current counter tick. */
	TimerGameTick::Ticks tick_offset = start_tick - TimerGameTick::counter;

	/* Compensate for the current date_fract. */
	tick_offset += TimerGameCalendar::date_fract;

	/* Return the current date plus the offset in days. */
	return TimerGameCalendar::date + (tick_offset / Ticks::DAY_TICKS);
}

/**
 * Change/update a particular timetable entry.
 * @param v            The vehicle to change the timetable of.
 * @param order_number The index of the timetable in the order list.
 * @param val          The new data of the timetable entry.
 * @param mtf          Which part of the timetable entry to change.
 * @param timetabled   If the new value is explicitly timetabled.
 */
static void ChangeTimetable(Vehicle *v, VehicleOrderID order_number, uint16_t val, ModifyTimetableFlags mtf, bool timetabled)
{
	Order *order = v->GetOrder(order_number);
	assert(order != nullptr);
	int total_delta = 0;
	int timetable_delta = 0;

	switch (mtf) {
		case MTF_WAIT_TIME:
			total_delta = val - order->GetWaitTime();
			timetable_delta = (timetabled ? val : 0) - order->GetTimetabledWait();
			order->SetWaitTime(val);
			order->SetWaitTimetabled(timetabled);
			break;

		case MTF_TRAVEL_TIME:
			total_delta = val - order->GetTravelTime();
			timetable_delta = (timetabled ? val : 0) - order->GetTimetabledTravel();
			order->SetTravelTime(val);
			order->SetTravelTimetabled(timetabled);
			break;

		case MTF_TRAVEL_SPEED:
			order->SetMaxSpeed(val);
			break;

		default:
			NOT_REACHED();
	}
	v->orders->UpdateTotalDuration(total_delta);
	v->orders->UpdateTimetableDuration(timetable_delta);

	for (v = v->FirstShared(); v != nullptr; v = v->NextShared()) {
		if (v->cur_real_order_index == order_number && v->current_order.Equals(*order)) {
			switch (mtf) {
				case MTF_WAIT_TIME:
					v->current_order.SetWaitTime(val);
					v->current_order.SetWaitTimetabled(timetabled);
					break;

				case MTF_TRAVEL_TIME:
					v->current_order.SetTravelTime(val);
					v->current_order.SetTravelTimetabled(timetabled);
					break;

				case MTF_TRAVEL_SPEED:
					v->current_order.SetMaxSpeed(val);
					break;

				default:
					NOT_REACHED();
			}
		}
		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}
}

/**
 * Change timetable data of an order.
 * @param flags Operation to perform.
 * @param veh Vehicle with the orders to change.
 * @param order_number Order index to modify.
 * @param mtf Timetable data to change (@see ModifyTimetableFlags)
 * @param data The data to modify as specified by \c mtf.
 *             0 to clear times, UINT16_MAX to clear speed limit.
 * @return the cost of this operation or an error
 */
CommandCost CmdChangeTimetable(DoCommandFlag flags, VehicleID veh, VehicleOrderID order_number, ModifyTimetableFlags mtf, uint16_t data)
{
	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	Order *order = v->GetOrder(order_number);
	if (order == nullptr || order->IsType(OT_IMPLICIT)) return CMD_ERROR;

	if (mtf >= MTF_END) return CMD_ERROR;

	int wait_time   = order->GetWaitTime();
	int travel_time = order->GetTravelTime();
	int max_speed   = order->GetMaxSpeed();
	switch (mtf) {
		case MTF_WAIT_TIME:
			wait_time = data;
			break;

		case MTF_TRAVEL_TIME:
			travel_time = data;
			break;

		case MTF_TRAVEL_SPEED:
			max_speed = data;
			if (max_speed == 0) max_speed = UINT16_MAX; // Disable speed limit.
			break;

		default:
			NOT_REACHED();
	}

	if (wait_time != order->GetWaitTime()) {
		switch (order->GetType()) {
			case OT_GOTO_STATION:
				if (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) return_cmd_error(STR_ERROR_TIMETABLE_NOT_STOPPING_HERE);
				break;

			case OT_CONDITIONAL:
				break;

			default: return_cmd_error(STR_ERROR_TIMETABLE_ONLY_WAIT_AT_STATIONS);
		}
	}

	if (travel_time != order->GetTravelTime() && order->IsType(OT_CONDITIONAL)) return CMD_ERROR;
	if (max_speed != order->GetMaxSpeed() && (order->IsType(OT_CONDITIONAL) || v->type == VEH_AIRCRAFT)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		switch (mtf) {
			case MTF_WAIT_TIME:
				/* Set time if changing the value or confirming an estimated time as timetabled. */
				if (wait_time != order->GetWaitTime() || (wait_time > 0 && !order->IsWaitTimetabled())) {
					ChangeTimetable(v, order_number, wait_time, MTF_WAIT_TIME, wait_time > 0);
				}
				break;

			case MTF_TRAVEL_TIME:
				/* Set time if changing the value or confirming an estimated time as timetabled. */
				if (travel_time != order->GetTravelTime() || (travel_time > 0 && !order->IsTravelTimetabled())) {
					ChangeTimetable(v, order_number, travel_time, MTF_TRAVEL_TIME, travel_time > 0);
				}
				break;

			case MTF_TRAVEL_SPEED:
				if (max_speed != order->GetMaxSpeed()) {
					ChangeTimetable(v, order_number, max_speed, MTF_TRAVEL_SPEED, max_speed != UINT16_MAX);
				}
				break;

			default:
				break;
		}
	}

	return CommandCost();
}

/**
 * Change timetable data of all orders of a vehicle.
 * @param flags Operation to perform.
 * @param veh Vehicle with the orders to change.
 * @param mtf Timetable data to change (@see ModifyTimetableFlags)
 * @param data The data to modify as specified by \c mtf.
 *             0 to clear times, UINT16_MAX to clear speed limit.
 * @return the cost of this operation or an error
 */
CommandCost CmdBulkChangeTimetable(DoCommandFlag flags, VehicleID veh, ModifyTimetableFlags mtf, uint16_t data)
{
	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (mtf >= MTF_END) return CMD_ERROR;

	if (v->GetNumOrders() == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		for (VehicleOrderID order_number = 0; order_number < v->GetNumOrders(); order_number++) {
			Order *order = v->GetOrder(order_number);
			if (order == nullptr || order->IsType(OT_IMPLICIT)) continue;

			Command<CMD_CHANGE_TIMETABLE>::Do(DC_EXEC, v->index, order_number, mtf, data);
		}
	}

	return CommandCost();
}

/**
 * Clear the lateness counter to make the vehicle on time.
 * @param flags Operation to perform.
 * @param veh Vehicle with the orders to change.
 * @param apply_to_group Set to reset the late counter for all vehicles sharing the orders.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetVehicleOnTime(DoCommandFlag flags, VehicleID veh, bool apply_to_group)
{
	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle() || v->orders == nullptr) return CMD_ERROR;

	/* A vehicle can't be late if its timetable hasn't started.
	 * If we're setting all vehicles in the group, we handle that below. */
	if (!apply_to_group && !HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) return CommandCost(STR_ERROR_TIMETABLE_NOT_STARTED);

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		if (apply_to_group) {
			TimerGameTick::Ticks most_late = 0;
			for (Vehicle *u = v->FirstShared(); u != nullptr; u = u->NextShared()) {
				/* A vehicle can't be late if its timetable hasn't started. */
				if (!HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) continue;

				if (u->lateness_counter > most_late) {
					most_late = u->lateness_counter;
				}
			}
			if (most_late > 0) {
				for (Vehicle *u = v->FirstShared(); u != nullptr; u = u->NextShared()) {
					/* A vehicle can't be late if its timetable hasn't started. */
					if (!HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) continue;

					u->lateness_counter -= most_late;
					SetWindowDirty(WC_VEHICLE_TIMETABLE, u->index);
				}
			}
		} else {
			v->lateness_counter = 0;
			SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
		}
	}

	return CommandCost();
}

/**
 * Order vehicles based on their timetable. The vehicles will be sorted in order
 * they would reach the first station.
 *
 * @param a First Vehicle pointer.
 * @param b Second Vehicle pointer.
 * @return Comparison value.
 */
static bool VehicleTimetableSorter(Vehicle * const &a, Vehicle * const &b)
{
	VehicleOrderID a_order = a->cur_real_order_index;
	VehicleOrderID b_order = b->cur_real_order_index;
	int j = (int)b_order - (int)a_order;

	/* Are we currently at an ordered station (un)loading? */
	bool a_load = a->current_order.IsType(OT_LOADING) && a->current_order.GetNonStopType() != ONSF_STOP_EVERYWHERE;
	bool b_load = b->current_order.IsType(OT_LOADING) && b->current_order.GetNonStopType() != ONSF_STOP_EVERYWHERE;

	/* If the current order is not loading at the ordered station, decrease the order index by one since we have
	 * not yet arrived at the station (and thus the timetable entry; still in the travelling of the previous one).
	 * Since the ?_order variables are unsigned the -1 will flow under and place the vehicles going to order #0 at
	 * the begin of the list with vehicles arriving at #0. */
	if (!a_load) a_order--;
	if (!b_load) b_order--;

	/* First check the order index that accounted for loading, then just the raw one. */
	int i = (int)b_order - (int)a_order;
	if (i != 0) return i < 0;
	if (j != 0) return j < 0;

	/* Look at the time we spent in this order; the higher, the closer to its destination. */
	i = b->current_order_time - a->current_order_time;
	if (i != 0) return i < 0;

	/* If all else is equal, use some unique index to sort it the same way. */
	return b->unitnumber < a->unitnumber;
}

/**
 * Set the start date of the timetable.
 * @param flags Operation to perform.
 * @param veh_id Vehicle ID.
 * @param timetable_all Set to set timetable start for all vehicles sharing this order
 * @param start_tick The TimerGameTick::counter tick when the timetable starts.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetTimetableStart(DoCommandFlag flags, VehicleID veh_id, bool timetable_all, TimerGameTick::TickCounter start_tick)
{
	Vehicle *v = Vehicle::GetIfValid(veh_id);
	if (v == nullptr || !v->IsPrimaryVehicle() || v->orders == nullptr) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	TimerGameTick::Ticks total_duration = v->orders->GetTimetableTotalDuration();

	TimerGameCalendar::Date start_date = GetDateFromStartTick(start_tick);

	/* Don't let a timetable start at an invalid date. */
	if (start_date < 0 || start_date > CalendarTime::MAX_DATE) return CMD_ERROR;

	/* Don't let a timetable start more than 15 years into the future... */
	if (start_date - TimerGameCalendar::date > TimerGameCalendar::DateAtStartOfYear(MAX_TIMETABLE_START_YEARS)) return CMD_ERROR;
	/* ...or 1 year in the past. */
	if (TimerGameCalendar::date - start_date > CalendarTime::DAYS_IN_LEAP_YEAR) return CMD_ERROR;

	/* If trying to distribute start dates over a shared order group, we need to know the total duration. */
	if (timetable_all && !v->orders->IsCompleteTimetable()) return CommandCost(STR_ERROR_TIMETABLE_INCOMPLETE);

	/* Don't allow invalid start dates for other vehicles in the shared order group. */
	if (timetable_all && start_date + (total_duration / Ticks::DAY_TICKS) > CalendarTime::MAX_DATE) return CMD_ERROR;

	if (flags & DC_EXEC) {
		std::vector<Vehicle *> vehs;

		if (timetable_all) {
			for (Vehicle *w = v->orders->GetFirstSharedVehicle(); w != nullptr; w = w->NextShared()) {
				vehs.push_back(w);
			}
		} else {
			vehs.push_back(v);
		}

		int num_vehs = (uint)vehs.size();

		if (num_vehs >= 2) {
			std::sort(vehs.begin(), vehs.end(), &VehicleTimetableSorter);
		}

		int idx = 0;

		for (Vehicle *w : vehs) {

			w->lateness_counter = 0;
			ClrBit(w->vehicle_flags, VF_TIMETABLE_STARTED);
			/* Do multiplication, then division to reduce rounding errors. */
			w->timetable_start = start_tick + (idx * total_duration / num_vehs);
			SetWindowDirty(WC_VEHICLE_TIMETABLE, w->index);
			++idx;
		}

	}

	return CommandCost();
}


/**
 * Start or stop filling the timetable automatically from the time the vehicle
 * actually takes to complete it. When starting to autofill the current times
 * are cleared and the timetable will start again from scratch.
 * @param flags Operation to perform.
 * @param veh Vehicle index.
 * @param autofill Enable or disable autofill
 * @param preserve_wait_time Set to preserve waiting times in non-destructive mode
 * @return the cost of this operation or an error
 */
CommandCost CmdAutofillTimetable(DoCommandFlag flags, VehicleID veh, bool autofill, bool preserve_wait_time)
{
	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle() || v->orders == nullptr) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		if (autofill) {
			/* Start autofilling the timetable, which clears the
			 * "timetable has started" bit. Times are not cleared anymore, but are
			 * overwritten when the order is reached now. */
			SetBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_TIMETABLE_STARTED);

			/* Overwrite waiting times only if they got longer */
			if (preserve_wait_time) SetBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);

			v->timetable_start = 0;
			v->lateness_counter = 0;
		} else {
			ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
		}

		for (Vehicle *v2 = v->FirstShared(); v2 != nullptr; v2 = v2->NextShared()) {
			if (v2 != v) {
				/* Stop autofilling; only one vehicle at a time can perform autofill */
				ClrBit(v2->vehicle_flags, VF_AUTOFILL_TIMETABLE);
				ClrBit(v2->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
			}
			SetWindowDirty(WC_VEHICLE_TIMETABLE, v2->index);
		}
	}

	return CommandCost();
}

/**
 * Update the timetable for the vehicle.
 * @param v The vehicle to update the timetable for.
 * @param travelling Whether we just travelled or waited at a station.
 */
void UpdateVehicleTimetable(Vehicle *v, bool travelling)
{
	TimerGameTick::Ticks time_taken = v->current_order_time;

	v->current_order_time = 0;

	if (v->current_order.IsType(OT_IMPLICIT)) return; // no timetabling of auto orders

	if (v->cur_real_order_index >= v->GetNumOrders()) return;
	Order *real_current_order = v->GetOrder(v->cur_real_order_index);
	assert(real_current_order != nullptr);

	VehicleOrderID first_manual_order = 0;
	for (Order *o = v->GetFirstOrder(); o != nullptr && o->IsType(OT_IMPLICIT); o = o->next) {
		++first_manual_order;
	}

	bool just_started = false;

	/* This vehicle is arriving at the first destination in the timetable. */
	if (v->cur_real_order_index == first_manual_order && travelling) {
		/* If the start date hasn't been set, or it was set automatically when
		 * the vehicle last arrived at the first destination, update it to the
		 * current time. Otherwise set the late counter appropriately to when
		 * the vehicle should have arrived. */
		just_started = !HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED);

		if (v->timetable_start != 0) {
			v->lateness_counter = TimerGameTick::counter - v->timetable_start;
			v->timetable_start = 0;
		}

		SetBit(v->vehicle_flags, VF_TIMETABLE_STARTED);
		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}

	if (!HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) return;

	bool autofilling = HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
	bool remeasure_wait_time = !real_current_order->IsWaitTimetabled() ||
			(autofilling && !HasBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME));

	if (travelling && remeasure_wait_time) {
		/* We just finished travelling and want to remeasure the loading time,
		 * so do not apply any restrictions for the loading to finish. */
		v->current_order.SetWaitTime(0);
	}

	if (just_started) return;

	/* Before modifying waiting times, check whether we want to preserve bigger ones. */
	if (!real_current_order->IsType(OT_CONDITIONAL) &&
			(travelling || time_taken > real_current_order->GetWaitTime() || remeasure_wait_time)) {
		/* Round up to the smallest unit of time commonly shown in the GUI (seconds) to avoid confusion.
		 * Players timetabling in Ticks can adjust later.
		 * For trains/aircraft multiple movement cycles are done in one
		 * tick. This makes it possible to leave the station and process
		 * e.g. a depot order in the same tick, causing it to not fill
		 * the timetable entry like is done for road vehicles/ships.
		 * Thus always make sure at least one tick is used between the
		 * processing of different orders when filling the timetable. */
		uint time_to_set = CeilDiv(std::max(time_taken, 1), Ticks::TICKS_PER_SECOND) * Ticks::TICKS_PER_SECOND;

		if (travelling && (autofilling || !real_current_order->IsTravelTimetabled())) {
			ChangeTimetable(v, v->cur_real_order_index, time_to_set, MTF_TRAVEL_TIME, autofilling);
		} else if (!travelling && (autofilling || !real_current_order->IsWaitTimetabled())) {
			ChangeTimetable(v, v->cur_real_order_index, time_to_set, MTF_WAIT_TIME, autofilling);
		}
	}

	if (v->cur_real_order_index == first_manual_order && travelling) {
		/* If we just started we would have returned earlier and have not reached
		 * this code. So obviously, we have completed our round: So turn autofill
		 * off again. */
		ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
		ClrBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
	}

	if (autofilling) return;

	TimerGameTick::Ticks timetabled = travelling ? real_current_order->GetTimetabledTravel() :
			real_current_order->GetTimetabledWait();

	/* Vehicles will wait at stations if they arrive early even if they are not
	 * timetabled to wait there, so make sure the lateness counter is updated
	 * when this happens. */
	if (timetabled == 0 && (travelling || v->lateness_counter >= 0)) return;

	v->lateness_counter -= (timetabled - time_taken);

	/* When we are more late than this timetabled bit takes we (somewhat expensively)
	 * check how many ticks the (fully filled) timetable has. If a timetable cycle is
	 * shorter than the amount of ticks we are late we reduce the lateness by the
	 * length of a full cycle till lateness is less than the length of a timetable
	 * cycle. When the timetable isn't fully filled the cycle will be Ticks::INVALID_TICKS. */
	if (v->lateness_counter > timetabled) {
		TimerGameTick::Ticks cycle = v->orders->GetTimetableTotalDuration();
		if (cycle != Ticks::INVALID_TICKS && v->lateness_counter > cycle) {
			v->lateness_counter %= cycle;
		}
	}

	for (v = v->FirstShared(); v != nullptr; v = v->NextShared()) {
		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}
}
