/* $Id$ */

/** @file timetable_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "variables.h"
#include "table/strings.h"
#include "command.h"
#include "vehicle.h"


/**
 * Add or remove waiting times from an order.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Various bitstuffed elements
 * - p1 = (bit  0-15) - Vehicle with the orders to change.
 * - p1 = (bit 16-23) - Order index to modify.
 * - p1 = (bit    24) - Whether to change the waiting time or the travelling
 *                      time.
 * @param p2 The amount of time to wait.
 */
CommandCost CmdChangeTimetable(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!_patches.timetabling) return CMD_ERROR;

	VehicleID veh = GB(p1, 0, 16);
	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	VehicleOrderID order_number = GB(p1, 16, 8);
	Order *order = GetVehicleOrder(v, order_number);
	if (order == NULL) return CMD_ERROR;

	bool is_journey = HASBIT(p1, 24);
	if (!is_journey) {
		if (order->type != OT_GOTO_STATION) return_cmd_error(STR_TIMETABLE_ONLY_WAIT_AT_STATIONS);
		if (_patches.new_nonstop && (order->flags & OF_NON_STOP)) return_cmd_error(STR_TIMETABLE_NOT_STOPPING_HERE);
	}

	if (flags & DC_EXEC) {
		if (is_journey) {
			order->travel_time = p2;
		} else {
			order->wait_time = p2;
		}

		if (v->cur_order_index == order_number && HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
			if (is_journey) {
				v->current_order.travel_time = p2;
			} else {
				v->current_order.wait_time = p2;
			}
		}

		InvalidateWindow(WC_VEHICLE_TIMETABLE, v->index);
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
	if (!_patches.timetabling) return CMD_ERROR;

	VehicleID veh = GB(p1, 0, 16);
	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	Vehicle *v = GetVehicle(veh);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->lateness_counter = 0;
	}

	return CommandCost();
}


void UpdateVehicleTimetable(Vehicle *v, bool travelling)
{
	uint timetabled = travelling ? v->current_order.travel_time : v->current_order.wait_time;
	uint time_taken = v->current_order_time;

	v->current_order_time = 0;

	if (!_patches.timetabling || timetabled == 0) return;

	v->lateness_counter -= (timetabled - time_taken);

	InvalidateWindow(WC_VEHICLE_TIMETABLE, v->index);
}
