/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file labelmaps_sl.cpp Code handling saving and loading of rail type label mappings */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/labelmaps_sl_compat.h"

#include "saveload_internal.h"
#include "../rail.h"
#include "../road.h"
#include "../newgrf_railtype.h"
#include "../newgrf_roadtype.h"

#include "../safeguards.h"

extern std::vector<LabelObject<RailTypeLabel>> _railtype_list;
extern std::vector<LabelObject<RoadTypeLabel>> _roadtype_list;

/** Perform rail type and road type conversion if necessary. */
void AfterLoadLabelMaps()
{
	ConvertRailTypes();
	ConvertRoadTypes();

	SetCurrentRailTypeLabelList();
	SetCurrentRoadTypeLabelList();
}

struct RAILChunkHandler : ChunkHandler {
	RAILChunkHandler() : ChunkHandler('RAIL', CH_TABLE) {}

	static inline const SaveLoad description[] = {
		SLE_VAR(LabelObject<RailTypeLabel>, label, SLE_UINT32),
		SLE_VAR(LabelObject<RailTypeLabel>, index, SLE_UINT8),
	};

	void Save() const override
	{
		SlTableHeader(description);

		int index = 0;
		LabelObject<RailTypeLabel> lo;
		for (const RailTypeInfo &rti : GetRailTypeInfo()) {
			if (rti.label == 0) continue;

			MapRailType map_railtype = _railtype_mapping.GetMappedType(rti.Index());
			if (map_railtype == RailTypeMapping::INVALID_MAP_TYPE) continue;

			lo.label = rti.label;
			lo.index = map_railtype.base();

			SlSetArrayIndex(index++);
			SlObject(&lo, description);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(description, _label_object_sl_compat);
		bool convert = IsSavegameVersionBefore(SLV_TRANSPORT_TYPE_MAPPING);

		_railtype_list.reserve(RailTypeMapping::MAX_SIZE);

		LabelObject<RailTypeLabel> lo;

		int index;
		while ((index = SlIterateArray()) != -1) {
			SlObject(&lo, slt);
			if (convert) lo.index = index;
			_railtype_list.push_back(lo);
		}
	}
};

struct ROTTChunkHandler : ChunkHandler {
	ROTTChunkHandler() : ChunkHandler('ROTT', CH_TABLE) {}

	static inline const SaveLoad description[] = {
		SLE_VAR(LabelObject<RoadTypeLabel>, label, SLE_UINT32),
		SLE_VAR(LabelObject<RoadTypeLabel>, index, SLE_UINT8),
		SLE_VAR(LabelObject<RoadTypeLabel>, subtype, SLE_UINT8),
	};

	void Save() const override
	{
		SlTableHeader(description);

		int index = 0;
		LabelObject<RoadTypeLabel> lo;
		for (const RoadTypeInfo &rti : GetRoadTypeInfo()) {
			if (rti.label == 0) continue;

			if (GetRoadTramType(rti.Index()) == RTT_ROAD) {
				MapRoadType map_roadtype = _roadtype_mapping.GetMappedType(rti.Index());
				if (map_roadtype == RoadTypeMapping::INVALID_MAP_TYPE) continue;

				lo.index = map_roadtype.base();
			} else {
				MapTramType map_tramtype = _tramtype_mapping.GetMappedType(rti.Index());
				if (map_tramtype == TramTypeMapping::INVALID_MAP_TYPE) continue;

				lo.index = map_tramtype.base();
			}

			lo.label = rti.label;
			lo.subtype = GetRoadTramType(rti.Index());

			SlSetArrayIndex(index++);
			SlObject(&lo, description);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(description, _label_object_sl_compat);
		bool convert = IsSavegameVersionBefore(SLV_TRANSPORT_TYPE_MAPPING);

		_roadtype_list.reserve(64);

		LabelObject<RoadTypeLabel> lo;

		int index;
		while ((index = SlIterateArray()) != -1) {
			SlObject(&lo, slt);
			if (convert) lo.index = index;
			_roadtype_list.push_back(lo);
		}
	}
};

static const RAILChunkHandler RAIL;
static const ROTTChunkHandler ROTT;

static const ChunkHandlerRef labelmaps_chunk_handlers[] = {
	RAIL,
	ROTT,
};

extern const ChunkHandlerTable _labelmaps_chunk_handlers(labelmaps_chunk_handlers);

