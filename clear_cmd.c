#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "viewport.h"
#include "command.h"

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
	TerraformAddDirtyTile(ts, tile+TILE_XY(0,-1));
	TerraformAddDirtyTile(ts, tile+TILE_XY(-1,-1));
	TerraformAddDirtyTile(ts, tile+TILE_XY(-1,0));
	TerraformAddDirtyTile(ts, tile);
}

static int TerraformProc(TerraformerState *ts, uint tile, int mode)
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
		if ((_map5[tile]&~0x40) == _railway_modes[mode])
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

static bool TerraformTileHeight(TerraformerState *ts, uint tile, int height)
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

	if (TerraformProc(ts, tile, 0)<0)
		return false;

	if (TerraformProc(ts, tile + TILE_XY(0,-1), 1)<0)
		return false;

	if (TerraformProc(ts, tile + TILE_XY(-1,-1), 2)<0)
		return false;

	if (TerraformProc(ts, tile + TILE_XY(-1,0), 3)<0)
		return false;

	mod = ts->modheight;
	count = ts->modheight_count;

	for(;;) {
		if (count == 0) {
			if (ts->modheight_count >= 576)
				return false;
			ts->modheight_count++;
			break;
		}
		if (mod->tile == (TileIndex)tile)
			break;
		mod++;
		count--;
	}

	mod->tile = (TileIndex)tile;
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

/* Terraform land
 * p1 - corners
 * p2 - direction
 */

int32 CmdTerraformLand(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TerraformerState ts;
	uint tile;
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

	tile = TILE_FROM_XY(x,y);

	if (p1 & 1) {
		if (!TerraformTileHeight(&ts, tile+TILE_XY(1,0),
				TileHeight(tile + TILE_XY(1, 0)) + direction))
					return CMD_ERROR;
	}

	if (p1 & 2) {
		if (!TerraformTileHeight(&ts, tile+TILE_XY(1,1),
				TileHeight(tile + TILE_XY(1, 1)) + direction))
					return CMD_ERROR;
	}

	if (p1 & 4) {
		if (!TerraformTileHeight(&ts, tile+TILE_XY(0,1),
				TileHeight(tile + TILE_XY(0, 1)) + direction))
					return CMD_ERROR;
	}

	if (p1 & 8) {
		if (!TerraformTileHeight(&ts, tile+TILE_XY(0,0),
				TileHeight(tile + TILE_XY(0, 0)) + direction))
					return CMD_ERROR;
	}

	if (direction == -1) {
		/* Check if tunnel would take damage */
		int count;
		TileIndex *ti = ts.tile_table;

		for(count = ts.tile_table_count; count != 0; count--, ti++) {
			uint z, t;
			uint tile = *ti;

			z = TerraformGetHeightOfTile(&ts, tile + TILE_XY(0,0));
			t = TerraformGetHeightOfTile(&ts, tile + TILE_XY(1,0));
			if (t <= z) z = t;
			t = TerraformGetHeightOfTile(&ts, tile + TILE_XY(1,1));
			if (t <= z) z = t;
			t = TerraformGetHeightOfTile(&ts, tile + TILE_XY(0,1));
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
			for(count = ts.tile_table_count; count != 0; count--, ti++) {
				DoCommandByTile(*ti, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
		}

		/* change the height */
		{
			int count;
			TerraformerHeightMod *mod;
			uint til;

			mod = ts.modheight;
			for(count = ts.modheight_count; count != 0; count--, mod++) {
				til = mod->tile;

				SetTileHeight(til, mod->height);
				TerraformAddDirtyTileAround(&ts, til);
			}
		}

		/* finally mark the dirty tiles dirty */
		{
			int count;
			TileIndex *ti = ts.tile_table;
			for(count = ts.tile_table_count; count != 0; count--, ti++) {
				MarkTileDirtyByTile(*ti);
			}
		}
	}
	return ts.cost;
}


/*
 * p1 - start
 */

int32 CmdLevelLand(int ex, int ey, uint32 flags, uint32 p1, uint32 p2)
{
	int size_x, size_y;
	int sx, sy;
	uint h, curh;
	uint tile;
	int32 ret, cost, money;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// remember level height
	h = TileHeight(p1);

	ex >>= 4; ey >>= 4;

	// make sure sx,sy are smaller than ex,ey
	sx = TileX(p1);
	sy = TileY(p1);
	if (ex < sx) intswap(ex, sx);
	if (ey < sy) intswap(ey, sy);
	tile = TILE_XY(sx,sy);

	size_x = ex-sx+1;
	size_y = ey-sy+1;

	money = GetAvailableMoneyForCommand();
	cost = 0;

	BEGIN_TILE_LOOP(tile2, size_x, size_y, tile)
		curh = TileHeight(tile2);
		while (curh != h) {
			ret = DoCommandByTile(tile2, 8, (curh > h)?0:1, flags & ~DC_EXEC, CMD_TERRAFORM_LAND);
			if (ret == CMD_ERROR) break;
			cost += ret;

			if (flags & DC_EXEC) {
				if ((money -= ret) < 0) {
					_additional_cash_required = ret;
					return cost - ret;
				}
				DoCommandByTile(tile2, 8, (curh > h)?0:1, flags, CMD_TERRAFORM_LAND);
			}

			curh += (curh > h) ? -1 : 1;
		}
	END_TILE_LOOP(tile2, size_x, size_y, tile)

	if (cost == 0) return CMD_ERROR;
	return cost;
}

/* Purchase a land area
 * p1 = unused
 * p2 = unused
 */

int32 CmdPurchaseLandArea(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile;
	int32 cost;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TILE_FROM_XY(x,y);

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (IsTileType(tile, MP_UNMOVABLE) &&
			_map5[tile] == 3 &&
			_map_owner[tile] == _current_player)
		return_cmd_error(STR_5807_YOU_ALREADY_OWN_IT);

	cost = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (cost == CMD_ERROR)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		ModifyTile(tile,
			MP_SETTYPE(MP_UNMOVABLE) | MP_MAPOWNER_CURRENT | MP_MAP5,
			3 /* map5 */
			);
	}

	return cost + _price.purchase_land * 10;
}


int32 ClearTile_Clear(uint tile, byte flags) {
	static const int32 * _clear_price_table[] = {
			NULL,
			&_price.clear_1, &_price.clear_1,&_price.clear_1,
			&_price.purchase_land,&_price.purchase_land,&_price.purchase_land,&_price.purchase_land,
			&_price.clear_2,&_price.clear_2,&_price.clear_2,&_price.clear_2,
			&_price.clear_3,&_price.clear_3,&_price.clear_3,&_price.clear_3,
			&_price.purchase_land,&_price.purchase_land,&_price.purchase_land,&_price.purchase_land,
			&_price.purchase_land,&_price.purchase_land,&_price.purchase_land,&_price.purchase_land,
			&_price.clear_2,&_price.clear_2,&_price.clear_2,&_price.clear_2,
	};
	const int32 *price = _clear_price_table[_map5[tile] & 0x1F];

	if (flags & DC_EXEC)
		DoClearSquare(tile);

	if (price == NULL)
		return 0;
	return *price;
}

int32 CmdSellLandArea(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TILE_FROM_XY(x,y);

	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC)
		DoClearSquare(tile);

	return - _price.purchase_land*2;
}


#include "table/clear_land.h"


void DrawClearLandTile(TileInfo *ti, byte set)
{
	DrawGroundSprite(0xF54 + _tileh_to_sprite[ti->tileh] + set * 19);
}

void DrawHillyLandTile(TileInfo *ti)
{
	if (ti->tileh != 0) {
		DrawGroundSprite(0xFA0 + _tileh_to_sprite[ti->tileh]);
	} else {
		DrawGroundSprite(_landscape_clear_sprites[((ti->x^ti->y) >> 4) & 0x7]);
	}
}

void DrawClearLandFence(TileInfo *ti, byte img)
{
	byte z = ti->z;

	if (ti->tileh & 2) {
		z += 8;
		if (ti->tileh == 0x17)
			z += 8;
	}

	if (img & 0x38) {
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[((img >> 3) & 7) - 1] + _fence_mod_by_tileh[ti->tileh], ti->x, ti->y, z);
	}

	if (img & 0x7) {
		DrawGroundSpriteAt(_clear_land_fence_sprites_1[(img & 7) - 1] + _fence_mod_by_tileh_2[ti->tileh], ti->x, ti->y, z);
	}
}

static void DrawTile_Clear(TileInfo *ti)
{

	switch((ti->map5 & (7<<2)) >> 2) {
	case 0:
		DrawClearLandTile(ti, (ti->map5 & 3));
		break;

	case 1:
		DrawHillyLandTile(ti);
		break;

	case 2:
		DrawGroundSprite(0xFB7 + _tileh_to_sprite[ti->tileh]);
		break;

	case 3:
		DrawGroundSprite( _clear_land_sprites_1[_map3_lo[ti->tile]&0xF] + _tileh_to_sprite[ti->tileh]);
		break;

	case 4:
		DrawGroundSprite( _clear_land_sprites_2[ti->map5&3] + _tileh_to_sprite[ti->tileh]);
		break;

	case 5:
		DrawGroundSprite( _clear_land_sprites_3[ti->map5&3] + _tileh_to_sprite[ti->tileh]);
		break;
	}

	DrawClearLandFence(ti, _map3_hi[ti->tile] >> 2);
}

uint GetSlopeZ_Clear(TileInfo *ti) { return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z; }

uint GetSlopeTileh_Clear(TileInfo *ti)
{
	return ti->tileh;
}

static void GetAcceptedCargo_Clear(uint tile, AcceptedCargo ac)
{
	/* unused */
}

static void AnimateTile_Clear(uint tile)
{
	/* unused */
}

void TileLoopClearHelper(uint tile)
{
	byte img_1, img_2;
	static byte img_by_map5[8] = { 0,0,0,2, 1,1,0,0, };
	uint dirty = -1;

	img_1 = 0;
	if (IsTileType(tile, MP_CLEAR)) {
		img_1 = img_by_map5[(_map5[tile] & 0x1C) >> 2];
	} else if (IsTileType(tile, MP_TREES) && (_map2[tile] & 0x30) == 0x20) {
		img_1 = 1;
	}

	img_2 = 0;
	if (IsTileType(TILE_ADDXY(tile, 1, 0), MP_CLEAR)) {
		img_2 = img_by_map5[(_map5[TILE_ADDXY(tile, 1, 0)] & 0x1C) >> 2];
	} else if (IsTileType(TILE_ADDXY(tile, 1, 0), MP_TREES) && (_map2[TILE_ADDXY(tile, 1, 0)] & 0x30) == 0x20) {
		img_2 = 1;
	}

	if (!(_map3_hi[tile] & 0xE0)) {
		if ( (img_1&2) != (img_2&2) ) {
			_map3_hi[tile] |= 3 << 5;
			dirty = tile;
		}
	} else {
		if (img_1 == 1 && img_2 == 1) {
			_map3_hi[tile] &= ~(3 << 5);
			dirty = tile;
		}
	}

	img_2 = 0;
	if (IsTileType(TILE_ADDXY(tile, 0, 1), MP_CLEAR)) {
		img_2 = img_by_map5[(_map5[TILE_ADDXY(tile, 0, 1)] & 0x1C) >> 2];
	} else if (IsTileType(TILE_ADDXY(tile, 0, 1), MP_TREES) && (_map2[TILE_ADDXY(tile, 0, 1)] & 0x30) == 0x20) {
		img_2 = 1;
	}

	if (!(_map3_hi[tile] & 0x1C)) {
		if ( (img_1&2) != (img_2&2) ) {
			_map3_hi[tile] |= 3 << 2;
			dirty = tile;
		}
	} else {
		if (img_1 == 1 && img_2 == 1) {
			_map3_hi[tile] &= ~(3 << 2);
			dirty = tile;
		}
	}

	if (dirty != -1)
		MarkTileDirtyByTile(dirty);
}


/* convert into snowy tiles */
static void TileLoopClearAlps(uint tile)
{
	int k;
	byte m5,tmp;

	/* distance from snow line, in steps of 8 */
	k = GetTileZ(tile) - _opt.snow_line;

	m5 = _map5[tile] & 0x1C;
	tmp = _map5[tile] & 3;

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

	_map5[tile] = m5;
	MarkTileDirtyByTile(tile);
}

static void TileLoopClearDesert(uint tile)
{
 	if ( (_map5[tile] & 0x1C) == 0x14)
		return;

	if (GetMapExtraBits(tile) == 1) {
		_map5[tile] = 0x17;
	} else {
		if (GetMapExtraBits(tile+TILE_XY(1,0)) != 1 &&
				GetMapExtraBits(tile+TILE_XY(-1,0)) != 1 &&
				GetMapExtraBits(tile+TILE_XY(0,1)) != 1 &&
				GetMapExtraBits(tile+TILE_XY(0,-1)) != 1)
					return;
		_map5[tile] = 0x15;
	}

	MarkTileDirtyByTile(tile);
}

static void TileLoop_Clear(uint tile)
{
	byte m5,m3;

	TileLoopClearHelper(tile);

	if (_opt.landscape == LT_DESERT) {
		TileLoopClearDesert(tile);
	} else if (_opt.landscape == LT_HILLY) {
		TileLoopClearAlps(tile);
	}

	m5 = _map5[tile];
	if ( (m5 & 0x1C) == 0x10 || (m5 & 0x1C) == 0x14)
		return;

	if ( (m5 & 0x1C) != 0xC) {
		if ( (m5 & 3) == 3)
			return;

		if (_game_mode != GM_EDITOR) {
			m5 += 0x20;
			if (m5 >= 0x20) {
				// Didn't overflow
				_map5[tile] = m5;
				return;
			}
			/* did overflow, so continue */
		} else {
			m5 = ((byte)Random() > 21) ? (2) : (6);
		}
		m5++;
	} else if (_game_mode != GM_EDITOR) {
		/* handle farm field */
		m5 += 0x20;
		if (m5 >= 0x20) {
			// Didn't overflow
			_map5[tile] = m5;
			return;
		}
		/* overflowed */
		m3 = _map3_lo[tile] + 1;
		assert( (m3 & 0xF) != 0);
		if ( (m3 & 0xF) >= 9) /* NOTE: will not work properly if m3&0xF == 0xF */
			m3 &= ~0xF;
		_map3_lo[tile] = m3;
	}

	_map5[tile] = m5;
	MarkTileDirtyByTile(tile);
}

void GenerateClearTile()
{
	int i,j;
	uint tile,tile_new;
	uint32 r;

	/* add hills */
	i = (Random() & 0x3FF) | 0x400;
	do {
		tile = TILE_MASK(Random());
		if (IsTileType(tile, MP_CLEAR))
			_map5[tile] = (byte)((_map5[tile] & ~(3<<2)) | (1<<2));
	} while (--i);

	/* add grey squares */
	i = (Random() & 0x7F) | 0x80;
	do {
		r = Random();
		tile = TILE_MASK(r);
		if (IsTileType(tile, MP_CLEAR)) {
			j = ((r >> 16) & 0xF) + 5;
			for(;;) {
				_map5[tile] = (byte)((_map5[tile] & ~(3<<2)) | (2<<2));
				do {
					if (--j == 0) goto get_out;
					tile_new = tile + TileOffsByDir(Random() & 3);
				} while (!IsTileType(tile_new, MP_CLEAR));
				tile = tile_new;
			}
get_out:;
		}
	} while (--i);
}

static void ClickTile_Clear(uint tile)
{
	/* not used */
}

static uint32 GetTileTrackStatus_Clear(uint tile, TransportType mode)
{
	return 0;
}

static const StringID _clear_land_str[4+8-1] = {
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

static void GetTileDesc_Clear(uint tile, TileDesc *td)
{
	int i = (_map5[tile]>>2) & 7;
	if (i == 0)
		i = (_map5[tile] & 3) + 8;
	td->str = _clear_land_str[i - 1];
	td->owner = _map_owner[tile];
}

static void ChangeTileOwner_Clear(uint tile, byte old_player, byte new_player)
{
	return;
}

void InitializeClearLand() {
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
