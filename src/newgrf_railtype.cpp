/* $Id$ */

/** @file newgrf_railtype.cpp NewGRF handling of rail types. */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "debug.h"
#include "strings_type.h"
#include "rail.h"
#include "road_map.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_commons.h"
#include "newgrf_railtype.h"
#include "newgrf_spritegroup.h"
#include "core/bitmath_func.hpp"

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
		}
	}

	switch (variable) {
		case 0x40: return GetTerrainType(tile);
		case 0x41: return 0;
		case 0x42: return IsLevelCrossingTile(tile) && IsCrossingBarred(tile);
	}

	DEBUG(grf, 1, "Unhandled rail type tile property 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static const SpriteGroup *RailTypeResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	if (group->num_loading > 0) return group->loading[0];
	if (group->num_loaded  > 0) return group->loaded[0];
	return NULL;
}

static inline void NewRailTypeResolver(ResolverObject *res, TileIndex tile)
{
	res->GetRandomBits = &RailTypeGetRandomBits;
	res->GetTriggers   = &RailTypeGetTriggers;
	res->SetTriggers   = &RailTypeSetTriggers;
	res->GetVariable   = &RailTypeGetVariable;
	res->ResolveReal   = &RailTypeResolveReal;

	res->u.routes.tile = tile;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;
}

SpriteID GetCustomRailSprite(const RailtypeInfo *rti, TileIndex tile, RailTypeSpriteGroup rtsg)
{
	assert(rtsg < RTSG_END);

	if (rti->group[rtsg] == NULL) return 0;

	const SpriteGroup *group;
	ResolverObject object;

	NewRailTypeResolver(&object, tile);

	group = SpriteGroup::Resolve(rti->group[rtsg], &object);
	if (group == NULL || group->GetNumResults() == 0) return 0;

	return group->GetResult();
}

uint8 GetReverseRailTypeTranslation(RailType railtype, const GRFFile *grffile)
{
	/* No rail type table present, return rail type as-is */
	if (grffile->railtype_max == 0) return railtype;

	/* Look for a matching rail type label in the table */
	RailTypeLabel label = GetRailTypeInfo(railtype)->label;
	for (uint i = 0; i < grffile->railtype_max; i++) {
		if (label == grffile->railtype_list[i]) return i;
	}

	/* If not found, return as invalid */
	return 0xFF;
}
