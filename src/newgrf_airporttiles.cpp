/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airporttiles.cpp NewGRF handling of airport tiles. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf.h"
#include "newgrf_airporttiles.h"
#include "newgrf_spritegroup.h"
#include "newgrf_sound.h"
#include "station_base.h"
#include "water.h"
#include "viewport_func.h"
#include "landscape.h"
#include "company_base.h"
#include "town.h"
#include "table/strings.h"
#include "table/airporttiles.h"
#include "newgrf_animation_base.h"


AirportTileSpec AirportTileSpec::tiles[NUM_AIRPORTTILES];

AirportTileOverrideManager _airporttile_mngr(NEW_AIRPORTTILE_OFFSET, NUM_AIRPORTTILES, INVALID_AIRPORTTILE);

/**
 * Retrieve airport tile spec for the given airport tile
 * @param gfx index of airport tile
 * @return A pointer to the corresponding AirportTileSpec
 */
/* static */ const AirportTileSpec *AirportTileSpec::Get(StationGfx gfx)
{
	/* should be assert(gfx < lengthof(tiles)), but that gives compiler warnings
	 * since it's always true if the following holds: */
	assert_compile(MAX_UVALUE(StationGfx) + 1 == lengthof(tiles));
	return &AirportTileSpec::tiles[gfx];
}

/**
 * Retrieve airport tile spec for the given airport tile.
 * @param tile The airport tile.
 * @return A pointer to the corresponding AirportTileSpec.
 */
/* static */ const AirportTileSpec *AirportTileSpec::GetByTile(TileIndex tile)
{
	return AirportTileSpec::Get(GetAirportGfx(tile));
}

/**
 * This function initializes the tile array of AirportTileSpec
 */
void AirportTileSpec::ResetAirportTiles()
{
	memset(&AirportTileSpec::tiles, 0, sizeof(AirportTileSpec::tiles));
	memcpy(&AirportTileSpec::tiles, &_origin_airporttile_specs, sizeof(_origin_airporttile_specs));

	/* Reset any overrides that have been set. */
	_airporttile_mngr.ResetOverride();
}

void AirportTileOverrideManager::SetEntitySpec(const AirportTileSpec *airpts)
{
	StationGfx airpt_id = this->AddEntityID(airpts->grf_prop.local_id, airpts->grf_prop.grffile->grfid, airpts->grf_prop.subst_id);

	if (airpt_id == invalid_ID) {
		grfmsg(1, "AirportTile.SetEntitySpec: Too many airport tiles allocated. Ignoring.");
		return;
	}

	memcpy(&AirportTileSpec::tiles[airpt_id], airpts, sizeof(*airpts));

	/* Now add the overrides. */
	for (int i = 0; i < max_offset; i++) {
		AirportTileSpec *overridden_airpts = &AirportTileSpec::tiles[i];

		if (entity_overrides[i] != airpts->grf_prop.local_id || grfid_overrides[i] != airpts->grf_prop.grffile->grfid) continue;

		overridden_airpts->grf_prop.override = airpt_id;
		overridden_airpts->enabled = false;
		entity_overrides[i] = invalid_ID;
		grfid_overrides[i] = 0;
	}
}

/**
 * Do airporttile gfx ID translation for NewGRFs.
 * @param gfx the type to get the override for.
 * @return the gfx to actually work with.
 */
StationGfx GetTranslatedAirportTileID(StationGfx gfx)
{
	const AirportTileSpec *it = AirportTileSpec::Get(gfx);
	return it->grf_prop.override == INVALID_AIRPORTTILE ? gfx : it->grf_prop.override;
}


static const SpriteGroup *AirportTileResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	/* AirportTile do not have 'real' groups. */
	return NULL;
}

/**
 * Based on newhouses/newindustries equivalent, but adapted for airports.
 * @param parameter from callback. It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the industry been queried for
 * @return a construction of bits obeying the newgrf format
 */
uint32 GetNearbyAirportTileInformation(byte parameter, TileIndex tile, StationID index)
{
	if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required
	bool is_same_airport = (IsTileType(tile, MP_STATION) && IsAirport(tile) && GetStationIndex(tile) == index);

	return GetNearbyTileInformation(tile) | (is_same_airport ? 1 : 0) << 8;
}


/**
 * Make an analysis of a tile and check whether it belongs to the same
 * airport, and/or the same grf file
 * @param tile TileIndex of the tile to query
 * @param st Station to which to compare the tile to
 * @param cur_grfid GRFID of the current callback
 * @return value encoded as per NFO specs
 */
static uint32 GetAirportTileIDAtOffset(TileIndex tile, const Station *st, uint32 cur_grfid)
{
	if (!st->TileBelongsToAirport(tile)) {
		return 0xFFFF;
	}

	StationGfx gfx = GetAirportGfx(tile);
	const AirportTileSpec *ats = AirportTileSpec::Get(gfx);

	if (gfx < NEW_AIRPORTTILE_OFFSET) { // Does it belongs to an old type?
		/* It is an old tile.  We have to see if it's been overriden */
		if (ats->grf_prop.override == INVALID_AIRPORTTILE) { // has it been overridden?
			return 0xFF << 8 | gfx; // no. Tag FF + the gfx id of that tile
		}
		/* Overriden */
		const AirportTileSpec *tile_ovr = AirportTileSpec::Get(ats->grf_prop.override);

		if (tile_ovr->grf_prop.grffile->grfid == cur_grfid) {
			return tile_ovr->grf_prop.local_id; // same grf file
		} else {
			return 0xFFFE; // not the same grf file
		}
	}
	/* Not an 'old type' tile */
	if (ats->grf_prop.spritegroup[0] != NULL) { // tile has a spritegroup ?
		if (ats->grf_prop.grffile->grfid == cur_grfid) { // same airport, same grf ?
			return ats->grf_prop.local_id;
		} else {
			return 0xFFFE; // Defined in another grf file
		}
	}
	/* The tile has no spritegroup */
	return 0xFF << 8 | ats->grf_prop.subst_id; // so just give him the substitute
}

static uint32 AirportTileGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Station *st = object->u.airport.st;
	TileIndex tile    = object->u.airport.tile;
	assert(st != NULL);

	if (object->scope == VSG_SCOPE_PARENT) {
		DEBUG(grf, 1, "Parent scope for airport tiles unavailable");
		*available = false;
		return UINT_MAX;
	}

	extern uint32 GetRelativePosition(TileIndex tile, TileIndex ind_tile);

	switch (variable) {
		/* Terrain type */
		case 0x41: return GetTerrainType(tile);

		/* Current town zone of the tile in the nearest town */
		case 0x42: return GetTownRadiusGroup(ClosestTownFromTile(tile, UINT_MAX), tile);

		/* Position relative to most northern airport tile. */
		case 0x43: return GetRelativePosition(tile, st->airport.tile);

		/* Animation frame of tile */
		case 0x44: return GetAnimationFrame(tile);

		/* Land info of nearby tiles */
		case 0x60: return GetNearbyAirportTileInformation(parameter, tile, st->index);

		/* Animation stage of nearby tiles */
		case 0x61:
			tile = GetNearbyTile(parameter, tile);
			if (st->TileBelongsToAirport(tile)) {
				return GetAnimationFrame(tile);
			}
			return UINT_MAX;

		/* Get airport tile ID at offset */
		case 0x62: return GetAirportTileIDAtOffset(GetNearbyTile(parameter, tile), st, object->grffile->grfid);
	}

	DEBUG(grf, 1, "Unhandled airport tile variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static uint32 AirportTileGetRandomBits(const ResolverObject *object)
{
	const Station *st = object->u.airport.st;
	const TileIndex tile = object->u.airport.tile;
	return (st == NULL ? 0 : st->random_bits) | (tile == INVALID_TILE ? 0 : GetStationTileRandomBits(tile) << 16);
}

static void AirportTileResolver(ResolverObject *res, const AirportTileSpec *ats, TileIndex tile, Station *st)
{
	res->GetRandomBits = AirportTileGetRandomBits;
	res->GetTriggers   = NULL;
	res->SetTriggers   = NULL;
	res->GetVariable   = AirportTileGetVariable;
	res->ResolveReal   = AirportTileResolveReal;
	res->StorePSA      = NULL;

	assert(st != NULL);
	res->u.airport.airport_id = st->airport.type;
	res->u.airport.st         = st;
	res->u.airport.tile       = tile;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;

	res->grffile         = ats->grf_prop.grffile;
}

uint16 GetAirportTileCallback(CallbackID callback, uint32 param1, uint32 param2, const AirportTileSpec *ats, Station *st, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	AirportTileResolver(&object, ats, tile, st);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = SpriteGroup::Resolve(ats->grf_prop.spritegroup[0], &object);
	if (group == NULL) return CALLBACK_FAILED;

	return group->GetCallbackResult();
}

static void AirportDrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, byte colour, StationGfx gfx)
{
	const DrawTileSprites *dts = group->ProcessRegisters(NULL);

	SpriteID image = dts->ground.sprite;
	SpriteID pal   = dts->ground.pal;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		if (image == SPR_FLAT_WATER_TILE && IsTileOnWater(ti->tile)) {
			DrawWaterClassGround(ti);
		} else {
			DrawGroundSprite(image, GroundSpritePaletteTransform(image, pal, GENERAL_SPRITE_COLOUR(colour)));
		}
	}

	DrawNewGRFTileSeq(ti, dts, TO_BUILDINGS, 0, GENERAL_SPRITE_COLOUR(colour));
}

bool DrawNewAirportTile(TileInfo *ti, Station *st, StationGfx gfx, const AirportTileSpec *airts)
{
	const SpriteGroup *group;
	ResolverObject object;

	if (ti->tileh != SLOPE_FLAT) {
		bool draw_old_one = true;
		if (HasBit(airts->callback_mask, CBM_AIRT_DRAW_FOUNDATIONS)) {
			/* Called to determine the type (if any) of foundation to draw */
			uint32 callback_res = GetAirportTileCallback(CBID_AIRPTILE_DRAW_FOUNDATIONS, 0, 0, airts, st, ti->tile);
			draw_old_one = (callback_res != 0);
		}

		if (draw_old_one) DrawFoundation(ti, FOUNDATION_LEVELED);
	}

	AirportTileResolver(&object, airts, ti->tile, st);

	group = SpriteGroup::Resolve(airts->grf_prop.spritegroup[0], &object);
	if (group == NULL || group->type != SGT_TILELAYOUT) {
		return false;
	}

	const TileLayoutSpriteGroup *tlgroup = (const TileLayoutSpriteGroup *)group;
	AirportDrawTileLayout(ti, tlgroup, Company::Get(st->owner)->colour, gfx);
	return true;
}

/** Helper class for animation control. */
struct AirportTileAnimationBase : public AnimationBase<AirportTileAnimationBase, AirportTileSpec, Station, GetAirportTileCallback> {
	static const CallbackID cb_animation_speed      = CBID_AIRPTILE_ANIMATION_SPEED;
	static const CallbackID cb_animation_next_frame = CBID_AIRPTILE_ANIM_NEXT_FRAME;

	static const AirportTileCallbackMask cbm_animation_speed      = CBM_AIRT_ANIM_SPEED;
	static const AirportTileCallbackMask cbm_animation_next_frame = CBM_AIRT_ANIM_NEXT_FRAME;
};

void AnimateAirportTile(TileIndex tile)
{
	const AirportTileSpec *ats = AirportTileSpec::GetByTile(tile);
	if (ats == NULL) return;

	AirportTileAnimationBase::AnimateTile(ats, Station::GetByTile(tile), tile, HasBit(ats->animation_special_flags, 0));
}

void AirportTileAnimationTrigger(Station *st, TileIndex tile, AirpAnimationTrigger trigger, CargoID cargo_type)
{
	const AirportTileSpec *ats = AirportTileSpec::GetByTile(tile);
	if (!HasBit(ats->animation.triggers, trigger)) return;

	AirportTileAnimationBase::ChangeAnimationFrame(CBID_AIRPTILE_ANIM_START_STOP, ats, st, tile, Random(), (uint8)trigger | (cargo_type << 8));
}

void AirportAnimationTrigger(Station *st, AirpAnimationTrigger trigger, CargoID cargo_type)
{
	if (st->airport.tile == INVALID_TILE) return;

	TILE_AREA_LOOP(tile, st->airport) {
		if (st->TileBelongsToAirport(tile)) AirportTileAnimationTrigger(st, tile, trigger, cargo_type);
	}
}

/**
 * Resolve an airport tile's spec and such so we can get a variable.
 * @param ro    The resolver object to fill.
 * @param index The airport tile to get the data from.
 */
void GetAirportTileTypeResolver(ResolverObject *ro, uint index)
{
	AirportTileResolver(ro, AirportTileSpec::GetByTile(index), index, Station::GetByTile(index));
}
