/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bridge_sl.cpp Code handling saving and loading of bridges */

#include "../stdafx.h"

#include "saveload.h"

#include "../bridge.h"
#include "../bridge_map.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

static const SaveLoad _bridge_desc[] = {
		SLE_VAR(Bridge, build_date,                 SLE_UINT32),
		SLE_VAR(Bridge, type,                       SLE_VAR_U32 | SLE_FILE_U16),
		SLE_REF(Bridge, town,                       REF_TOWN),
		SLE_VAR(Bridge, heads[0],                   SLE_UINT32),
		SLE_VAR(Bridge, heads[1],                   SLE_UINT32),
		SLE_VAR(Bridge, random,                     SLE_UINT16),
};


struct BRDSChunkHandler : ChunkHandler {
	BRDSChunkHandler() : ChunkHandler('BRDS', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_bridge_desc);

		/* Write the objects */
		for (Bridge *b : Bridge::Iterate()) {
			SlSetArrayIndex(b->index);
			SlObject(b, _bridge_desc);
		}
	}

	void Load() const override
	{
		SlTableHeader(_bridge_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			Bridge *b = new (index) Bridge();
			SlObject(b, _bridge_desc);

			Axis axis = DiagDirToAxis(DiagdirBetweenTiles(b->heads[0], b->heads[1]));
			uint pos = axis == AXIS_X ? TileY(b->heads[0]) : TileX(b->heads[0]);

			_bridge_index[axis].Insert(pos, index);
		}
	}

	void FixPointers() const override
	{
		for (Bridge *b : Bridge::Iterate()) {
			SlObject(b, _bridge_desc);
		}
	}
};

struct BRIDChunkHandler : NewGRFMappingChunkHandler {
	BRIDChunkHandler() : NewGRFMappingChunkHandler('BRID', _bridge_mngr) {}
};

static const BRIDChunkHandler BRID;
static const BRDSChunkHandler BRDS;
static const ChunkHandlerRef bridge_chunk_handlers[] = {
	BRID,
	BRDS,
};

extern const ChunkHandlerTable _bridge_chunk_handlers(bridge_chunk_handlers);
