/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_depotlist.cpp Implementation of ScriptDepotList and friends. */

#include "../../stdafx.h"
#include "script_depotlist.hpp"
#include "../../depot_base.h"
#include "../../station_base.h"

#include "../../safeguards.h"

ScriptDepotList::ScriptDepotList(ScriptTile::TransportType transport_type)
{
	EnforceDeityOrCompanyModeValid_Void();
	::TileType tile_type;
	switch (transport_type) {
		default: return;

		case ScriptTile::TRANSPORT_ROAD:  tile_type = ::MP_ROAD; break;
		case ScriptTile::TRANSPORT_RAIL:  tile_type = ::MP_RAILWAY; break;
		case ScriptTile::TRANSPORT_WATER: tile_type = ::MP_WATER; break;

		case ScriptTile::TRANSPORT_AIR: {
			/* Hangars are not seen as real depots by the depot code. */
			bool is_deity = ScriptCompanyMode::IsDeity();
			CompanyID owner = ScriptObject::GetCompany();
			for (const Station *st : Station::Iterate()) {
				if (is_deity || st->owner == owner) {
					for (uint i = 0; i < st->airport.GetNumHangars(); i++) {
						this->AddItem(st->airport.GetHangarTile(i).base());
					}
				}
			}
			return;
		}
	}

	/* Handle 'standard' depots. */
	bool is_deity = ScriptCompanyMode::IsDeity();
	CompanyID owner = ScriptObject::GetCompany();
	for (const Depot *depot : Depot::Iterate()) {
		if ((is_deity || ::GetTileOwner(depot->xy) == owner) && ::IsTileType(depot->xy, tile_type)) this->AddItem(depot->xy.base());
	}
}
