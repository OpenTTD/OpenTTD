/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file animated_tile_sl.cpp Code handling saving and loading of animated tiles */

#include "../stdafx.h"
#include "../tile_type.h"
#include "../core/alloc_func.hpp"
#include "../core/smallvec_type.hpp"

#include "saveload.h"

#include "../safeguards.h"

extern std::vector<TileIndex> _animated_tiles;

/**
 * Save the ANIT chunk.
 */
static void Save_ANIT()
{
	SlSetLength(_animated_tiles.size() * sizeof(_animated_tiles.front()));
	SlArray(_animated_tiles.data(), _animated_tiles.size(), SLE_UINT32);
}

/**
 * Load the ANIT chunk; the chunk containing the animated tiles.
 */
static void Load_ANIT()
{
	/* Before version 80 we did NOT have a variable length animated tile table */
	if (IsSavegameVersionBefore(SLV_80)) {
		/* In pre version 6, we has 16bit per tile, now we have 32bit per tile, convert it ;) */
		TileIndex anim_list[256];
		SlArray(anim_list, 256, IsSavegameVersionBefore(SLV_6) ? (SLE_FILE_U16 | SLE_VAR_U32) : SLE_UINT32);

		for (int i = 0; i < 256; i++) {
			if (anim_list[i] == 0) break;
			_animated_tiles.push_back(anim_list[i]);
		}
		return;
	}

	uint count = (uint)SlGetFieldLength() / sizeof(_animated_tiles.front());
	_animated_tiles.clear();
	_animated_tiles.resize(_animated_tiles.size() + count);
	SlArray(_animated_tiles.data(), count, SLE_UINT32);
}

/**
 * "Definition" imported by the saveload code to be able to load and save
 * the animated tile table.
 */
extern const ChunkHandler _animated_tile_chunk_handlers[] = {
	{ 'ANIT', Save_ANIT, Load_ANIT, nullptr, nullptr, CH_RIFF | CH_LAST},
};
