/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_industrytiles.cpp NewGRF handling of industry tiles. */

#include "stdafx.h"
#include "debug.h"
#include "viewport_func.h"
#include "landscape.h"
#include "newgrf.h"
#include "newgrf_industrytiles.h"
#include "newgrf_sound.h"
#include "newgrf_text.h"
#include "industry.h"
#include "town.h"
#include "command_func.h"
#include "water.h"
#include "sprite.h"
#include "newgrf_animation_base.h"

#include "table/strings.h"

/**
 * Based on newhouses equivalent, but adapted for newindustries
 * @param parameter from callback.  It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the industry been queried for
 * @param signed_offsets Are the x and y offset encoded in parameter signed?
 * @return a construction of bits obeying the newgrf format
 */
uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, IndustryID index, bool signed_offsets)
{
	if (parameter != 0) tile = GetNearbyTile(parameter, tile, signed_offsets); // only perform if it is required
	bool is_same_industry = (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == index);

	return GetNearbyTileInformation(tile) | (is_same_industry ? 1 : 0) << 8;
}

/**
 * This is the position of the tile relative to the northernmost tile of the industry.
 * Format: 00yxYYXX
 * Variable  Content
 * x         the x offset from the northernmost tile
 * XX        same, but stored in a byte instead of a nibble
 * y         the y offset from the northernmost tile
 * YY        same, but stored in a byte instead of a nibble
 * @param tile TileIndex of the tile to evaluate
 * @param ind_tile northernmost tile of the industry
 */
uint32 GetRelativePosition(TileIndex tile, TileIndex ind_tile)
{
	byte x = TileX(tile) - TileX(ind_tile);
	byte y = TileY(tile) - TileY(ind_tile);

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
		case 0x40: return (IsTileType(tile, MP_INDUSTRY)) ? GetIndustryConstructionStage(tile) : 0;

		/* Terrain type */
		case 0x41: return GetTerrainType(tile);

		/* Current town zone of the tile in the nearest town */
		case 0x42: return GetTownRadiusGroup(ClosestTownFromTile(tile, UINT_MAX), tile);

		/* Relative position */
		case 0x43: return GetRelativePosition(tile, inds->location.tile);

		/* Animation frame. Like house variable 46 but can contain anything 0..FF. */
		case 0x44: return (IsTileType(tile, MP_INDUSTRY)) ? GetAnimationFrame(tile) : 0;

		/* Land info of nearby tiles */
		case 0x60: return GetNearbyIndustryTileInformation(parameter, tile, inds == NULL ? (IndustryID)INVALID_INDUSTRY : inds->index);

		/* Animation stage of nearby tiles */
		case 0x61:
			tile = GetNearbyTile(parameter, tile);
			if (IsTileType(tile, MP_INDUSTRY) && Industry::GetByTile(tile) == inds) {
				return GetAnimationFrame(tile);
			}
			return UINT_MAX;

		/* Get industry tile ID at offset */
		case 0x62: return GetIndustryIDAtOffset(GetNearbyTile(parameter, tile), inds, object->grffile->grfid);
	}

	DEBUG(grf, 1, "Unhandled industry tile variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static const SpriteGroup *IndustryTileResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	/* IndustryTile do not have 'real' groups.  Or do they?? */
	return NULL;
}

static uint32 IndustryTileGetRandomBits(const ResolverObject *object)
{
	const TileIndex tile = object->u.industry.tile;
	const Industry *ind = object->u.industry.ind;
	assert(ind != NULL && IsValidTile(tile));
	assert(ind->index == INVALID_INDUSTRY || IsTileType(tile, MP_INDUSTRY));

	return (object->scope == VSG_SCOPE_SELF) ?
			(ind->index != INVALID_INDUSTRY ? GetIndustryRandomBits(tile) : 0) :
			ind->random;
}

static uint32 IndustryTileGetTriggers(const ResolverObject *object)
{
	const TileIndex tile = object->u.industry.tile;
	const Industry *ind = object->u.industry.ind;
	assert(ind != NULL && IsValidTile(tile));
	assert(ind->index == INVALID_INDUSTRY || IsTileType(tile, MP_INDUSTRY));
	if (ind->index == INVALID_INDUSTRY) return 0;
	return (object->scope == VSG_SCOPE_SELF) ? GetIndustryTriggers(tile) : ind->random_triggers;
}

static void IndustryTileSetTriggers(const ResolverObject *object, int triggers)
{
	const TileIndex tile = object->u.industry.tile;
	Industry *ind = object->u.industry.ind;
	assert(ind != NULL && ind->index != INVALID_INDUSTRY && IsValidTile(tile) && IsTileType(tile, MP_INDUSTRY));

	if (object->scope == VSG_SCOPE_SELF) {
		SetIndustryTriggers(tile, triggers);
	} else {
		ind->random_triggers = triggers;
	}
}

static void NewIndustryTileResolver(ResolverObject *res, IndustryGfx gfx, TileIndex tile, Industry *indus)
{
	res->GetRandomBits = IndustryTileGetRandomBits;
	res->GetTriggers   = IndustryTileGetTriggers;
	res->SetTriggers   = IndustryTileSetTriggers;
	res->GetVariable   = IndustryTileGetVariable;
	res->ResolveReal   = IndustryTileResolveReal;

	res->psa             = &indus->psa;
	res->u.industry.tile = tile;
	res->u.industry.ind  = indus;
	res->u.industry.gfx  = gfx;
	res->u.industry.type = indus->type;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;

	const IndustryTileSpec *its = GetIndustryTileSpec(gfx);
	res->grffile         = (its != NULL ? its->grf_prop.grffile : NULL);
}

static void IndustryDrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, byte rnd_colour, byte stage, IndustryGfx gfx)
{
	const DrawTileSprites *dts = group->dts;

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) image += stage;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		/* If the ground sprite is the default flat water sprite, draw also canal/river borders
		 * Do not do this if the tile's WaterClass is 'land'. */
		if (image == SPR_FLAT_WATER_TILE && IsTileOnWater(ti->tile)) {
			DrawWaterClassGround(ti);
		} else {
			DrawGroundSprite(image, GroundSpritePaletteTransform(image, pal, GENERAL_SPRITE_COLOUR(rnd_colour)));
		}
	}

	DrawNewGRFTileSeq(ti, dts, TO_INDUSTRIES, stage, GENERAL_SPRITE_COLOUR(rnd_colour));
}

uint16 GetIndustryTileCallback(CallbackID callback, uint32 param1, uint32 param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	assert(industry != NULL && IsValidTile(tile));
	assert(industry->index == INVALID_INDUSTRY || IsTileType(tile, MP_INDUSTRY));

	NewIndustryTileResolver(&object, gfx_id, tile, industry);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = SpriteGroup::Resolve(GetIndustryTileSpec(gfx_id)->grf_prop.spritegroup[0], &object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;

	return group->GetCallbackResult();
}

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds)
{
	const SpriteGroup *group;
	ResolverObject object;

	if (ti->tileh != SLOPE_FLAT) {
		bool draw_old_one = true;
		if (HasBit(inds->callback_mask, CBM_INDT_DRAW_FOUNDATIONS)) {
			/* Called to determine the type (if any) of foundation to draw for industry tile */
			uint32 callback_res = GetIndustryTileCallback(CBID_INDTILE_DRAW_FOUNDATIONS, 0, 0, gfx, i, ti->tile);
			draw_old_one = (callback_res != 0);
		}

		if (draw_old_one) DrawFoundation(ti, FOUNDATION_LEVELED);
	}

	NewIndustryTileResolver(&object, gfx, ti->tile, i);

	group = SpriteGroup::Resolve(inds->grf_prop.spritegroup[0], &object);
	if (group == NULL || group->type != SGT_TILELAYOUT) {
		return false;
	} else {
		/* Limit the building stage to the number of stages supplied. */
		const TileLayoutSpriteGroup *tlgroup = (const TileLayoutSpriteGroup *)group;
		byte stage = GetIndustryConstructionStage(ti->tile);
		stage = Clamp(stage - 4 + tlgroup->num_building_stages, 0, tlgroup->num_building_stages - 1);
		IndustryDrawTileLayout(ti, tlgroup, i->random_colour, stage, gfx);
		return true;
	}
}

extern bool IsSlopeRefused(Slope current, Slope refused);

/**
 * Check the slope of a tile of a new industry.
 * @param ind_base_tile Base tile of the industry.
 * @param ind_tile      Tile to check.
 * @param its           Tile specification.
 * @param type          Industry type.
 * @param gfx           Gfx of the tile.
 * @param itspec_index  Layout.
 * @param initial_random_bits Random bits of industry after construction
 * @param founder       Industry founder
 * @param creation_type The circumstances the industry is created under.
 * @return Suceeded or failed command.
 */
CommandCost PerformIndustryTileSlopeCheck(TileIndex ind_base_tile, TileIndex ind_tile, const IndustryTileSpec *its, IndustryType type, IndustryGfx gfx, uint itspec_index, uint16 initial_random_bits, Owner founder, IndustryAvailabilityCallType creation_type)
{
	Industry ind;
	ind.index = INVALID_INDUSTRY;
	ind.location.tile = ind_base_tile;
	ind.location.w = 0;
	ind.type = type;
	ind.random = initial_random_bits;
	ind.founder = founder;

	uint16 callback_res = GetIndustryTileCallback(CBID_INDTILE_SHAPE_CHECK, 0, creation_type << 8 | itspec_index, gfx, &ind, ind_tile);
	if (callback_res == CALLBACK_FAILED) {
		if (!IsSlopeRefused(GetTileSlope(ind_tile, NULL), its->slopes_refused)) return CommandCost();
		return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	}
	if (its->grf_prop.grffile->grf_version < 7) {
		if (callback_res != 0) return CommandCost();
		return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	}
	if (callback_res == 0x400) return CommandCost();

	/* Copy some parameters from the registers to the error message text ref. stack */
	SwitchToErrorRefStack();
	PrepareTextRefStackUsage(4);
	SwitchToNormalRefStack();

	switch (callback_res) {
		case 0x401: return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
		case 0x402: return_cmd_error(STR_ERROR_CAN_ONLY_BE_BUILT_IN_RAINFOREST);
		case 0x403: return_cmd_error(STR_ERROR_CAN_ONLY_BE_BUILT_IN_DESERT);
		default:    return_cmd_error(GetGRFStringID(its->grf_prop.grffile->grfid, 0xD000 + callback_res));
	}
}

/* Simple wrapper for GetHouseCallback to keep the animation unified. */
uint16 GetSimpleIndustryCallback(CallbackID callback, uint32 param1, uint32 param2, const IndustryTileSpec *spec, const Industry *ind, TileIndex tile)
{
	return GetIndustryTileCallback(callback, param1, param2, spec - GetIndustryTileSpec(0), const_cast<Industry *>(ind), tile);
}

/** Helper class for animation control. */
struct IndustryAnimationBase : public AnimationBase<IndustryAnimationBase, IndustryTileSpec, Industry, GetSimpleIndustryCallback> {
	static const CallbackID cb_animation_speed      = CBID_INDTILE_ANIMATION_SPEED;
	static const CallbackID cb_animation_next_frame = CBID_INDTILE_ANIM_NEXT_FRAME;

	static const IndustryTileCallbackMask cbm_animation_speed      = CBM_INDT_ANIM_SPEED;
	static const IndustryTileCallbackMask cbm_animation_next_frame = CBM_INDT_ANIM_NEXT_FRAME;
};

void AnimateNewIndustryTile(TileIndex tile)
{
	const IndustryTileSpec *itspec = GetIndustryTileSpec(GetIndustryGfx(tile));
	if (itspec == NULL) return;

	IndustryAnimationBase::AnimateTile(itspec, Industry::GetByTile(tile), tile, (itspec->special_flags & INDTILE_SPECIAL_NEXTFRAME_RANDOMBITS) != 0);
}

bool StartStopIndustryTileAnimation(TileIndex tile, IndustryAnimationTrigger iat, uint32 random)
{
	const IndustryTileSpec *itspec = GetIndustryTileSpec(GetIndustryGfx(tile));

	if (!HasBit(itspec->animation.triggers, iat)) return false;

	IndustryAnimationBase::ChangeAnimationFrame(CBID_INDTILE_ANIM_START_STOP, itspec, Industry::GetByTile(tile), tile, random, iat);
	return true;
}

bool StartStopIndustryTileAnimation(const Industry *ind, IndustryAnimationTrigger iat)
{
	bool ret = true;
	uint32 random = Random();
	TILE_AREA_LOOP(tile, ind->location) {
		if (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == ind->index) {
			if (StartStopIndustryTileAnimation(tile, iat, random)) {
				SB(random, 0, 16, Random());
			} else {
				ret = false;
			}
		}
	}

	return ret;
}

static void DoTriggerIndustryTile(TileIndex tile, IndustryTileTrigger trigger, Industry *ind)
{
	ResolverObject object;

	assert(IsValidTile(tile) && IsTileType(tile, MP_INDUSTRY));

	IndustryGfx gfx = GetIndustryGfx(tile);
	const IndustryTileSpec *itspec = GetIndustryTileSpec(gfx);

	if (itspec->grf_prop.spritegroup[0] == NULL) return;

	NewIndustryTileResolver(&object, gfx, tile, ind);

	object.callback = CBID_RANDOM_TRIGGER;
	object.trigger = trigger;

	const SpriteGroup *group = SpriteGroup::Resolve(itspec->grf_prop.spritegroup[0], &object);
	if (group == NULL) return;

	byte new_random_bits = Random();
	byte random_bits = GetIndustryRandomBits(tile);
	random_bits &= ~object.reseed;
	random_bits |= new_random_bits & object.reseed;
	SetIndustryRandomBits(tile, random_bits);
	MarkTileDirtyByTile(tile);
}

void TriggerIndustryTile(TileIndex tile, IndustryTileTrigger trigger)
{
	DoTriggerIndustryTile(tile, trigger, Industry::GetByTile(tile));
}

void TriggerIndustry(Industry *ind, IndustryTileTrigger trigger)
{
	TILE_AREA_LOOP(tile, ind->location) {
		if (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == ind->index) {
			DoTriggerIndustryTile(tile, trigger, ind);
		}
	}
}

/**
 * Resolve a industry tile's spec and such so we can get a variable.
 * @param ro    The resolver object to fill.
 * @param index The industry tile to get the data from.
 */
void GetIndustryTileResolver(ResolverObject *ro, uint index)
{
	NewIndustryTileResolver(ro, GetIndustryGfx(index), index, Industry::GetByTile(index));
}
