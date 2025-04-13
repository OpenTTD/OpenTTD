/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_roadtype.cpp NewGRF handling of road types. */

#include "stdafx.h"
#include "core/container_func.hpp"
#include "debug.h"
#include "newgrf_roadtype.h"
#include "newgrf_railtype.h"
#include "timer/timer_game_calendar.h"
#include "depot_base.h"
#include "town.h"
#include "tunnelbridge_map.h"

#include "safeguards.h"

/**
 * Variable 0x45 of road-/tram-/rail-types to query track types on a tile.
 *
 * Format: __RRttrr
 * - rr: Translated roadtype.
 * - tt: Translated tramtype.
 * - RR: Translated railtype.
 *
 * Special values for rr, tt, RR:
 * - 0xFF: Track not present on tile.
 * - 0xFE: Track present, but no matching entry in translation table.
 */
uint32_t GetTrackTypes(TileIndex tile, const GRFFile *grffile)
{
	uint8_t road = 0xFF;
	uint8_t tram = 0xFF;
	if (MayHaveRoad(tile)) {
		if (auto tt = GetRoadTypeRoad(tile); tt != INVALID_ROADTYPE) {
			road = GetReverseRoadTypeTranslation(tt, grffile);
			if (road == 0xFF) road = 0xFE;
		}
		if (auto tt = GetRoadTypeTram(tile); tt != INVALID_ROADTYPE) {
			tram = GetReverseRoadTypeTranslation(tt, grffile);
			if (tram == 0xFF) tram = 0xFE;
		}
	}
	uint8_t rail = 0xFF;
	if (auto tt = GetTileRailType(tile); tt != INVALID_RAILTYPE) {
		rail = GetReverseRailTypeTranslation(tt, grffile);
		if (rail == 0xFF) rail = 0xFE;
	}
	return road | tram << 8 | rail << 16;
}

/* virtual */ uint32_t RoadTypeScopeResolver::GetRandomBits() const
{
	uint tmp = CountBits(this->tile.base() + (TileX(this->tile) + TileY(this->tile)) * TILE_SIZE);
	return GB(tmp, 0, 2);
}

/* virtual */ uint32_t RoadTypeScopeResolver::GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const
{
	if (this->tile == INVALID_TILE) {
		switch (variable) {
			case 0x40: return 0;
			case 0x41: return 0;
			case 0x42: return 0;
			case 0x43: return TimerGameCalendar::date.base();
			case 0x44: return HZB_TOWN_EDGE;
			case 0x45: {
				auto rt = GetRoadTypeInfoIndex(this->rti);
				uint8_t local = GetReverseRoadTypeTranslation(rt, this->ro.grffile);
				if (local == 0xFF) local = 0xFE;
				if (RoadTypeIsRoad(rt)) {
					return 0xFFFF00 | local;
				} else {
					return 0xFF00FF | local << 8;
				}
			}
		}
	}

	switch (variable) {
		case 0x40: return GetTerrainType(this->tile, this->context);
		case 0x41: return 0;
		case 0x42: return IsLevelCrossingTile(this->tile) && IsCrossingBarred(this->tile);
		case 0x43:
			if (IsRoadDepotTile(this->tile)) return Depot::GetByTile(this->tile)->build_date.base();
			return TimerGameCalendar::date.base();
		case 0x44: {
			const Town *t = nullptr;
			if (IsRoadDepotTile(this->tile)) {
				t = Depot::GetByTile(this->tile)->town;
			} else {
				t = ClosestTownFromTile(this->tile, UINT_MAX);
			}
			return t != nullptr ? GetTownRadiusGroup(t, this->tile) : HZB_TOWN_EDGE;
		}
		case 0x45:
			return GetTrackTypes(this->tile, ro.grffile);
	}

	Debug(grf, 1, "Unhandled road type tile variable 0x{:X}", variable);

	available = false;
	return UINT_MAX;
}

GrfSpecFeature RoadTypeResolverObject::GetFeature() const
{
	RoadType rt = GetRoadTypeByLabel(this->roadtype_scope.rti->label, false);
	switch (GetRoadTramType(rt)) {
		case RTT_ROAD: return GSF_ROADTYPES;
		case RTT_TRAM: return GSF_TRAMTYPES;
		default: return GSF_INVALID;
	}
}

uint32_t RoadTypeResolverObject::GetDebugID() const
{
	return this->roadtype_scope.rti->label;
}

/**
 * Resolver object for road types.
 * @param rti Roadtype. nullptr in NewGRF Inspect window.
 * @param tile %Tile containing the track. For track on a bridge this is the southern bridgehead.
 * @param context Are we resolving sprites for the upper halftile, or on a bridge?
 * @param rtsg Roadpart of interest
 * @param param1 Extra parameter (first parameter of the callback, except roadtypes do not have callbacks).
 * @param param2 Extra parameter (second parameter of the callback, except roadtypes do not have callbacks).
 */
RoadTypeResolverObject::RoadTypeResolverObject(const RoadTypeInfo *rti, TileIndex tile, TileContext context, RoadTypeSpriteGroup rtsg, uint32_t param1, uint32_t param2)
	: ResolverObject(rti != nullptr ? rti->grffile[rtsg] : nullptr, CBID_NO_CALLBACK, param1, param2), roadtype_scope(*this, rti, tile, context)
{
	this->root_spritegroup = rti != nullptr ? rti->group[rtsg] : nullptr;
}

/**
 * Get the sprite to draw for the given tile.
 * @param rti The road type data (spec).
 * @param tile The tile to get the sprite for.
 * @param rtsg The type of sprite to draw.
 * @param content Where are we drawing the tile?
 * @param [out] num_results If not nullptr, return the number of sprites in the spriteset.
 * @return The sprite to draw.
 */
SpriteID GetCustomRoadSprite(const RoadTypeInfo *rti, TileIndex tile, RoadTypeSpriteGroup rtsg, TileContext context, uint *num_results)
{
	assert(rtsg < ROTSG_END);

	if (rti->group[rtsg] == nullptr) return 0;

	RoadTypeResolverObject object(rti, tile, context, rtsg);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr || group->GetNumResults() == 0) return 0;

	if (num_results) *num_results = group->GetNumResults();

	return group->GetResult();
}

/**
 * Translate an index to the GRF-local road/tramtype-translation table into a RoadType.
 * @param rtt       Whether to index the road- or tramtype-table.
 * @param tracktype Index into GRF-local translation table.
 * @param grffile   Originating GRF file.
 * @return RoadType or INVALID_ROADTYPE if the roadtype is unknown.
 */
RoadType GetRoadTypeTranslation(RoadTramType rtt, uint8_t tracktype, const GRFFile *grffile)
{
	/* Because OpenTTD mixes RoadTypes and TramTypes into the same type,
	 * the mapping of the original road- and tramtypes does not match the default GRF-local mapping.
	 * So, this function cannot provide any similar behavior to GetCargoTranslation() and GetRailTypeTranslation()
	 * when the GRF defines no translation table.
	 * But since there is only one default road/tram-type, this makes little sense anyway.
	 * So for GRF without translation table, we always return INVALID_ROADTYPE.
	 */

	if (grffile == nullptr) return INVALID_ROADTYPE;

	const auto &list = rtt == RTT_TRAM ? grffile->tramtype_list : grffile->roadtype_list;
	if (tracktype >= list.size()) return INVALID_ROADTYPE;

	/* Look up roadtype including alternate labels. */
	RoadType result = GetRoadTypeByLabel(list[tracktype]);

	/* Check whether the result is actually the wanted road/tram-type */
	if (result != INVALID_ROADTYPE && GetRoadTramType(result) != rtt) return INVALID_ROADTYPE;

	return result;
}

/**
 * Perform a reverse roadtype lookup to get the GRF internal ID.
 * @param roadtype The global (OpenTTD) roadtype.
 * @param grffile The GRF to do the lookup for.
 * @return the GRF internal ID.
 */
uint8_t GetReverseRoadTypeTranslation(RoadType roadtype, const GRFFile *grffile)
{
	/* No road type table present, return road type as-is */
	if (grffile == nullptr) return roadtype;

	const std::vector<RoadTypeLabel> *list = RoadTypeIsRoad(roadtype) ? &grffile->roadtype_list : &grffile->tramtype_list;
	if (list->empty()) return roadtype;

	/* Look for a matching road type label in the table */
	RoadTypeLabel label = GetRoadTypeInfo(roadtype)->label;

	int index = find_index(*list, label);
	if (index >= 0) return index;

	/* If not found, return as invalid */
	return 0xFF;
}

std::vector<LabelObject<RoadTypeLabel>> _roadtype_list;

/**
 * Test if any saved road type labels are different to the currently loaded
 * road types. Road types stored in the map will be converted if necessary.
 */
void ConvertRoadTypes()
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
				if (IsAnyRoadStop(t)) {
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

/** Populate road type label list with current values. */
void SetCurrentRoadTypeLabelList()
{
	_roadtype_list.clear();
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		_roadtype_list.emplace_back(GetRoadTypeInfo(rt)->label, GetRoadTramType(rt));
	}
}

void ClearRoadTypeLabelList()
{
	_roadtype_list.clear();
}
