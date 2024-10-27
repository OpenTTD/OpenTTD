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
#include "../station_map.h"
#include "../tunnelbridge_map.h"

#include "../safeguards.h"

/** Container for a label for rail or road type conversion. */
template <typename T>
struct LabelObject {
	T label = {}; ///< Label of rail or road type.
	uint8_t subtype = 0; ///< Subtype of type (road or tram).
};

static std::vector<LabelObject<RailTypeLabel>> _railtype_list;
static std::vector<LabelObject<RoadTypeLabel>> _roadtype_list;

/**
 * Test if any saved rail type labels are different to the currently loaded
 * rail types. Rail types stored in the map will be converted if necessary.
 */
static void ConvertRailTypes()
{
	std::vector<RailType> railtype_conversion_map;
	bool needs_conversion = false;

	for (auto it = std::begin(_railtype_list); it != std::end(_railtype_list); ++it) {
		RailType rt = GetRailTypeByLabel(it->label);
		if (rt == INVALID_RAILTYPE) {
			rt = RAILTYPE_RAIL;
		}

		railtype_conversion_map.push_back(rt);

		/* Conversion is needed if the rail type is in a different position than the list. */
		if (it->label != 0 && rt != std::distance(std::begin(_railtype_list), it)) needs_conversion = true;
	}
	if (!needs_conversion) return;

	for (const auto t : Map::Iterate()) {
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

/**
 * Test if any saved road type labels are different to the currently loaded
 * road types. Road types stored in the map will be converted if necessary.
 */
static void ConvertRoadTypes()
{
	std::vector<RoadType> roadtype_conversion_map;
	bool needs_conversion = false;
	for (auto it = std::begin(_roadtype_list); it != std::end(_roadtype_list); ++it) {
		RoadType rt = GetRoadTypeByLabel(it->label);
		if (rt == INVALID_ROADTYPE || GetRoadTramType(rt) != it->subtype) {
			rt = it->subtype ? ROADTYPE_TRAM : ROADTYPE_ROAD;
		}

		roadtype_conversion_map.push_back(rt);

		/* Conversion is needed if the road type is in a different position than the list. */
		if (it->label != 0 && rt != std::distance(std::begin(_roadtype_list), it)) needs_conversion = true;
	}
	if (!needs_conversion) return;

	for (TileIndex t : Map::Iterate()) {
		switch (GetTileType(t)) {
			case MP_ROAD:
				if (RoadType rt = GetRoadTypeRoad(t); rt != INVALID_ROADTYPE) SetRoadTypeRoad(t, roadtype_conversion_map[rt]);
				if (RoadType rt = GetRoadTypeTram(t); rt != INVALID_ROADTYPE) SetRoadTypeTram(t, roadtype_conversion_map[rt]);
				break;

			case MP_STATION:
				if (IsStationRoadStop(t) || IsRoadWaypoint(t)) {
					if (RoadType rt = GetRoadTypeRoad(t); rt != INVALID_ROADTYPE) SetRoadTypeRoad(t, roadtype_conversion_map[rt]);
					if (RoadType rt = GetRoadTypeTram(t); rt != INVALID_ROADTYPE) SetRoadTypeTram(t, roadtype_conversion_map[rt]);
				}
				break;

			case MP_TUNNELBRIDGE:
				if (GetTunnelBridgeTransportType(t) == TRANSPORT_ROAD) {
					if (RoadType rt = GetRoadTypeRoad(t); rt != INVALID_ROADTYPE) SetRoadTypeRoad(t, roadtype_conversion_map[rt]);
					if (RoadType rt = GetRoadTypeTram(t); rt != INVALID_ROADTYPE) SetRoadTypeTram(t, roadtype_conversion_map[rt]);
				}
				break;

			default:
				break;
		}
	}
}

/** Populate label lists with current values. */
static void SetCurrentLabelLists()
{
	_railtype_list.clear();
	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		_railtype_list.push_back({GetRailTypeInfo(rt)->label, 0});
	}

	_roadtype_list.clear();
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		_roadtype_list.push_back({GetRoadTypeInfo(rt)->label, GetRoadTramType(rt)});
	}
}

/** Perform rail type and road type conversion if necessary. */
void AfterLoadLabelMaps()
{
	ConvertRailTypes();
	ConvertRoadTypes();

	SetCurrentLabelLists();
}

void ResetLabelMaps()
{
	_railtype_list.clear();
	_roadtype_list.clear();
}

struct RAILChunkHandler : ChunkHandler {
	RAILChunkHandler() : ChunkHandler('RAIL', CH_TABLE) {}

	static inline const SaveLoad description[] = {
		SLE_VAR(LabelObject<RailTypeLabel>, label, SLE_UINT32),
	};

	void Save() const override
	{
		SlTableHeader(description);

		LabelObject<RailTypeLabel> lo;
		for (RailType r = RAILTYPE_BEGIN; r != RAILTYPE_END; r++) {
			lo.label = GetRailTypeInfo(r)->label;

			SlSetArrayIndex(r);
			SlObject(&lo, description);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(description, _label_object_sl_compat);

		_railtype_list.reserve(RAILTYPE_END);

		LabelObject<RailTypeLabel> lo;

		while (SlIterateArray() != -1) {
			SlObject(&lo, slt);
			_railtype_list.push_back(lo);
		}
	}
};

struct ROTTChunkHandler : ChunkHandler {
	ROTTChunkHandler() : ChunkHandler('ROTT', CH_TABLE) {}

	static inline const SaveLoad description[] = {
		SLE_VAR(LabelObject<RoadTypeLabel>, label, SLE_UINT32),
		SLE_VAR(LabelObject<RoadTypeLabel>, subtype, SLE_UINT8),
	};

	void Save() const override
	{
		SlTableHeader(description);

		LabelObject<RoadTypeLabel> lo;
		for (RoadType r = ROADTYPE_BEGIN; r != ROADTYPE_END; r++) {
			const RoadTypeInfo *rti = GetRoadTypeInfo(r);
			lo.label = rti->label;
			lo.subtype = GetRoadTramType(r);

			SlSetArrayIndex(r);
			SlObject(&lo, description);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(description, _label_object_sl_compat);

		_roadtype_list.reserve(ROADTYPE_END);

		LabelObject<RoadTypeLabel> lo;

		while (SlIterateArray() != -1) {
			SlObject(&lo, slt);
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

