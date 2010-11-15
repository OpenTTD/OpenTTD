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
#include "newgrf.h"
#include "newgrf_spritegroup.h"
#include "date_func.h"
#include "depot_base.h"

static uint32 RailTypeGetRandomBits(const ResolverObject *object)
{
	TileIndex tile = object->u.routes.tile;
	uint tmp = CountBits(tile + (TileX(tile) + TileY(tile)) * TILE_SIZE);
	return GB(tmp, 0, 2);
}

static uint32 RailTypeGetTriggers(const ResolverObject *object)
{
	return 0;
}

static void RailTypeSetTriggers(const ResolverObject *object, int triggers)
{
}

static uint32 RailTypeGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	TileIndex tile = object->u.routes.tile;

	if (tile == INVALID_TILE) {
		switch (variable) {
			case 0x40: return 0;
			case 0x41: return 0;
			case 0x42: return 0;
			case 0x43: return _date;
		}
	}

	switch (variable) {
		case 0x40: return GetTerrainType(tile, object->u.routes.context);
		case 0x41: return 0;
		case 0x42: return IsLevelCrossingTile(tile) && IsCrossingBarred(tile);
		case 0x43:
			if (IsRailDepotTile(tile)) return Depot::GetByTile(tile)->build_date;
			return _date;
	}

	DEBUG(grf, 1, "Unhandled rail type tile variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static const SpriteGroup *RailTypeResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	if (group->num_loading > 0) return group->loading[0];
	if (group->num_loaded  > 0) return group->loaded[0];
	return NULL;
}

static inline void NewRailTypeResolver(ResolverObject *res, TileIndex tile, TileContext context)
{
	res->GetRandomBits = &RailTypeGetRandomBits;
	res->GetTriggers   = &RailTypeGetTriggers;
	res->SetTriggers   = &RailTypeSetTriggers;
	res->GetVariable   = &RailTypeGetVariable;
	res->ResolveReal   = &RailTypeResolveReal;

	res->u.routes.tile = tile;
	res->u.routes.context = context;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;
}

SpriteID GetCustomRailSprite(const RailtypeInfo *rti, TileIndex tile, RailTypeSpriteGroup rtsg, TileContext context)
{
	assert(rtsg < RTSG_END);

	if (rti->group[rtsg] == NULL) return 0;

	const SpriteGroup *group;
	ResolverObject object;

	NewRailTypeResolver(&object, tile, context);

	group = SpriteGroup::Resolve(rti->group[rtsg], &object);
	if (group == NULL || group->GetNumResults() == 0) return 0;

	return group->GetResult();
}

uint8 GetReverseRailTypeTranslation(RailType railtype, const GRFFile *grffile)
{
	/* No rail type table present, return rail type as-is */
	if (grffile == NULL || grffile->railtype_max == 0) return railtype;

	/* Look for a matching rail type label in the table */
	RailTypeLabel label = GetRailTypeInfo(railtype)->label;
	for (uint i = 0; i < grffile->railtype_max; i++) {
		if (label == grffile->railtype_list[i]) return i;
	}

	/* If not found, return as invalid */
	return 0xFF;
}

/**
 * Resolve a railtypes's spec and such so we can get a variable.
 * @param ro    The resolver object to fill.
 * @param index The rail tile to get the data from.
 */
void GetRailTypeResolver(ResolverObject *ro, uint index)
{
	NewRailTypeResolver(ro, index, TCX_NORMAL);
}
