/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file clear_cmd.cpp Commands related to clear tiles. */

#include "stdafx.h"
#include "clear_map.h"
#include "command_func.h"
#include "landscape.h"
#include "genworld.h"
#include "viewport_func.h"
#include "core/random_func.hpp"
#include "newgrf_generic.h"
#include "landscape_cmd.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/clear_land.h"

#include "safeguards.h"

static CommandCost ClearTile_Clear(TileIndex tile, DoCommandFlags flags)
{
	static constexpr Price clear_price_table[] = {
		PR_CLEAR_GRASS,
		PR_CLEAR_ROUGH,
		PR_CLEAR_ROCKS,
		PR_CLEAR_FIELDS,
		PR_CLEAR_ROUGH,
		PR_CLEAR_ROUGH,
	};
	CommandCost price(EXPENSES_CONSTRUCTION);

	ClearGround ground = GetClearGround(tile);
	uint8_t density = GetClearDensity(tile);
	if (IsSnowTile(tile)) {
		price.AddCost(_price[clear_price_table[ground]]);
		/* Add a little more for removing snow. */
		price.AddCost(std::abs(_price[PR_CLEAR_ROUGH] - _price[PR_CLEAR_GRASS]));
	} else if (ground != CLEAR_GRASS || density != 0) {
		price.AddCost(_price[clear_price_table[ground]]);
	}

	if (flags.Test(DoCommandFlag::Execute)) DoClearSquare(tile);

	return price;
}

void DrawClearLandTile(const TileInfo *ti, uint8_t set)
{
	DrawGroundSprite(SPR_FLAT_BARE_LAND + SlopeToSpriteOffset(ti->tileh) + set * 19, PAL_NONE);
}

void DrawHillyLandTile(const TileInfo *ti)
{
	if (ti->tileh != SLOPE_FLAT) {
		DrawGroundSprite(SPR_FLAT_ROUGH_LAND + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
	} else {
		DrawGroundSprite(_landscape_clear_sprites_rough[GB(TileHash(ti->x, ti->y), 0, 3)], PAL_NONE);
	}
}

static void DrawClearLandFence(const TileInfo *ti)
{
	/* combine fences into one sprite object */
	StartSpriteCombine();

	SpriteBounds bounds{{}, {TILE_SIZE, TILE_SIZE, 4}, {}};

	bounds.extent.z += GetSlopeMaxPixelZ(ti->tileh);

	uint fence_nw = GetFence(ti->tile, DIAGDIR_NW);
	if (fence_nw != 0) {
		bounds.offset.x = 0;
		bounds.offset.y = -static_cast<int>(TILE_SIZE);
		bounds.offset.z = GetSlopePixelZInCorner(ti->tileh, CORNER_W);
		SpriteID sprite = _clear_land_fence_sprites[fence_nw - 1] + _fence_mod_by_tileh_nw[ti->tileh];
		AddSortableSpriteToDraw(sprite, PAL_NONE, *ti, bounds, false);
	}

	uint fence_ne = GetFence(ti->tile, DIAGDIR_NE);
	if (fence_ne != 0) {
		bounds.offset.x = -static_cast<int>(TILE_SIZE);
		bounds.offset.y = 0;
		bounds.offset.z = GetSlopePixelZInCorner(ti->tileh, CORNER_E);
		SpriteID sprite = _clear_land_fence_sprites[fence_ne - 1] + _fence_mod_by_tileh_ne[ti->tileh];
		AddSortableSpriteToDraw(sprite, PAL_NONE, *ti, bounds, false);
	}

	uint fence_sw = GetFence(ti->tile, DIAGDIR_SW);
	uint fence_se = GetFence(ti->tile, DIAGDIR_SE);

	if (fence_sw != 0 || fence_se != 0) {
		bounds.offset.x = 0;
		bounds.offset.y = 0;
		bounds.offset.z = GetSlopePixelZInCorner(ti->tileh, CORNER_S);

		if (fence_sw != 0) {
			SpriteID sprite = _clear_land_fence_sprites[fence_sw - 1] + _fence_mod_by_tileh_sw[ti->tileh];
			AddSortableSpriteToDraw(sprite, PAL_NONE, *ti, bounds, false);
		}

		if (fence_se != 0) {
			SpriteID sprite = _clear_land_fence_sprites[fence_se - 1] + _fence_mod_by_tileh_se[ti->tileh];
			AddSortableSpriteToDraw(sprite, PAL_NONE, *ti, bounds, false);

		}
	}
	EndSpriteCombine();
}

static void DrawTile_Clear(TileInfo *ti)
{
	ClearGround real_ground = GetClearGround(ti->tile);
	ClearGround ground = IsSnowTile(ti->tile) ? CLEAR_SNOW : real_ground;

	switch (ground) {
		case CLEAR_GRASS:
			DrawClearLandTile(ti, GetClearDensity(ti->tile));
			break;

		case CLEAR_ROUGH:
			DrawHillyLandTile(ti);
			break;

		case CLEAR_ROCKS:
			DrawGroundSprite((HasGrfMiscBit(GrfMiscBit::SecondRockyTileSet) && (TileHash(ti->x, ti->y) & 1) ? SPR_FLAT_ROCKY_LAND_2 : SPR_FLAT_ROCKY_LAND_1) + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
			break;

		case CLEAR_FIELDS:
			DrawGroundSprite(_clear_land_sprites_farmland[GetFieldType(ti->tile)] + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
			DrawClearLandFence(ti);
			break;

		case CLEAR_SNOW: {
			uint8_t density = GetClearDensity(ti->tile);
			DrawGroundSprite(_clear_land_sprites_snow_desert[density] + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
			if (real_ground == CLEAR_ROCKS) {
				/* There 4 levels of snowy overlay rocks, each with 19 sprites. */
				++density;
				DrawGroundSprite(SPR_OVERLAY_ROCKS_BASE + (density * 19) + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
			}
			break;
		}

		case CLEAR_DESERT:
			DrawGroundSprite(_clear_land_sprites_snow_desert[GetClearDensity(ti->tile)] + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
			break;
	}

	DrawBridgeMiddle(ti, {});
}

static int GetSlopePixelZ_Clear(TileIndex tile, uint x, uint y, bool)
{
	auto [tileh, z] = GetTilePixelSlope(tile);

	return z + GetPartialPixelZ(x & 0xF, y & 0xF, tileh);
}

static Foundation GetFoundation_Clear(TileIndex, Slope)
{
	return FOUNDATION_NONE;
}

static void UpdateFences(TileIndex tile)
{
	assert(IsTileType(tile, MP_CLEAR) && IsClearGround(tile, CLEAR_FIELDS));
	bool dirty = false;

	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		if (GetFence(tile, dir) != 0) continue;
		TileIndex neighbour = tile + TileOffsByDiagDir(dir);
		if (IsTileType(neighbour, MP_CLEAR) && IsClearGround(neighbour, CLEAR_FIELDS)) continue;
		SetFence(tile, dir, 3);
		dirty = true;
	}

	if (dirty) MarkTileDirtyByTile(tile);
}


/** Convert to or from snowy tiles. */
static void TileLoopClearAlps(TileIndex tile)
{
	int k = GetTileZ(tile) - GetSnowLine() + 1;

	if (!IsSnowTile(tile)) {
		/* Below the snow line, do nothing if no snow. */
		/* At or above the snow line, make snow tile if needed. */
		if (k >= 0) {
			/* Snow density is started at 0 so that it can gradually reach the required density. */
			MakeSnow(tile, 0);
			MarkTileDirtyByTile(tile);
		}
		return;
	}

	/* Update snow density. */
	uint current_density = GetClearDensity(tile);
	uint req_density = (k < 0) ? 0u : std::min<uint>(k, 3u);

	if (current_density == req_density) {
		/* Density at the required level. */
		if (k >= 0) return;
		ClearSnow(tile);
	} else {
		AddClearDensity(tile, current_density < req_density ? 1 : -1);
	}

	MarkTileDirtyByTile(tile);
}

/**
 * Tests if at least one surrounding tile is non-desert
 * @param tile tile to check
 * @return does this tile have at least one non-desert tile around?
 */
static inline bool NeighbourIsNormal(TileIndex tile)
{
	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		TileIndex t = tile + TileOffsByDiagDir(dir);
		if (!IsValidTile(t)) continue;
		if (GetTropicZone(t) != TROPICZONE_DESERT) return true;
		if (HasTileWaterClass(t) && GetWaterClass(t) == WaterClass::Sea) return true;
	}
	return false;
}

static void TileLoopClearDesert(TileIndex tile)
{
	/* Current desert level - 0 if it is not desert */
	uint current = 0;
	if (IsClearGround(tile, CLEAR_DESERT)) current = GetClearDensity(tile);

	/* Expected desert level - 0 if it shouldn't be desert */
	uint expected = 0;
	if (GetTropicZone(tile) == TROPICZONE_DESERT) {
		expected = NeighbourIsNormal(tile) ? 1 : 3;
	}

	if (current == expected) return;

	if (expected == 0) {
		SetClearGroundDensity(tile, CLEAR_GRASS, 3);
	} else {
		/* Transition from clear to desert is not smooth (after clearing desert tile) */
		SetClearGroundDensity(tile, CLEAR_DESERT, expected);
	}

	MarkTileDirtyByTile(tile);
}

static void TileLoop_Clear(TileIndex tile)
{
	AmbientSoundEffect(tile);

	switch (_settings_game.game_creation.landscape) {
		case LandscapeType::Tropic: TileLoopClearDesert(tile); break;
		case LandscapeType::Arctic: TileLoopClearAlps(tile);   break;
		default: break;
	}

	if (IsSnowTile(tile)) return;

	switch (GetClearGround(tile)) {
		case CLEAR_GRASS:
			if (GetClearDensity(tile) == 3) return;

			if (_game_mode != GM_EDITOR) {
				if (GetClearCounter(tile) < 7) {
					AddClearCounter(tile, 1);
					return;
				} else {
					SetClearCounter(tile, 0);
					AddClearDensity(tile, 1);
				}
			} else {
				SetClearGroundDensity(tile, GB(Random(), 0, 8) > 21 ? CLEAR_GRASS : CLEAR_ROUGH, 3);
			}
			break;

		case CLEAR_FIELDS:
			UpdateFences(tile);

			if (_game_mode == GM_EDITOR) return;

			if (GetClearCounter(tile) < 7) {
				AddClearCounter(tile, 1);
				return;
			} else {
				SetClearCounter(tile, 0);
			}

			if (GetIndustryIndexOfField(tile) == IndustryID::Invalid() && GetFieldType(tile) >= 7) {
				/* This farmfield is no longer farmfield, so make it grass again */
				MakeClear(tile, CLEAR_GRASS, 2);
			} else {
				uint field_type = GetFieldType(tile);
				field_type = (field_type < 8) ? field_type + 1 : 0;
				SetFieldType(tile, field_type);
			}
			break;

		default:
			return;
	}

	MarkTileDirtyByTile(tile);
}

void GenerateClearTile()
{
	uint i, gi;
	TileIndex tile;

	/* add rough tiles */
	i = Map::ScaleBySize(GB(Random(), 0, 10) + 0x400);
	gi = Map::ScaleBySize(GB(Random(), 0, 7) + 0x80);

	SetGeneratingWorldProgress(GWP_ROUGH_ROCKY, gi + i);
	do {
		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		tile = RandomTile();
		if (IsTileType(tile, MP_CLEAR) && !IsClearGround(tile, CLEAR_DESERT)) SetClearGroundDensity(tile, CLEAR_ROUGH, 3);
	} while (--i);

	/* add rocky tiles */
	i = gi;
	do {
		uint32_t r = Random();
		tile = RandomTileSeed(r);

		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		if (IsTileType(tile, MP_CLEAR) && !IsClearGround(tile, CLEAR_DESERT)) {
			uint j = GB(r, 16, 4) + 5;
			for (;;) {
				TileIndex tile_new;

				SetClearGroundDensity(tile, CLEAR_ROCKS, 3);
				MarkTileDirtyByTile(tile);
				do {
					if (--j == 0) goto get_out;
					tile_new = tile + TileOffsByDiagDir((DiagDirection)GB(Random(), 0, 2));
				} while (!IsTileType(tile_new, MP_CLEAR) || IsClearGround(tile_new, CLEAR_DESERT));
				tile = tile_new;
			}
get_out:;
		}
	} while (--i);
}

static TrackStatus GetTileTrackStatus_Clear(TileIndex, TransportType, uint, DiagDirection)
{
	return 0;
}

static void GetTileDesc_Clear(TileIndex tile, TileDesc &td)
{
	/* Each pair holds a normal and a snowy ClearGround description. */
	static constexpr std::pair<StringID, StringID> clear_land_str[] = {
		{STR_LAI_CLEAR_DESCRIPTION_GRASS,      STR_LAI_CLEAR_DESCRIPTION_SNOWY_GRASS},
		{STR_LAI_CLEAR_DESCRIPTION_ROUGH_LAND, STR_LAI_CLEAR_DESCRIPTION_SNOWY_ROUGH_LAND},
		{STR_LAI_CLEAR_DESCRIPTION_ROCKS,      STR_LAI_CLEAR_DESCRIPTION_SNOWY_ROCKS},
		{STR_LAI_CLEAR_DESCRIPTION_FIELDS,     STR_EMPTY},
		{STR_EMPTY,                            STR_EMPTY}, // CLEAR_SNOW does not appear in the map.
		{STR_LAI_CLEAR_DESCRIPTION_DESERT,     STR_EMPTY},
	};

	if (!IsSnowTile(tile) && IsClearGround(tile, CLEAR_GRASS) && GetClearDensity(tile) == 0) {
		td.str = STR_LAI_CLEAR_DESCRIPTION_BARE_LAND;
	} else {
		const auto &[name, snowy_name] = clear_land_str[GetClearGround(tile)];
		td.str = IsSnowTile(tile) ? snowy_name : name;
	}
	td.owner[0] = GetTileOwner(tile);
}

static void ChangeTileOwner_Clear(TileIndex, Owner, Owner)
{
	return;
}

static CommandCost TerraformTile_Clear(TileIndex tile, DoCommandFlags flags, int, Slope)
{
	return Command<CMD_LANDSCAPE_CLEAR>::Do(flags, tile);
}

static CommandCost CheckBuildAbove_Clear(TileIndex, DoCommandFlags, Axis, int)
{
	/* Can always build above clear tiles. */
	return CommandCost();
}

extern const TileTypeProcs _tile_type_clear_procs = {
	DrawTile_Clear,           ///< draw_tile_proc
	GetSlopePixelZ_Clear,     ///< get_slope_z_proc
	ClearTile_Clear,          ///< clear_tile_proc
	nullptr,                     ///< add_accepted_cargo_proc
	GetTileDesc_Clear,        ///< get_tile_desc_proc
	GetTileTrackStatus_Clear, ///< get_tile_track_status_proc
	nullptr,                     ///< click_tile_proc
	nullptr,                     ///< animate_tile_proc
	TileLoop_Clear,           ///< tile_loop_proc
	ChangeTileOwner_Clear,    ///< change_tile_owner_proc
	nullptr,                     ///< add_produced_cargo_proc
	nullptr,                     ///< vehicle_enter_tile_proc
	GetFoundation_Clear,      ///< get_foundation_proc
	TerraformTile_Clear,      ///< terraform_tile_proc
	CheckBuildAbove_Clear, // check_build_above_proc
};
