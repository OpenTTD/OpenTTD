/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot.cpp Handling of depots. */

#include "stdafx.h"
#include "depot_base.h"
#include "order_backup.h"
#include "order_func.h"
#include "window_func.h"
#include "core/pool_func.hpp"
#include "vehicle_gui.h"
#include "vehiclelist.h"
#include "command_func.h"
#include "vehicle_base.h"

#include "safeguards.h"

#include "table/strings.h"

/** All our depots tucked away in a pool. */
DepotPool _depot_pool("Depot");
INSTANTIATE_POOL_METHODS(Depot)

/**
 * Clean up a depot
 */
Depot::~Depot()
{
	if (CleaningPool()) return;

	if (this->owner == INVALID_OWNER) {
		/* Deleting depot remnants of TTD savegames while saveload conversion. */
		assert(this->veh_type == VEH_INVALID);
		return;
	}

	/* Clear the order backup. */
	OrderBackup::Reset(this->index, false);

	/* Make sure no vehicle is going to the old depot. */
	for (Vehicle *v : Vehicle::Iterate()) {
		if (v->First() != v) continue;
		if (!v->current_order.IsType(OT_GOTO_DEPOT)) continue;
		if (v->current_order.GetDestination() != this->index) continue;

		v->current_order.MakeDummy();
	}

	/* Clear the depot from all order-lists */
	RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, this->index);

	/* Delete the depot-window */
	CloseWindowById(WC_VEHICLE_DEPOT, this->index);

	/* Delete the depot list */
	CloseWindowById(GetWindowClassForVehicleType(this->veh_type),
			VehicleListIdentifier(VL_DEPOT_LIST,
			this->veh_type, this->owner, this->index).Pack());
}

/**
 * Of all the depot parts a depot has, return the best destination for a vehicle.
 * @param v The vehicle.
 * @param dep The depot vehicle \a v is heading for.
 * @return The closest part of depot to vehicle v.
 */
TileIndex Depot::GetBestDepotTile(Vehicle *v) const
{
	assert(this->veh_type == v->type);
	TileIndex best_depot = INVALID_TILE;
	uint best_distance = UINT_MAX;

	for (const auto &tile : this->depot_tiles) {
		uint new_distance = DistanceManhattan(v->tile, tile);
		if (new_distance < best_distance) {
			best_depot = tile;
			best_distance = new_distance;
		}
	}

	return best_depot;
}

/**
 * Check we can add some tiles to this depot.
 * @param ta The affected tile area.
 * @return Whether it is possible to add the tiles or an error message.
 */
CommandCost Depot::BeforeAddTiles(TileArea ta)
{
	assert(ta.tile != INVALID_TILE);

	if (this->ta.tile != INVALID_TILE) {
		/* Important when the old rect is completely inside the new rect, resp. the old one was empty. */
		ta.Add(this->ta.tile);
		ta.Add(TileAddXY(this->ta.tile, this->ta.w - 1, this->ta.h - 1));
	}

	if ((ta.w > _settings_game.depot.depot_spread) || (ta.h > _settings_game.depot.depot_spread)) {
		return_cmd_error(STR_ERROR_DEPOT_TOO_SPREAD_OUT);
	}
	return CommandCost();
}

/**
 * Add some tiles to this depot and rescan area for depot_tiles.
 * @param ta Affected tile area
 * @param adding Whether adding or removing depot tiles.
 */
void Depot::AfterAddRemove(TileArea ta, bool adding)
{
	assert(ta.tile != INVALID_TILE);

	if (adding) {
		if (this->ta.tile != INVALID_TILE) {
			ta.Add(this->ta.tile);
			ta.Add(TileAddXY(this->ta.tile, this->ta.w - 1, this->ta.h - 1));
		}
	} else {
		ta = this->ta;
	}

	this->ta.Clear();

	for (TileIndex tile : ta) {
		if (!IsDepotTile(tile)) continue;
		if (GetDepotIndex(tile) != this->index) continue;
		this->ta.Add(tile);
	}

	if (this->ta.tile != INVALID_TILE) {
		this->RescanDepotTiles();
		assert(!this->depot_tiles.empty());
		this->xy = this->depot_tiles[0];
		InvalidateWindowData(WC_SELECT_DEPOT, this->veh_type);
	} else {
		delete this;
	}
}

/**
 * Rescan depot_tiles. Done after AfterAddRemove and SaveLoad.
 * Updates the tiles of the depot and its railtypes/roadtypes...
 */
void Depot::RescanDepotTiles()
{
	this->depot_tiles.clear();
	RailTypes old_rail_types = this->r_types.rail_types;
	this->r_types.rail_types = RAILTYPES_NONE;

	for (TileIndex tile : this->ta) {
		if (!IsDepotTile(tile)) continue;
		if (GetDepotIndex(tile) != this->index) continue;
		this->depot_tiles.push_back(tile);
		switch (veh_type) {
			case VEH_ROAD:
				this->r_types.road_types |= GetPresentRoadTypes(tile);
				break;
			case VEH_TRAIN:
				this->r_types.rail_types |= (RailTypes)(1 << GetRailType(tile));
				break;
			case VEH_SHIP:
				/* Mark this ship depot has at least one part, so ships can be built. */
				this->r_types.rail_types |= INVALID_RAILTYPES;
				break;
			default: break;
		}
	}

	if (old_rail_types != this->r_types.rail_types) {
		InvalidateWindowData(WC_BUILD_VEHICLE, this->index, 0, true);
	}
}
