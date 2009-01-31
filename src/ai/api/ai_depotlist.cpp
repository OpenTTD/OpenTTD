/* $Id$ */

/** @file ai_depotlist.cpp Implementation of AIDepotList and friends. */

#include "ai_depotlist.hpp"
#include "../../tile_map.h"
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
					const AirportFTAClass *afc = st->Airport();
					for (uint i = 0; i < afc->nof_depots; i++) {
						this->AddItem(st->xy + ToTileIndexDiff(afc->airport_depots[i]));
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
