/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airporttiles.cpp NewGRF handling of airport tiles. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_airporttiles.h"
#include "newgrf_spritegroup.h"
#include "newgrf_sound.h"
#include "station_base.h"
#include "water.h"
#include "landscape.h"
#include "company_base.h"
#include "town.h"
#include "table/strings.h"
#include "table/airporttiles.h"
#include "newgrf_animation_base.h"

#include "safeguards.h"

extern uint32_t GetRelativePosition(TileIndex tile, TileIndex ind_tile);

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
	static_assert(MAX_UVALUE(StationGfx) + 1 == lengthof(tiles));
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
	auto insert = std::copy(std::begin(_origin_airporttile_specs), std::end(_origin_airporttile_specs), std::begin(AirportTileSpec::tiles));
	std::fill(insert, std::end(AirportTileSpec::tiles), AirportTileSpec{});

	/* Reset any overrides that have been set. */
	_airporttile_mngr.ResetOverride();
}

void AirportTileOverrideManager::SetEntitySpec(const AirportTileSpec *airpts)
{
	StationGfx airpt_id = this->AddEntityID(airpts->grf_prop.local_id, airpts->grf_prop.grffile->grfid, airpts->grf_prop.subst_id);

	if (airpt_id == this->invalid_id) {
		GrfMsg(1, "AirportTile.SetEntitySpec: Too many airport tiles allocated. Ignoring.");
		return;
	}

	AirportTileSpec::tiles[airpt_id] = *airpts;

	/* Now add the overrides. */
	for (int i = 0; i < this->max_offset; i++) {
		AirportTileSpec *overridden_airpts = &AirportTileSpec::tiles[i];

		if (this->entity_overrides[i] != airpts->grf_prop.local_id || this->grfid_overrides[i] != airpts->grf_prop.grffile->grfid) continue;

		overridden_airpts->grf_prop.override = airpt_id;
		overridden_airpts->enabled = false;
		this->entity_overrides[i] = this->invalid_id;
		this->grfid_overrides[i] = 0;
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

/**
 * Based on newhouses/newindustries equivalent, but adapted for airports.
 * @param parameter from callback. It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the industry been queried for
 * @param grf_version8 True, if we are dealing with a new NewGRF which uses GRF version >= 8.
 * @return a construction of bits obeying the newgrf format
 */
static uint32_t GetNearbyAirportTileInformation(byte parameter, TileIndex tile, StationID index, bool grf_version8)
{
	if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required
	bool is_same_airport = (IsTileType(tile, MP_STATION) && IsAirport(tile) && GetStationIndex(tile) == index);

	return GetNearbyTileInformation(tile, grf_version8) | (is_same_airport ? 1 : 0) << 8;
}


/**
 * Make an analysis of a tile and check whether it belongs to the same
 * airport, and/or the same grf file
 * @param tile TileIndex of the tile to query
 * @param st Station to which to compare the tile to
 * @param cur_grfid GRFID of the current callback
 * @return value encoded as per NFO specs
 */
static uint32_t GetAirportTileIDAtOffset(TileIndex tile, const Station *st, uint32_t cur_grfid)
{
	if (!st->TileBelongsToAirport(tile)) {
		return 0xFFFF;
	}

	StationGfx gfx = GetAirportGfx(tile);
	const AirportTileSpec *ats = AirportTileSpec::Get(gfx);

	if (gfx < NEW_AIRPORTTILE_OFFSET) { // Does it belongs to an old type?
		/* It is an old tile.  We have to see if it's been overridden */
		if (ats->grf_prop.override == INVALID_AIRPORTTILE) { // has it been overridden?
			return 0xFF << 8 | gfx; // no. Tag FF + the gfx id of that tile
		}
		/* Overridden */
		const AirportTileSpec *tile_ovr = AirportTileSpec::Get(ats->grf_prop.override);

		if (tile_ovr->grf_prop.grffile->grfid == cur_grfid) {
			return tile_ovr->grf_prop.local_id; // same grf file
		} else {
			return 0xFFFE; // not the same grf file
		}
	}
	/* Not an 'old type' tile */
	if (ats->grf_prop.spritegroup[0] != nullptr) { // tile has a spritegroup ?
		if (ats->grf_prop.grffile->grfid == cur_grfid) { // same airport, same grf ?
			return ats->grf_prop.local_id;
		} else {
			return 0xFFFE; // Defined in another grf file
		}
	}
	/* The tile has no spritegroup */
	return 0xFF << 8 | ats->grf_prop.subst_id; // so just give it the substitute
}

/* virtual */ uint32_t AirportTileScopeResolver::GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const
{
	assert(this->st != nullptr);

	switch (variable) {
		/* Terrain type */
		case 0x41: return GetTerrainType(this->tile);

		/* Current town zone of the tile in the nearest town */
		case 0x42: return GetTownRadiusGroup(ClosestTownFromTile(this->tile, UINT_MAX), this->tile);

		/* Position relative to most northern airport tile. */
		case 0x43: return GetRelativePosition(this->tile, this->st->airport.tile);

		/* Animation frame of tile */
		case 0x44: return GetAnimationFrame(this->tile);

		/* Land info of nearby tiles */
		case 0x60: return GetNearbyAirportTileInformation(parameter, this->tile, this->st->index, this->ro.grffile->grf_version >= 8);

		/* Animation stage of nearby tiles */
		case 0x61: {
			TileIndex tile = GetNearbyTile(parameter, this->tile);
			if (this->st->TileBelongsToAirport(tile)) {
				return GetAnimationFrame(tile);
			}
			return UINT_MAX;
		}

		/* Get airport tile ID at offset */
		case 0x62: return GetAirportTileIDAtOffset(GetNearbyTile(parameter, this->tile), this->st, this->ro.grffile->grfid);
	}

	Debug(grf, 1, "Unhandled airport tile variable 0x{:X}", variable);

	*available = false;
	return UINT_MAX;
}

/* virtual */ uint32_t AirportTileScopeResolver::GetRandomBits() const
{
	return (this->st == nullptr ? 0 : this->st->random_bits) | (this->tile == INVALID_TILE ? 0 : GetStationTileRandomBits(this->tile) << 16);
}

/**
 * Constructor of the resolver for airport tiles.
 * @param ats Specification of the airport tiles.
 * @param tile %Tile for the callback, only valid for airporttile callbacks.
 * @param st Station of the airport for which the callback is run, or \c nullptr for build gui.
 * @param callback Callback ID.
 * @param callback_param1 First parameter (var 10) of the callback.
 * @param callback_param2 Second parameter (var 18) of the callback.
 */
AirportTileResolverObject::AirportTileResolverObject(const AirportTileSpec *ats, TileIndex tile, Station *st,
		CallbackID callback, uint32_t callback_param1, uint32_t callback_param2)
	: ResolverObject(ats->grf_prop.grffile, callback, callback_param1, callback_param2),
		tiles_scope(*this, ats, tile, st),
		airport_scope(*this, tile, st, st != nullptr ? st->airport.type : (byte)AT_DUMMY, st != nullptr ? st->airport.layout : 0)
{
	this->root_spritegroup = ats->grf_prop.spritegroup[0];
}

GrfSpecFeature AirportTileResolverObject::GetFeature() const
{
	return GSF_AIRPORTTILES;
}

uint32_t AirportTileResolverObject::GetDebugID() const
{
	return this->tiles_scope.ats->grf_prop.local_id;
}

uint16_t GetAirportTileCallback(CallbackID callback, uint32_t param1, uint32_t param2, const AirportTileSpec *ats, Station *st, TileIndex tile, [[maybe_unused]] int extra_data = 0)
{
	AirportTileResolverObject object(ats, tile, st, callback, param1, param2);
	return object.ResolveCallback();
}

static void AirportDrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, byte colour)
{
	const DrawTileSprites *dts = group->ProcessRegisters(nullptr);

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

bool DrawNewAirportTile(TileInfo *ti, Station *st, const AirportTileSpec *airts)
{
	if (ti->tileh != SLOPE_FLAT) {
		bool draw_old_one = true;
		if (HasBit(airts->callback_mask, CBM_AIRT_DRAW_FOUNDATIONS)) {
			/* Called to determine the type (if any) of foundation to draw */
			uint32_t callback_res = GetAirportTileCallback(CBID_AIRPTILE_DRAW_FOUNDATIONS, 0, 0, airts, st, ti->tile);
			if (callback_res != CALLBACK_FAILED) draw_old_one = ConvertBooleanCallback(airts->grf_prop.grffile, CBID_AIRPTILE_DRAW_FOUNDATIONS, callback_res);
		}

		if (draw_old_one) DrawFoundation(ti, FOUNDATION_LEVELED);
	}

	AirportTileResolverObject object(airts, ti->tile, st);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr || group->type != SGT_TILELAYOUT) {
		return false;
	}

	const TileLayoutSpriteGroup *tlgroup = (const TileLayoutSpriteGroup *)group;
	AirportDrawTileLayout(ti, tlgroup, Company::Get(st->owner)->colour);
	return true;
}

/** Helper class for animation control. */
struct AirportTileAnimationBase : public AnimationBase<AirportTileAnimationBase, AirportTileSpec, Station, int, GetAirportTileCallback, TileAnimationFrameAnimationHelper<Station> > {
	static const CallbackID cb_animation_speed      = CBID_AIRPTILE_ANIMATION_SPEED;
	static const CallbackID cb_animation_next_frame = CBID_AIRPTILE_ANIM_NEXT_FRAME;

	static const AirportTileCallbackMask cbm_animation_speed      = CBM_AIRT_ANIM_SPEED;
	static const AirportTileCallbackMask cbm_animation_next_frame = CBM_AIRT_ANIM_NEXT_FRAME;
};

void AnimateAirportTile(TileIndex tile)
{
	const AirportTileSpec *ats = AirportTileSpec::GetByTile(tile);
	if (ats == nullptr) return;

	AirportTileAnimationBase::AnimateTile(ats, Station::GetByTile(tile), tile, HasBit(ats->animation_special_flags, 0));
}

void AirportTileAnimationTrigger(Station *st, TileIndex tile, AirpAnimationTrigger trigger, CargoID cargo_type)
{
	const AirportTileSpec *ats = AirportTileSpec::GetByTile(tile);
	if (!HasBit(ats->animation.triggers, trigger)) return;

	AirportTileAnimationBase::ChangeAnimationFrame(CBID_AIRPTILE_ANIM_START_STOP, ats, st, tile, Random(), (uint8_t)trigger | (cargo_type << 8));
}

void AirportAnimationTrigger(Station *st, AirpAnimationTrigger trigger, CargoID cargo_type)
{
	if (st->airport.tile == INVALID_TILE) return;

	for (TileIndex tile : st->airport) {
		if (st->TileBelongsToAirport(tile)) AirportTileAnimationTrigger(st, tile, trigger, cargo_type);
	}
}

