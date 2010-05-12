/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_cmd.cpp Command Handling for depots. */

#include "stdafx.h"
#include "command_func.h"
#include "depot_base.h"
#include "functions.h"
#include "string_func.h"
#include "town.h"
#include "vehicle_gui.h"
#include "window_func.h"

#include "table/strings.h"


static bool IsUniqueDepotName(const char *name)
{
	const Depot *d;

	FOR_ALL_DEPOTS(d) {
		if (d->name != NULL && strcmp(d->name, name) == 0) return false;
	}

	return true;
}

/**
 * Rename a depot.
 * @param tile unused
 * @param flags type of operation
 * @param p1 id of depot
 * @param p2 unused
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameDepot(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Depot *d = Depot::GetIfValid(p1);
	if (d == NULL) return CMD_ERROR;

	CommandCost ret = CheckTileOwnership(d->xy);
	if (ret.Failed()) return ret;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (strlen(text) >= MAX_LENGTH_DEPOT_NAME_BYTES) return CMD_ERROR;
		if (!IsUniqueDepotName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		free(d->name);

		if (reset) {
			d->name = NULL;
			MakeDefaultName(d);
		} else {
			d->name = strdup(text);
		}

		/* Update the orders and depot */
		SetWindowClassesDirty(WC_VEHICLE_ORDERS);
		SetWindowDirty(WC_VEHICLE_DEPOT, d->xy);

		/* Update the depot list */
		WindowNumber wno = (d->index << 16) | VLW_DEPOT_LIST | GetTileOwner(d->xy);
		switch (GetTileType(d->xy)) {
			default: break;
			case MP_RAILWAY: SetWindowDirty(WC_TRAINS_LIST,  wno | (VEH_TRAIN << 11)); break;
			case MP_ROAD:    SetWindowDirty(WC_ROADVEH_LIST, wno | (VEH_ROAD  << 11)); break;
			case MP_WATER:   SetWindowDirty(WC_SHIPS_LIST,   wno | (VEH_SHIP  << 11)); break;
		}
	}
	return CommandCost();
}
