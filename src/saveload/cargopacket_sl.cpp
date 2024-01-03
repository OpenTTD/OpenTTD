/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargopacket_sl.cpp Code handling saving and loading of cargo packets */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/cargopacket_sl_compat.h"

#include "../vehicle_base.h"
#include "../station_base.h"

#include "../safeguards.h"

/**
 * Savegame conversion for cargopackets.
 */
/* static */ void CargoPacket::AfterLoad()
{
	if (IsSavegameVersionBefore(SLV_44)) {
		/* If we remove a station while cargo from it is still en route, payment calculation will assume
		 * 0, 0 to be the source of the cargo, resulting in very high payments usually. v->source_xy
		 * stores the coordinates, preserving them even if the station is removed. However, if a game is loaded
		 * where this situation exists, the cargo-source information is lost. in this case, we set the source
		 * to the current tile of the vehicle to prevent excessive profits
		 */
		for (const Vehicle *v : Vehicle::Iterate()) {
			const CargoPacketList *packets = v->cargo.Packets();
			for (VehicleCargoList::ConstIterator it(packets->begin()); it != packets->end(); it++) {
				CargoPacket *cp = *it;
				cp->source_xy = Station::IsValidID(cp->first_station) ? Station::Get(cp->first_station)->xy : v->tile;
			}
		}

		/* Store position of the station where the goods come from, so there
		 * are no very high payments when stations get removed. However, if the
		 * station where the goods came from is already removed, the source
		 * information is lost. In that case we set it to the position of this
		 * station */
		for (Station *st : Station::Iterate()) {
			for (GoodsEntry &ge : st->goods) {
				const StationCargoPacketMap *packets = ge.cargo.Packets();
				for (StationCargoList::ConstIterator it(packets->begin()); it != packets->end(); it++) {
					CargoPacket *cp = *it;
					cp->source_xy = Station::IsValidID(cp->first_station) ? Station::Get(cp->first_station)->xy : st->xy;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_120)) {
		/* CargoPacket's source should be either INVALID_STATION or a valid station */
		for (CargoPacket *cp : CargoPacket::Iterate()) {
			if (!Station::IsValidID(cp->first_station)) cp->first_station = INVALID_STATION;
		}
	}

	if (!IsSavegameVersionBefore(SLV_68)) {
		/* Only since version 68 we have cargo packets. Savegames from before used
		 * 'new CargoPacket' + cargolist.Append so their caches are already
		 * correct and do not need rebuilding. */
		for (Vehicle *v : Vehicle::Iterate()) v->cargo.InvalidateCache();

		for (Station *st : Station::Iterate()) {
			for (GoodsEntry &ge : st->goods) ge.cargo.InvalidateCache();
		}
	}

	if (IsSavegameVersionBefore(SLV_181)) {
		for (Vehicle *v : Vehicle::Iterate()) v->cargo.KeepAll();
	}

	/* Before this version, we didn't track how far cargo actually traveled in vehicles. Make best-effort estimates of this. */
	if (IsSavegameVersionBefore(SLV_CARGO_TRAVELLED)) {
		/* Update the cargo-traveled in stations as if they arrived from the source tile. */
		for (Station *st : Station::Iterate()) {
			for (GoodsEntry &ge : st->goods) {
				for (auto it = ge.cargo.Packets()->begin(); it != ge.cargo.Packets()->end(); ++it) {
					for (CargoPacket *cp : it->second) {
						if (cp->source_xy != INVALID_TILE && cp->source_xy != st->xy) {
							cp->travelled.x = TileX(cp->source_xy) - TileX(st->xy);
							cp->travelled.y = TileY(cp->source_xy) - TileY(st->xy);
						}
					}
				}
			}
		}

		/* Update the cargo-traveled in vehicles as if they were loaded at the source tile. */
		for (Vehicle *v : Vehicle::Iterate()) {
			for (auto it = v->cargo.Packets()->begin(); it != v->cargo.Packets()->end(); it++) {
				if ((*it)->source_xy != INVALID_TILE) {
					(*it)->UpdateLoadingTile((*it)->source_xy);
				}
			}
		}
	}

#ifdef WITH_ASSERT
	/* in_vehicle is a NOSAVE; it tells if cargo is in a vehicle or not. Restore the value in here. */
	for (Vehicle *v : Vehicle::Iterate()) {
		for (auto it = v->cargo.Packets()->begin(); it != v->cargo.Packets()->end(); it++) {
			(*it)->in_vehicle = true;
		}
	}
#endif /* WITH_ASSERT */
}

/**
 * Wrapper function to get the CargoPacket's internal structure while
 * some of the variables itself are private.
 * @return the saveload description for CargoPackets.
 */
SaveLoadTable GetCargoPacketDesc()
{
	static const SaveLoad _cargopacket_desc[] = {
		SLE_VARNAME(CargoPacket, first_station, "source", SLE_UINT16),
		SLE_VAR(CargoPacket, source_xy,       SLE_UINT32),
		SLE_CONDVARNAME(CargoPacket, next_hop, "loaded_at_xy", SLE_FILE_U32 | SLE_VAR_U16, SL_MIN_VERSION, SLV_REMOVE_LOADED_AT_XY),
		SLE_CONDVARNAME(CargoPacket, next_hop, "loaded_at_xy", SLE_UINT16, SLV_REMOVE_LOADED_AT_XY, SL_MAX_VERSION),
		SLE_VAR(CargoPacket, count,           SLE_UINT16),
		SLE_CONDVARNAME(CargoPacket, periods_in_transit, "days_in_transit", SLE_FILE_U8 | SLE_VAR_U16, SL_MIN_VERSION, SLV_MORE_CARGO_AGE),
		SLE_CONDVARNAME(CargoPacket, periods_in_transit, "days_in_transit", SLE_UINT16, SLV_MORE_CARGO_AGE, SLV_PERIODS_IN_TRANSIT_RENAME),
		SLE_CONDVAR(CargoPacket, periods_in_transit, SLE_UINT16, SLV_PERIODS_IN_TRANSIT_RENAME, SL_MAX_VERSION),
		SLE_VAR(CargoPacket, feeder_share,    SLE_INT64),
		SLE_CONDVAR(CargoPacket, source_type,     SLE_UINT8,  SLV_125, SL_MAX_VERSION),
		SLE_CONDVAR(CargoPacket, source_id,       SLE_UINT16, SLV_125, SL_MAX_VERSION),
		SLE_CONDVAR(CargoPacket, travelled.x, SLE_INT16, SLV_CARGO_TRAVELLED, SL_MAX_VERSION),
		SLE_CONDVAR(CargoPacket, travelled.y, SLE_INT16, SLV_CARGO_TRAVELLED, SL_MAX_VERSION),
	};
	return _cargopacket_desc;
}

struct CAPAChunkHandler : ChunkHandler {
	CAPAChunkHandler() : ChunkHandler('CAPA', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(GetCargoPacketDesc());

		for (CargoPacket *cp : CargoPacket::Iterate()) {
			SlSetArrayIndex(cp->index);
			SlObject(cp, GetCargoPacketDesc());
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(GetCargoPacketDesc(), _cargopacket_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			CargoPacket *cp = new (index) CargoPacket();
			SlObject(cp, slt);
		}
	}
};

static const CAPAChunkHandler CAPA;
static const ChunkHandlerRef cargopacket_chunk_handlers[] = {
	CAPA,
};

extern const ChunkHandlerTable _cargopacket_chunk_handlers(cargopacket_chunk_handlers);
