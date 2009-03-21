/* $Id$ */

/** @file labelmaps_sl.cpp Code handling saving and loading of rail type label mappings */

#include "../stdafx.h"
#include "../strings_type.h"
#include "../rail.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../rail_map.h"
#include "../road_map.h"
#include "../station_map.h"
#include "../tunnelbridge_map.h"
#include "../core/alloc_func.hpp"
#include "../core/smallvec_type.hpp"
#include "../settings_type.h"

#include "saveload.h"

static SmallVector<RailTypeLabel, RAILTYPE_END> _railtype_list;

/**
 * Test if any saved rail type labels are different to the currently loaded
 * rail types, which therefore requires conversion.
 * @return true if (and only if) conversion due to rail type changes is needed.
 */
static bool NeedRailTypeConversion()
{
	for (uint i = 0; i < _railtype_list.Length(); i++) {
		if ((RailType)i < RAILTYPE_END) {
			const RailtypeInfo *rti = GetRailTypeInfo((RailType)i);
			if (rti->label != _railtype_list[i]) return true;
		} else {
			if (_railtype_list[i] != 0) return true;
		}
	}

	/* No rail type conversion is necessary */
	return false;
}

void AfterLoadLabelMaps()
{
	if (NeedRailTypeConversion()) {
		SmallVector<RailType, RAILTYPE_END> railtype_conversion_map;

		for (uint i = 0; i < _railtype_list.Length(); i++) {
			RailType r = GetRailTypeByLabel(_railtype_list[i]);
			if (r == INVALID_RAILTYPE) r = RAILTYPE_BEGIN;

			*railtype_conversion_map.Append() = r;
		}

		for (TileIndex t = 0; t < MapSize(); t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					SetRailType(t, railtype_conversion_map[GetRailType(t)]);
					break;

				case MP_ROAD:
					if (IsLevelCrossing(t)) {
						SetRailType(t, railtype_conversion_map[GetRailType(t)]);
					}
					break;

				case MP_STATION:
					if (IsRailwayStation(t)) {
						SetRailType(t, railtype_conversion_map[GetRailType(t)]);
					}
					break;

				case MP_TUNNELBRIDGE:
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) {
						SetRailType(t, railtype_conversion_map[GetRailType(t)]);
					}
					break;

				default:
					break;
			}
		}
	}

	_railtype_list.Clear();
}

/** Container for a label for SaveLoad system */
struct LabelObject {
	uint32 label;
};

static const SaveLoad _label_object_desc[] = {
	SLE_VAR(LabelObject, label, SLE_UINT32),
	SLE_END(),
};

static void Save_RAIL()
{
	LabelObject lo;

	for (RailType r = RAILTYPE_BEGIN; r != RAILTYPE_END; r++) {
		lo.label = GetRailTypeInfo(r)->label;

		SlSetArrayIndex(r);
		SlObject(&lo, _label_object_desc);
	}
}

static void Load_RAIL()
{
	_railtype_list.Clear();

	LabelObject lo;
	int index;

	while ((index = SlIterateArray()) != -1) {
		SlObject(&lo, _label_object_desc);
		*_railtype_list.Append() = (RailTypeLabel)lo.label;
	}
}

extern const ChunkHandler _labelmaps_chunk_handlers[] = {
	{ 'RAIL', Save_RAIL, Load_RAIL, CH_ARRAY | CH_LAST},
};

