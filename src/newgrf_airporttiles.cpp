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
#include "airport.h"
#include "newgrf.h"
#include "newgrf_airporttiles.h"
#include "newgrf_spritegroup.h"
#include "station_base.h"
#include "water.h"
#include "viewport_func.h"
#include "landscape.h"
#include "company_base.h"
#include "town.h"
#include "table/airporttiles.h"


AirportTileSpec AirportTileSpec::tiles[NUM_AIRPORTTILES];

AirportTileOverrideManager _airporttile_mngr(NEW_AIRPORTTILE_OFFSET, NUM_AIRPORTTILES, INVALID_AIRPORTTILE);

/**
 * Retrieve airport tile spec for the given airport tile
 * @param gfx index of airport tile
 * @return A pointer to the corresponding AirportTileSpec
 */
/* static */ const AirportTileSpec *AirportTileSpec::Get(StationGfx gfx)
{
	assert(gfx < lengthof(AirportTileSpec::tiles));
	return &AirportTileSpec::tiles[gfx];
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
	assert(gfx < NUM_AIRPORTTILES);
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

	/* Don't use GetAirportGfx here as we want the original (not overriden)
	 * graphics id. */
	StationGfx gfx = GetStationGfx(tile);
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
	if (ats->grf_prop.spritegroup != NULL) { // tile has a spritegroup ?
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
		case 0x43: return GetRelativePosition(tile, st->airport_tile);

		/* Animation frame of tile */
		case 0x44: return GetStationAnimationFrame(tile);

		/* Land info of nearby tiles */
		case 0x60: return GetNearbyAirportTileInformation(parameter, tile, st->index);

		/* Animation stage of nearby tiles */
		case 0x61:
			tile = GetNearbyTile(parameter, tile);
			if (st->TileBelongsToAirport(tile)) {
				return GetStationAnimationFrame(tile);
			}
			return UINT_MAX;

		/* Get airport tile ID at offset */
		case 0x62: return GetAirportTileIDAtOffset(GetNearbyTile(parameter, tile), st, object->grffile->grfid);
	}

	DEBUG(grf, 1, "Unhandled airport tile property 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static void AirportTileResolver(ResolverObject *res, StationGfx gfx, TileIndex tile, Station *st)
{
	res->GetRandomBits = NULL;
	res->GetTriggers   = NULL;
	res->SetTriggers   = NULL;
	res->GetVariable   = AirportTileGetVariable;
	res->ResolveReal   = AirportTileResolveReal;

	assert(st != NULL);
	res->psa                  = NULL;
	res->u.airport.airport_id = st->airport_type;
	res->u.airport.st         = st;
	res->u.airport.tile       = tile;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;

	const AirportTileSpec *ats = AirportTileSpec::Get(gfx);
	res->grffile         = ats->grf_prop.grffile;
}

uint16 GetAirportTileCallback(CallbackID callback, uint32 param1, uint32 param2, StationGfx gfx_id, Station *st, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	AirportTileResolver(&object, gfx_id, tile, st);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = SpriteGroup::Resolve(AirportTileSpec::Get(gfx_id)->grf_prop.spritegroup, &object);
	if (group == NULL) return CALLBACK_FAILED;

	return group->GetCallbackResult();
}

static void AirportDrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, byte colour, StationGfx gfx)
{
	const DrawTileSprites *dts = group->dts;

	SpriteID image = dts->ground.sprite;
	SpriteID pal   = dts->ground.pal;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		if (image == SPR_FLAT_WATER_TILE && GetWaterClass(ti->tile) != WATER_CLASS_INVALID) {
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
		if (HasBit(airts->callback_flags, CBM_AIRT_DRAW_FOUNDATIONS)) {
			/* Called to determine the type (if any) of foundation to draw */
			uint32 callback_res = GetAirportTileCallback(CBID_AIRPTILE_DRAW_FOUNDATIONS, 0, 0, gfx, st, ti->tile);
			draw_old_one = (callback_res != 0);
		}

		if (draw_old_one) DrawFoundation(ti, FOUNDATION_LEVELED);
	}

	AirportTileResolver(&object, gfx, ti->tile, st);

	group = SpriteGroup::Resolve(airts->grf_prop.spritegroup, &object);
	if (group == NULL || group->type != SGT_TILELAYOUT) {
		return false;
	}

	const TileLayoutSpriteGroup *tlgroup = (const TileLayoutSpriteGroup *)group;
	AirportDrawTileLayout(ti, tlgroup, Company::Get(st->owner)->colour, gfx);
	return true;
}


