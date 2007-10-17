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
#include "newgrf_industrytiles.h"
#include "newgrf_text.h"
#include "industry_map.h"
#include "clear_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "sprite.h"

/**
 * Based on newhouses equivalent, but adapted for newindustries
 * @param parameter from callback.  It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the industry been queried for
 * @return a construction of bits obeying the newgrf format
 */
uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, IndustryID index)
{
	byte tile_type;
	bool is_same_industry;

	if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required
	is_same_industry = (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == index);
	tile_type = GetTerrainType(tile) << 2 | (IsTileType(tile, MP_WATER) ? 1 : 0) << 1 | (is_same_industry ? 1 : 0);

	uint z;
	Slope tileh = GetTileSlope(tile, &z);
	return GetTileType(tile) << 24 | z << 16 | tile_type << 8 | tileh;
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
		case 0x40 : return (IsTileType(tile, MP_INDUSTRY)) ? GetIndustryConstructionStage(tile) : 0;

		case 0x41 : return GetTerrainType(tile);

		/* Current town zone of the tile in the nearest town */
		case 0x42 : return GetTownRadiusGroup(ClosestTownFromTile(tile, (uint)-1), tile);

		/* Relative position */
		case 0x43 : return GetRelativePosition(tile, inds->xy);

		/* Animation frame. Like house variable 46 but can contain anything 0..FF. */
		case 0x44 : return (IsTileType(tile, MP_INDUSTRY)) ? GetIndustryAnimationState(tile) : 0;

		/* Land info of nearby tiles */
		case 0x60 : return GetNearbyIndustryTileInformation(parameter, tile, inds == NULL ? (IndustryID)INVALID_INDUSTRY : inds->index);

		/* Animation stage of nearby tiles */
		case 0x61 : {
			tile = GetNearbyTile(parameter, tile);
			if (IsTileType(tile, MP_INDUSTRY) && GetIndustryByTile(tile) == inds) {
				return GetIndustryAnimationState(tile);
			}
			return 0xFFFFFFFF;
		}

		/* Get industry tile ID at offset */
		case 0x62 : return GetIndustryIDAtOffset(GetNearbyTile(parameter, tile), inds);
	}

	DEBUG(grf, 1, "Unhandled industry tile property 0x%X", variable);

	*available = false;
	return (uint32)-1;
}

static const SpriteGroup *IndustryTileResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	/* IndustryTile do not have 'real' groups.  Or do they?? */
	return NULL;
}

static uint32 IndustryTileGetRandomBits(const ResolverObject *object)
{
	const TileIndex tile = object->u.industry.tile;
	if (tile == INVALID_TILE || !IsTileType(tile, MP_INDUSTRY)) return 0;
	return (object->scope == VSG_SCOPE_SELF) ? GetIndustryRandomBits(tile) : 0; //GetIndustryByTile(tile)->random_bits;
}

static uint32 IndustryTileGetTriggers(const ResolverObject *object)
{
	const TileIndex tile = object->u.industry.tile;
	if (tile == INVALID_TILE || !IsTileType(tile, MP_INDUSTRY)) return 0;
	return (object->scope == VSG_SCOPE_SELF) ? GetIndustryTriggers(tile) : 0; //GetIndustryByTile(tile)->triggers;
}

static void IndustryTileSetTriggers(const ResolverObject *object, int triggers)
{
	const TileIndex tile = object->u.industry.tile;
	if (tile == INVALID_TILE || !IsTileType(tile, MP_INDUSTRY)) return;

	if (object->scope != VSG_SCOPE_SELF) {
		SetIndustryTriggers(tile, triggers);
	} else {
		//GetIndustryByTile(tile)->triggers = triggers;
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

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}

void IndustryDrawTileLayout(const TileInfo *ti, const SpriteGroup *group, byte rnd_color, byte stage, IndustryGfx gfx)
{
	const DrawTileSprites *dts = group->g.layout.dts;
	const DrawTileSeqStruct *dtss;

	SpriteID image = dts->ground_sprite;
	SpriteID pal   = dts->ground_pal;

	if (IS_CUSTOM_SPRITE(image)) image += stage;

	if (GB(image, 0, SPRITE_WIDTH) != 0) DrawGroundSprite(image, pal);

	foreach_draw_tile_seq(dtss, dts->seq) {
		if (GB(dtss->image, 0, SPRITE_WIDTH) == 0) continue;

		image = dtss->image;
		pal   = dtss->pal;

		if (IS_CUSTOM_SPRITE(image)) image += stage;

		if (HASBIT(image, PALETTE_MODIFIER_COLOR)) {
			pal = GENERAL_SPRITE_COLOR(rnd_color);
		} else {
			pal = PAL_NONE;
		}

		if ((byte)dtss->delta_z != 0x80) {
			AddSortableSpriteToDraw(
				image, pal,
				ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->size_x, dtss->size_y,
				dtss->size_z, ti->z + dtss->delta_z,
				!HASBIT(image, SPRITE_MODIFIER_OPAQUE) && HASBIT(_transparent_opt, TO_INDUSTRIES)
			);
		} else {
			AddChildSpriteScreen(image, pal, (byte)dtss->delta_x, (byte)dtss->delta_y, HASBIT(_transparent_opt, TO_INDUSTRIES));
		}
	}
}

uint16 GetIndustryTileCallback(CallbackID callback, uint32 param1, uint32 param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile)
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

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds)
{
	const SpriteGroup *group;
	ResolverObject object;

	if (ti->tileh != SLOPE_FLAT) {
		bool draw_old_one = true;
		if (HASBIT(inds->callback_flags, CBM_INDT_DRAW_FOUNDATIONS)) {
			/* Called to determine the type (if any) of foundation to draw for industry tile */
			uint32 callback_res = GetIndustryTileCallback(CBID_INDUSTRY_DRAW_FOUNDATIONS, 0, 0, gfx, i, ti->tile);
			draw_old_one = callback_res != 0;
		}

		if (draw_old_one) DrawFoundation(ti, FOUNDATION_LEVELED);
	}

	NewIndustryTileResolver(&object, gfx, ti->tile, i);

	group = Resolve(inds->grf_prop.spritegroup, &object);
	if (group == NULL || group->type != SGT_TILELAYOUT) {
		return false;
	} else {
		/* Limit the building stage to the number of stages supplied. */
		byte stage = GetIndustryConstructionStage(ti->tile);
		stage = clamp(stage - 4 + group->g.layout.num_sprites, 0, group->g.layout.num_sprites - 1);
		IndustryDrawTileLayout(ti, group, i->random_color, stage, gfx);
		return true;
	}
}

extern bool IsSlopeRefused(Slope current, Slope refused);

bool PerformIndustryTileSlopeCheck(TileIndex ind_base_tile, TileIndex ind_tile, const IndustryTileSpec *its, IndustryType type, IndustryGfx gfx, uint itspec_index)
{
	Industry ind;
	ind.index = INVALID_INDUSTRY;
	ind.xy = ind_base_tile;
	ind.width = 0;
	ind.type = type;

	uint16 callback_res = GetIndustryTileCallback(CBID_INDTILE_SHAPE_CHECK, 0, itspec_index, gfx, &ind, ind_tile);
	if (callback_res == CALLBACK_FAILED) {
		return !IsSlopeRefused(GetTileSlope(ind_tile, NULL), its->slopes_refused);
	}
	if (its->grf_prop.grffile->grf_version < 7) {
		return callback_res != 0;
	}

	/* Copy some parameters from the registers to the error message text ref. stack */
	SwitchToErrorRefStack();
	PrepareTextRefStackUsage(4);
	SwitchToNormalRefStack();

	switch (callback_res) {
		case 0x400: return true;
		case 0x401: _error_message = STR_0239_SITE_UNSUITABLE;                 return false;
		case 0x402: _error_message = STR_0317_CAN_ONLY_BE_BUILT_IN_RAINFOREST; return false;
		case 0x403: _error_message = STR_0318_CAN_ONLY_BE_BUILT_IN_DESERT;     return false;
		default: _error_message = GetGRFStringID(its->grf_prop.grffile->grfid, 0xD000 + callback_res); return false;
	}
}

void AnimateNewIndustryTile(TileIndex tile)
{
	Industry *ind = GetIndustryByTile(tile);
	IndustryGfx gfx = GetIndustryGfx(tile);
	const IndustryTileSpec *itspec = GetIndustryTileSpec(gfx);
	byte animation_speed = itspec->animation_speed;

	if (HASBIT(itspec->callback_flags, CBM_INDT_ANIM_SPEED)) {
		uint16 callback_res = GetIndustryTileCallback(CBID_INDTILE_ANIMATION_SPEED, 0, 0, gfx, ind, tile);
		if (callback_res != CALLBACK_FAILED) animation_speed = clamp(callback_res & 0xFF, 0, 16);
	}

	/* An animation speed of 2 means the animation frame changes 4 ticks, and
	 * increasing this value by one doubles the wait. 0 is the minimum value
	 * allowed for animation_speed, which corresponds to 30ms, and 16 is the
	 * maximum, corresponding to around 33 minutes. */
	if ((_tick_counter % (1 << animation_speed)) != 0) return;

	bool frame_set_by_callback = false;
	byte frame = GetIndustryAnimationState(tile);
	uint16 num_frames = GB(itspec->animation_info, 0, 8);

	if (HASBIT(itspec->callback_flags, CBM_INDT_ANIM_NEXT_FRAME)) {
		uint16 callback_res = GetIndustryTileCallback(CBID_INDTILE_ANIM_NEXT_FRAME, HASBIT(itspec->animation_special_flags, 0) ? Random() : 0, 0, gfx, ind, tile);

		if (callback_res != CALLBACK_FAILED) {
			frame_set_by_callback = true;

			switch (callback_res & 0xFF) {
				case 0xFF:
					DeleteAnimatedTile(tile);
					break;
				case 0xFE:
					/* Carry on as normal. */
					frame_set_by_callback = false;
					break;
				default:
					frame = callback_res & 0xFF;
					break;
			}
		}
	}

	if (!frame_set_by_callback) {
		if (frame < num_frames) {
			frame++;
		} else if (frame == num_frames && GB(itspec->animation_info, 8, 8) == 1) {
			/* This animation loops, so start again from the beginning */
			frame = 0;
		} else {
			/* This animation doesn't loop, so stay here */
			DeleteAnimatedTile(tile);
		}
	}

	SetIndustryAnimationState(tile, frame);
	MarkTileDirtyByTile(tile);
}

static void ChangeIndustryTileAnimationFrame(TileIndex tile, IndustryAnimationTrigger iat, uint32 random_bits, IndustryGfx gfx, Industry *ind)
{
	uint16 callback_res = GetIndustryTileCallback(CBID_INDTILE_ANIM_START_STOP, random_bits, iat, gfx, ind, tile);
	if (callback_res == CALLBACK_FAILED) return;

	switch (callback_res & 0xFF) {
		case 0xFD: /* Do nothing. */         break;
		case 0xFE: AddAnimatedTile(tile);    break;
		case 0xFF: DeleteAnimatedTile(tile); break;
		default:
			SetIndustryAnimationState(tile, callback_res & 0xFF);
			AddAnimatedTile(tile);
			break;
	}
}

bool StartStopIndustryTileAnimation(TileIndex tile, IndustryAnimationTrigger iat, uint32 random)
{
	IndustryGfx gfx = GetIndustryGfx(tile);
	const IndustryTileSpec *itspec = GetIndustryTileSpec(gfx);

	if (!HASBIT(itspec->animation_triggers, iat)) return false;

	Industry *ind = GetIndustryByTile(tile);
	ChangeIndustryTileAnimationFrame(tile, iat, random, gfx, ind);
	return true;
}

bool StartStopIndustryTileAnimation(const Industry *ind, IndustryAnimationTrigger iat)
{
	bool ret = true;
	uint32 random = Random();
	BEGIN_TILE_LOOP(tile, ind->width, ind->height, ind->xy)
		if (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == ind->index) {
			if (StartStopIndustryTileAnimation(tile, iat, random)) {
				SB(random, 0, 16, Random());
			} else {
				ret = false;
			}
		}
	END_TILE_LOOP(tile, ind->width, ind->height, ind->xy)

	return ret;
}
