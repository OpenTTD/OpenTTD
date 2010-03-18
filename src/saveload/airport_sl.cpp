/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport_sl.cpp Code handling saving and loading airport ids */

#include "../stdafx.h"
#include "../newgrf_commons.h"

#include "saveload.h"

/** Save and load the mapping between the airport id in the AirportSpec array
 * and the grf file it came from. */
static const SaveLoad _airport_id_mapping_desc[] = {
	SLE_VAR(EntityIDMapping, grfid,         SLE_UINT32),
	SLE_VAR(EntityIDMapping, entity_id,     SLE_UINT8),
	SLE_VAR(EntityIDMapping, substitute_id, SLE_UINT8),
	SLE_END()
};

static void Save_APID()
{
	uint i;
	uint j = _airport_mngr.GetMaxMapping();

	for (i = 0; i < j; i++) {
		SlSetArrayIndex(i);
		SlObject(&_airport_mngr.mapping_ID[i], _airport_id_mapping_desc);
	}
}

static void Load_APID()
{
	/* clear the current mapping stored.
	 * This will create the manager if ever it is not yet done */
	_airport_mngr.ResetMapping();

	uint max_id = _airport_mngr.GetMaxMapping();

	int index;
	while ((index = SlIterateArray()) != -1) {
		if ((uint)index >= max_id) break;
		SlObject(&_airport_mngr.mapping_ID[index], _airport_id_mapping_desc);
	}
}

extern const ChunkHandler _airport_chunk_handlers[] = {
	{ 'APID', Save_APID, Load_APID, NULL, CH_ARRAY | CH_LAST },
};
