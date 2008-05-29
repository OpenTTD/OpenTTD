/* $Id$ */

/** @file timetable_cmd.cpp Commands related to time tabling. */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "command_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "vehicle_base.h"
#include "settings_type.h"

#include "table/strings.h"

static void ChangeTimetable(Vehicle *v, VehicleOrderID order_number, uint16 time, bool is_journey)
{
	Order *order = GetVehicleOrder(v, order_number);

	if (is_journey) {
		order->travel_time = time;
	} else {
		order->wait_time = time;
	}

	if (v->cur_order_index == order_number && v->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) {
		if (is_journey) {
			v->current_order.travel_time = time;
		} else {
			v->current_order.wait_time = time;
		}
	}

	for (v = GetFirstVehicleFromSharedList(v); v != NULL; v = v->next_shared) {
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
CommandCost CmdChangeTimetable(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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
	if (!is_journey) {
		if (!order->IsType(OT_GOTO_STATION)) return_cmd_error(STR_TIMETABLE_ONLY_WAIT_AT_STATIONS);
		if (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) return_cmd_error(STR_TIMETABLE_NOT_STOPPING_HERE);
	}

	if (flags & DC_EXEC) {
		ChangeTimetable(v, order_number, GB(p2, 0, 16), is_journey);
		if (packed_time) ChangeTimetable(v, order_number, GB(p2, 16, 16), false);
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
CommandCost CmdSetVehicleOnTime(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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
 * @param p2 Set to 1 to enable, 0 to disable.
 */
CommandCost CmdAutofillTimetable(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!_settings_game.order.timetabling) return CMD_ERROR;

	VehicleID veh = GB(p1, 0, 16);
	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (p2 == 1) {
			/* Start autofilling the timetable, which clears all the current
			 * timings and clears the "timetable has started" bit. */
			SetBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_TIMETABLE_STARTED);

			for (Order *order = GetVehicleOrder(v, 0); order != NULL; order = order->next) {
				order->wait_time = 0;
				order->travel_time = 0;
			}

			v->current_order.wait_time = 0;
			v->current_order.travel_time = 0;
		} else {
			ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
		}
	}

	for (v = GetFirstVehicleFromSharedList(v); v != NULL; v = v->next_shared) {
		InvalidateWindow(WC_VEHICLE_TIMETABLE, v->index);
	}

	return CommandCost();
}

void UpdateVehicleTimetable(Vehicle *v, bool travelling)
{
	uint timetabled = travelling ? v->current_order.travel_time : v->current_order.wait_time;
	uint time_taken = v->current_order_time;

	v->current_order_time = 0;

	if (!_settings_game.order.timetabling) return;

	/* Make sure the timetable only starts when the vehicle reaches the first
	 * order, not when travelling from the depot to the first station. */
	if (v->cur_order_index == 0 && !HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) {
		SetBit(v->vehicle_flags, VF_TIMETABLE_STARTED);
		return;
	}

	if (!HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) return;

	if (HasBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE)) {
		if (timetabled == 0) {
			/* Round the time taken up to the nearest day, as this will avoid
			 * confusion for people who are timetabling in days, and can be
			 * adjusted later by people who aren't. */
			time_taken = (((time_taken - 1) / DAY_TICKS) + 1) * DAY_TICKS;

			ChangeTimetable(v, v->cur_order_index, time_taken, travelling);
			return;
		} else if (v->cur_order_index == 0) {
			/* Otherwise if we're at the beginning and it already has a value,
			 * assume that autofill is finished and turn it off again. */
			ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
		}
	}

	/* Vehicles will wait at stations if they arrive early even if they are not
	 * timetabled to wait there, so make sure the lateness counter is updated
	 * when this happens. */
	if (timetabled == 0 && (travelling || v->lateness_counter >= 0)) return;

	v->lateness_counter -= (timetabled - time_taken);

	for (v = GetFirstVehicleFromSharedList(v); v != NULL; v = v->next_shared) {
		InvalidateWindow(WC_VEHICLE_TIMETABLE, v->index);
	}
}
