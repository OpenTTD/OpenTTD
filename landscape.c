#include "stdafx.h"
#include "ttd.h"
#include "map.h"
#include <stdarg.h>
#include "gfx.h"
#include "viewport.h"
#include "command.h"
#include "vehicle.h"

extern const TileTypeProcs
	_tile_type_clear_procs,
	_tile_type_rail_procs,
	_tile_type_road_procs,
	_tile_type_town_procs,
	_tile_type_trees_procs,
	_tile_type_station_procs,
	_tile_type_water_procs,
	_tile_type_dummy_procs,
	_tile_type_industry_procs,
	_tile_type_tunnelbridge_procs,
	_tile_type_unmovable_procs;

const TileTypeProcs * const _tile_type_procs[16] = {
	&_tile_type_clear_procs,
	&_tile_type_rail_procs,
	&_tile_type_road_procs,
	&_tile_type_town_procs,
	&_tile_type_trees_procs,
	&_tile_type_station_procs,
	&_tile_type_water_procs,
	&_tile_type_dummy_procs,
	&_tile_type_industry_procs,
	&_tile_type_tunnelbridge_procs,
	&_tile_type_unmovable_procs,
};

/* landscape slope => sprite */
const byte _tileh_to_sprite[32] = {
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,
	0,0,0,0,0,0,0,16,0,0,0,17,0,15,18,0,
};

uint GetTileSlope(uint tile, int *h)
{
	uint a,b,c,d,min;
	int r;

	if (GET_TILE_X(tile) == TILE_X_MAX || GET_TILE_Y(tile) == TILE_Y_MAX) {
		if (h)
			*h = 0;
		return 0;
	}

	assert(tile < TILES_X * TILES_Y && GET_TILE_X(tile) != TILE_X_MAX && GET_TILE_Y(tile) != TILE_Y_MAX);

	min = a = _map_type_and_height[tile] & 0xF;
	b = _map_type_and_height[tile+TILE_XY(1,0)] & 0xF;
	if (min >= b) min = b;
	c = _map_type_and_height[tile+TILE_XY(0,1)] & 0xF;
	if (min >= c) min = c;
	d = _map_type_and_height[tile+TILE_XY(1,1)] & 0xF;
	if (min >= d) min = d;

	r = 0;
	if ((a-=min)!=0) { r += (--a << 4) + 8; }
	if ((c-=min)!=0) { r += (--c << 4) + 4; }
	if ((d-=min)!=0) { r += (--d << 4) + 2; }
	if ((b-=min)!=0) { r += (--b << 4) + 1; }

	if (h != 0)
		*h = min * 8;

	return r;
}

int GetTileZ(uint tile)
{
	int h;
	GetTileSlope(tile, &h);
	return h;
}

void FindLandscapeHeightByTile(TileInfo *ti, uint tile)
{
	if (GET_TILE_X(tile) == TILE_X_MAX ||
			GET_TILE_Y(tile) == TILE_Y_MAX) {
		ti->tileh = 0;
		ti->type = MP_STRANGE;
		ti->tile = 0;
		ti->map5 = 0;
		ti->z = 0;
		return;
	}

	ti->tile = tile;
	ti->map5 = _map5[tile];
	ti->type = GET_TILETYPE(tile);
	ti->tileh = GetTileSlope(tile, &ti->z);
//	ti->z = min * 8;
}

/* find the landscape height for the coordinates x y */
void FindLandscapeHeight(TileInfo *ti, uint x, uint y)
{
	int tile;

	ti->x = x;
	ti->y = y;

	if (x >= TILE_X_MAX*16-1 || y >= TILE_Y_MAX*16-1) {
		ti->tileh = 0;
		ti->type = MP_STRANGE;
		ti->tile = 0;
		ti->map5 = 0;
		ti->z = 0;
		return;
	}

	tile = TILE_FROM_XY(x,y);
	FindLandscapeHeightByTile(ti, tile);
}

uint GetPartialZ(int x, int y, int corners)
{
	int z = 0;

	switch(corners) {
	case 1:
		if (x - y >= 0)
			z = (x - y) >> 1;
		break;

	case 2:
		y^=0xF;
		if ( (x - y) >= 0)
			z = (x - y) >> 1;
		break;

	case 3:
		z = (x>>1) + 1;
		break;

	case 4:
		if (y - x >= 0)
			z = (y - x) >> 1;
		break;

	case 5:
	case 10:
	case 15:
		z = 4;
		break;

	case 6:
		z = (y>>1) + 1;
		break;

	case 7:
		z = 8;
		y^=0xF;
		if (x - y < 0)
			z += (x - y) >> 1;
		break;

	case 8:
		y ^= 0xF;
		if (y - x >= 0)
			z = (y - x) >> 1;
		break;

	case 9:
		z = (y^0xF)>>1;
		break;

	case 11:
		z = 8;
		if (x - y < 0)
			z += (x - y) >> 1;
		break;

	case 12:
		z = (x^0xF)>>1;
		break;

	case 13:
		z = 8;
		y ^= 0xF;
		if (y - x < 0)
			z += (y - x) >> 1;
		break;

	case 14:
		z = 8;
		if (y - x < 0)
			z += (y - x) >> 1;
		break;

	case 23:
		z = 1 + ((x+y)>>1);
		break;

	case 27:
		z = 1 + ((x+(y^0xF))>>1);
		break;

	case 29:
		z = 1 + (((x^0xF)+(y^0xF))>>1);
		break;

	case 30:
		z = 1 + (((x^0xF)+(y^0xF))>>1);
		break;
	}

	return z;
}

uint GetSlopeZ(int x,  int y)
{
	TileInfo ti;
//	int z;

	FindLandscapeHeight(&ti, x, y);

/*
	z = ti.z;
	x &= 0xF;
	y &= 0xF;


	assert(z < 256);
*/
	return _tile_type_procs[ti.type]->get_slope_z_proc(&ti);
}

// direction=true:  check for foundation in east and south corner
// direction=false: check for foundation in west and south corner
bool hasFoundation(TileInfo *ti, bool direction)
{
	bool south, other; // southern corner and east/west corner
	uint slope = _tile_type_procs[ti->type]->get_slope_tileh_proc(ti);
	uint tileh = ti->tileh;

	if(slope==0 && slope!=tileh) tileh=15;
	south = (tileh & 2) != (slope & 2);

	if(direction)
		other = (tileh & 4) != (slope & 4);
	else
		other = (tileh & 1) != (slope & 1);
	return south || other;

}

void DrawFoundation(TileInfo *ti, uint f)
{
	uint32 sprite_base = SPR_SLOPES_BASE-14;

	TileInfo ti2;
	FindLandscapeHeight(&ti2, ti->x, ti->y-1);
	if(hasFoundation( &ti2, true )) sprite_base += 22;		// foundation in NW direction
	FindLandscapeHeight(&ti2, ti->x-1, ti->y);
	if(hasFoundation( &ti2, false )) sprite_base += 22*2;	// foundation in NE direction

	if (f < 15) {
		// leveled foundation
		if( sprite_base < SPR_SLOPES_BASE ) sprite_base = 990; // use original slope sprites

		AddSortableSpriteToDraw(f-1 + sprite_base, ti->x, ti->y, 16, 16, 7, ti->z);
		ti->z += 8;
		ti->tileh = 0;
		OffsetGroundSprite(31, 1);
	} else {
		// inclined foundation
		sprite_base += 14;

		AddSortableSpriteToDraw(
			HASBIT( (1<<1) | (1<<2) | (1<<4) | (1<<8), ti->tileh) ? sprite_base + (f - 15) : 	ti->tileh + 0x3DE - 1,
			ti->x, ti->y, 1, 1, 1, ti->z
		);

		ti->tileh = _inclined_tileh[f - 15];
		OffsetGroundSprite(31, 9);
	}
}

void DoClearSquare(uint tile)
{
	ModifyTile(tile,
		MP_SETTYPE(MP_CLEAR) |
		MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR | MP_MAPOWNER | MP_MAP5,
		OWNER_NONE, /* map_owner */
		_generating_world ? 3 : 0 /* map5 */
	);
}

uint32 GetTileTrackStatus(uint tile, TransportType mode)
{
	return _tile_type_procs[GET_TILETYPE(tile)]->get_tile_track_status_proc(tile, mode);
}

void ChangeTileOwner(uint tile, byte old_player, byte new_player)
{
	_tile_type_procs[GET_TILETYPE(tile)]->change_tile_owner_proc(tile, old_player, new_player);
}

void GetAcceptedCargo(uint tile, AcceptedCargo ac)
{
	memset(ac, 0, sizeof(AcceptedCargo));
	_tile_type_procs[GET_TILETYPE(tile)]->get_accepted_cargo_proc(tile, ac);
}

void AnimateTile(uint tile)
{
	_tile_type_procs[GET_TILETYPE(tile)]->animate_tile_proc(tile);
}

void ClickTile(uint tile)
{
	_tile_type_procs[GET_TILETYPE(tile)]->click_tile_proc(tile);
}

void DrawTile(TileInfo *ti)
{
	_tile_type_procs[ti->type]->draw_tile_proc(ti);
}

void GetTileDesc(uint tile, TileDesc *td)
{
	_tile_type_procs[GET_TILETYPE(tile)]->get_tile_desc_proc(tile, td);
}

/* Clear a piece of landscape
 * p1 = 0,
 * p2 = 0
 */

int32 CmdLandscapeClear(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile;
	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TILE_FROM_XY(x,y);
	return _tile_type_procs[GET_TILETYPE(tile)]->clear_tile_proc(tile, flags);
}

// p1 = end tile
int32 CmdClearArea(int ex, int ey, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost,ret, money;
	int sx,sy;
	int x,y;
	bool success = false;

	// make sure sx,sy are smaller than ex,ey
	sx = GET_TILE_X(p1)*16;
	sy = GET_TILE_Y(p1)*16;
	if (ex < sx) intswap(ex, sx);
	if (ey < sy) intswap(ey, sy);

	money = GetAvailableMoneyForCommand();
	cost = 0;

	for(x=sx; x<=ex; x+=16) {
		for(y=sy; y<=ey; y+=16) {
			ret = DoCommandByTile(TILE_FROM_XY(x,y), 0, 0, flags &~DC_EXEC, CMD_LANDSCAPE_CLEAR);
			if (ret == CMD_ERROR) continue;
			cost += ret;
			success = true;

			if (flags & DC_EXEC) {
				if ( ret>0 && (money -= ret) < 0) {
					_additional_cash_required = ret;
					return cost - ret;
				}
				DoCommandByTile(TILE_FROM_XY(x,y), 0, 0, flags, CMD_LANDSCAPE_CLEAR);

				// draw explosion animation...
				if ((x==sx || x==ex) && (y==sy || y==ey)) {
					// big explosion in each corner, or small explosion for single tiles
					CreateEffectVehicleAbove(x + 8,y + 8, 2, sy==ey && sx==ex ? EV_DEMOLISH : EV_CRASHED_SMOKE);
				}
			}
		}
	}

	if (!success)
		cost = CMD_ERROR;
	return cost;
}


/* utility function used to modify a tile */
void CDECL ModifyTile(uint tile, uint flags, ...)
{
	va_list va;
	int i;

	va_start(va, flags);

	if ((i = (flags >> 8) & 0xF) != 0) {
		_map_type_and_height[tile] = (_map_type_and_height[tile]&~0xF0)|((i-1) << 4);
	}

	if (flags & (MP_MAP2_CLEAR | MP_MAP2)) {
		int x = 0;
		if (flags & MP_MAP2) x = va_arg(va, int);
		_map2[tile] = x;
	}

	if (flags & (MP_MAP3LO_CLEAR | MP_MAP3LO)) {
		int x = 0;
		if (flags & MP_MAP3LO) x = va_arg(va, int);
		_map3_lo[tile] = x;
	}

	if (flags & (MP_MAP3HI_CLEAR | MP_MAP3HI)) {
		int x = 0;
		if (flags & MP_MAP3HI) x = va_arg(va, int);
		_map3_hi[tile] = x;
	}

	if (flags & (MP_MAPOWNER|MP_MAPOWNER_CURRENT)) {
		byte x = _current_player;
		if (flags & MP_MAPOWNER) x = va_arg(va, int);
		_map_owner[tile] = x;
	}

	if (flags & MP_MAP5) {
		_map5[tile] = va_arg(va, int);
	}

	va_end(va);

	if (!(flags & MP_NODIRTY))
		MarkTileDirtyByTile(tile);
}

void SetMapExtraBits(uint tile, byte bits)
{
	_map_extra_bits[tile >> 2] &= ~(3 << ((tile&3)*2));
	_map_extra_bits[tile >> 2] |= (bits&3) << ((tile&3)*2);
}

uint GetMapExtraBits(uint tile)
{
	if (GET_TILE_X(tile) < MapSizeX() && GET_TILE_Y(tile) < MapSizeY() &&
			GET_TILE_X(tile) > 0 && GET_TILE_Y(tile) > 0)
		return (_map_extra_bits[tile >> 2] >> (tile&3)*2)&3;
	else
		return 0;
}

#define TILELOOP_BITS 4
#define TILELOOP_SIZE (1 << TILELOOP_BITS)
#define TILELOOP_ASSERTMASK ((TILELOOP_SIZE-1) + ((TILELOOP_SIZE-1) << TILE_X_BITS))
#define TILELOOP_CHKMASK (((1 << (TILE_X_BITS - TILELOOP_BITS))-1) << TILELOOP_BITS)

void RunTileLoop()
{
	uint tile;
	uint count;

	tile = _cur_tileloop_tile;

	assert( (tile & ~TILELOOP_ASSERTMASK) == 0);
	count = (TILES_X/TILELOOP_SIZE) * (TILES_Y/TILELOOP_SIZE);
	do {
		_tile_type_procs[GET_TILETYPE(tile)]->tile_loop_proc(tile);

		if ( GET_TILE_X(tile) < TILES_X - TILELOOP_SIZE) {
			tile += TILELOOP_SIZE; /* no overflow */
		} else {
			tile = TILE_MASK(tile - TILELOOP_SIZE * (TILES_X/TILELOOP_SIZE-1) + TILE_XY(0, TILELOOP_SIZE)); /* x would overflow, also increase y */
		}
	} while (--count);
	assert( (tile & ~TILELOOP_ASSERTMASK) == 0);

	tile += 9;
	if (tile & TILELOOP_CHKMASK)
		tile = (tile + TILES_X) & TILELOOP_ASSERTMASK;
	_cur_tileloop_tile = tile;
}

void InitializeLandscape()
{
	uint map_size = MapSize();
	int i;

	memset(_map_owner, OWNER_NONE, map_size);
	memset(_map2, 0, map_size * sizeof(uint16));
	memset(_map3_lo, 0, map_size);
	memset(_map3_hi, 0, map_size);
	memset(_map_extra_bits, 0, map_size / 4);
	memset(_map_type_and_height, MP_CLEAR << 4, map_size);

	for(i=0; i!=TILES_Y-1; i++)
		memset(_map_type_and_height + i*TILES_X, 0, TILES_X-1);

	memset(_map5, 3, map_size);
}

void ConvertGroundTilesIntoWaterTiles()
{
	uint tile = 0;
	int h;

	while(true) {
		if (IS_TILETYPE(tile, MP_CLEAR) && GetTileSlope(tile, &h) == 0 && h == 0) {
			_map_type_and_height[tile] = MP_WATER << 4;
			_map5[tile] = 0;
			_map_owner[tile] = OWNER_WATER;
		}
		tile++;
		if (GET_TILE_X(tile) == TILE_X_MAX) {
			tile += TILE_XY(-TILE_X_MAX, 1);
			if (GET_TILE_Y(tile) == TILE_Y_MAX)
				break;
		}
	}
}

static const byte _genterrain_tbl_1[5] = { 10, 22, 33, 37, 4 };
static const byte _genterrain_tbl_2[5] = { 0, 0, 0, 0, 33 };

static void GenerateTerrain(int type, int flag)
{
	uint32 r;
	int x,y;
	int w,h;
	byte *p,*tile;
	byte direction;

	r = Random();
	p = GetSpritePtr((((r >> 24) * _genterrain_tbl_1[type]) >> 8) + _genterrain_tbl_2[type] + 4845);

	x = r & TILE_X_MAX;
	y = (r >> TILE_X_BITS) & TILE_Y_MAX;


	if (x < 2 || y < 2)
		return;

	direction = (byte)(r >> 22) & 3;
	w = p[2];
	h = p[1];
	if (direction & 1) { w = p[1]; h = p[2]; }
	p += 8;

	if (flag & 4) {
		if (!(flag & 2)) {
			if (!(flag & 1)) {
				if (x + y > 190)
					return;
			} else {
				if (y < 30 + x)
					return;
			}
		} else {
			if (!(flag & 1)) {
				if (x + y < 256)
					return;
			} else {
				if (x < 30 + y)
					return;
			}
		}
	}

	if (x + w >= TILE_X_MAX-1)
		return;

	if (y + h >= TILE_Y_MAX-1)
		return;

	tile = &_map_type_and_height[TILE_XY(x,y)];

	if (direction == 0) {
		do {
			int w_cur = w;
			byte *tile_cur = tile;
			do {
				if (*p >= *tile_cur) *tile_cur = *p;
				p++;
				tile_cur++;
			} while (--w_cur != 0);
			tile += TILE_XY(0,1);
		} while (--h != 0);
	} else if (direction == 1) {
		do {
			int h_cur = h;
			byte *tile_cur = tile;
			do {
				if (*p >= *tile_cur) *tile_cur = *p;
				p++;
				tile_cur+=TILE_XY(0,1);
			} while (--h_cur != 0);
			tile++;
		} while (--w != 0);
	} else if (direction == 2) {
		tile += w - 1;
		do {
			int w_cur = w;
			byte *tile_cur = tile;
			do {
				if (*p >= *tile_cur) *tile_cur = *p;
				p++;
				tile_cur--;
			} while (--w_cur != 0);
			tile += TILE_XY(0,1);
		} while (--h != 0);
	} else  {
		tile += (h - 1) * TILE_XY(0,1);
		do {
			int h_cur = h;
			byte *tile_cur = tile;
			do {
				if (*p >= *tile_cur) *tile_cur = *p;
				p++;
				tile_cur-=TILE_XY(0,1);
			} while (--h_cur != 0);
			tile++;
		} while (--w != 0);
	}
}


#include "table/genland.h"

static void CreateDesertOrRainForest()
{
	uint tile;
	const TileIndexDiff *data;
	byte mt;
	int i;

	tile = 0;
	do {
		data = _make_desert_or_rainforest_data;
		do {
			if ((i = *data++) == MDORD_LAST) {
				SetMapExtraBits(tile, 1);
				break;
			}
			mt = _map_type_and_height[TILE_MASK(tile + i)];
		} while ((mt & 0xC) == 0 && (mt >> 4) != MP_WATER);
	} while (++tile != TILES_X*TILES_Y);

	for(i=0; i!=256; i++)
		RunTileLoop();

	tile = 0;
	do {
		data = _make_desert_or_rainforest_data;
		do {
			if ((i = *data++) == MDORD_LAST) {
				SetMapExtraBits(tile, 2);
				break;
			}
		} while ( !IS_TILETYPE(TILE_MASK(tile+i), MP_CLEAR) || (_map5[TILE_MASK(tile + i)]&0x1C) != 0x14);
	} while (++tile != TILES_X*TILES_Y);
}

void GenerateLandscape()
{
	int i,flag;
	uint32 r;

	if (_opt.landscape == LT_HILLY) {
		i = ((Random() & 0x7F) + 950) * LANDSCAPE_SIZE_FACTOR;
		do {
			GenerateTerrain(2, 0);
		} while (--i);

		r = Random();
		flag = (r & 3) | 4;
		i = (((r >> 16) & 0x7F) + 450) * LANDSCAPE_SIZE_FACTOR;
		do {
			GenerateTerrain(4, flag);
		} while (--i);
	} else if (_opt.landscape == LT_DESERT) {
		i = ((Random()&0x7F) + 170) * LANDSCAPE_SIZE_FACTOR;
		do {
			GenerateTerrain(0, 0);
		} while (--i);

		r = Random();
		flag = (r & 3) | 4;
		i = (((r >> 16) & 0xFF) + 1700) * LANDSCAPE_SIZE_FACTOR;
		do {
			GenerateTerrain(0, flag);
		} while (--i);

		flag ^= 2;

		i = ((Random() & 0x7F) + 410) * LANDSCAPE_SIZE_FACTOR;
		do {
			GenerateTerrain(3, flag);
		} while (--i);
	} else {
		i = ((Random() & 0x7F) + (3 - _opt.diff.quantity_sea_lakes)*256 + 100) * LANDSCAPE_SIZE_FACTOR;
		do {
			GenerateTerrain(_opt.diff.terrain_type, 0);
		} while (--i);
	}

	ConvertGroundTilesIntoWaterTiles();

	if (_opt.landscape == LT_DESERT)
		CreateDesertOrRainForest();
}

void OnTick_Town();
void OnTick_Trees();
void OnTick_Station();
void OnTick_Industry();

void OnTick_Players();
void OnTick_Train();

void CallLandscapeTick()
{
	OnTick_Town();
	OnTick_Trees();
	OnTick_Station();
	OnTick_Industry();

	OnTick_Players();
	OnTick_Train();
}

TileIndex AdjustTileCoordRandomly(TileIndex a, byte rng)
{
	int rn = rng;
	uint32 r = Random();

	return TILE_XY(
		GET_TILE_X(a) + ((byte)r * rn * 2 >> 8) - rn,
		GET_TILE_Y(a) + ((byte)(r>>8) * rn * 2 >> 8) - rn
	);
}

// This function checks if we add addx/addy to tile, if we
//  do wrap around the edges. For example, tile = (10,2) and
//  addx = +3 and addy = -4. This function will now return
//  TILE_WRAPPED, because the y is wrapped. This is needed in
//  for example, farmland. When the tile is not wrapped,
//  the result will be tile + TILE_XY(addx, addy)
uint TileAddWrap(TileIndex tile, int addx, int addy)
{
	int x, y;
	x = GET_TILE_X(tile) + addx;
	y = GET_TILE_Y(tile) + addy;

	// Are we about to wrap?
	if (x > 0 && x < TILE_X_MAX && y > 0 && y < TILE_Y_MAX)
		return tile + TILE_XY(addx, addy);

	return TILE_WRAPPED;
}

bool IsValidTile(uint tile)
{
	return (tile < TILES_X * TILE_Y_MAX && GET_TILE_X(tile) != TILE_X_MAX);
}
