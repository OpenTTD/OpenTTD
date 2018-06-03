/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_roadtype.cpp NewGRF handling of road types. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_roadtype.h"
#include "date_func.h"
#include "depot_base.h"
#include "town.h"

#include "safeguards.h"

/* virtual */ uint32 RoadTypeScopeResolver::GetRandomBits() const
{
	uint tmp = CountBits(this->tile + (TileX(this->tile) + TileY(this->tile)) * TILE_SIZE);
	return GB(tmp, 0, 2);
}

/* virtual */ uint32 RoadTypeScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	if (this->tile == INVALID_TILE) {
		switch (variable) {
			case 0x40: return 0;
			case 0x41: return 0;
			case 0x42: return 0;
			case 0x43: return _date;
			case 0x44: return HZB_TOWN_EDGE;
		}
	}

	switch (variable) {
		case 0x40: return GetTerrainType(this->tile, this->context);
		case 0x41: return 0;
		case 0x42: return IsLevelCrossingTile(this->tile) && IsCrossingBarred(this->tile);
		case 0x43:
			if (IsRoadDepotTile(this->tile)) return Depot::GetByTile(this->tile)->build_date;
			return _date;
		case 0x44: {
			const Town *t = NULL;
			if (IsRoadDepotTile(this->tile)) {
				t = Depot::GetByTile(this->tile)->town;
			} else if (IsTileType(this->tile, MP_ROAD)) {
				t = ClosestTownFromTile(this->tile, UINT_MAX);
			}
			return t != NULL ? GetTownRadiusGroup(t, this->tile) : HZB_TOWN_EDGE;
		}
	}

	DEBUG(grf, 1, "Unhandled road type tile variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

/* virtual */ const SpriteGroup *RoadTypeResolverObject::ResolveReal(const RealSpriteGroup *group) const
{
	if (group->num_loading > 0) return group->loading[0];
	if (group->num_loaded  > 0) return group->loaded[0];
	return NULL;
}

/**
 * Constructor of the roadtype scope resolvers.
 * @param ro Surrounding resolver.
 * @param tile %Tile containing the track. For track on a bridge this is the southern bridgehead.
 * @param context Are we resolving sprites for the upper halftile, or on a bridge?
 */
RoadTypeScopeResolver::RoadTypeScopeResolver(ResolverObject &ro, TileIndex tile, TileContext context) : ScopeResolver(ro)
{
	this->tile = tile;
	this->context = context;
}

/**
 * Resolver object for road types.
 * @param rti Roadtype. NULL in NewGRF Inspect window.
 * @param tile %Tile containing the track. For track on a bridge this is the southern bridgehead.
 * @param context Are we resolving sprites for the upper halftile, or on a bridge?
 * @param rtsg Roadpart of interest
 * @param param1 Extra parameter (first parameter of the callback, except roadtypes do not have callbacks).
 * @param param2 Extra parameter (second parameter of the callback, except roadtypes do not have callbacks).
 */
RoadTypeResolverObject::RoadTypeResolverObject(const RoadtypeInfo *rti, TileIndex tile, TileContext context, RoadTypeSpriteGroup rtsg, uint32 param1, uint32 param2)
	: ResolverObject(rti != NULL ? rti->grffile[rtsg] : NULL, CBID_NO_CALLBACK, param1, param2), roadtype_scope(*this, tile, context)
{
	this->root_spritegroup = rti != NULL ? rti->group[rtsg] : NULL;
}

/**
 * Get the sprite to draw for the given tile.
 * @param rti The road type data (spec).
 * @param tile The tile to get the sprite for.
 * @param rtsg The type of sprite to draw.
 * @param content Where are we drawing the tile?
 * @param [out] num_results If not NULL, return the number of sprites in the spriteset.
 * @return The sprite to draw.
 */
SpriteID GetCustomRoadSprite(const RoadtypeInfo *rti, TileIndex tile, RoadTypeSpriteGroup rtsg, TileContext context, uint *num_results)
{
	assert(rtsg < ROTSG_END);

	if (rti->group[rtsg] == NULL) return 0;

	RoadTypeResolverObject object(rti, tile, context, rtsg);
	const SpriteGroup *group = object.Resolve();
	if (group == NULL || group->GetNumResults() == 0) return 0;

	if (num_results) *num_results = group->GetNumResults();

	return group->GetResult();
}

/**
 * Perform a reverse roadtype lookup to get the GRF internal ID.
 * @param rtid The global (OpenTTD) roadtype.
 * @param grffile The GRF to do the lookup for.
 * @return the GRF internal ID.
 */
uint8 GetReverseRoadTypeTranslation(RoadTypeIdentifier rtid, const GRFFile *grffile)
{
	/* No road type table present, return road type as-is */
	if (grffile == NULL || grffile->roadtype_list[rtid.basetype].Length() == 0) return rtid.subtype;

	/* Look for a matching road type label in the table */
	RoadTypeLabel label = GetRoadTypeInfo(rtid)->label;
	int index = grffile->roadtype_list[rtid.basetype].FindIndex(label);
	if (index >= 0) return index;

	/* If not found, return as invalid */
	return 0xFF;
}
