/* $Id$ */

/** @file newgrf_industrytiles.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "variables.h"
#include "debug.h"
#include "viewport.h"
#include "landscape.h"
#include "newgrf.h"
#include "industry.h"
#include "newgrf_commons.h"
#include "newgrf_spritegroup.h"
#include "newgrf_callbacks.h"
#include "newgrf_industries.h"
#include "industry_map.h"
#include "clear_map.h"
#include "table/sprites.h"
#include "sprite.h"

/**
 * Based on newhouses equivalent, but adapted for newindustries
 * @param parameter from callback.  It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the industry been queried for
 * @return a construction of bits obeying the newgrf format
 */
static uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, IndustryID index)
{
	byte tile_type;
	bool is_same_industry;

	tile = GetNearbyTile(parameter, tile);
	is_same_industry = (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == index);
	tile_type = GetTerrainType(tile) << 2 | (IsTileType(tile, MP_WATER) ? 1 : 0) << 1 | (is_same_industry ? 1 : 0);

	return GetTileType(tile) << 24 | (TileHeight(tile) * 8) << 16 | tile_type << 8 | GetTileSlope(tile, NULL);
}

/** This is the position of the tile relative to the northernmost tile of the industry.
 * Format: 00yxYYXX
 * Variable  Content
 * x         the x offset from the northernmost tile
 * XX        same, but stored in a byte instead of a nibble
 * y         the y offset from the northernmost tile
 * YY        same, but stored in a byte instead of a nibble
 * @param tile TileIndex of the tile to evaluate
 * @param ind_tile northernmost tile of the industry
 */
static uint32 GetRelativePosition(TileIndex tile, TileIndex ind_tile)
{
	byte x = TileX(ind_tile) - TileX(tile);
	byte y = TileY(ind_tile) - TileY(tile);

	return ((y & 0xF) << 20) | ((x & 0xF) << 16) | (y << 8) | x;
}

static uint32 IndustryTileGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Industry *inds = object->u.industry.ind;
	TileIndex tile       = object->u.industry.tile;

	if (object->scope == VSG_SCOPE_PARENT) {
		return IndustryGetVariable(object, variable, parameter, available);
	}

	switch (variable) {
		 /* Construction state of the tile: a value between 0 and 3 */
		case 0x40 : return (IsTileType(tile, MP_INDUSTRY)) ? GetIndustryConstructionStage(tile) : 0;

		case 0x41 : return GetTerrainType(tile);

		/* Current town zone of the tile in the nearest town */
		case 0x42 : return GetTownRadiusGroup(ClosestTownFromTile(tile, (uint)-1), tile);

		/* Relative position */
		case 0x43 : return GetRelativePosition(tile, inds->xy);

		/* Animation frame. Like house variable 46 but can contain anything 0..FF. */
		case 0x44 : return (IsTileType(tile, MP_INDUSTRY)) ? GetIndustryAnimationState(tile) : 0;

		/* Land info of nearby tiles */
		case 0x60 : return GetNearbyIndustryTileInformation(parameter, tile, inds->index);

		case 0x61 : {/* Animation stage of nearby tiles */
			tile = GetNearbyTile(parameter, tile);
			if (IsTileType(tile, MP_INDUSTRY) && GetIndustryByTile(tile) == inds) {
				return GetIndustryAnimationState(tile);
			}
			return 0xFFFFFFFF;
		}

		/* Get industry tile ID at offset */
		case 0x62 : return GetIndustryIDAtOffset(GetNearbyTile(parameter, tile), tile, inds);
	}

	return 0;
}

static const SpriteGroup *IndustryTileResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	/* IndustryTile do not have 'real' groups.  Or do they?? */
	return NULL;
}

uint32 IndustryTileGetRandomBits(const ResolverObject *object)
{
	const TileIndex tile = object->u.industry.tile;
	return (tile == INVALID_TILE || !IsTileType(tile, MP_INDUSTRY)) ? 0 : GetIndustryRandomBits(tile);
}

uint32 IndustryTileGetTriggers(const ResolverObject *object)
{
	const TileIndex tile = object->u.industry.tile;
	return (tile == INVALID_TILE || !IsTileType(tile, MP_INDUSTRY)) ? 0 : GetIndustryTriggers(tile);
}

void IndustryTileSetTriggers(const ResolverObject *object, int triggers)
{
	const TileIndex tile = object->u.industry.tile;
	if (IsTileType(tile, MP_INDUSTRY)) SetIndustryTriggers(tile, triggers);
}

static void NewIndustryTileResolver(ResolverObject *res, IndustryGfx gfx, TileIndex tile, Industry *indus)
{
	res->GetRandomBits = IndustryTileGetRandomBits;
	res->GetTriggers   = IndustryTileGetTriggers;
	res->SetTriggers   = IndustryTileSetTriggers;
	res->GetVariable   = IndustryTileGetVariable;
	res->ResolveReal   = IndustryTileResolveReal;

	res->u.industry.tile = tile;
	res->u.industry.ind  = indus;
	res->u.industry.gfx  = gfx;

	res->callback        = 0;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}

uint16 GetIndustryTileCallback(uint16 callback, uint32 param1, uint32 param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	NewIndustryTileResolver(&object, gfx_id, tile, industry);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = Resolve(GetIndustryTileSpec(gfx_id)->grf_prop.spritegroup, &object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;

	return group->g.callback.result;
}
