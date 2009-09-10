/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot.cpp Handling of depots. */

#include "stdafx.h"
#include "depot_base.h"
#include "company_type.h"
#include "order_func.h"
#include "window_func.h"
#include "core/bitmath_func.hpp"
#include "tile_map.h"
#include "water_map.h"
#include "core/pool_func.hpp"
#include "vehicle_gui.h"

DepotPool _depot_pool("Depot");
INSTANTIATE_POOL_METHODS(Depot)

/**
 * Clean up a depot
 */
Depot::~Depot()
{
	if (CleaningPool()) return;

	/* Clear the depot from all order-lists */
	RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, this->index);

	/* Delete the depot-window */
	DeleteWindowById(WC_VEHICLE_DEPOT, this->xy);

	/* Delete the depot list */
	WindowNumber wno = (this->index << 16) | VLW_DEPOT_LIST | GetTileOwner(this->xy);
	switch (GetTileType(this->xy)) {
		default: break; // It can happen there is no depot here anymore (TTO/TTD savegames)
		case MP_RAILWAY: DeleteWindowById(WC_TRAINS_LIST,  wno | (VEH_TRAIN << 11)); break;
		case MP_ROAD:    DeleteWindowById(WC_ROADVEH_LIST, wno | (VEH_ROAD  << 11)); break;
		case MP_WATER:   DeleteWindowById(WC_SHIPS_LIST,   wno | (VEH_SHIP  << 11)); break;
	}
}

void InitializeDepots()
{
	_depot_pool.CleanPool();
}
