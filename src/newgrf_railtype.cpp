/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_railtype.cpp NewGRF handling of rail types. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_railtype.h"
#include "date_func.h"
#include "depot_base.h"
#include "town.h"

#include "safeguards.h"

/* virtual */ uint32 RailTypeScopeResolver::GetRandomBits() const
{
	uint tmp = CountBits(this->tile + (TileX(this->tile) + TileY(this->tile)) * TILE_SIZE);
	return GB(tmp, 0, 2);
}

/* virtual */ uint32 RailTypeScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
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
			if (IsRailDepotTile(this->tile)) return Depot::GetByTile(this->tile)->build_date;
			return _date;
		case 0x44: {
			const Town *t = NULL;
			if (IsRailDepotTile(this->tile)) {
				t = Depot::GetByTile(this->tile)->town;
			} else if (IsLevelCrossingTile(this->tile)) {
				t = ClosestTownFromTile(this->tile, UINT_MAX);
			}
			return t != NULL ? GetTownRadiusGroup(t, this->tile) : HZB_TOWN_EDGE;
		}
	}

	DEBUG(grf, 1, "Unhandled rail type tile variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

/* virtual */ const SpriteGroup *RailTypeResolverObject::ResolveReal(const RealSpriteGroup *group) const
{
	if (group->num_loading > 0) return group->loading[0];
	if (group->num_loaded  > 0) return group->loaded[0];
	return NULL;
}

/**
 * Resolver object for rail types.
 * @param rti Railtype. NULL in NewGRF Inspect window.
 * @param tile %Tile containing the track. For track on a bridge this is the southern bridgehead.
 * @param context Are we resolving sprites for the upper halftile, or on a bridge?
 * @param rtsg Railpart of interest
 * @param param1 Extra parameter (first parameter of the callback, except railtypes do not have callbacks).
 * @param param2 Extra parameter (second parameter of the callback, except railtypes do not have callbacks).
 */
RailTypeResolverObject::RailTypeResolverObject(const RailtypeInfo *rti, TileIndex tile, TileContext context, RailTypeSpriteGroup rtsg, uint32 param1, uint32 param2)
	: ResolverObject(rti != NULL ? rti->grffile[rtsg] : NULL, CBID_NO_CALLBACK, param1, param2), railtype_scope(*this, tile, context)
{
	this->root_spritegroup = rti != NULL ? rti->group[rtsg] : NULL;
}

/**
 * Get the sprite to draw for the given tile.
 * @param rti The rail type data (spec).
 * @param tile The tile to get the sprite for.
 * @param rtsg The type of sprite to draw.
 * @param context Where are we drawing the tile?
 * @param[out] num_results If not NULL, return the number of sprites in the spriteset.
 * @return The sprite to draw.
 */
SpriteID GetCustomRailSprite(const RailtypeInfo *rti, TileIndex tile, RailTypeSpriteGroup rtsg, TileContext context, uint *num_results)
{
	assert(rtsg < RTSG_END);

	if (rti->group[rtsg] == NULL) return 0;

	RailTypeResolverObject object(rti, tile, context, rtsg);
	const SpriteGroup *group = object.Resolve();
	if (group == NULL || group->GetNumResults() == 0) return 0;

	if (num_results) *num_results = group->GetNumResults();

	return group->GetResult();
}

/**
 * Get the sprite to draw for a given signal.
 * @param rti The rail type data (spec).
 * @param tile The tile to get the sprite for.
 * @param type Signal type.
 * @param var Signal variant.
 * @param state Signal state.
 * @param gui Is the sprite being used on the map or in the GUI?
 * @return The sprite to draw.
 */
SpriteID GetCustomSignalSprite(const RailtypeInfo *rti, TileIndex tile, SignalType type, SignalVariant var, SignalState state, bool gui)
{
	if (rti->group[RTSG_SIGNALS] == NULL) return 0;

	uint32 param1 = gui ? 0x10 : 0x00;
	uint32 param2 = (type << 16) | (var << 8) | state;
	RailTypeResolverObject object(rti, tile, TCX_NORMAL, RTSG_SIGNALS, param1, param2);

	const SpriteGroup *group = object.Resolve();
	if (group == NULL || group->GetNumResults() == 0) return 0;

	return group->GetResult();
}

/**
 * Perform a reverse railtype lookup to get the GRF internal ID.
 * @param railtype The global (OpenTTD) railtype.
 * @param grffile The GRF to do the lookup for.
 * @return the GRF internal ID.
 */
uint8 GetReverseRailTypeTranslation(RailType railtype, const GRFFile *grffile)
{
	/* No rail type table present, return rail type as-is */
	if (grffile == NULL || grffile->railtype_list.Length() == 0) return railtype;

	/* Look for a matching rail type label in the table */
	RailTypeLabel label = GetRailTypeInfo(railtype)->label;
	int index = grffile->railtype_list.FindIndex(label);
	if (index >= 0) return index;

	/* If not found, return as invalid */
	return 0xFF;
}
