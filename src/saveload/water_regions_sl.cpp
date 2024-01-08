/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file water_regions_sl.cpp Handles saving and loading of water region data */

#include "../stdafx.h"

#include "saveload.h"
#include "pathfinder/water_regions.h"

#include "../safeguards.h"

static const SaveLoad _water_region_desc[] = {
	SLE_VAR(WaterRegionSaveLoadInfo, initialized, SLE_BOOL),
};

struct WRGNChunkHandler : ChunkHandler {
	WRGNChunkHandler() : ChunkHandler('WRGN', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_water_region_desc);

		int index = 0;
		for (WaterRegionSaveLoadInfo &region : GetWaterRegionSaveLoadInfo()) {
			SlSetArrayIndex(index++);
			SlObject(&region, _water_region_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_water_region_desc);

		int index;

		std::vector<WaterRegionSaveLoadInfo> loaded_info;
		while ((index = SlIterateArray()) != -1) {
			WaterRegionSaveLoadInfo region_info;
			SlObject(&region_info, slt);
			loaded_info.push_back(std::move(region_info));
		}

		LoadWaterRegions(loaded_info);
	}
};

static const WRGNChunkHandler WRGN;
static const ChunkHandlerRef water_region_chunk_handlers[] = { WRGN };
extern const ChunkHandlerTable _water_region_chunk_handlers(water_region_chunk_handlers);
