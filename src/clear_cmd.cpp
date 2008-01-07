/* $Id$ */

/** @file clear_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "clear_map.h"
#include "table/strings.h"
#include "player.h"
#include "viewport.h"
#include "command_func.h"
#include "bridge.h"
#include "landscape.h"
#include "variables.h"
#include "table/sprites.h"
#include "unmovable_map.h"
#include "genworld.h"
#include "industry.h"
#include "water_map.h"
#include "tile_cmd.h"
#include "functions.h"
#include "economy_func.h"
#include "table/clear_land.h"

static CommandCost ClearTile_Clear(TileIndex tile, byte flags)
{
	static const Money* clear_price_table[] = {
		&_price.clear_grass,
		&_price.clear_roughland,
		&_price.clear_rocks,
		&_price.clear_fields,
		&_price.clear_roughland,
		&_price.clear_roughland,
	};
	CommandCost price;

	if (!IsClearGround(tile, CLEAR_GRASS) || GetClearDensity(tile) != 0) {
		price.AddCost(*clear_price_table[GetClearGround(tile)]);
	}

	if (flags & DC_EXEC) DoClearSquare(tile);

	return price;
}

void DrawClearLandTile(const TileInfo *ti, byte set)
{
	DrawGroundSprite(SPR_FLAT_BARE_LAND + _tileh_to_sprite[ti->tileh] + set * 19, PAL_NONE);
}

void DrawHillyLandTile(const TileInfo *ti)
{
	if (ti->tileh != SLOPE_FLAT) {
		DrawGroundSprite(SPR_FLAT_ROUGH_LAND + _tileh_to_sprite[ti->tileh], PAL_NONE);
	} else {
		DrawGroundSprite(_landscape_clear_sprites[GB(ti->x ^ ti->y, 4, 3)], PAL_NONE);
	}
}

void DrawClearLandFence(const TileInfo *ti)
{
	byte z = ti->z;

	if (ti->tileh & SLOPE_S) {
		z += TILE_HEIGHT;
		if (ti->tileh == SLOPE_STEEP_S) z += TILE_HEIGHT;
	}

	if (GetFenceSW(ti->tile) != 0) {
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[GetFenceSW(ti->tile) - 1] + _fence_mod_by_tileh[ti->tileh], PAL_NONE, ti->x, ti->y, z);
	}

	if (GetFenceSE(ti->tile) != 0) {
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[GetFenceSE(ti->tile) - 1] + _fence_mod_by_tileh_2[ti->tileh], PAL_NONE, ti->x, ti->y, z);
	}
}

static void DrawTile_Clear(TileInfo *ti)
{
	switch (GetClearGround(ti->tile)) {
		case CLEAR_GRASS:
			DrawClearLandTile(ti, GetClearDensity(ti->tile));
			break;

		case CLEAR_ROUGH:
			DrawHillyLandTile(ti);
			break;

		case CLEAR_ROCKS:
			DrawGroundSprite(SPR_FLAT_ROCKY_LAND_1 + _tileh_to_sprite[ti->tileh], PAL_NONE);
			break;

		case CLEAR_FIELDS:
			DrawGroundSprite(_clear_land_sprites_1[GetFieldType(ti->tile)] + _tileh_to_sprite[ti->tileh], PAL_NONE);
			break;

		case CLEAR_SNOW:
			DrawGroundSprite(_clear_land_sprites_2[GetClearDensity(ti->tile)] + _tileh_to_sprite[ti->tileh], PAL_NONE);
			break;

		case CLEAR_DESERT:
			DrawGroundSprite(_clear_land_sprites_3[GetClearDensity(ti->tile)] + _tileh_to_sprite[ti->tileh], PAL_NONE);
			break;
	}

	DrawClearLandFence(ti);
	DrawBridgeMiddle(ti);
}

static uint GetSlopeZ_Clear(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
}

static Foundation GetFoundation_Clear(TileIndex tile, Slope tileh)
{
	return FOUNDATION_NONE;
}

static void GetAcceptedCargo_Clear(TileIndex tile, AcceptedCargo ac)
{
	/* unused */
}

static void AnimateTile_Clear(TileIndex tile)
{
	/* unused */
}

void TileLoopClearHelper(TileIndex tile)
{
	byte self;
	byte neighbour;
	TileIndex dirty = INVALID_TILE;

	self = (IsTileType(tile, MP_CLEAR) && IsClearGround(tile, CLEAR_FIELDS));

	neighbour = (IsTileType(TILE_ADDXY(tile, 1, 0), MP_CLEAR) && IsClearGround(TILE_ADDXY(tile, 1, 0), CLEAR_FIELDS));
	if (GetFenceSW(tile) == 0) {
		if (self != neighbour) {
			SetFenceSW(tile, 3);
			dirty = tile;
		}
	} else {
		if (self == 0 && neighbour == 0) {
			SetFenceSW(tile, 0);
			dirty = tile;
		}
	}

	neighbour = (IsTileType(TILE_ADDXY(tile, 0, 1), MP_CLEAR) && IsClearGround(TILE_ADDXY(tile, 0, 1), CLEAR_FIELDS));
	if (GetFenceSE(tile) == 0) {
		if (self != neighbour) {
			SetFenceSE(tile, 3);
			dirty = tile;
		}
	} else {
		if (self == 0 && neighbour == 0) {
			SetFenceSE(tile, 0);
			dirty = tile;
		}
	}

	if (dirty != INVALID_TILE) MarkTileDirtyByTile(dirty);
}


/* convert into snowy tiles */
static void TileLoopClearAlps(TileIndex tile)
{
	int k = GetTileZ(tile) - GetSnowLine() + TILE_HEIGHT;

	if (k < 0) { // well below the snow line
		if (!IsClearGround(tile, CLEAR_SNOW)) return;
		if (GetClearDensity(tile) == 0) SetClearGroundDensity(tile, CLEAR_GRASS, 3);
	} else {
		if (!IsClearGround(tile, CLEAR_SNOW)) {
			SetClearGroundDensity(tile, CLEAR_SNOW, 0);
		} else {
			uint density = min((uint)k / TILE_HEIGHT, 3);

			if (GetClearDensity(tile) < density) {
				AddClearDensity(tile, 1);
			} else if (GetClearDensity(tile) > density) {
				AddClearDensity(tile, -1);
			} else {
				return;
			}
		}
	}

	MarkTileDirtyByTile(tile);
}

static void TileLoopClearDesert(TileIndex tile)
{
	if (IsClearGround(tile, CLEAR_DESERT)) return;

	if (GetTropicZone(tile) == TROPICZONE_DESERT) {
		SetClearGroundDensity(tile, CLEAR_DESERT, 3);
	} else {
		if (GetTropicZone(tile + TileDiffXY( 1,  0)) != TROPICZONE_DESERT &&
				GetTropicZone(tile + TileDiffXY(-1,  0)) != TROPICZONE_DESERT &&
				GetTropicZone(tile + TileDiffXY( 0,  1)) != TROPICZONE_DESERT &&
				GetTropicZone(tile + TileDiffXY( 0, -1)) != TROPICZONE_DESERT)
			return;
		SetClearGroundDensity(tile, CLEAR_DESERT, 1);
	}

	MarkTileDirtyByTile(tile);
}

static void TileLoop_Clear(TileIndex tile)
{
	TileLoopClearHelper(tile);

	switch (_opt.landscape) {
		case LT_TROPIC: TileLoopClearDesert(tile); break;
		case LT_ARCTIC: TileLoopClearAlps(tile);   break;
	}

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

		case CLEAR_FIELDS: {
			uint field_type;

			if (_game_mode == GM_EDITOR) return;

			if (GetClearCounter(tile) < 7) {
				AddClearCounter(tile, 1);
				return;
			} else {
				SetClearCounter(tile, 0);
			}

			if (GetIndustryIndexOfField(tile) == INVALID_INDUSTRY && GetFieldType(tile) >= 7) {
				/* This farmfield is no longer farmfield, so make it grass again */
				MakeClear(tile, CLEAR_GRASS, 2);
			} else {
				field_type = GetFieldType(tile);
				field_type = (field_type < 8) ? field_type + 1 : 0;
				SetFieldType(tile, field_type);
			}
			break;
		}

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
	i = ScaleByMapSize(GB(Random(), 0, 10) + 0x400);
	gi = ScaleByMapSize(GB(Random(), 0, 7) + 0x80);

	SetGeneratingWorldProgress(GWP_ROUGH_ROCKY, gi + i);
	do {
		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		tile = RandomTile();
		if (IsTileType(tile, MP_CLEAR) && !IsClearGround(tile, CLEAR_DESERT)) SetClearGroundDensity(tile, CLEAR_ROUGH, 3);
	} while (--i);

	/* add rocky tiles */
	i = gi;
	do {
		uint32 r = Random();
		tile = RandomTileSeed(r);

		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		if (IsTileType(tile, MP_CLEAR) && !IsClearGround(tile, CLEAR_DESERT)) {
			uint j = GB(r, 16, 4) + 5;
			for (;;) {
				TileIndex tile_new;

				SetClearGroundDensity(tile, CLEAR_ROCKS, 3);
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

static void ClickTile_Clear(TileIndex tile)
{
	/* not used */
}

static uint32 GetTileTrackStatus_Clear(TileIndex tile, TransportType mode, uint sub_mode)
{
	return 0;
}

static const StringID _clear_land_str[] = {
	STR_080D_GRASS,
	STR_080B_ROUGH_LAND,
	STR_080A_ROCKS,
	STR_080E_FIELDS,
	STR_080F_SNOW_COVERED_LAND,
	STR_0810_DESERT
};

static void GetTileDesc_Clear(TileIndex tile, TileDesc *td)
{
	if (IsClearGround(tile, CLEAR_GRASS) && GetClearDensity(tile) == 0) {
		td->str = STR_080C_BARE_LAND;
	} else {
		td->str = _clear_land_str[GetClearGround(tile)];
	}
	td->owner = GetTileOwner(tile);
}

static void ChangeTileOwner_Clear(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	return;
}

void InitializeClearLand()
{
	_opt.snow_line = _patches.snow_line_height * TILE_HEIGHT;
}

static CommandCost TerraformTile_Clear(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}

extern const TileTypeProcs _tile_type_clear_procs = {
	DrawTile_Clear,           ///< draw_tile_proc
	GetSlopeZ_Clear,          ///< get_slope_z_proc
	ClearTile_Clear,          ///< clear_tile_proc
	GetAcceptedCargo_Clear,   ///< get_accepted_cargo_proc
	GetTileDesc_Clear,        ///< get_tile_desc_proc
	GetTileTrackStatus_Clear, ///< get_tile_track_status_proc
	ClickTile_Clear,          ///< click_tile_proc
	AnimateTile_Clear,        ///< animate_tile_proc
	TileLoop_Clear,           ///< tile_loop_clear
	ChangeTileOwner_Clear,    ///< change_tile_owner_clear
	NULL,                     ///< get_produced_cargo_proc
	NULL,                     ///< vehicle_enter_tile_proc
	GetFoundation_Clear,      ///< get_foundation_proc
	TerraformTile_Clear,      ///< terraform_tile_proc
};
