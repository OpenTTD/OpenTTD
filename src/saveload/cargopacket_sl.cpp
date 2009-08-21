/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargopacket_sl.cpp Code handling saving and loading of cargo packets */

#include "../stdafx.h"
#include "../cargopacket.h"

#include "saveload.h"

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

static void Save_CAPA()
{
	CargoPacket *cp;

	FOR_ALL_CARGOPACKETS(cp) {
		SlSetArrayIndex(cp->index);
		SlObject(cp, _cargopacket_desc);
	}
}

static void Load_CAPA()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		CargoPacket *cp = new (index) CargoPacket();
		SlObject(cp, _cargopacket_desc);
	}
}

extern const ChunkHandler _cargopacket_chunk_handlers[] = {
	{ 'CAPA', Save_CAPA, Load_CAPA, NULL, CH_ARRAY | CH_LAST},
};
