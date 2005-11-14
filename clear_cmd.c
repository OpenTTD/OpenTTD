/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "player.h"
#include "tile.h"
#include "viewport.h"
#include "command.h"
#include "variables.h"
#include "table/sprites.h"

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

	if (TileX(tile) == MapMaxX() || TileY(tile) == MapMaxY())
		return -1;

	t = ts->tile_table;
	for(count = ts->tile_table_count; count != 0; count--,t++) {
		if (*t == tile)
			return 0;
	}

	return 1;
}

static int TerraformGetHeightOfTile(TerraformerState *ts, TileIndex tile)
{
	TerraformerHeightMod *mod = ts->modheight;
	int count;

	for(count = ts->modheight_count; count != 0; count--, mod++) {
		if (mod->tile == tile)
			return mod->height;
	}

	return TileHeight(tile);
}

static void TerraformAddDirtyTile(TerraformerState *ts, TileIndex tile)
{
	int count;
	TileIndex *t;

	count = ts->tile_table_count;

	if (count >= 625)
		return;

	for(t = ts->tile_table; count != 0; count--,t++) {
		if (*t == tile)
			return;
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

static int TerraformProc(TerraformerState *ts, TileIndex tile, int mode)
{
	int r;
	int32 ret;

	assert(tile < MapSize());

	if ((r=TerraformAllowTileProcess(ts, tile)) <= 0)
		return r;

	if (IsTileType(tile, MP_RAILWAY)) {
		static const byte _railway_modes[4] = {8, 0x10, 4, 0x20};
		static const byte _railway_dangslopes[4] = {0xd, 0xe, 7, 0xb};
		static const byte _railway_dangslopes2[4] = {0x2, 0x1, 0x8, 0x4};

		// Nothing could be built at the steep slope - this avoids a bug
		// when you have a single diagonal track in one corner on a
		// basement and then you raise/lower the other corner.
		int tileh = GetTileSlope(tile, NULL) & 0xF;
		if (tileh == _railway_dangslopes[mode] ||
				tileh == _railway_dangslopes2[mode]) {
			_terraform_err_tile = tile;
			_error_message = STR_1008_MUST_REMOVE_RAILROAD_TRACK;
			return -1;
		}

		// If we have a single diagonal track there, the other side of
		// tile can be terraformed.
		if ((_m[tile].m5&~0x40) == _railway_modes[mode])
			return 0;
	}

	ret = DoCommandByTile(tile, 0,0, ts->flags & ~DC_EXEC, CMD_LANDSCAPE_CLEAR);

	if (ret == CMD_ERROR) {
		_terraform_err_tile = tile;
		return -1;
	}

	ts->cost += ret;

	if (ts->tile_table_count >= 625)
		return -1;
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

	if (height > 0xF)
		return false;

	nh = TerraformGetHeightOfTile(ts, tile);
	if (nh < 0 || height == nh)
		return false;

	if (TerraformProc(ts, tile, 0) < 0) return false;
	if (TerraformProc(ts, tile + TileDiffXY( 0, -1), 1) < 0) return false;
	if (TerraformProc(ts, tile + TileDiffXY(-1, -1), 2) < 0) return false;
	if (TerraformProc(ts, tile + TileDiffXY(-1,  0), 3) < 0) return false;

	mod = ts->modheight;
	count = ts->modheight_count;

	for(;;) {
		if (count == 0) {
			if (ts->modheight_count >= 576)
				return false;
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

		for(ttm = _terraform_tilepos; ttm != endof(_terraform_tilepos); ttm++) {
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
 * @param x,y coordinates to terraform
 * @param p1 corners to terraform.
 * @param p2 direction; eg up or down
 */
int32 CmdTerraformLand(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TerraformerState ts;
	TileIndex tile;
	int direction;

	TerraformerHeightMod modheight_data[576];
	TileIndex tile_table_data[625];

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	_error_message = INVALID_STRING_ID;
	_terraform_err_tile = 0;

	ts.direction = direction = p2 ? 1 : -1;
	ts.flags = flags;
	ts.modheight_count = ts.tile_table_count = 0;
	ts.cost = 0;
	ts.modheight = modheight_data;
	ts.tile_table = tile_table_data;

	tile = TileVirtXY(x, y);

	/* Make an extra check for map-bounds cause we add tiles to the originating tile */
	if (tile + TileDiffXY(1, 1) > MapSize()) return CMD_ERROR;

	if (p1 & 1) {
		if (!TerraformTileHeight(&ts, tile + TileDiffXY(1, 0),
				TileHeight(tile + TileDiffXY(1, 0)) + direction))
					return CMD_ERROR;
	}

	if (p1 & 2) {
		if (!TerraformTileHeight(&ts, tile + TileDiffXY(1, 1),
				TileHeight(tile + TileDiffXY(1, 1)) + direction))
					return CMD_ERROR;
	}

	if (p1 & 4) {
		if (!TerraformTileHeight(&ts, tile + TileDiffXY(0, 1),
				TileHeight(tile + TileDiffXY(0, 1)) + direction))
					return CMD_ERROR;
	}

	if (p1 & 8) {
		if (!TerraformTileHeight(&ts, tile + TileDiffXY(0, 0),
				TileHeight(tile + TileDiffXY(0, 0)) + direction))
					return CMD_ERROR;
	}

	if (direction == -1) {
		/* Check if tunnel would take damage */
		int count;
		TileIndex *ti = ts.tile_table;

		for (count = ts.tile_table_count; count != 0; count--, ti++) {
			uint z, t;
			TileIndex tile = *ti;

			z = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(0, 0));
			t = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(1, 0));
			if (t <= z) z = t;
			t = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(1, 1));
			if (t <= z) z = t;
			t = TerraformGetHeightOfTile(&ts, tile + TileDiffXY(0, 1));
			if (t <= z) z = t;

			if (!CheckTunnelInWay(tile, z*8))
				return_cmd_error(STR_1002_EXCAVATION_WOULD_DAMAGE);
		}
	}

	if (flags & DC_EXEC) {
		/* Clear the landscape at the tiles */
		{
			int count;
			TileIndex *ti = ts.tile_table;
			for (count = ts.tile_table_count; count != 0; count--, ti++) {
				DoCommandByTile(*ti, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
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
 * @param x,y end tile of area-drag
 * @param p1 start tile of area drag
 * @param p2 unused
 */
int32 CmdLevelLand(int ex, int ey, uint32 flags, uint32 p1, uint32 p2)
{
	int size_x, size_y;
	int sx, sy;
	uint h, curh;
	TileIndex tile;
	int32 ret, cost, money;

	if (p1 > MapSize()) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// remember level height
	h = TileHeight(p1);

	ex >>= 4; ey >>= 4;

	// make sure sx,sy are smaller than ex,ey
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
			ret = DoCommandByTile(tile2, 8, (curh > h) ? 0 : 1, flags & ~DC_EXEC, CMD_TERRAFORM_LAND);
			if (CmdFailed(ret)) break;
			cost += ret;

			if (flags & DC_EXEC) {
				if ((money -= ret) < 0) {
					_additional_cash_required = ret;
					return cost - ret;
				}
				DoCommandByTile(tile2, 8, (curh > h) ? 0 : 1, flags, CMD_TERRAFORM_LAND);
			}

			curh += (curh > h) ? -1 : 1;
		}
	} END_TILE_LOOP(tile2, size_x, size_y, tile)

	return (cost == 0) ? CMD_ERROR : cost;
}

/** Purchase a land area. Actually you only purchase one tile, so
 * the name is a bit confusing ;p
 * @param x,y the tile the player is purchasing
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdPurchaseLandArea(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile;
	int32 cost;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TileVirtXY(x, y);

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (IsTileType(tile, MP_UNMOVABLE) && _m[tile].m5 == 3 &&
			IsTileOwner(tile, _current_player))
		return_cmd_error(STR_5807_YOU_ALREADY_OWN_IT);

	cost = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		ModifyTile(tile,
			MP_SETTYPE(MP_UNMOVABLE) | MP_MAPOWNER_CURRENT | MP_MAP5,
			3 /* map5 */
			);
	}

	return cost + _price.purchase_land * 10;
}


static int32 ClearTile_Clear(TileIndex tile, byte flags)
{
	static const int32 null = 0;
	static const int32* clear_price_table[] = {
		&null,
		&_price.clear_1,
		&_price.clear_1,
		&_price.clear_1,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.clear_2,
		&_price.clear_2,
		&_price.clear_2,
		&_price.clear_2,
		&_price.clear_3,
		&_price.clear_3,
		&_price.clear_3,
		&_price.clear_3,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.purchase_land,
		&_price.clear_2,
		&_price.clear_2,
		&_price.clear_2,
		&_price.clear_2,
	};
	const int32 *price = clear_price_table[GB(_m[tile].m5, 0, 5)];

	if (flags & DC_EXEC) DoClearSquare(tile);

	return *price;
}

/** Sell a land area. Actually you only sell one tile, so
 * the name is a bit confusing ;p
 * @param x,y the tile the player is selling
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdSellLandArea(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TileVirtXY(x, y);

	if (!IsTileType(tile, MP_UNMOVABLE) || _m[tile].m5 != 3) return CMD_ERROR;
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER) return CMD_ERROR;


	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (flags & DC_EXEC)
		DoClearSquare(tile);

	return - _price.purchase_land * 2;
}


#include "table/clear_land.h"


void DrawClearLandTile(const TileInfo *ti, byte set)
{
	DrawGroundSprite(SPR_FLAT_BARE_LAND + _tileh_to_sprite[ti->tileh] + set * 19);
}

void DrawHillyLandTile(const TileInfo *ti)
{
	if (ti->tileh != 0) {
		DrawGroundSprite(SPR_FLAT_ROUGH_LAND + _tileh_to_sprite[ti->tileh]);
	} else {
		DrawGroundSprite(_landscape_clear_sprites[GB(ti->x ^ ti->y, 4, 3)]);
	}
}

void DrawClearLandFence(const TileInfo *ti)
{
	byte m4 = _m[ti->tile].m4;
	byte z = ti->z;

	if (ti->tileh & 2) {
		z += 8;
		if (ti->tileh == 0x17)
			z += 8;
	}

	if (GB(m4, 5, 3) != 0) {
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[GB(m4, 5, 3) - 1] + _fence_mod_by_tileh[ti->tileh], ti->x, ti->y, z);
	}

	if (GB(m4, 2, 3) != 0) {
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[GB(m4, 2, 3) - 1] + _fence_mod_by_tileh_2[ti->tileh], ti->x, ti->y, z);
	}
}

static void DrawTile_Clear(TileInfo *ti)
{
	switch (GB(ti->map5, 2, 3)) {
	case 0:
		DrawClearLandTile(ti, GB(ti->map5, 0, 2));
		break;

	case 1:
		DrawHillyLandTile(ti);
		break;

	case 2:
		DrawGroundSprite(SPR_FLAT_ROCKY_LAND_1 + _tileh_to_sprite[ti->tileh]);
		break;

	case 3:
		DrawGroundSprite(_clear_land_sprites_1[GB(_m[ti->tile].m3, 0, 4)] + _tileh_to_sprite[ti->tileh]);
		break;

	case 4:
		DrawGroundSprite(_clear_land_sprites_2[GB(ti->map5, 0, 2)] + _tileh_to_sprite[ti->tileh]);
		break;

	case 5:
		DrawGroundSprite(_clear_land_sprites_3[GB(ti->map5, 0, 2)] + _tileh_to_sprite[ti->tileh]);
		break;
	}

	DrawClearLandFence(ti);
}

static uint GetSlopeZ_Clear(const TileInfo* ti)
{
	return GetPartialZ(ti->x & 0xF, ti->y & 0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Clear(const TileInfo *ti)
{
	return ti->tileh;
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

	switch (GetTileType(tile)) {
		case MP_CLEAR:
			self = (GB(_m[tile].m5, 0, 5) == 15);
			break;

		default:
			self = 0;
			break;
	}

	switch (GetTileType(TILE_ADDXY(tile, 1, 0))) {
		case MP_CLEAR:
			neighbour = (GB(_m[TILE_ADDXY(tile, 1, 0)].m5, 0, 5) == 15);
			break;

		default:
			neighbour = 0;
			break;
	}

	if (GB(_m[tile].m4, 5, 3) == 0) {
		if (self != neighbour) {
			SB(_m[tile].m4, 5, 3, 3);
			dirty = tile;
		}
	} else {
		if (self == 0 && neighbour == 0) {
			SB(_m[tile].m4, 5, 3, 0);
			dirty = tile;
		}
	}

	switch (GetTileType(TILE_ADDXY(tile, 0, 1))) {
		case MP_CLEAR:
			neighbour = (GB(_m[TILE_ADDXY(tile, 0, 1)].m5, 0, 5) == 15);
			break;

		default:
			neighbour = 0;
			break;
	}

	if (GB(_m[tile].m4, 2, 3) == 0) {
		if (self != neighbour) {
			SB(_m[tile].m4, 2, 3, 3);
			dirty = tile;
		}
	} else {
		if (self == 0 && neighbour == 0) {
			SB(_m[tile].m4, 2, 3, 0);
			dirty = tile;
		}
	}

	if (dirty != INVALID_TILE) MarkTileDirtyByTile(dirty);
}


/* convert into snowy tiles */
static void TileLoopClearAlps(TileIndex tile)
{
	int k;
	byte m5,tmp;

	/* distance from snow line, in steps of 8 */
	k = GetTileZ(tile) - _opt.snow_line;

	m5 = _m[tile].m5 & 0x1C;
	tmp = _m[tile].m5 & 3;

	if (k < -8) {
		/* snow_m2_down */
		if (m5 != 0x10)
			return;
		if (tmp == 0)
			m5 = 3;
	} else if (k == -8) {
		/* snow_m1 */
		if (m5 != 0x10) {
			m5 = 0x10;
		} else if (tmp != 0) {
			m5 = (tmp - 1) + 0x10;
		} else
			return;
	} else if (k < 8) {
		/* snow_0 */
		if (m5 != 0x10) {
			m5 = 0x10;
		} else if (tmp != 1) {
			m5 = 1;
			if (tmp != 0)
				m5 = tmp - 1;
			m5 += 0x10;
		} else
			return;
	} else if (k == 8) {
		/* snow_p1 */
		if (m5 != 0x10) {
			m5 = 0x10;
		} else if (tmp != 2) {
			m5 = 2;
			if (tmp <= 2)
				m5 = tmp + 1;
			m5 += 0x10;
		} else
			return;
	} else {
		/* snow_p2_up */
		if (m5 != 0x10) {
			m5 = 0x10;
		} else if (tmp != 3) {
			m5 = tmp + 1 + 0x10;
		} else
			return;
	}

	_m[tile].m5 = m5;
	MarkTileDirtyByTile(tile);
}

static void TileLoopClearDesert(TileIndex tile)
{
 	if ( (_m[tile].m5 & 0x1C) == 0x14)
		return;

	if (GetMapExtraBits(tile) == 1) {
		_m[tile].m5 = 0x17;
	} else {
		if (GetMapExtraBits(tile + TileDiffXY( 1,  0)) != 1 &&
				GetMapExtraBits(tile + TileDiffXY(-1,  0)) != 1 &&
				GetMapExtraBits(tile + TileDiffXY( 0,  1)) != 1 &&
				GetMapExtraBits(tile + TileDiffXY( 0, -1)) != 1)
			return;
		_m[tile].m5 = 0x15;
	}

	MarkTileDirtyByTile(tile);
}

static void TileLoop_Clear(TileIndex tile)
{
	byte m5,m3;

	TileLoopClearHelper(tile);

	if (_opt.landscape == LT_DESERT) {
		TileLoopClearDesert(tile);
	} else if (_opt.landscape == LT_HILLY) {
		TileLoopClearAlps(tile);
	}

	m5 = _m[tile].m5;
	if ( (m5 & 0x1C) == 0x10 || (m5 & 0x1C) == 0x14)
		return;

	if ( (m5 & 0x1C) != 0xC) {
		if ( (m5 & 3) == 3)
			return;

		if (_game_mode != GM_EDITOR) {
			m5 += 0x20;
			if (m5 >= 0x20) {
				// Didn't overflow
				_m[tile].m5 = m5;
				return;
			}
			/* did overflow, so continue */
		} else {
			m5 = (GB(Random(), 0, 8) > 21) ? 2 : 6;
		}
		m5++;
	} else if (_game_mode != GM_EDITOR) {
		/* handle farm field */
		m5 += 0x20;
		if (m5 >= 0x20) {
			// Didn't overflow
			_m[tile].m5 = m5;
			return;
		}
		/* overflowed */
		m3 = _m[tile].m3 + 1;
		assert( (m3 & 0xF) != 0);
		if ( (m3 & 0xF) >= 9) /* NOTE: will not work properly if m3&0xF == 0xF */
			m3 &= ~0xF;
		_m[tile].m3 = m3;
	}

	_m[tile].m5 = m5;
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
		if (IsTileType(tile, MP_CLEAR)) SB(_m[tile].m5, 2, 2, 1);
	} while (--i);

	/* add grey squares */
	i = ScaleByMapSize(GB(Random(), 0, 7) + 0x80);
	do {
		uint32 r = Random();
		tile = RandomTileSeed(r);
		if (IsTileType(tile, MP_CLEAR)) {
			uint j = GB(r, 16, 4) + 5;
			for(;;) {
				TileIndex tile_new;

				SB(_m[tile].m5, 2, 2, 2);
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
	STR_080B_ROUGH_LAND,
	STR_080A_ROCKS,
	STR_080E_FIELDS,
	STR_080F_SNOW_COVERED_LAND,
	STR_0810_DESERT,
	0,
	0,
	STR_080C_BARE_LAND,
	STR_080D_GRASS,
	STR_080D_GRASS,
	STR_080D_GRASS,
};

static void GetTileDesc_Clear(TileIndex tile, TileDesc *td)
{
	uint i = GB(_m[tile].m5, 2, 3);
	if (i == 0) i = GB(_m[tile].m5, 0, 2) + 8;
	td->str = _clear_land_str[i - 1];
	td->owner = GetTileOwner(tile);
}

static void ChangeTileOwner_Clear(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	return;
}

void InitializeClearLand(void)
{
	_opt.snow_line = _patches.snow_line_height * 8;
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
	NULL,											/* vehicle_leave_tile_proc */
	GetSlopeTileh_Clear,			/* get_slope_tileh_proc */
};
