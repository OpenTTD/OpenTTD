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
#include "timer/timer_game_calendar.h"
#include "depot_base.h"
#include "town.h"

#include "safeguards.h"

/* virtual */ uint32_t RoadTypeScopeResolver::GetRandomBits() const
{
	uint tmp = CountBits(this->tile.base() + (TileX(this->tile) + TileY(this->tile)) * TILE_SIZE);
	return GB(tmp, 0, 2);
}

/* virtual */ uint32_t RoadTypeScopeResolver::GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const
{
	if (this->tile == INVALID_TILE) {
		switch (variable) {
			case 0x40: return 0;
			case 0x41: return 0;
			case 0x42: return 0;
			case 0x43: return TimerGameCalendar::date.base();
			case 0x44: return HZB_TOWN_EDGE;
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
	}

	Debug(grf, 1, "Unhandled road type tile variable 0x{:X}", variable);

	*available = false;
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
