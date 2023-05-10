/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_cmd.cpp %Command Handling for depots. */

#include "stdafx.h"
#include "command_func.h"
#include "depot_base.h"
#include "company_func.h"
#include "string_func.h"
#include "town.h"
#include "vehicle_gui.h"
#include "vehiclelist.h"
#include "window_func.h"
#include "depot_cmd.h"
#include "timer/timer_game_tick.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Check whether the given name is globally unique amongst depots.
 * @param name The name to check.
 * @return True if there is no depot with the given name.
 */
static bool IsUniqueDepotName(const std::string &name)
{
	for (const Depot *d : Depot::Iterate()) {
		if (!d->name.empty() && d->name == name) return false;
	}

	return true;
}

/**
 * Rename a depot.
 * @param flags type of operation
 * @param depot_id id of depot
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameDepot(DoCommandFlag flags, DepotID depot_id, const std::string &text)
{
	Depot *d = Depot::GetIfValid(depot_id);
	if (d == nullptr) return CMD_ERROR;

	CommandCost ret = CheckOwnership(d->owner);
	if (ret.Failed()) return ret;

	bool reset = text.empty();

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_DEPOT_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueDepotName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		if (reset) {
			d->name.clear();
			MakeDefaultName(d);
		} else {
			d->name = text;
		}

		/* Update the orders and depot */
		SetWindowClassesDirty(WC_VEHICLE_ORDERS);
		SetWindowDirty(WC_VEHICLE_DEPOT, d->index);

		/* Update the depot list */
		SetWindowDirty(GetWindowClassForVehicleType(d->veh_type), VehicleListIdentifier(VL_DEPOT_LIST, d->veh_type, d->owner, d->index).Pack());
	}
	return CommandCost();
}

void OnTick_Depot()
{
	if (_game_mode == GM_EDITOR) return;

	/* Clean up demolished depots. */
	for (Depot *d : Depot::Iterate()) {
		if (d->IsInUse()) continue;
		if ((TimerGameTick::counter + d->index) % Ticks::DEPOT_REMOVAL_TICKS != 0) continue;
		if (--d->delete_ctr != 0) continue;
		delete d;
	}
}


/**
 * Look for or check depot to join to, building a new one if necessary.
 * @param ta The area of the new depot.
 * @param veh_type The vehicle type of the new depot.
 * @param join_to DepotID of the depot to join to.
 *                     If INVALID_DEPOT, look whether it is possible to join to an existing depot.
 *                     If NEW_DEPOT, directly create a new depot.
 * @param depot The pointer to the depot.
 * @param adjacent Whether adjacent depots are allowed
 * @return command cost with the error or 'okay'
 */
CommandCost FindJoiningDepot(TileArea ta, VehicleType veh_type, DepotID &join_to, Depot *&depot, bool adjacent, DoCommandFlag flags)
{
	/* Look for a joining depot if needed. */
	if (join_to == INVALID_DEPOT) {
		assert(depot == nullptr);
		DepotID closest_depot = INVALID_DEPOT;

		TileArea check_area(ta);
		check_area.Expand(1);

		/* Check around to see if there's any depot there. */
		for (TileIndex tile_cur : check_area) {
			if (IsValidTile(tile_cur) && IsDepotTile(tile_cur)) {
				Depot *d = Depot::GetByTile(tile_cur);
				assert(d != nullptr);
				if (d->veh_type != veh_type) continue;
				if (d->owner != _current_company) continue;

				if (closest_depot == INVALID_DEPOT) {
					closest_depot = d->index;
				} else if (closest_depot != d->index) {
					if (!adjacent) return_cmd_error(STR_ERROR_ADJOINS_MORE_THAN_ONE_EXISTING_DEPOT);
				}
			}
		}

		if (closest_depot != INVALID_DEPOT) {
			assert(Depot::IsValidID(closest_depot));
			depot = Depot::Get(closest_depot);
		}

		join_to = depot == nullptr ? NEW_DEPOT : depot->index;
	}

	/* At this point, join_to is NEW_DEPOT or a valid DepotID. */

	if (join_to == NEW_DEPOT) {
		/* New depot needed. */
		if (!Depot::CanAllocateItem()) return CMD_ERROR;
		if (flags & DC_EXEC) {
			depot = new Depot(ta.tile, veh_type, _current_company);
			depot->build_date = TimerGameCalendar::date;
		}
	} else {
		/* Joining depots. */
		assert(Depot::IsValidID(join_to));
		depot = Depot::Get(join_to);
		assert(depot->owner == _current_company);
		assert(depot->veh_type == veh_type);
		return depot->BeforeAddTiles(ta);
	}

	return CommandCost();
}
