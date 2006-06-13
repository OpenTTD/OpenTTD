/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "clear_map.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "player.h"
#include "tile.h"
#include "viewport.h"
#include "command.h"
#include "tunnel_map.h"
#include "variables.h"
#include "table/sprites.h"
#include "unmovable_map.h"

typedef struct TerraformerHeightMod {
	TileIndex tile;
	byte height;
} TerraformerHeightMod;

typedef struct TerraformerState {
	int height[4];
	uint32 flags;

	int direction;
	int modheight_count;
	int tile_table_count;

	int32 cost;

	TileIndex *tile_table;
	TerraformerHeightMod *modheight;

} TerraformerState;

static int TerraformAllowTileProcess(TerraformerState *ts, TileIndex tile)
{
	TileIndex *t;
	int count;

	if (TileX(tile) == MapMaxX() || TileY(tile) == MapMaxY()) return -1;

	t = ts->tile_table;
	for (count = ts->tile_table_count; count != 0; count--, t++) {
		if (*t == tile) return 0;
	}

	return 1;
}

static int TerraformGetHeightOfTile(TerraformerState *ts, TileIndex tile)
{
	TerraformerHeightMod *mod = ts->modheight;
	int count;

	for (count = ts->modheight_count; count != 0; count--, mod++) {
		if (mod->tile == tile) return mod->height;
	}

	return TileHeight(tile);
}

static void TerraformAddDirtyTile(TerraformerState *ts, TileIndex tile)
{
	int count;
	TileIndex *t;

	count = ts->tile_table_count;

	if (count >= 625) return;

	for (t = ts->tile_table; count != 0; count--,t++) {
		if (*t == tile) return;
	}

	ts->tile_table[ts->tile_table_count++] = tile;
}

static void TerraformAddDirtyTileAround(TerraformerState *ts, TileIndex tile)
{
	TerraformAddDirtyTile(ts, tile + TileDiffXY( 0, -1));
	TerraformAddDirtyTile(ts, tile + TileDiffXY(-1, -1));
	TerraformAddDirtyTile(ts, tile + TileDiffXY(-1,  0));
	TerraformAddDirtyTile(ts, tile);
}

static int TerraformProc(TerraformerState *ts, TileIndex tile)
{
	int r;

	assert(tile < MapSize());

	if ((r = TerraformAllowTileProcess(ts, tile)) <= 0) return r;

	if (!IsTileType(tile, MP_RAILWAY)) {
		int32 ret = DoCommand(tile, 0,0, ts->flags & ~DC_EXEC, CMD_LANDSCAPE_CLEAR);

		if (CmdFailed(ret)) {
			_terraform_err_tile = tile;
			return -1;
		}

		ts->cost += ret;
	}

	if (ts->tile_table_count >= 625) return -1;
	ts->tile_table[ts->tile_table_count++] = tile;

	return 0;
}

static bool TerraformTileHeight(TerraformerState *ts, TileIndex tile, int height)
{
	int nh;
	TerraformerHeightMod *mod;
	int count;

	assert(tile < MapSize());

	if (height < 0) {
		_error_message = STR_1003_ALREADY_AT_SEA_LEVEL;
		return false;
	}

	_error_message = STR_1004_TOO_HIGH;

	if (height > 15) return false;

	nh = TerraformGetHeightOfTile(ts, tile);
	if (nh < 0 || height == nh) return false;

	if (TerraformProc(ts, tile) < 0) return false;
	if (TerraformProc(ts, tile + TileDiffXY( 0, -1)) < 0) return false;
	if (TerraformProc(ts, tile + TileDiffXY(-1, -1)) < 0) return false;
	if (TerraformProc(ts, tile + TileDiffXY(-1,  0)) < 0) return false;

	mod = ts->modheight;
	count = ts->modheight_count;

	for (;;) {
		if (count == 0) {
			if (ts->modheight_count >= 576) return false;
			ts->modheight_count++;
			break;
		}
		if (mod->tile == tile) break;
		mod++;
		count--;
	}

	mod->tile = tile;
	mod->height = (byte)height;

	ts->cost += _price.terraform;

	{
		int direction = ts->direction, r;
		const TileIndexDiffC *ttm;

		static const TileIndexDiffC _terraform_tilepos[] = {
			{ 1,  0},
			{-2,  0},
			{ 1,  1},
			{ 0, -2}
		};

		for (ttm = _terraform_tilepos; ttm != endof(_terraform_tilepos); ttm++) {
			tile += ToTileIndexDiff(*ttm);

			r = TerraformGetHeightOfTile(ts, tile);
			if (r != height && r-direction != height && r+direction != height) {
				if (!TerraformTileHeight(ts, tile, r+direction))
					return false;
			}
		}
	}

	return true;
}

/** Terraform land
 * @param tile tile to terraform
 * @param p1 corners to terraform.
 * @param p2 direction; eg up or down
 */
int32 CmdTerraformLand(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	TerraformerState ts;
	TileIndex t;
	int direction;

	TerraformerHeightMod modheight_data[576];
	TileIndex tile_table_data[625];

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	_terraform_err_tile = 0;

	ts.direction = direction = p2 ? 1 : -1;
	ts.flags = flags;
	ts.modheight_count = ts.tile_table_count = 0;
	ts.cost = 0;
	ts.modheight = modheight_data;
	ts.tile_table = tile_table_data;

	/* Make an extra check for map-bounds cause we add tiles to the originating tile */
	if (tile + TileDiffXY(1, 1) >= MapSize()) return CMD_ERROR;

	if (p1 & 1) {
		t = tile + TileDiffXY(1, 0);
		if (!TerraformTileHeight(&ts, t, TileHeight(t) + direction)) {
			return CMD_ERROR;
		}
	}

	if (p1 & 2) {
		t = tile + TileDiffXY(1, 1);
		if (!TerraformTileHeight(&ts, t, TileHeight(t) + direction)) {
			return CMD_ERROR;
		}
	}

	if (p1 & 4) {
		t = tile + TileDiffXY(0, 1);
		if (!TerraformTileHeight(&ts, t, TileHeight(t) + direction)) {
			return CMD_ERROR;
		}
	}

	if (p1 & 8) {
		t = tile + TileDiffXY(0, 0);
		if (!TerraformTileHeight(&ts, t, TileHeight(t) + direction)) {
			return CMD_ERROR;
		}
	}

	{ /* Check if tunnel or track would take damage */
		int count;
		TileIndex *ti = ts.tile_table;

		for (count = ts.tile_table_count; count != 0; count--, ti++) {
			uint a, b, c, d, r, min;
			TileIndex tile = *ti;

			_terraform_err_tile = tile;

			a = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(0, 0));
			b = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(1, 0));
			c = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(0, 1));
			d = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(1, 1));

			r = GetTileh(a, b, c, d, &min);

			if (IsTileType(tile, MP_RAILWAY)) {
				if (IsSteepSlope(r)) return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);

				if (IsPlainRailTile(tile)) {
					extern const TrackBits _valid_tileh_slopes[2][15];
					if (GetTrackBits(tile) & ~_valid_tileh_slopes[0][r]) return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);
				} else return_cmd_error(STR_5800_OBJECT_IN_THE_WAY);
			}

			if (direction == -1 && IsTunnelInWay(tile, min)) return_cmd_error(STR_1002_EXCAVATION_WOULD_DAMAGE);

			_terraform_err_tile = 0;
		}
	}

	if (flags & DC_EXEC) {
		/* Clear the landscape at the tiles */
		{
			int count;
			TileIndex *ti = ts.tile_table;
			for (count = ts.tile_table_count; count != 0; count--, ti++) {
				DoCommand(*ti, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
		}

		/* change the height */
		{
			int count;
			TerraformerHeightMod *mod;

			mod = ts.modheight;
			for (count = ts.modheight_count; count != 0; count--, mod++) {
				TileIndex til = mod->tile;

				SetTileHeight(til, mod->height);
				TerraformAddDirtyTileAround(&ts, til);
			}
		}

		/* finally mark the dirty tiles dirty */
		{
			int count;
			TileIndex *ti = ts.tile_table;
			for (count = ts.tile_table_count; count != 0; count--, ti++) {
				MarkTileDirtyByTile(*ti);
			}
		}
	}
	return ts.cost;
}


/** Levels a selected (rectangle) area of land
 * @param tile end tile of area-drag
 * @param p1 start tile of area drag
 * @param p2 unused
 */
int32 CmdLevelLand(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int size_x, size_y;
	int ex;
	int ey;
	int sx, sy;
	uint h, curh;
	int32 ret, cost, money;

	if (p1 >= MapSize()) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// remember level height
	h = TileHeight(p1);

	// make sure sx,sy are smaller than ex,ey
	ex = TileX(tile);
	ey = TileY(tile);
	sx = TileX(p1);
	sy = TileY(p1);
	if (ex < sx) intswap(ex, sx);
	if (ey < sy) intswap(ey, sy);
	tile = TileXY(sx, sy);

	size_x = ex-sx+1;
	size_y = ey-sy+1;

	money = GetAvailableMoneyForCommand();
	cost = 0;

	BEGIN_TILE_LOOP(tile2, size_x, size_y, tile) {
		curh = TileHeight(tile2);
		while (curh != h) {
			ret = DoCommand(tile2, 8, (curh > h) ? 0 : 1, flags & ~DC_EXEC, CMD_TERRAFORM_LAND);
			if (CmdFailed(ret)) break;
			cost += ret;

			if (flags & DC_EXEC) {
				if ((money -= ret) < 0) {
					_additional_cash_required = ret;
					return cost - ret;
				}
				DoCommand(tile2, 8, (curh > h) ? 0 : 1, flags, CMD_TERRAFORM_LAND);
			}

			curh += (curh > h) ? -1 : 1;
		}
	} END_TILE_LOOP(tile2, size_x, size_y, tile)

	return (cost == 0) ? CMD_ERROR : cost;
}

/** Purchase a land area. Actually you only purchase one tile, so
 * the name is a bit confusing ;p
 * @param tile the tile the player is purchasing
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdPurchaseLandArea(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (IsOwnedLandTile(tile) && IsTileOwner(tile, _current_player)) {
		return_cmd_error(STR_5807_YOU_ALREADY_OWN_IT);
	}

	cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		MakeOwnedLand(tile, _current_player);
		MarkTileDirtyByTile(tile);
	}

	return cost + _price.purchase_land * 10;
}


static int32 ClearTile_Clear(TileIndex tile, byte flags)
{
	static const int32* clear_price_table[] = {
		&_price.clear_1,
		&_price.purchase_land,
		&_price.clear_2,
		&_price.clear_3,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.clear_2, // XXX unused?
	};
	int32 price;

	if (IsClearGround(tile, CLEAR_GRASS) && GetClearDensity(tile) == 0) {
		price = 0;
	} else {
		price = *clear_price_table[GetClearGround(tile)];
	}

	if (flags & DC_EXEC) DoClearSquare(tile);

	return price;
}

/** Sell a land area. Actually you only sell one tile, so
 * the name is a bit confusing ;p
 * @param tile the tile the player is selling
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdSellLandArea(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!IsOwnedLandTile(tile)) return CMD_ERROR;
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER) return CMD_ERROR;


	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) DoClearSquare(tile);

	return - _price.purchase_land * 2;
}


#include "table/clear_land.h"


void DrawClearLandTile(const TileInfo *ti, byte set)
{
	DrawGroundSprite(SPR_FLAT_BARE_LAND + _tileh_to_sprite[ti->tileh] + set * 19);
}

void DrawHillyLandTile(const TileInfo *ti)
{
	if (ti->tileh != SLOPE_FLAT) {
		DrawGroundSprite(SPR_FLAT_ROUGH_LAND + _tileh_to_sprite[ti->tileh]);
	} else {
		DrawGroundSprite(_landscape_clear_sprites[GB(ti->x ^ ti->y, 4, 3)]);
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
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[GetFenceSW(ti->tile) - 1] + _fence_mod_by_tileh[ti->tileh], ti->x, ti->y, z);
	}

	if (GetFenceSE(ti->tile) != 0) {
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[GetFenceSE(ti->tile) - 1] + _fence_mod_by_tileh_2[ti->tileh], ti->x, ti->y, z);
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
			DrawGroundSprite(SPR_FLAT_ROCKY_LAND_1 + _tileh_to_sprite[ti->tileh]);
			break;

		case CLEAR_FIELDS:
			DrawGroundSprite(_clear_land_sprites_1[GetFieldType(ti->tile)] + _tileh_to_sprite[ti->tileh]);
			break;

		case CLEAR_SNOW:
			DrawGroundSprite(_clear_land_sprites_2[GetClearDensity(ti->tile)] + _tileh_to_sprite[ti->tileh]);
			break;

		case CLEAR_DESERT:
			DrawGroundSprite(_clear_land_sprites_3[GetClearDensity(ti->tile)] + _tileh_to_sprite[ti->tileh]);
			break;
	}

	DrawClearLandFence(ti);
}

static uint GetSlopeZ_Clear(const TileInfo* ti)
{
	return GetPartialZ(ti->x & 0xF, ti->y & 0xF, ti->tileh) + ti->z;
}

static Slope GetSlopeTileh_Clear(TileIndex tile, Slope tileh)
{
	return tileh;
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
	/* distance from snow line, in steps of 8 */
	int k = GetTileZ(tile) - _opt.snow_line;

	if (k < -TILE_HEIGHT) { // well below the snow line
		if (!IsClearGround(tile, CLEAR_SNOW)) return;
		if (GetClearDensity(tile) == 0) SetClearGroundDensity(tile, CLEAR_GRASS, 3);
	} else {
		if (!IsClearGround(tile, CLEAR_SNOW)) {
			SetClearGroundDensity(tile, CLEAR_SNOW, 0);
		} else {
			uint density = min((uint)(k + TILE_HEIGHT) / TILE_HEIGHT, 3);

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
		case LT_DESERT: TileLoopClearDesert(tile); break;
		case LT_HILLY:  TileLoopClearAlps(tile);   break;
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

			field_type = GetFieldType(tile);
			field_type = (field_type < 8) ? field_type + 1 : 0;
			SetFieldType(tile, field_type);
			break;
		}

		default:
			return;
	}

	MarkTileDirtyByTile(tile);
}

void GenerateClearTile(void)
{
	uint i;
	TileIndex tile;

	/* add hills */
	i = ScaleByMapSize(GB(Random(), 0, 10) + 0x400);
	do {
		tile = RandomTile();
		if (IsTileType(tile, MP_CLEAR)) SetClearGroundDensity(tile, CLEAR_ROUGH, 3);
	} while (--i);

	/* add grey squares */
	i = ScaleByMapSize(GB(Random(), 0, 7) + 0x80);
	do {
		uint32 r = Random();
		tile = RandomTileSeed(r);
		if (IsTileType(tile, MP_CLEAR)) {
			uint j = GB(r, 16, 4) + 5;
			for (;;) {
				TileIndex tile_new;

				SetClearGroundDensity(tile, CLEAR_ROCKS, 3);
				do {
					if (--j == 0) goto get_out;
					tile_new = tile + TileOffsByDir(GB(Random(), 0, 2));
				} while (!IsTileType(tile_new, MP_CLEAR));
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

static uint32 GetTileTrackStatus_Clear(TileIndex tile, TransportType mode)
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

void InitializeClearLand(void)
{
	_opt.snow_line = _patches.snow_line_height * TILE_HEIGHT;
}

const TileTypeProcs _tile_type_clear_procs = {
	DrawTile_Clear,						/* draw_tile_proc */
	GetSlopeZ_Clear,					/* get_slope_z_proc */
	ClearTile_Clear,					/* clear_tile_proc */
	GetAcceptedCargo_Clear,		/* get_accepted_cargo_proc */
	GetTileDesc_Clear,				/* get_tile_desc_proc */
	GetTileTrackStatus_Clear,	/* get_tile_track_status_proc */
	ClickTile_Clear,					/* click_tile_proc */
	AnimateTile_Clear,				/* animate_tile_proc */
	TileLoop_Clear,						/* tile_loop_clear */
	ChangeTileOwner_Clear,		/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	NULL,											/* vehicle_enter_tile_proc */
	GetSlopeTileh_Clear,			/* get_slope_tileh_proc */
};
