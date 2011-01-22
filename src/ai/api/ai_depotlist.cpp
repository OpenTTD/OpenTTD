/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_depotlist.cpp Implementation of AIDepotList and friends. */

#include "../../stdafx.h"
#include "ai_depotlist.hpp"
#include "../../company_func.h"
#include "../../depot_base.h"
#include "../../station_base.h"

AIDepotList::AIDepotList(AITile::TransportType transport_type)
{
	::TileType tile_type;
	switch (transport_type) {
		default: return;

		case AITile::TRANSPORT_ROAD:  tile_type = ::MP_ROAD; break;
		case AITile::TRANSPORT_RAIL:  tile_type = ::MP_RAILWAY; break;
		case AITile::TRANSPORT_WATER: tile_type = ::MP_WATER; break;

		case AITile::TRANSPORT_AIR: {
			/* Hangars are not seen as real depots by the depot code. */
			const Station *st;
			FOR_ALL_STATIONS(st) {
				if (st->owner == ::_current_company) {
					for (uint i = 0; i < st->airport.GetNumHangars(); i++) {
						this->AddItem(st->airport.GetHangarTile(i));
					}
				}
			}
			return;
		}
	}

	/* Handle 'standard' depots. */
	const Depot *depot;
	FOR_ALL_DEPOTS(depot) {
		if (::GetTileOwner(depot->xy) == ::_current_company && ::IsTileType(depot->xy, tile_type)) this->AddItem(depot->xy);
	}
}
