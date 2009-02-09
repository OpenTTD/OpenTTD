/* $Id$ */

/** @file timetable_cmd.cpp Commands related to time tabling. */

#include "stdafx.h"
#include "command_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_base.h"
#include "settings_type.h"

#include "table/strings.h"

static void ChangeTimetable(Vehicle *v, VehicleOrderID order_number, uint16 time, bool is_journey)
{
	Order *order = GetVehicleOrder(v, order_number);
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
		if (v->cur_order_index == order_number && v->current_order.Equals(*order)) {
			if (is_journey) {
				v->current_order.travel_time = time;
			} else {
				v->current_order.wait_time = time;
			}
		}
		InvalidateWindow(WC_VEHICLE_TIMETABLE, v->index);
	}
}

/**
 * Add or remove waiting times from an order.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Various bitstuffed elements
 * - p1 = (bit  0-15) - Vehicle with the orders to change.
 * - p1 = (bit 16-23) - Order index to modify.
 * - p1 = (bit    24) - Whether to change the waiting time or the travelling
 *                      time.
 * - p1 = (bit    25) - Whether p2 contains waiting and travelling time.
 * @param p2 The amount of time to wait.
 * - p2 = (bit  0-15) - Waiting or travelling time as specified by p1 bit 24 if p1 bit 25 is not set,
 *                      Travelling time if p1 bit 25 is set.
 * - p2 = (bit 16-31) - Waiting time if p1 bit 25 is set
 */
CommandCost CmdChangeTimetable(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!_settings_game.order.timetabling) return CMD_ERROR;

	VehicleID veh = GB(p1, 0, 16);
	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	VehicleOrderID order_number = GB(p1, 16, 8);
	Order *order = GetVehicleOrder(v, order_number);
	if (order == NULL) return CMD_ERROR;

	bool packed_time = HasBit(p1, 25);
	bool is_journey = HasBit(p1, 24) || packed_time;

	int wait_time   = order->wait_time;
	int travel_time = order->travel_time;
	if (packed_time) {
		travel_time = GB(p2, 0, 16);
		wait_time   = GB(p2, 16, 16);;
	} else if (is_journey) {
		travel_time = GB(p2, 0, 16);
	} else {
		wait_time   = GB(p2, 0, 16);
	}

	if (wait_time != order->wait_time) {
		switch (order->GetType()) {
			case OT_GOTO_STATION:
				if (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) return_cmd_error(STR_TIMETABLE_NOT_STOPPING_HERE);
				break;

			case OT_CONDITIONAL:
				break;

			default: return_cmd_error(STR_TIMETABLE_ONLY_WAIT_AT_STATIONS);
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
 * - p1 = (bit  0-15) - Vehicle with the orders to change.
 */
CommandCost CmdSetVehicleOnTime(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!_settings_game.order.timetabling) return CMD_ERROR;

	VehicleID veh = GB(p1, 0, 16);
	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->lateness_counter = 0;
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
 */
CommandCost CmdAutofillTimetable(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!_settings_game.order.timetabling) return CMD_ERROR;

	VehicleID veh = GB(p1, 0, 16);
	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (HasBit(p2, 0)) {
			/* Start autofilling the timetable, which clears the
			 * "timetable has started" bit. Times are not cleared anymore, but are
			 * overwritten when the order is reached now. */
			SetBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_TIMETABLE_STARTED);

			/* Overwrite waiting times only if they got longer */
			if (HasBit(p2, 1)) SetBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);

			v->lateness_counter = 0;
		} else {
			ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
		}
	}

	for (Vehicle *v2 = v->FirstShared(); v2 != NULL; v2 = v2->NextShared()) {
		if (v2 != v) {
			/* Stop autofilling; only one vehicle at a time can perform autofill */
			ClrBit(v2->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v2->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
		}
		InvalidateWindow(WC_VEHICLE_TIMETABLE, v2->index);
	}

	return CommandCost();
}

void UpdateVehicleTimetable(Vehicle *v, bool travelling)
{
	uint timetabled = travelling ? v->current_order.travel_time : v->current_order.wait_time;
	uint time_taken = v->current_order_time;

	v->current_order_time = 0;

	if (!_settings_game.order.timetabling) return;

	bool just_started = false;

	/* Make sure the timetable only starts when the vehicle reaches the first
	 * order, not when travelling from the depot to the first station. */
	if (v->cur_order_index == 0 && !HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) {
		SetBit(v->vehicle_flags, VF_TIMETABLE_STARTED);
		just_started = true;
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
			 * adjusted later by people who aren't. */
			time_taken = (((time_taken - 1) / DAY_TICKS) + 1) * DAY_TICKS;

			ChangeTimetable(v, v->cur_order_index, time_taken, travelling);
		}

		if (v->cur_order_index == 0 && travelling) {
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

	for (v = v->FirstShared(); v != NULL; v = v->NextShared()) {
		InvalidateWindow(WC_VEHICLE_TIMETABLE, v->index);
	}
}
