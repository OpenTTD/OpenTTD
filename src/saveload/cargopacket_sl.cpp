/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargopacket_sl.cpp Code handling saving and loading of cargo packets */

#include "../stdafx.h"
#include "../vehicle_base.h"
#include "../station_base.h"

#include "saveload.h"

/**
 * Savegame conversion for cargopackets.
 */
/* static */ void CargoPacket::AfterLoad()
{
	if (IsSavegameVersionBefore(44)) {
		Vehicle *v;
		/* If we remove a station while cargo from it is still enroute, payment calculation will assume
		 * 0, 0 to be the source of the cargo, resulting in very high payments usually. v->source_xy
		 * stores the coordinates, preserving them even if the station is removed. However, if a game is loaded
		 * where this situation exists, the cargo-source information is lost. in this case, we set the source
		 * to the current tile of the vehicle to prevent excessive profits
		 */
		FOR_ALL_VEHICLES(v) {
			const VehicleCargoList::List *packets = v->cargo.Packets();
			for (VehicleCargoList::ConstIterator it(packets->begin()); it != packets->end(); it++) {
				CargoPacket *cp = *it;
				cp->source_xy = Station::IsValidID(cp->source) ? Station::Get(cp->source)->xy : v->tile;
				cp->loaded_at_xy = cp->source_xy;
			}
		}

		/* Store position of the station where the goods come from, so there
		 * are no very high payments when stations get removed. However, if the
		 * station where the goods came from is already removed, the source
		 * information is lost. In that case we set it to the position of this
		 * station */
		Station *st;
		FOR_ALL_STATIONS(st) {
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				GoodsEntry *ge = &st->goods[c];

				const StationCargoList::List *packets = ge->cargo.Packets();
				for (StationCargoList::ConstIterator it(packets->begin()); it != packets->end(); it++) {
					CargoPacket *cp = *it;
					cp->source_xy = Station::IsValidID(cp->source) ? Station::Get(cp->source)->xy : st->xy;
					cp->loaded_at_xy = cp->source_xy;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(120)) {
		/* CargoPacket's source should be either INVALID_STATION or a valid station */
		CargoPacket *cp;
		FOR_ALL_CARGOPACKETS(cp) {
			if (!Station::IsValidID(cp->source)) cp->source = INVALID_STATION;
		}
	}

	if (!IsSavegameVersionBefore(68)) {
		/* Only since version 68 we have cargo packets. Savegames from before used
		 * 'new CargoPacket' + cargolist.Append so their caches are already
		 * correct and do not need rebuilding. */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) v->cargo.InvalidateCache();

		Station *st;
		FOR_ALL_STATIONS(st) {
			for (CargoID c = 0; c < NUM_CARGO; c++) st->goods[c].cargo.InvalidateCache();
		}
	}
}

/**
 * Wrapper function to get the CargoPacket's internal structure while
 * some of the variables itself are private.
 * @return the saveload description for CargoPackets.
 */
const SaveLoad *GetCargoPacketDesc()
{
	static const SaveLoad _cargopacket_desc[] = {
		     SLE_VAR(CargoPacket, source,          SLE_UINT16),
		     SLE_VAR(CargoPacket, source_xy,       SLE_UINT32),
		     SLE_VAR(CargoPacket, loaded_at_xy,    SLE_UINT32),
		     SLE_VAR(CargoPacket, count,           SLE_UINT16),
		     SLE_VAR(CargoPacket, days_in_transit, SLE_UINT8),
		     SLE_VAR(CargoPacket, feeder_share,    SLE_INT64),
		 SLE_CONDVAR(CargoPacket, source_type,     SLE_UINT8,  125, SL_MAX_VERSION),
		 SLE_CONDVAR(CargoPacket, source_id,       SLE_UINT16, 125, SL_MAX_VERSION),

		/* Used to be paid_for, but that got changed. */
		SLE_CONDNULL(1, 0, 120),

		SLE_END()
	};
	return _cargopacket_desc;
}

/**
 * Save the cargo packets.
 */
static void Save_CAPA()
{
	CargoPacket *cp;

	FOR_ALL_CARGOPACKETS(cp) {
		SlSetArrayIndex(cp->index);
		SlObject(cp, GetCargoPacketDesc());
	}
}

/**
 * Load the cargo packets.
 */
static void Load_CAPA()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		CargoPacket *cp = new (index) CargoPacket();
		SlObject(cp, GetCargoPacketDesc());
	}
}

/** Chunk handlers related to cargo packets. */
extern const ChunkHandler _cargopacket_chunk_handlers[] = {
	{ 'CAPA', Save_CAPA, Load_CAPA, NULL, NULL, CH_ARRAY | CH_LAST},
};
