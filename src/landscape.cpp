/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "heightmap.h"
#include "clear_map.h"
#include "spritecache.h"
#include <stdarg.h>
#include "viewport_func.h"
#include "command_func.h"
#include "landscape.h"
#include "variables.h"
#include "void_map.h"
#include "water_map.h"
#include "tgp.h"
#include "genworld.h"
#include "tile_cmd.h"
#include "core/alloc_func.hpp"
#include "fios.h"
#include "window_func.h"
#include "functions.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "settings_type.h"
#include "water.h"

#include "table/sprites.h"

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
	0, 1, 2, 3, 4, 5, 6,  7, 8, 9, 10, 11, 12, 13, 14, 0,
	0, 0, 0, 0, 0, 0, 0, 16, 0, 0,  0, 17,  0, 15, 18, 0,
};

SnowLine *_snow_line = NULL;

/**
 * Applys a foundation to a slope.
 *
 * @pre      Foundation and slope must be valid combined.
 * @param f  The #Foundation.
 * @param s  The #Slope to modify.
 * @return   Increment to the tile Z coordinate.
 */
uint ApplyFoundationToSlope(Foundation f, Slope *s)
{

	if (!IsFoundation(f)) return 0;

	if (IsLeveledFoundation(f)) {
		*s = SLOPE_FLAT;
		return TILE_HEIGHT;
	}

	if (f != FOUNDATION_STEEP_BOTH && IsNonContinuousFoundation(f)) {
		*s = HalftileSlope(*s, GetHalftileFoundationCorner(f));
		return 0;
	}

	if (IsSpecialRailFoundation(f)) {
		*s = SlopeWithThreeCornersRaised(OppositeCorner(GetRailFoundationCorner(f)));
		return 0;
	}

	uint dz = IsSteepSlope(*s) ? TILE_HEIGHT : 0;
	Corner highest_corner = GetHighestSlopeCorner(*s);

	switch (f) {
		case FOUNDATION_INCLINED_X:
			*s = (((highest_corner == CORNER_W) || (highest_corner == CORNER_S)) ? SLOPE_SW : SLOPE_NE);
			break;

		case FOUNDATION_INCLINED_Y:
			*s = (((highest_corner == CORNER_S) || (highest_corner == CORNER_E)) ? SLOPE_SE : SLOPE_NW);
			break;

		case FOUNDATION_STEEP_LOWER:
			*s = SlopeWithOneCornerRaised(highest_corner);
			break;

		case FOUNDATION_STEEP_BOTH:
			*s = HalftileSlope(SlopeWithOneCornerRaised(highest_corner), highest_corner);
			break;

		default: NOT_REACHED();
	}
	return dz;
}


uint GetPartialZ(int x, int y, Slope corners)
{
	if (IsHalftileSlope(corners)) {
		switch (GetHalftileSlopeCorner(corners)) {
			case CORNER_W:
				if (x - y >= 0) return GetSlopeMaxZ(corners);
				break;

			case CORNER_S:
				if (x - (y ^ 0xF) >= 0) return GetSlopeMaxZ(corners);
				break;

			case CORNER_E:
				if (y - x >= 0) return GetSlopeMaxZ(corners);
				break;

			case CORNER_N:
				if ((y ^ 0xF) - x >= 0) return GetSlopeMaxZ(corners);
				break;

			default: NOT_REACHED();
		}
	}

	int z = 0;

	switch (corners & ~SLOPE_HALFTILE_MASK) {
	case SLOPE_W:
		if (x - y >= 0)
			z = (x - y) >> 1;
		break;

	case SLOPE_S:
		y ^= 0xF;
		if ( (x - y) >= 0)
			z = (x - y) >> 1;
		break;

	case SLOPE_SW:
		z = (x >> 1) + 1;
		break;

	case SLOPE_E:
		if (y - x >= 0)
			z = (y - x) >> 1;
		break;

	case SLOPE_EW:
	case SLOPE_NS:
	case SLOPE_ELEVATED:
		z = 4;
		break;

	case SLOPE_SE:
		z = (y >> 1) + 1;
		break;

	case SLOPE_WSE:
		z = 8;
		y ^= 0xF;
		if (x - y < 0)
			z += (x - y) >> 1;
		break;

	case SLOPE_N:
		y ^= 0xF;
		if (y - x >= 0)
			z = (y - x) >> 1;
		break;

	case SLOPE_NW:
		z = (y ^ 0xF) >> 1;
		break;

	case SLOPE_NWS:
		z = 8;
		if (x - y < 0)
			z += (x - y) >> 1;
		break;

	case SLOPE_NE:
		z = (x ^ 0xF) >> 1;
		break;

	case SLOPE_ENW:
		z = 8;
		y ^= 0xF;
		if (y - x < 0)
			z += (y - x) >> 1;
		break;

	case SLOPE_SEN:
		z = 8;
		if (y - x < 0)
			z += (y - x) >> 1;
		break;

	case SLOPE_STEEP_S:
		z = 1 + ((x + y) >> 1);
		break;

	case SLOPE_STEEP_W:
		z = 1 + ((x + (y ^ 0xF)) >> 1);
		break;

	case SLOPE_STEEP_N:
		z = 1 + (((x ^ 0xF) + (y ^ 0xF)) >> 1);
		break;

	case SLOPE_STEEP_E:
		z = 1 + (((x ^ 0xF) + y) >> 1);
		break;

		default: break;
	}

	return z;
}

uint GetSlopeZ(int x, int y)
{
	TileIndex tile = TileVirtXY(x, y);

	return _tile_type_procs[GetTileType(tile)]->get_slope_z_proc(tile, x, y);
}

/**
 * Determine the Z height of a corner relative to TileZ.
 *
 * @pre The slope must not be a halftile slope.
 *
 * @param tileh The slope.
 * @param corner The corner.
 * @return Z position of corner relative to TileZ.
 */
int GetSlopeZInCorner(Slope tileh, Corner corner)
{
	assert(!IsHalftileSlope(tileh));
	static const int _corner_slopes[4][2] = {
		{ SLOPE_W, SLOPE_STEEP_W }, { SLOPE_S, SLOPE_STEEP_S }, { SLOPE_E, SLOPE_STEEP_E }, { SLOPE_N, SLOPE_STEEP_N }
	};
	return ((tileh & _corner_slopes[corner][0]) != 0 ? TILE_HEIGHT : 0) + (tileh == _corner_slopes[corner][1] ? TILE_HEIGHT : 0);
}

/**
 * Determine the Z height of the corners of a specific tile edge
 *
 * @note If a tile has a non-continuous halftile foundation, a corner can have different heights wrt. it's edges.
 *
 * @pre z1 and z2 must be initialized (typ. with TileZ). The corner heights just get added.
 *
 * @param tileh The slope of the tile.
 * @param edge The edge of interest.
 * @param z1 Gets incremented by the height of the first corner of the edge. (near corner wrt. the camera)
 * @param z2 Gets incremented by the height of the second corner of the edge. (far corner wrt. the camera)
 */
void GetSlopeZOnEdge(Slope tileh, DiagDirection edge, int *z1, int *z2)
{
	static const Slope corners[4][4] = {
		/*    corner     |          steep slope
		 *  z1      z2   |       z1             z2        */
		{SLOPE_E, SLOPE_N, SLOPE_STEEP_E, SLOPE_STEEP_N}, // DIAGDIR_NE, z1 = E, z2 = N
		{SLOPE_S, SLOPE_E, SLOPE_STEEP_S, SLOPE_STEEP_E}, // DIAGDIR_SE, z1 = S, z2 = E
		{SLOPE_S, SLOPE_W, SLOPE_STEEP_S, SLOPE_STEEP_W}, // DIAGDIR_SW, z1 = S, z2 = W
		{SLOPE_W, SLOPE_N, SLOPE_STEEP_W, SLOPE_STEEP_N}, // DIAGDIR_NW, z1 = W, z2 = N
	};

	int halftile_test = (IsHalftileSlope(tileh) ? SlopeWithOneCornerRaised(GetHalftileSlopeCorner(tileh)) : 0);
	if (halftile_test == corners[edge][0]) *z2 += TILE_HEIGHT; // The slope is non-continuous in z2. z2 is on the upper side.
	if (halftile_test == corners[edge][1]) *z1 += TILE_HEIGHT; // The slope is non-continuous in z1. z1 is on the upper side.

	if ((tileh & corners[edge][0]) != 0) *z1 += TILE_HEIGHT; // z1 is raised
	if ((tileh & corners[edge][1]) != 0) *z2 += TILE_HEIGHT; // z2 is raised
	if ((tileh & ~SLOPE_HALFTILE_MASK) == corners[edge][2]) *z1 += TILE_HEIGHT; // z1 is highest corner of a steep slope
	if ((tileh & ~SLOPE_HALFTILE_MASK) == corners[edge][3]) *z2 += TILE_HEIGHT; // z2 is highest corner of a steep slope
}

Slope GetFoundationSlope(TileIndex tile, uint* z)
{
	Slope tileh = GetTileSlope(tile, z);
	Foundation f = _tile_type_procs[GetTileType(tile)]->get_foundation_proc(tile, tileh);
	uint z_inc = ApplyFoundationToSlope(f, &tileh);
	if (z != NULL) *z += z_inc;
	return tileh;
}


static bool HasFoundationNW(TileIndex tile, Slope slope_here, uint z_here)
{
	uint z;

	int z_W_here = z_here;
	int z_N_here = z_here;
	GetSlopeZOnEdge(slope_here, DIAGDIR_NW, &z_W_here, &z_N_here);

	Slope slope = GetFoundationSlope(TILE_ADDXY(tile, 0, -1), &z);
	int z_W = z;
	int z_N = z;
	GetSlopeZOnEdge(slope, DIAGDIR_SE, &z_W, &z_N);

	return (z_N_here > z_N) || (z_W_here > z_W);
}


static bool HasFoundationNE(TileIndex tile, Slope slope_here, uint z_here)
{
	uint z;

	int z_E_here = z_here;
	int z_N_here = z_here;
	GetSlopeZOnEdge(slope_here, DIAGDIR_NE, &z_E_here, &z_N_here);

	Slope slope = GetFoundationSlope(TILE_ADDXY(tile, -1, 0), &z);
	int z_E = z;
	int z_N = z;
	GetSlopeZOnEdge(slope, DIAGDIR_SW, &z_E, &z_N);

	return (z_N_here > z_N) || (z_E_here > z_E);
}


void DrawFoundation(TileInfo *ti, Foundation f)
{
	if (!IsFoundation(f)) return;

	/* Two part foundations must be drawn separately */
	assert(f != FOUNDATION_STEEP_BOTH);

	uint sprite_block = 0;
	uint z;
	Slope slope = GetFoundationSlope(ti->tile, &z);

	/* Select the needed block of foundations sprites
	 * Block 0: Walls at NW and NE edge
	 * Block 1: Wall  at        NE edge
	 * Block 2: Wall  at NW        edge
	 * Block 3: No walls at NW or NE edge
	 */
	if (!HasFoundationNW(ti->tile, slope, z)) sprite_block += 1;
	if (!HasFoundationNE(ti->tile, slope, z)) sprite_block += 2;

	/* Use the original slope sprites if NW and NE borders should be visible */
	SpriteID leveled_base = (sprite_block == 0 ? (int)SPR_FOUNDATION_BASE : (SPR_SLOPES_VIRTUAL_BASE + sprite_block * SPR_TRKFOUND_BLOCK_SIZE));
	SpriteID inclined_base = SPR_SLOPES_VIRTUAL_BASE + SPR_SLOPES_INCLINED_OFFSET + sprite_block * SPR_TRKFOUND_BLOCK_SIZE;
	SpriteID halftile_base = SPR_HALFTILE_FOUNDATION_BASE + sprite_block * SPR_HALFTILE_BLOCK_SIZE;

	if (IsSteepSlope(ti->tileh)) {
		if (!IsNonContinuousFoundation(f)) {
			/* Lower part of foundation */
			AddSortableSpriteToDraw(
				leveled_base + (ti->tileh & ~SLOPE_STEEP), PAL_NONE, ti->x, ti->y, 16, 16, 7, ti->z
			);
		}

		Corner highest_corner = GetHighestSlopeCorner(ti->tileh);
		ti->z += ApplyFoundationToSlope(f, &ti->tileh);

		if (IsInclinedFoundation(f)) {
			/* inclined foundation */
			byte inclined = highest_corner * 2 + (f == FOUNDATION_INCLINED_Y ? 1 : 0);

			AddSortableSpriteToDraw(inclined_base + inclined, PAL_NONE, ti->x, ti->y, 16, 16, 1, ti->z);
			OffsetGroundSprite(31, 9);
		} else if (f == FOUNDATION_STEEP_LOWER) {
			/* one corner raised */
			OffsetGroundSprite(31, 1);
		} else {
			/* halftile foundation */
			int x_bb = (((highest_corner == CORNER_W) || (highest_corner == CORNER_S)) ? 8 : 0);
			int y_bb = (((highest_corner == CORNER_S) || (highest_corner == CORNER_E)) ? 8 : 0);

			AddSortableSpriteToDraw(halftile_base + highest_corner, PAL_NONE, ti->x + x_bb, ti->y + y_bb, 8, 8, 7, ti->z + TILE_HEIGHT);
			OffsetGroundSprite(31, 9);
		}
	} else {
		if (IsLeveledFoundation(f)) {
			/* leveled foundation */
			AddSortableSpriteToDraw(leveled_base + ti->tileh, PAL_NONE, ti->x, ti->y, 16, 16, 7, ti->z);
			OffsetGroundSprite(31, 1);
		} else if (IsNonContinuousFoundation(f)) {
			/* halftile foundation */
			Corner halftile_corner = GetHalftileFoundationCorner(f);
			int x_bb = (((halftile_corner == CORNER_W) || (halftile_corner == CORNER_S)) ? 8 : 0);
			int y_bb = (((halftile_corner == CORNER_S) || (halftile_corner == CORNER_E)) ? 8 : 0);

			AddSortableSpriteToDraw(halftile_base + halftile_corner, PAL_NONE, ti->x + x_bb, ti->y + y_bb, 8, 8, 7, ti->z);
			OffsetGroundSprite(31, 9);
		} else if (IsSpecialRailFoundation(f)) {
			/* anti-zig-zag foundation */
			SpriteID spr;
			if (ti->tileh == SLOPE_NS || ti->tileh == SLOPE_EW) {
				/* half of leveled foundation under track corner */
				spr = leveled_base + SlopeWithThreeCornersRaised(GetRailFoundationCorner(f));
			} else {
				/* tile-slope = sloped along X/Y, foundation-slope = three corners raised */
				spr = inclined_base + 2 * GetRailFoundationCorner(f) + ((ti->tileh == SLOPE_SW || ti->tileh == SLOPE_NE) ? 1 : 0);
			}
			AddSortableSpriteToDraw(spr, PAL_NONE, ti->x, ti->y, 16, 16, 7, ti->z);
			OffsetGroundSprite(31, 9);
		} else {
			/* inclined foundation */
			byte inclined = GetHighestSlopeCorner(ti->tileh) * 2 + (f == FOUNDATION_INCLINED_Y ? 1 : 0);

			AddSortableSpriteToDraw(inclined_base + inclined, PAL_NONE, ti->x, ti->y, 16, 16, 1, ti->z);
			OffsetGroundSprite(31, 9);
		}
		ti->z += ApplyFoundationToSlope(f, &ti->tileh);
	}
}

void DoClearSquare(TileIndex tile)
{
	MakeClear(tile, CLEAR_GRASS, _generating_world ? 3 : 0);
	MarkTileDirtyByTile(tile);
}

uint32 GetTileTrackStatus(TileIndex tile, TransportType mode, uint sub_mode)
{
	return _tile_type_procs[GetTileType(tile)]->get_tile_track_status_proc(tile, mode, sub_mode);
}

void ChangeTileOwner(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	_tile_type_procs[GetTileType(tile)]->change_tile_owner_proc(tile, old_player, new_player);
}

void GetAcceptedCargo(TileIndex tile, AcceptedCargo ac)
{
	memset(ac, 0, sizeof(AcceptedCargo));
	_tile_type_procs[GetTileType(tile)]->get_accepted_cargo_proc(tile, ac);
}

void AnimateTile(TileIndex tile)
{
	_tile_type_procs[GetTileType(tile)]->animate_tile_proc(tile);
}

void ClickTile(TileIndex tile)
{
	_tile_type_procs[GetTileType(tile)]->click_tile_proc(tile);
}

void GetTileDesc(TileIndex tile, TileDesc *td)
{
	_tile_type_procs[GetTileType(tile)]->get_tile_desc_proc(tile, td);
}

/**
 * Has a snow line table already been loaded.
 * @return true if the table has been loaded already.
 */
bool IsSnowLineSet(void)
{
	return _snow_line != NULL;
}

/**
 * Set a variable snow line, as loaded from a newgrf file.
 * @param table the 12 * 32 byte table containing the snowline for each day
 */
void SetSnowLine(byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS])
{
	_snow_line = CallocT<SnowLine>(1);
	memcpy(_snow_line->table, table, sizeof(_snow_line->table));

	for (uint i = 0; i < SNOW_LINE_MONTHS; i++) {
		for (uint j = 0; j < SNOW_LINE_DAYS; j++) {
			_snow_line->highest_value = max(_snow_line->highest_value, table[i][j]);
		}
	}
}

/**
 * Get the current snow line, either variable or static.
 * @return the snow line height.
 */
byte GetSnowLine(void)
{
	if (_snow_line == NULL) return _opt.snow_line;

	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);
	return _snow_line->table[ymd.month][ymd.day];
}

/**
 * Get the highest possible snow line height, either variable or static.
 * @return the highest snow line height.
 */
byte HighestSnowLine(void)
{
	return _snow_line == NULL ? _opt.snow_line : _snow_line->highest_value;
}

/**
 * Clear the variable snow line table and free the memory.
 */
void ClearSnowLine(void)
{
	free(_snow_line);
	_snow_line = NULL;
}

/** Clear a piece of landscape
 * @param tile tile to clear
 * @param flags of operation to conduct
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdLandscapeClear(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return _tile_type_procs[GetTileType(tile)]->clear_tile_proc(tile, flags);
}

/** Clear a big piece of landscape
 * @param tile end tile of area dragging
 * @param p1 start tile of area dragging
 * @param flags of operation to conduct
 * @param p2 unused
 */
CommandCost CmdClearArea(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost ret, money;
	CommandCost cost(EXPENSES_CONSTRUCTION);
	int ex;
	int ey;
	int sx, sy;
	int x, y;
	bool success = false;

	if (p1 >= MapSize()) return CMD_ERROR;

	/* make sure sx,sy are smaller than ex,ey */
	ex = TileX(tile);
	ey = TileY(tile);
	sx = TileX(p1);
	sy = TileY(p1);
	if (ex < sx) Swap(ex, sx);
	if (ey < sy) Swap(ey, sy);

	money.AddCost(GetAvailableMoneyForCommand());

	for (x = sx; x <= ex; ++x) {
		for (y = sy; y <= ey; ++y) {
			ret = DoCommand(TileXY(x, y), 0, 0, flags & ~DC_EXEC, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) continue;
			success = true;

			if (flags & DC_EXEC) {
				money.AddCost(-ret.GetCost());
				if (ret.GetCost() > 0 && money.GetCost() < 0) {
					_additional_cash_required = ret.GetCost();
					return cost;
				}
				DoCommand(TileXY(x, y), 0, 0, flags, CMD_LANDSCAPE_CLEAR);

				/* draw explosion animation... */
				if ((x == sx || x == ex) && (y == sy || y == ey)) {
					/* big explosion in each corner, or small explosion for single tiles */
					CreateEffectVehicleAbove(x * TILE_SIZE + TILE_SIZE / 2, y * TILE_SIZE + TILE_SIZE / 2, 2,
						sy == ey && sx == ex ? EV_EXPLOSION_SMALL : EV_EXPLOSION_LARGE
					);
				}
			}
			cost.AddCost(ret);
		}
	}

	return (success) ? cost : CMD_ERROR;
}


#define TILELOOP_BITS 4
#define TILELOOP_SIZE (1 << TILELOOP_BITS)
#define TILELOOP_ASSERTMASK ((TILELOOP_SIZE - 1) + ((TILELOOP_SIZE - 1) << MapLogX()))
#define TILELOOP_CHKMASK (((1 << (MapLogX() - TILELOOP_BITS))-1) << TILELOOP_BITS)

void RunTileLoop()
{
	TileIndex tile;
	uint count;

	tile = _cur_tileloop_tile;

	assert( (tile & ~TILELOOP_ASSERTMASK) == 0);
	count = (MapSizeX() / TILELOOP_SIZE) * (MapSizeY() / TILELOOP_SIZE);
	do {
		_tile_type_procs[GetTileType(tile)]->tile_loop_proc(tile);

		if (TileX(tile) < MapSizeX() - TILELOOP_SIZE) {
			tile += TILELOOP_SIZE; // no overflow
		} else {
			tile = TILE_MASK(tile - TILELOOP_SIZE * (MapSizeX() / TILELOOP_SIZE - 1) + TileDiffXY(0, TILELOOP_SIZE)); /* x would overflow, also increase y */
		}
	} while (--count);
	assert( (tile & ~TILELOOP_ASSERTMASK) == 0);

	tile += 9;
	if (tile & TILELOOP_CHKMASK)
		tile = (tile + MapSizeX()) & TILELOOP_ASSERTMASK;
	_cur_tileloop_tile = tile;
}

void InitializeLandscape()
{
	uint maxx = MapMaxX();
	uint maxy = MapMaxY();
	uint sizex = MapSizeX();
	uint x;
	uint y;

	for (y = 0; y < maxy; y++) {
		for (x = 0; x < maxx; x++) {
			MakeClear(sizex * y + x, CLEAR_GRASS, 3);
			SetTileHeight(sizex * y + x, 0);
			SetTropicZone(sizex * y + x, TROPICZONE_INVALID);
			ClearBridgeMiddle(sizex * y + x);
		}
		MakeVoid(sizex * y + x);
	}
	for (x = 0; x < sizex; x++) MakeVoid(sizex * y + x);
}

static const byte _genterrain_tbl_1[5] = { 10, 22, 33, 37, 4  };
static const byte _genterrain_tbl_2[5] = {  0,  0,  0,  0, 33 };

static void GenerateTerrain(int type, int flag)
{
	uint32 r;
	uint x;
	uint y;
	uint w;
	uint h;
	const Sprite* templ;
	const byte *p;
	Tile* tile;
	byte direction;

	r = Random();
	templ = GetSprite((((r >> 24) * _genterrain_tbl_1[type]) >> 8) + _genterrain_tbl_2[type] + 4845);

	x = r & MapMaxX();
	y = (r >> MapLogX()) & MapMaxY();


	if (x < 2 || y < 2) return;

	direction = GB(r, 22, 2);
	if (direction & 1) {
		w = templ->height;
		h = templ->width;
	} else {
		w = templ->width;
		h = templ->height;
	}
	p = templ->data;

	if (flag & 4) {
		uint xw = x * MapSizeY();
		uint yw = y * MapSizeX();
		uint bias = (MapSizeX() + MapSizeY()) * 16;

		switch (flag & 3) {
			case 0:
				if (xw + yw > MapSize() - bias) return;
				break;

			case 1:
				if (yw < xw + bias) return;
				break;

			case 2:
				if (xw + yw < MapSize() + bias) return;
				break;

			case 3:
				if (xw < yw + bias) return;
				break;
		}
	}

	if (x + w >= MapMaxX() - 1) return;
	if (y + h >= MapMaxY() - 1) return;

	tile = &_m[TileXY(x, y)];

	switch (direction) {
		case 0:
			do {
				Tile* tile_cur = tile;
				uint w_cur;

				for (w_cur = w; w_cur != 0; --w_cur) {
					if (*p >= tile_cur->type_height) tile_cur->type_height = *p;
					p++;
					tile_cur++;
				}
				tile += TileDiffXY(0, 1);
			} while (--h != 0);
			break;

		case 1:
			do {
				Tile* tile_cur = tile;
				uint h_cur;

				for (h_cur = h; h_cur != 0; --h_cur) {
					if (*p >= tile_cur->type_height) tile_cur->type_height = *p;
					p++;
					tile_cur += TileDiffXY(0, 1);
				}
				tile++;
			} while (--w != 0);
			break;

		case 2:
			tile += TileDiffXY(w - 1, 0);
			do {
				Tile* tile_cur = tile;
				uint w_cur;

				for (w_cur = w; w_cur != 0; --w_cur) {
					if (*p >= tile_cur->type_height) tile_cur->type_height = *p;
					p++;
					tile_cur--;
				}
				tile += TileDiffXY(0, 1);
			} while (--h != 0);
			break;

		case 3:
			tile += TileDiffXY(0, h - 1);
			do {
				Tile* tile_cur = tile;
				uint h_cur;

				for (h_cur = h; h_cur != 0; --h_cur) {
					if (*p >= tile_cur->type_height) tile_cur->type_height = *p;
					p++;
					tile_cur -= TileDiffXY(0, 1);
				}
				tile++;
			} while (--w != 0);
			break;
	}
}


#include "table/genland.h"

static void CreateDesertOrRainForest()
{
	TileIndex tile;
	TileIndex update_freq = MapSize() / 4;
	const TileIndexDiffC *data;
	uint i;

	for (tile = 0; tile != MapSize(); ++tile) {
		if ((tile % update_freq) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		for (data = _make_desert_or_rainforest_data;
				data != endof(_make_desert_or_rainforest_data); ++data) {
			TileIndex t = TILE_MASK(tile + ToTileIndexDiff(*data));
			if (TileHeight(t) >= 4 || IsTileType(t, MP_WATER)) break;
		}
		if (data == endof(_make_desert_or_rainforest_data))
			SetTropicZone(tile, TROPICZONE_DESERT);
	}

	for (i = 0; i != 256; i++) {
		if ((i % 64) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		RunTileLoop();
	}

	for (tile = 0; tile != MapSize(); ++tile) {
		if ((tile % update_freq) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		for (data = _make_desert_or_rainforest_data;
				data != endof(_make_desert_or_rainforest_data); ++data) {
			TileIndex t = TILE_MASK(tile + ToTileIndexDiff(*data));
			if (IsTileType(t, MP_CLEAR) && IsClearGround(t, CLEAR_DESERT)) break;
		}
		if (data == endof(_make_desert_or_rainforest_data))
			SetTropicZone(tile, TROPICZONE_RAINFOREST);
	}
}

void GenerateLandscape(byte mode)
{
	const int gwp_desert_amount = 4 + 8;
	uint i;
	uint flag;
	uint32 r;

	if (mode == GW_HEIGHTMAP) {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, (_opt.landscape == LT_TROPIC) ? 1 + gwp_desert_amount : 1);
		LoadHeightmap(_file_to_saveload.name);
		IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);
	} else if (_patches.land_generator == LG_TERRAGENESIS) {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, (_opt.landscape == LT_TROPIC) ? 3 + gwp_desert_amount : 3);
		GenerateTerrainPerlin();
	} else {
		switch (_opt.landscape) {
			case LT_ARCTIC:
				SetGeneratingWorldProgress(GWP_LANDSCAPE, 2);

				for (i = ScaleByMapSize((Random() & 0x7F) + 950); i != 0; --i) {
					GenerateTerrain(2, 0);
				}
				IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

				r = Random();
				flag = GB(r, 0, 2) | 4;
				for (i = ScaleByMapSize(GB(r, 16, 7) + 450); i != 0; --i) {
					GenerateTerrain(4, flag);
				}
				IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);
				break;

			case LT_TROPIC:
				SetGeneratingWorldProgress(GWP_LANDSCAPE, 3 + gwp_desert_amount);

				for (i = ScaleByMapSize((Random() & 0x7F) + 170); i != 0; --i) {
					GenerateTerrain(0, 0);
				}
				IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

				r = Random();
				flag = GB(r, 0, 2) | 4;
				for (i = ScaleByMapSize(GB(r, 16, 8) + 1700); i != 0; --i) {
					GenerateTerrain(0, flag);
				}
				IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

				flag ^= 2;

				for (i = ScaleByMapSize((Random() & 0x7F) + 410); i != 0; --i) {
					GenerateTerrain(3, flag);
				}
				IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);
				break;

			default:
				SetGeneratingWorldProgress(GWP_LANDSCAPE, 1);

				i = ScaleByMapSize((Random() & 0x7F) + (3 - _opt.diff.quantity_sea_lakes) * 256 + 100);
				for (; i != 0; --i) {
					GenerateTerrain(_opt.diff.terrain_type, 0);
				}
				IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);
				break;
		}
	}

	ConvertGroundTilesIntoWaterTiles();

	if (_opt.landscape == LT_TROPIC) CreateDesertOrRainForest();
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

	return TILE_MASK(TileXY(
		TileX(a) + (GB(r, 0, 8) * rn * 2 >> 8) - rn,
		TileY(a) + (GB(r, 8, 8) * rn * 2 >> 8) - rn
	));
}

bool IsValidTile(TileIndex tile)
{
	return (tile < MapSizeX() * MapMaxY() && TileX(tile) != MapMaxX());
}
