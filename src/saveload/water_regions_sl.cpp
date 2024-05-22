/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file water_regions_sl.cpp Handles saving and loading of water region data */

#include "../stdafx.h"

#include "saveload.h"

#include "../safeguards.h"

extern void SlSkipArray();

/* Water Region savegame data is no longer used, but still needed for old savegames to load without errors. */
struct WaterRegionChunkHandler : ChunkHandler {
	WaterRegionChunkHandler() : ChunkHandler('WRGN', CH_READONLY)
	{}

	void Load() const override
	{
		SlTableHeader({});
		SlSkipArray();
	};
};

static const WaterRegionChunkHandler WRGN;
static const ChunkHandlerRef water_region_chunk_handlers[] = { WRGN };
extern const ChunkHandlerTable _water_region_chunk_handlers(water_region_chunk_handlers);
