/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_cmd.cpp Commands related to time tabling. */

#include "stdafx.h"
#include "command_func.h"
#include "functions.h"
#include "date_func.h"
#include "window_func.h"
#include "vehicle_base.h"

#include "table/strings.h"

static void ChangeTimetable(Vehicle *v, VehicleOrderID order_number, uint16 time, bool is_journey)
{
	Order *order = v->GetOrder(order_number);
	int delta;

	if (is_journey) {
		delta = time - order->travel_time;
		order->travel_time = time;
	} else {
		delta = time - order->wait_time;
		order->wait_time = time;
	}
	v->orders.list->UpdateOrderTimetable(delta);

	for (v = v->FirstShared(); v != NULL; v = v->NextShared()) {
		if (v->cur_real_order_index == order_number && v->current_order.Equals(*order)) {
			if (is_journey) {
				v->current_order.travel_time = time;
			} else {
				v->current_order.wait_time = time;
			}
		}
		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}
}

/**
 * Add or remove waiting times from an order.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Various bitstuffed elements
 * - p1 = (bit  0-19) - Vehicle with the orders to change.
 * - p1 = (bit 20-27) - Order index to modify.
 * - p1 = (bit    28) - Whether to change the waiting time or the travelling
 *                      time.
 * @param p2 The amount of time to wait.
 * - p2 = (bit  0-15) - Waiting or travelling time as specified by p1 bit 28
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdChangeTimetable(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID veh = GB(p1, 0, 20);

	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == NULL || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	VehicleOrderID order_number = GB(p1, 20, 8);
	Order *order = v->GetOrder(order_number);
	if (order == NULL || order->IsType(OT_AUTOMATIC)) return CMD_ERROR;

	bool is_journey = HasBit(p1, 28);

	int wait_time   = order->wait_time;
	int travel_time = order->travel_time;
	if (is_journey) {
		travel_time = GB(p2, 0, 16);
	} else {
		wait_time   = GB(p2, 0, 16);
	}

	if (wait_time != order->wait_time) {
		switch (order->GetType()) {
			case OT_GOTO_STATION:
				if (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) return_cmd_error(STR_ERROR_TIMETABLE_NOT_STOPPING_HERE);
				break;

			case OT_CONDITIONAL:
				break;

			default: return_cmd_error(STR_ERROR_TIMETABLE_ONLY_WAIT_AT_STATIONS);
		}
	}

	if (travel_time != order->travel_time && order->IsType(OT_CONDITIONAL)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (wait_time   != order->wait_time)   ChangeTimetable(v, order_number, wait_time,   false);
		if (travel_time != order->travel_time) ChangeTimetable(v, order_number, travel_time, true);
	}

	return CommandCost();
}

/**
 * Clear the lateness counter to make the vehicle on time.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Various bitstuffed elements
 * - p1 = (bit  0-19) - Vehicle with the orders to change.
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSetVehicleOnTime(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID veh = GB(p1, 0, 20);

	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == NULL || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		v->lateness_counter = 0;
		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}

	return CommandCost();
}

/**
 * Set the start date of the timetable.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Vehicle id.
 * @param p2 The timetable start date.
 * @param text Not used.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetTimetableStart(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v = Vehicle::GetIfValid(GB(p1, 0, 20));
	if (v == NULL || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	/* Don't let a timetable start more than 15 years into the future or 1 year in the past. */
	Date start_date = (Date)p2;
	if (start_date < 0 || start_date > MAX_DAY) return CMD_ERROR;
	if (start_date - _date > 15 * DAYS_IN_LEAP_YEAR) return CMD_ERROR;
	if (_date - start_date > DAYS_IN_LEAP_YEAR) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->lateness_counter = 0;
		ClrBit(v->vehicle_flags, VF_TIMETABLE_STARTED);
		v->timetable_start = start_date;

		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}

	return CommandCost();
}


/**
 * Start or stop filling the timetable automatically from the time the vehicle
 * actually takes to complete it. When starting to autofill the current times
 * are cleared and the timetable will start again from scratch.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Vehicle index.
 * @param p2 Various bitstuffed elements
 * - p2 = (bit 0) - Set to 1 to enable, 0 to disable autofill.
 * - p2 = (bit 1) - Set to 1 to preserve waiting times in non-destructive mode
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdAutofillTimetable(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID veh = GB(p1, 0, 20);

	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == NULL || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		if (HasBit(p2, 0)) {
			/* Start autofilling the timetable, which clears the
			 * "timetable has started" bit. Times are not cleared anymore, but are
			 * overwritten when the order is reached now. */
			SetBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_TIMETABLE_STARTED);

			/* Overwrite waiting times only if they got longer */
			if (HasBit(p2, 1)) SetBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);

			v->timetable_start = 0;
			v->lateness_counter = 0;
		} else {
			ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
		}

		for (Vehicle *v2 = v->FirstShared(); v2 != NULL; v2 = v2->NextShared()) {
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

void UpdateVehicleTimetable(Vehicle *v, bool travelling)
{
	uint timetabled = travelling ? v->current_order.travel_time : v->current_order.wait_time;
	uint time_taken = v->current_order_time;

	v->current_order_time = 0;

	if (v->current_order.IsType(OT_AUTOMATIC)) return; // no timetabling of auto orders

	VehicleOrderID first_manual_order = 0;
	for (Order *o = v->GetFirstOrder(); o != NULL && o->IsType(OT_AUTOMATIC); o = o->next) {
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
			v->lateness_counter = (_date - v->timetable_start) * DAY_TICKS + _date_fract;
			v->timetable_start = 0;
		}

		SetBit(v->vehicle_flags, VF_TIMETABLE_STARTED);
		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}

	if (!HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) return;

	if (HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE)) {
		if (travelling && !HasBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME)) {
			/* Need to clear that now as otherwise we are not able to reduce the wait time */
			v->current_order.wait_time = 0;
		}

		if (just_started) return;

		/* Modify station waiting time only if our new value is larger (this is
		 * always the case when we cleared the timetable). */
		if (!v->current_order.IsType(OT_CONDITIONAL) && (travelling || time_taken > v->current_order.wait_time)) {
			/* Round the time taken up to the nearest day, as this will avoid
			 * confusion for people who are timetabling in days, and can be
			 * adjusted later by people who aren't.
			 * For trains/aircraft multiple movement cycles are done in one
			 * tick. This makes it possible to leave the station and process
			 * e.g. a depot order in the same tick, causing it to not fill
			 * the timetable entry like is done for road vehicles/ships.
			 * Thus always make sure at least one tick is used between the
			 * processing of different orders when filling the timetable. */
			time_taken = CeilDiv(max(time_taken, 1U), DAY_TICKS) * DAY_TICKS;

			ChangeTimetable(v, v->cur_real_order_index, time_taken, travelling);
		}

		if (v->cur_real_order_index == first_manual_order && travelling) {
			/* If we just started we would have returned earlier and have not reached
			 * this code. So obviously, we have completed our round: So turn autofill
			 * off again. */
			ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
		}
		return;
	}

	if (just_started) return;

	/* Vehicles will wait at stations if they arrive early even if they are not
	 * timetabled to wait there, so make sure the lateness counter is updated
	 * when this happens. */
	if (timetabled == 0 && (travelling || v->lateness_counter >= 0)) return;

	v->lateness_counter -= (timetabled - time_taken);

	/* When we are more late than this timetabled bit takes we (somewhat expensively)
	 * check how many ticks the (fully filled) timetable has. If a timetable cycle is
	 * shorter than the amount of ticks we are late we reduce the lateness by the
	 * length of a full cycle till lateness is less than the length of a timetable
	 * cycle. When the timetable isn't fully filled the cycle will be INVALID_TICKS. */
	if (v->lateness_counter > (int)timetabled) {
		Ticks cycle = v->orders.list->GetTimetableTotalDuration();
		if (cycle != INVALID_TICKS && v->lateness_counter > cycle) {
			v->lateness_counter %= cycle;
		}
	}

	for (v = v->FirstShared(); v != NULL; v = v->NextShared()) {
		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}
}
