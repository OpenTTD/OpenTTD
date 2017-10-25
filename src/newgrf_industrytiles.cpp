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
#include "landscape.h"
#include "newgrf_industrytiles.h"
#include "newgrf_sound.h"
#include "industry.h"
#include "town.h"
#include "command_func.h"
#include "water.h"
#include "newgrf_animation_base.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Based on newhouses equivalent, but adapted for newindustries
 * @param parameter from callback.  It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the industry been queried for
 * @param signed_offsets Are the x and y offset encoded in parameter signed?
 * @param grf_version8 True, if we are dealing with a new NewGRF which uses GRF version >= 8.
 * @return a construction of bits obeying the newgrf format
 */
uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, IndustryID index, bool signed_offsets, bool grf_version8)
{
	if (parameter != 0) tile = GetNearbyTile(parameter, tile, signed_offsets); // only perform if it is required
	bool is_same_industry = (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == index);

	return GetNearbyTileInformation(tile, grf_version8) | (is_same_industry ? 1 : 0) << 8;
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

/* virtual */ uint32 IndustryTileScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	switch (variable) {
		/* Construction state of the tile: a value between 0 and 3 */
		case 0x40: return (IsTileType(this->tile, MP_INDUSTRY)) ? GetIndustryConstructionStage(this->tile) : 0;

		/* Terrain type */
		case 0x41: return GetTerrainType(this->tile);

		/* Current town zone of the tile in the nearest town */
		case 0x42: return GetTownRadiusGroup(ClosestTownFromTile(this->tile, UINT_MAX), this->tile);

		/* Relative position */
		case 0x43: return GetRelativePosition(this->tile, this->industry->location.tile);

		/* Animation frame. Like house variable 46 but can contain anything 0..FF. */
		case 0x44: return IsTileType(this->tile, MP_INDUSTRY) ? GetAnimationFrame(this->tile) : 0;

		/* Land info of nearby tiles */
		case 0x60: return GetNearbyIndustryTileInformation(parameter, this->tile,
				this->industry == NULL ? (IndustryID)INVALID_INDUSTRY : this->industry->index, true, this->ro.grffile->grf_version >= 8);

		/* Animation stage of nearby tiles */
		case 0x61: {
			TileIndex tile = GetNearbyTile(parameter, this->tile);
			if (IsTileType(tile, MP_INDUSTRY) && Industry::GetByTile(tile) == this->industry) {
				return GetAnimationFrame(tile);
			}
			return UINT_MAX;
		}

		/* Get industry tile ID at offset */
		case 0x62: return GetIndustryIDAtOffset(GetNearbyTile(parameter, this->tile), this->industry, this->ro.grffile->grfid);
	}

	DEBUG(grf, 1, "Unhandled industry tile variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

/* virtual */ uint32 IndustryTileScopeResolver::GetRandomBits() const
{
	assert(this->industry != NULL && IsValidTile(this->tile));
	assert(this->industry->index == INVALID_INDUSTRY || IsTileType(this->tile, MP_INDUSTRY));

	return (this->industry->index != INVALID_INDUSTRY) ? GetIndustryRandomBits(this->tile) : 0;
}

/* virtual */ uint32 IndustryTileScopeResolver::GetTriggers() const
{
	assert(this->industry != NULL && IsValidTile(this->tile));
	assert(this->industry->index == INVALID_INDUSTRY || IsTileType(this->tile, MP_INDUSTRY));
	if (this->industry->index == INVALID_INDUSTRY) return 0;
	return GetIndustryTriggers(this->tile);
}

/**
 * Get the associated NewGRF file from the industry graphics.
 * @param gfx Graphics to query.
 * @return Grf file associated with the graphics, if any.
 */
static const GRFFile *GetIndTileGrffile(IndustryGfx gfx)
{
	const IndustryTileSpec *its = GetIndustryTileSpec(gfx);
	return (its != NULL) ? its->grf_prop.grffile : NULL;
}

/**
 * Constructor of the industry tiles scope resolver.
 * @param gfx Graphics of the industry.
 * @param tile %Tile of the industry.
 * @param indus %Industry owning the tile.
 * @param callback Callback ID.
 * @param callback_param1 First parameter (var 10) of the callback.
 * @param callback_param2 Second parameter (var 18) of the callback.
 */
IndustryTileResolverObject::IndustryTileResolverObject(IndustryGfx gfx, TileIndex tile, Industry *indus,
			CallbackID callback, uint32 callback_param1, uint32 callback_param2)
	: ResolverObject(GetIndTileGrffile(gfx), callback, callback_param1, callback_param2),
	indtile_scope(*this, indus, tile),
	ind_scope(*this, tile, indus, indus->type)
{
	this->root_spritegroup = GetIndustryTileSpec(gfx)->grf_prop.spritegroup[0];
}

/**
 * Constructor of the scope resolver for the industry tile.
 * @param ro Surrounding resolver.
 * @param industry %Industry owning the tile.
 * @param tile %Tile of the industry.
 */
IndustryTileScopeResolver::IndustryTileScopeResolver(ResolverObject &ro, Industry *industry, TileIndex tile) : ScopeResolver(ro)
{
	this->industry = industry;
	this->tile = tile;
}

static void IndustryDrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, byte rnd_colour, byte stage, IndustryGfx gfx)
{
	const DrawTileSprites *dts = group->ProcessRegisters(&stage);

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) image += stage;
	if (HasBit(pal, SPRITE_MODIFIER_CUSTOM_SPRITE)) pal += stage;

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
	assert(industry != NULL && IsValidTile(tile));
	assert(industry->index == INVALID_INDUSTRY || IsTileType(tile, MP_INDUSTRY));

	IndustryTileResolverObject object(gfx_id, tile, industry, callback, param1, param2);
	return object.ResolveCallback();
}

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds)
{
	if (ti->tileh != SLOPE_FLAT) {
		bool draw_old_one = true;
		if (HasBit(inds->callback_mask, CBM_INDT_DRAW_FOUNDATIONS)) {
			/* Called to determine the type (if any) of foundation to draw for industry tile */
			uint32 callback_res = GetIndustryTileCallback(CBID_INDTILE_DRAW_FOUNDATIONS, 0, 0, gfx, i, ti->tile);
			if (callback_res != CALLBACK_FAILED) draw_old_one = ConvertBooleanCallback(inds->grf_prop.grffile, CBID_INDTILE_DRAW_FOUNDATIONS, callback_res);
		}

		if (draw_old_one) DrawFoundation(ti, FOUNDATION_LEVELED);
	}

	IndustryTileResolverObject object(gfx, ti->tile, i);

	const SpriteGroup *group = object.Resolve();
	if (group == NULL || group->type != SGT_TILELAYOUT) return false;

	/* Limit the building stage to the number of stages supplied. */
	const TileLayoutSpriteGroup *tlgroup = (const TileLayoutSpriteGroup *)group;
	byte stage = GetIndustryConstructionStage(ti->tile);
	IndustryDrawTileLayout(ti, tlgroup, i->random_colour, stage, gfx);
	return true;
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
 * @return Succeeded or failed command.
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
		if (!IsSlopeRefused(GetTileSlope(ind_tile), its->slopes_refused)) return CommandCost();
		return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	}
	if (its->grf_prop.grffile->grf_version < 7) {
		if (callback_res != 0) return CommandCost();
		return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	}

	return GetErrorMessageFromLocationCallbackResult(callback_res, its->grf_prop.grffile, STR_ERROR_SITE_UNSUITABLE);
}

/* Simple wrapper for GetHouseCallback to keep the animation unified. */
uint16 GetSimpleIndustryCallback(CallbackID callback, uint32 param1, uint32 param2, const IndustryTileSpec *spec, Industry *ind, TileIndex tile, int extra_data)
{
	return GetIndustryTileCallback(callback, param1, param2, spec - GetIndustryTileSpec(0), ind, tile);
}

/** Helper class for animation control. */
struct IndustryAnimationBase : public AnimationBase<IndustryAnimationBase, IndustryTileSpec, Industry, int, GetSimpleIndustryCallback> {
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
		if (ind->TileBelongsToIndustry(tile)) {
			if (StartStopIndustryTileAnimation(tile, iat, random)) {
				SB(random, 0, 16, Random());
			} else {
				ret = false;
			}
		}
	}

	return ret;
}

/**
 * Trigger random triggers for an industry tile and reseed its random bits.
 * @param tile Industry tile to trigger.
 * @param trigger Trigger to trigger.
 * @param ind Industry of the tile.
 * @param [in,out] reseed_industry Collects bits to reseed for the industry.
 */
static void DoTriggerIndustryTile(TileIndex tile, IndustryTileTrigger trigger, Industry *ind, uint32 &reseed_industry)
{
	assert(IsValidTile(tile) && IsTileType(tile, MP_INDUSTRY));

	IndustryGfx gfx = GetIndustryGfx(tile);
	const IndustryTileSpec *itspec = GetIndustryTileSpec(gfx);

	if (itspec->grf_prop.spritegroup[0] == NULL) return;

	IndustryTileResolverObject object(gfx, tile, ind, CBID_RANDOM_TRIGGER);
	object.waiting_triggers = GetIndustryTriggers(tile) | trigger;
	SetIndustryTriggers(tile, object.waiting_triggers); // store now for var 5F

	const SpriteGroup *group = object.Resolve();
	if (group == NULL) return;

	/* Store remaining triggers. */
	SetIndustryTriggers(tile, object.GetRemainingTriggers());

	/* Rerandomise tile bits */
	byte new_random_bits = Random();
	byte random_bits = GetIndustryRandomBits(tile);
	random_bits &= ~object.reseed[VSG_SCOPE_SELF];
	random_bits |= new_random_bits & object.reseed[VSG_SCOPE_SELF];
	SetIndustryRandomBits(tile, random_bits);
	MarkTileDirtyByTile(tile);

	reseed_industry |= object.reseed[VSG_SCOPE_PARENT];
}

/**
 * Reseeds the random bits of an industry.
 * @param ind Industry.
 * @param reseed Bits to reseed.
 */
static void DoReseedIndustry(Industry *ind, uint32 reseed)
{
	if (reseed == 0 || ind == NULL) return;

	uint16 random_bits = Random();
	ind->random &= reseed;
	ind->random |= random_bits & reseed;
}

/**
 * Trigger a random trigger for a single industry tile.
 * @param tile Industry tile to trigger.
 * @param trigger Trigger to trigger.
 */
void TriggerIndustryTile(TileIndex tile, IndustryTileTrigger trigger)
{
	uint32 reseed_industry = 0;
	Industry *ind = Industry::GetByTile(tile);
	DoTriggerIndustryTile(tile, trigger, ind, reseed_industry);
	DoReseedIndustry(ind, reseed_industry);
}

/**
 * Trigger a random trigger for all industry tiles.
 * @param ind Industry to trigger.
 * @param trigger Trigger to trigger.
 */
void TriggerIndustry(Industry *ind, IndustryTileTrigger trigger)
{
	uint32 reseed_industry = 0;
	TILE_AREA_LOOP(tile, ind->location) {
		if (ind->TileBelongsToIndustry(tile)) {
			DoTriggerIndustryTile(tile, trigger, ind, reseed_industry);
		}
	}
	DoReseedIndustry(ind, reseed_industry);
}

