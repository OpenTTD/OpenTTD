/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file labelmaps_sl.cpp Code handling saving and loading of rail type label mappings */

#include "../stdafx.h"
#include "../station_map.h"
#include "../tunnelbridge_map.h"

#include "saveload.h"

#include "../safeguards.h"

static std::vector<RailTypeLabel> _railtype_list;

/**
 * Test if any saved rail type labels are different to the currently loaded
 * rail types, which therefore requires conversion.
 * @return true if (and only if) conversion due to rail type changes is needed.
 */
static bool NeedRailTypeConversion()
{
	for (uint i = 0; i < _railtype_list.size(); i++) {
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
		std::vector<RailType> railtype_conversion_map;

		for (uint i = 0; i < _railtype_list.size(); i++) {
			RailType r = GetRailTypeByLabel(_railtype_list[i]);
			if (r == INVALID_RAILTYPE) r = RAILTYPE_BEGIN;

			railtype_conversion_map.push_back(r);
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
					if (HasStationRail(t)) {
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

	_railtype_list.clear();
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
	_railtype_list.clear();

	LabelObject lo;

	while (SlIterateArray() != -1) {
		SlObject(&lo, _label_object_desc);
		_railtype_list.push_back((RailTypeLabel)lo.label);
	}
}

extern const ChunkHandler _labelmaps_chunk_handlers[] = {
	{ 'RAIL', Save_RAIL, Load_RAIL, NULL, NULL, CH_ARRAY | CH_LAST},
};

