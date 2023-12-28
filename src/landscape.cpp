/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file landscape.cpp Functions related to the landscape (slopes etc.). */

/** @defgroup SnowLineGroup Snowline functions and data structures */

#include "stdafx.h"
#include "heightmap.h"
#include "clear_map.h"
#include "spritecache.h"
#include "viewport_func.h"
#include "command_func.h"
#include "landscape.h"
#include "void_map.h"
#include "tgp.h"
#include "genworld.h"
#include "fios.h"
#include "error_func.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_tick.h"
#include "water.h"
#include "effectvehicle_func.h"
#include "landscape_type.h"
#include "animated_tile_func.h"
#include "core/random_func.hpp"
#include "object_base.h"
#include "company_func.h"
#include "pathfinder/npf/aystar.h"
#include "saveload/saveload.h"
#include "framerate_type.h"
#include "landscape_cmd.h"
#include "terraform_cmd.h"
#include "station_func.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

extern const TileTypeProcs
	_tile_type_clear_procs,
	_tile_type_rail_procs,
	_tile_type_road_procs,
	_tile_type_town_procs,
	_tile_type_trees_procs,
	_tile_type_station_procs,
	_tile_type_water_procs,
	_tile_type_void_procs,
	_tile_type_industry_procs,
	_tile_type_tunnelbridge_procs,
	_tile_type_object_procs;

/**
 * Tile callback functions for each type of tile.
 * @ingroup TileCallbackGroup
 * @see TileType
 */
const TileTypeProcs * const _tile_type_procs[16] = {
	&_tile_type_clear_procs,        ///< Callback functions for MP_CLEAR tiles
	&_tile_type_rail_procs,         ///< Callback functions for MP_RAILWAY tiles
	&_tile_type_road_procs,         ///< Callback functions for MP_ROAD tiles
	&_tile_type_town_procs,         ///< Callback functions for MP_HOUSE tiles
	&_tile_type_trees_procs,        ///< Callback functions for MP_TREES tiles
	&_tile_type_station_procs,      ///< Callback functions for MP_STATION tiles
	&_tile_type_water_procs,        ///< Callback functions for MP_WATER tiles
	&_tile_type_void_procs,         ///< Callback functions for MP_VOID tiles
	&_tile_type_industry_procs,     ///< Callback functions for MP_INDUSTRY tiles
	&_tile_type_tunnelbridge_procs, ///< Callback functions for MP_TUNNELBRIDGE tiles
	&_tile_type_object_procs,       ///< Callback functions for MP_OBJECT tiles
};

/** landscape slope => sprite */
extern const byte _slope_to_sprite_offset[32] = {
	0, 1, 2, 3, 4, 5, 6,  7, 8, 9, 10, 11, 12, 13, 14, 0,
	0, 0, 0, 0, 0, 0, 0, 16, 0, 0,  0, 17,  0, 15, 18, 0,
};

/**
 * Description of the snow line throughout the year.
 *
 * If it is \c nullptr, a static snowline height is used, as set by \c _settings_game.game_creation.snow_line_height.
 * Otherwise it points to a table loaded from a newGRF file that describes the variable snowline.
 * @ingroup SnowLineGroup
 * @see GetSnowLine() GameCreationSettings
 */
static SnowLine *_snow_line = nullptr;

/**
 * Map 2D viewport or smallmap coordinate to 3D world or tile coordinate.
 * Function takes into account height of tiles and foundations.
 *
 * @param x X viewport 2D coordinate.
 * @param y Y viewport 2D coordinate.
 * @param clamp_to_map Clamp the coordinate outside of the map to the closest, non-void tile within the map.
 * @param[out] clamped Whether coordinates were clamped.
 * @return 3D world coordinate of point visible at the given screen coordinate (3D perspective).
 *
 * @note Inverse of #RemapCoords2 function. Smaller values may get rounded.
 * @see InverseRemapCoords
 */
Point InverseRemapCoords2(int x, int y, bool clamp_to_map, bool *clamped)
{
	if (clamped != nullptr) *clamped = false; // Not clamping yet.

	/* Initial x/y world coordinate is like if the landscape
	 * was completely flat on height 0. */
	Point pt = InverseRemapCoords(x, y);

	const uint min_coord = _settings_game.construction.freeform_edges ? TILE_SIZE : 0;
	const uint max_x = Map::MaxX() * TILE_SIZE - 1;
	const uint max_y = Map::MaxY() * TILE_SIZE - 1;

	if (clamp_to_map) {
		/* Bring the coordinates near to a valid range. At the top we allow a number
		 * of extra tiles. This is mostly due to the tiles on the north side of
		 * the map possibly being drawn higher due to the extra height levels. */
		int extra_tiles = CeilDiv(_settings_game.construction.map_height_limit * TILE_HEIGHT, TILE_PIXELS);
		Point old_pt = pt;
		pt.x = Clamp(pt.x, -extra_tiles * TILE_SIZE, max_x);
		pt.y = Clamp(pt.y, -extra_tiles * TILE_SIZE, max_y);
		if (clamped != nullptr) *clamped = (pt.x != old_pt.x) || (pt.y != old_pt.y);
	}

	/* Now find the Z-world coordinate by fix point iteration.
	 * This is a bit tricky because the tile height is non-continuous at foundations.
	 * The clicked point should be approached from the back, otherwise there are regions that are not clickable.
	 * (FOUNDATION_HALFTILE_LOWER on SLOPE_STEEP_S hides north halftile completely)
	 * So give it a z-malus of 4 in the first iterations. */
	int z = 0;
	if (clamp_to_map) {
		for (int i = 0; i < 5; i++) z = GetSlopePixelZ(Clamp(pt.x + std::max(z, 4) - 4, min_coord, max_x), Clamp(pt.y + std::max(z, 4) - 4, min_coord, max_y)) / 2;
		for (int m = 3; m > 0; m--) z = GetSlopePixelZ(Clamp(pt.x + std::max(z, m) - m, min_coord, max_x), Clamp(pt.y + std::max(z, m) - m, min_coord, max_y)) / 2;
		for (int i = 0; i < 5; i++) z = GetSlopePixelZ(Clamp(pt.x + z,             min_coord, max_x), Clamp(pt.y + z,             min_coord, max_y)) / 2;
	} else {
		for (int i = 0; i < 5; i++) z = GetSlopePixelZOutsideMap(pt.x + std::max(z, 4) - 4, pt.y + std::max(z, 4) - 4) / 2;
		for (int m = 3; m > 0; m--) z = GetSlopePixelZOutsideMap(pt.x + std::max(z, m) - m, pt.y + std::max(z, m) - m) / 2;
		for (int i = 0; i < 5; i++) z = GetSlopePixelZOutsideMap(pt.x + z,             pt.y + z            ) / 2;
	}

	pt.x += z;
	pt.y += z;
	if (clamp_to_map) {
		Point old_pt = pt;
		pt.x = Clamp(pt.x, min_coord, max_x);
		pt.y = Clamp(pt.y, min_coord, max_y);
		if (clamped != nullptr) *clamped = *clamped || (pt.x != old_pt.x) || (pt.y != old_pt.y);
	}

	return pt;
}

/**
 * Applies a foundation to a slope.
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
		uint dz = 1 + (IsSteepSlope(*s) ? 1 : 0);
		*s = SLOPE_FLAT;
		return dz;
	}

	if (f != FOUNDATION_STEEP_BOTH && IsNonContinuousFoundation(f)) {
		*s = HalftileSlope(*s, GetHalftileFoundationCorner(f));
		return 0;
	}

	if (IsSpecialRailFoundation(f)) {
		*s = SlopeWithThreeCornersRaised(OppositeCorner(GetRailFoundationCorner(f)));
		return 0;
	}

	uint dz = IsSteepSlope(*s) ? 1 : 0;
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


/**
 * Determines height at given coordinate of a slope.
 *
 * At the northern corner (0, 0) the result is always a multiple of TILE_HEIGHT.
 * When the height is a fractional Z, then the height is rounded down. For example,
 * when at the height is 0 at x = 0 and the height is 8 at x = 16 (actually x = 0
 * of the next tile), then height is 0 at x = 1, 1 at x = 2, and 7 at x = 15.
 * @param x x coordinate (value from 0 to 15)
 * @param y y coordinate (value from 0 to 15)
 * @param corners slope to examine
 * @return height of given point of given slope
 */
uint GetPartialPixelZ(int x, int y, Slope corners)
{
	if (IsHalftileSlope(corners)) {
		/* A foundation is placed on half the tile at a specific corner. This means that,
		 * depending on the corner, that one half of the tile is at the maximum height. */
		switch (GetHalftileSlopeCorner(corners)) {
			case CORNER_W:
				if (x > y) return GetSlopeMaxPixelZ(corners);
				break;

			case CORNER_S:
				if (x + y >= (int)TILE_SIZE) return GetSlopeMaxPixelZ(corners);
				break;

			case CORNER_E:
				if (x <= y) return GetSlopeMaxPixelZ(corners);
				break;

			case CORNER_N:
				if (x + y < (int)TILE_SIZE) return GetSlopeMaxPixelZ(corners);
				break;

			default: NOT_REACHED();
		}
	}

	switch (RemoveHalftileSlope(corners)) {
		case SLOPE_FLAT: return 0;

		/* One corner is up.*/
		case SLOPE_N: return x + y <= (int)TILE_SIZE ? (TILE_SIZE - x - y)     >> 1 : 0;
		case SLOPE_E: return y >= x                  ? (1 + y - x)             >> 1 : 0;
		case SLOPE_S: return x + y >= (int)TILE_SIZE ? (1 + x + y - TILE_SIZE) >> 1 : 0;
		case SLOPE_W: return x >= y                  ? (x - y)                 >> 1 : 0;

		/* Two corners next to eachother are up. */
		case SLOPE_NE: return (TILE_SIZE - x) >> 1;
		case SLOPE_SE: return (y + 1) >> 1;
		case SLOPE_SW: return (x + 1) >> 1;
		case SLOPE_NW: return (TILE_SIZE - y) >> 1;

		/* Three corners are up on the same level. */
		case SLOPE_ENW: return x + y >= (int)TILE_SIZE ? TILE_HEIGHT - ((1 + x + y - TILE_SIZE) >> 1) : TILE_HEIGHT;
		case SLOPE_SEN: return y < x                   ? TILE_HEIGHT - ((x - y)                 >> 1) : TILE_HEIGHT;
		case SLOPE_WSE: return x + y <= (int)TILE_SIZE ? TILE_HEIGHT - ((TILE_SIZE - x - y)     >> 1) : TILE_HEIGHT;
		case SLOPE_NWS: return x < y                   ? TILE_HEIGHT - ((1 + y - x)             >> 1) : TILE_HEIGHT;

		/* Two corners at opposite sides are up. */
		case SLOPE_NS: return x + y < (int)TILE_SIZE ? (TILE_SIZE - x - y) >> 1 : (1 + x + y - TILE_SIZE) >> 1;
		case SLOPE_EW: return x >= y ? (x - y) >> 1 : (1 + y - x) >> 1;

		/* Very special cases. */
		case SLOPE_ELEVATED: return TILE_HEIGHT;

		/* Steep slopes. The top is at 2 * TILE_HEIGHT. */
		case SLOPE_STEEP_N: return (TILE_SIZE - x + TILE_SIZE - y) >> 1;
		case SLOPE_STEEP_E: return (TILE_SIZE + 1 + y - x) >> 1;
		case SLOPE_STEEP_S: return (1 + x + y) >> 1;
		case SLOPE_STEEP_W: return (TILE_SIZE + x - y) >> 1;

		default: NOT_REACHED();
	}
}

/**
 * Return world \c Z coordinate of a given point of a tile. Normally this is the
 * Z of the ground/foundation at the given location, but in some cases the
 * ground/foundation can differ from the Z coordinate that the (ground) vehicle
 * passing over it would take. For example when entering a tunnel or bridge.
 *
 * @param x World X coordinate in tile "units".
 * @param y World Y coordinate in tile "units".
 * @param ground_vehicle Whether to get the Z coordinate of the ground vehicle, or the ground.
 * @return World Z coordinate at tile ground (vehicle) level, including slopes and foundations.
 */
int GetSlopePixelZ(int x, int y, bool ground_vehicle)
{
	TileIndex tile = TileVirtXY(x, y);

	return _tile_type_procs[GetTileType(tile)]->get_slope_z_proc(tile, x, y, ground_vehicle);
}

/**
 * Return world \c z coordinate of a given point of a tile,
 * also for tiles outside the map (virtual "black" tiles).
 *
 * @param x World X coordinate in tile "units", may be outside the map.
 * @param y World Y coordinate in tile "units", may be outside the map.
 * @return World Z coordinate at tile ground level, including slopes and foundations.
 */
int GetSlopePixelZOutsideMap(int x, int y)
{
	if (IsInsideBS(x, 0, Map::SizeX() * TILE_SIZE) && IsInsideBS(y, 0, Map::SizeY() * TILE_SIZE)) {
		return GetSlopePixelZ(x, y, false);
	} else {
		return _tile_type_procs[MP_VOID]->get_slope_z_proc(INVALID_TILE, x, y, false);
	}
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
	return ((tileh & SlopeWithOneCornerRaised(corner)) != 0 ? 1 : 0) + (tileh == SteepSlope(corner) ? 1 : 0);
}

/**
 * Determine the Z height of the corners of a specific tile edge
 *
 * @note If a tile has a non-continuous halftile foundation, a corner can have different heights wrt. its edges.
 *
 * @pre z1 and z2 must be initialized (typ. with TileZ). The corner heights just get added.
 *
 * @param tileh The slope of the tile.
 * @param edge The edge of interest.
 * @param z1 Gets incremented by the height of the first corner of the edge. (near corner wrt. the camera)
 * @param z2 Gets incremented by the height of the second corner of the edge. (far corner wrt. the camera)
 */
void GetSlopePixelZOnEdge(Slope tileh, DiagDirection edge, int *z1, int *z2)
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
	if (RemoveHalftileSlope(tileh) == corners[edge][2]) *z1 += TILE_HEIGHT; // z1 is highest corner of a steep slope
	if (RemoveHalftileSlope(tileh) == corners[edge][3]) *z2 += TILE_HEIGHT; // z2 is highest corner of a steep slope
}

/**
 * Get slope of a tile on top of a (possible) foundation
 * If a tile does not have a foundation, the function returns the same as GetTileSlope.
 *
 * @param tile The tile of interest.
 * @param z returns the z of the foundation slope. (Can be nullptr, if not needed)
 * @return The slope on top of the foundation.
 */
Slope GetFoundationSlope(TileIndex tile, int *z)
{
	Slope tileh = GetTileSlope(tile, z);
	Foundation f = _tile_type_procs[GetTileType(tile)]->get_foundation_proc(tile, tileh);
	uint z_inc = ApplyFoundationToSlope(f, &tileh);
	if (z != nullptr) *z += z_inc;
	return tileh;
}


bool HasFoundationNW(TileIndex tile, Slope slope_here, uint z_here)
{
	int z;

	int z_W_here = z_here;
	int z_N_here = z_here;
	GetSlopePixelZOnEdge(slope_here, DIAGDIR_NW, &z_W_here, &z_N_here);

	Slope slope = GetFoundationPixelSlope(TILE_ADDXY(tile, 0, -1), &z);
	int z_W = z;
	int z_N = z;
	GetSlopePixelZOnEdge(slope, DIAGDIR_SE, &z_W, &z_N);

	return (z_N_here > z_N) || (z_W_here > z_W);
}


bool HasFoundationNE(TileIndex tile, Slope slope_here, uint z_here)
{
	int z;

	int z_E_here = z_here;
	int z_N_here = z_here;
	GetSlopePixelZOnEdge(slope_here, DIAGDIR_NE, &z_E_here, &z_N_here);

	Slope slope = GetFoundationPixelSlope(TILE_ADDXY(tile, -1, 0), &z);
	int z_E = z;
	int z_N = z;
	GetSlopePixelZOnEdge(slope, DIAGDIR_SW, &z_E, &z_N);

	return (z_N_here > z_N) || (z_E_here > z_E);
}

/**
 * Draw foundation \a f at tile \a ti. Updates \a ti.
 * @param ti Tile to draw foundation on
 * @param f  Foundation to draw
 */
void DrawFoundation(TileInfo *ti, Foundation f)
{
	if (!IsFoundation(f)) return;

	/* Two part foundations must be drawn separately */
	assert(f != FOUNDATION_STEEP_BOTH);

	uint sprite_block = 0;
	int z;
	Slope slope = GetFoundationPixelSlope(ti->tile, &z);

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
				leveled_base + (ti->tileh & ~SLOPE_STEEP), PAL_NONE, ti->x, ti->y, TILE_SIZE, TILE_SIZE, TILE_HEIGHT - 1, ti->z
			);
		}

		Corner highest_corner = GetHighestSlopeCorner(ti->tileh);
		ti->z += ApplyPixelFoundationToSlope(f, &ti->tileh);

		if (IsInclinedFoundation(f)) {
			/* inclined foundation */
			byte inclined = highest_corner * 2 + (f == FOUNDATION_INCLINED_Y ? 1 : 0);

			AddSortableSpriteToDraw(inclined_base + inclined, PAL_NONE, ti->x, ti->y,
				f == FOUNDATION_INCLINED_X ? TILE_SIZE : 1,
				f == FOUNDATION_INCLINED_Y ? TILE_SIZE : 1,
				TILE_HEIGHT, ti->z
			);
			OffsetGroundSprite(0, 0);
		} else if (IsLeveledFoundation(f)) {
			AddSortableSpriteToDraw(leveled_base + SlopeWithOneCornerRaised(highest_corner), PAL_NONE, ti->x, ti->y, TILE_SIZE, TILE_SIZE, TILE_HEIGHT - 1, ti->z - TILE_HEIGHT);
			OffsetGroundSprite(0, -(int)TILE_HEIGHT);
		} else if (f == FOUNDATION_STEEP_LOWER) {
			/* one corner raised */
			OffsetGroundSprite(0, -(int)TILE_HEIGHT);
		} else {
			/* halftile foundation */
			int x_bb = (((highest_corner == CORNER_W) || (highest_corner == CORNER_S)) ? TILE_SIZE / 2 : 0);
			int y_bb = (((highest_corner == CORNER_S) || (highest_corner == CORNER_E)) ? TILE_SIZE / 2 : 0);

			AddSortableSpriteToDraw(halftile_base + highest_corner, PAL_NONE, ti->x + x_bb, ti->y + y_bb, TILE_SIZE / 2, TILE_SIZE / 2, TILE_HEIGHT - 1, ti->z + TILE_HEIGHT);
			/* Reposition ground sprite back to original position after bounding box change above. This is similar to
			 * RemapCoords() but without zoom scaling. */
			Point pt = {(y_bb - x_bb) * 2, y_bb + x_bb};
			OffsetGroundSprite(-pt.x, -pt.y);
		}
	} else {
		if (IsLeveledFoundation(f)) {
			/* leveled foundation */
			AddSortableSpriteToDraw(leveled_base + ti->tileh, PAL_NONE, ti->x, ti->y, TILE_SIZE, TILE_SIZE, TILE_HEIGHT - 1, ti->z);
			OffsetGroundSprite(0, -(int)TILE_HEIGHT);
		} else if (IsNonContinuousFoundation(f)) {
			/* halftile foundation */
			Corner halftile_corner = GetHalftileFoundationCorner(f);
			int x_bb = (((halftile_corner == CORNER_W) || (halftile_corner == CORNER_S)) ? TILE_SIZE / 2 : 0);
			int y_bb = (((halftile_corner == CORNER_S) || (halftile_corner == CORNER_E)) ? TILE_SIZE / 2 : 0);

			AddSortableSpriteToDraw(halftile_base + halftile_corner, PAL_NONE, ti->x + x_bb, ti->y + y_bb, TILE_SIZE / 2, TILE_SIZE / 2, TILE_HEIGHT - 1, ti->z);
			/* Reposition ground sprite back to original position after bounding box change above. This is similar to
			 * RemapCoords() but without zoom scaling. */
			Point pt = {(y_bb - x_bb) * 2, y_bb + x_bb};
			OffsetGroundSprite(-pt.x, -pt.y);
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
			AddSortableSpriteToDraw(spr, PAL_NONE, ti->x, ti->y, TILE_SIZE, TILE_SIZE, TILE_HEIGHT - 1, ti->z);
			OffsetGroundSprite(0, 0);
		} else {
			/* inclined foundation */
			byte inclined = GetHighestSlopeCorner(ti->tileh) * 2 + (f == FOUNDATION_INCLINED_Y ? 1 : 0);

			AddSortableSpriteToDraw(inclined_base + inclined, PAL_NONE, ti->x, ti->y,
				f == FOUNDATION_INCLINED_X ? TILE_SIZE : 1,
				f == FOUNDATION_INCLINED_Y ? TILE_SIZE : 1,
				TILE_HEIGHT, ti->z
			);
			OffsetGroundSprite(0, 0);
		}
		ti->z += ApplyPixelFoundationToSlope(f, &ti->tileh);
	}
}

void DoClearSquare(TileIndex tile)
{
	/* If the tile can have animation and we clear it, delete it from the animated tile list. */
	if (_tile_type_procs[GetTileType(tile)]->animate_tile_proc != nullptr) DeleteAnimatedTile(tile);

	bool remove = IsDockingTile(tile);
	MakeClear(tile, CLEAR_GRASS, _generating_world ? 3 : 0);
	MarkTileDirtyByTile(tile);
	if (remove) RemoveDockingTile(tile);
}

/**
 * Returns information about trackdirs and signal states.
 * If there is any trackbit at 'side', return all trackdirbits.
 * For TRANSPORT_ROAD, return no trackbits if there is no roadbit (of given subtype) at given side.
 * @param tile tile to get info about
 * @param mode transport type
 * @param sub_mode for TRANSPORT_ROAD, roadtypes to check
 * @param side side we are entering from, INVALID_DIAGDIR to return all trackbits
 * @return trackdirbits and other info depending on 'mode'
 */
TrackStatus GetTileTrackStatus(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	return _tile_type_procs[GetTileType(tile)]->get_tile_track_status_proc(tile, mode, sub_mode, side);
}

/**
 * Change the owner of a tile
 * @param tile      Tile to change
 * @param old_owner Current owner of the tile
 * @param new_owner New owner of the tile
 */
void ChangeTileOwner(TileIndex tile, Owner old_owner, Owner new_owner)
{
	_tile_type_procs[GetTileType(tile)]->change_tile_owner_proc(tile, old_owner, new_owner);
}

void GetTileDesc(TileIndex tile, TileDesc *td)
{
	_tile_type_procs[GetTileType(tile)]->get_tile_desc_proc(tile, td);
}

/**
 * Has a snow line table already been loaded.
 * @return true if the table has been loaded already.
 * @ingroup SnowLineGroup
 */
bool IsSnowLineSet()
{
	return _snow_line != nullptr;
}

/**
 * Set a variable snow line, as loaded from a newgrf file.
 * @param table the 12 * 32 byte table containing the snowline for each day
 * @ingroup SnowLineGroup
 */
void SetSnowLine(byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS])
{
	_snow_line = CallocT<SnowLine>(1);
	_snow_line->lowest_value = 0xFF;
	memcpy(_snow_line->table, table, sizeof(_snow_line->table));

	for (uint i = 0; i < SNOW_LINE_MONTHS; i++) {
		for (uint j = 0; j < SNOW_LINE_DAYS; j++) {
			_snow_line->highest_value = std::max(_snow_line->highest_value, table[i][j]);
			_snow_line->lowest_value = std::min(_snow_line->lowest_value, table[i][j]);
		}
	}
}

/**
 * Get the current snow line, either variable or static.
 * @return the snow line height.
 * @ingroup SnowLineGroup
 */
byte GetSnowLine()
{
	if (_snow_line == nullptr) return _settings_game.game_creation.snow_line_height;

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	return _snow_line->table[ymd.month][ymd.day];
}

/**
 * Get the highest possible snow line height, either variable or static.
 * @return the highest snow line height.
 * @ingroup SnowLineGroup
 */
byte HighestSnowLine()
{
	return _snow_line == nullptr ? _settings_game.game_creation.snow_line_height : _snow_line->highest_value;
}

/**
 * Get the lowest possible snow line height, either variable or static.
 * @return the lowest snow line height.
 * @ingroup SnowLineGroup
 */
byte LowestSnowLine()
{
	return _snow_line == nullptr ? _settings_game.game_creation.snow_line_height : _snow_line->lowest_value;
}

/**
 * Clear the variable snow line table and free the memory.
 * @ingroup SnowLineGroup
 */
void ClearSnowLine()
{
	free(_snow_line);
	_snow_line = nullptr;
}

/**
 * Clear a piece of landscape
 * @param flags of operation to conduct
 * @param tile tile to clear
 * @return the cost of this operation or an error
 */
CommandCost CmdLandscapeClear(DoCommandFlag flags, TileIndex tile)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);
	bool do_clear = false;
	/* Test for stuff which results in water when cleared. Then add the cost to also clear the water. */
	if ((flags & DC_FORCE_CLEAR_TILE) && HasTileWaterClass(tile) && IsTileOnWater(tile) && !IsWaterTile(tile) && !IsCoastTile(tile)) {
		if ((flags & DC_AUTO) && GetWaterClass(tile) == WATER_CLASS_CANAL) return_cmd_error(STR_ERROR_MUST_DEMOLISH_CANAL_FIRST);
		do_clear = true;
		cost.AddCost(GetWaterClass(tile) == WATER_CLASS_CANAL ? _price[PR_CLEAR_CANAL] : _price[PR_CLEAR_WATER]);
	}

	Company *c = (flags & (DC_AUTO | DC_BANKRUPT)) ? nullptr : Company::GetIfValid(_current_company);
	if (c != nullptr && (int)GB(c->clear_limit, 16, 16) < 1) {
		return_cmd_error(STR_ERROR_CLEARING_LIMIT_REACHED);
	}

	const ClearedObjectArea *coa = FindClearedObject(tile);

	/* If this tile was the first tile which caused object destruction, always
	 * pass it on to the tile_type_proc. That way multiple test runs and the exec run stay consistent. */
	if (coa != nullptr && coa->first_tile != tile) {
		/* If this tile belongs to an object which was already cleared via another tile, pretend it has been
		 * already removed.
		 * However, we need to check stuff, which is not the same for all object tiles. (e.g. being on water or not) */

		/* If a object is removed, it leaves either bare land or water. */
		if ((flags & DC_NO_WATER) && HasTileWaterClass(tile) && IsTileOnWater(tile)) {
			return_cmd_error(STR_ERROR_CAN_T_BUILD_ON_WATER);
		}
	} else {
		cost.AddCost(_tile_type_procs[GetTileType(tile)]->clear_tile_proc(tile, flags));
	}

	if (flags & DC_EXEC) {
		if (c != nullptr) c->clear_limit -= 1 << 16;
		if (do_clear) DoClearSquare(tile);
	}
	return cost;
}

/**
 * Clear a big piece of landscape
 * @param flags of operation to conduct
 * @param tile end tile of area dragging
 * @param start_tile start tile of area dragging
 * @param diagonal Whether to use the Orthogonal (false) or Diagonal (true) iterator.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, Money> CmdClearArea(DoCommandFlag flags, TileIndex tile, TileIndex start_tile, bool diagonal)
{
	if (start_tile >= Map::Size()) return { CMD_ERROR, 0 };

	Money money = GetAvailableMoneyForCommand();
	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost last_error = CMD_ERROR;
	bool had_success = false;

	const Company *c = (flags & (DC_AUTO | DC_BANKRUPT)) ? nullptr : Company::GetIfValid(_current_company);
	int limit = (c == nullptr ? INT32_MAX : GB(c->clear_limit, 16, 16));

	std::unique_ptr<TileIterator> iter = TileIterator::Create(tile, start_tile, diagonal);
	for (; *iter != INVALID_TILE; ++(*iter)) {
		TileIndex t = *iter;
		CommandCost ret = Command<CMD_LANDSCAPE_CLEAR>::Do(flags & ~DC_EXEC, t);
		if (ret.Failed()) {
			last_error = ret;

			/* We may not clear more tiles. */
			if (c != nullptr && GB(c->clear_limit, 16, 16) < 1) break;
			continue;
		}

		had_success = true;
		if (flags & DC_EXEC) {
			money -= ret.GetCost();
			if (ret.GetCost() > 0 && money < 0) {
				return { cost, ret.GetCost() };
			}
			Command<CMD_LANDSCAPE_CLEAR>::Do(flags, t);

			/* draw explosion animation...
			 * Disable explosions when game is paused. Looks silly and blocks the view. */
			if ((t == tile || t == start_tile) && _pause_mode == PM_UNPAUSED) {
				/* big explosion in two corners, or small explosion for single tiles */
				CreateEffectVehicleAbove(TileX(t) * TILE_SIZE + TILE_SIZE / 2, TileY(t) * TILE_SIZE + TILE_SIZE / 2, 2,
					TileX(tile) == TileX(start_tile) && TileY(tile) == TileY(start_tile) ? EV_EXPLOSION_SMALL : EV_EXPLOSION_LARGE
				);
			}
		} else {
			/* When we're at the clearing limit we better bail (unneed) testing as well. */
			if (ret.GetCost() != 0 && --limit <= 0) break;
		}
		cost.AddCost(ret);
	}

	return { had_success ? cost : last_error, 0 };
}


TileIndex _cur_tileloop_tile;

/**
 * Gradually iterate over all tiles on the map, calling their TileLoopProcs once every 256 ticks.
 */
void RunTileLoop()
{
	PerformanceAccumulator framerate(PFE_GL_LANDSCAPE);

	/* The pseudorandom sequence of tiles is generated using a Galois linear feedback
	 * shift register (LFSR). This allows a deterministic pseudorandom ordering, but
	 * still with minimal state and fast iteration. */

	/* Maximal length LFSR feedback terms, from 12-bit (for 64x64 maps) to 24-bit (for 4096x4096 maps).
	 * Extracted from http://www.ece.cmu.edu/~koopman/lfsr/ */
	static const uint32_t feedbacks[] = {
		0xD8F, 0x1296, 0x2496, 0x4357, 0x8679, 0x1030E, 0x206CD, 0x403FE, 0x807B8, 0x1004B2, 0x2006A8, 0x4004B2, 0x800B87
	};
	static_assert(lengthof(feedbacks) == 2 * MAX_MAP_SIZE_BITS - 2 * MIN_MAP_SIZE_BITS + 1);
	const uint32_t feedback = feedbacks[Map::LogX() + Map::LogY() - 2 * MIN_MAP_SIZE_BITS];

	/* We update every tile every 256 ticks, so divide the map size by 2^8 = 256 */
	uint count = 1 << (Map::LogX() + Map::LogY() - 8);

	TileIndex tile = _cur_tileloop_tile;
	/* The LFSR cannot have a zeroed state. */
	assert(tile != 0);

	/* Manually update tile 0 every 256 ticks - the LFSR never iterates over it itself.  */
	if (TimerGameTick::counter % 256 == 0) {
		_tile_type_procs[GetTileType(0)]->tile_loop_proc(0);
		count--;
	}

	while (count--) {
		_tile_type_procs[GetTileType(tile)]->tile_loop_proc(tile);

		/* Get the next tile in sequence using a Galois LFSR. */
		tile = (tile.base() >> 1) ^ (-(int32_t)(tile.base() & 1) & feedback);
	}

	_cur_tileloop_tile = tile;
}

void InitializeLandscape()
{
	for (uint y = _settings_game.construction.freeform_edges ? 1 : 0; y < Map::MaxY(); y++) {
		for (uint x = _settings_game.construction.freeform_edges ? 1 : 0; x < Map::MaxX(); x++) {
			MakeClear(TileXY(x, y), CLEAR_GRASS, 3);
			SetTileHeight(TileXY(x, y), 0);
			SetTropicZone(TileXY(x, y), TROPICZONE_NORMAL);
			ClearBridgeMiddle(TileXY(x, y));
		}
	}

	for (uint x = 0; x < Map::SizeX(); x++) MakeVoid(TileXY(x, Map::MaxY()));
	for (uint y = 0; y < Map::SizeY(); y++) MakeVoid(TileXY(Map::MaxX(), y));
}

static const byte _genterrain_tbl_1[5] = { 10, 22, 33, 37, 4  };
static const byte _genterrain_tbl_2[5] = {  0,  0,  0,  0, 33 };

static void GenerateTerrain(int type, uint flag)
{
	uint32_t r = Random();

	/* Choose one of the templates from the graphics file. */
	const Sprite *templ = GetSprite((((r >> 24) * _genterrain_tbl_1[type]) >> 8) + _genterrain_tbl_2[type] + SPR_MAPGEN_BEGIN, SpriteType::MapGen);
	if (templ == nullptr) UserError("Map generator sprites could not be loaded");

	/* Chose a random location to apply the template to. */
	uint x = r & Map::MaxX();
	uint y = (r >> Map::LogX()) & Map::MaxY();

	/* Make sure the template is not too close to the upper edges; bottom edges are checked later. */
	uint edge_distance = 1 + (_settings_game.construction.freeform_edges ? 1 : 0);
	if (x <= edge_distance || y <= edge_distance) return;

	DiagDirection direction = (DiagDirection)GB(r, 22, 2);
	uint w = templ->width;
	uint h = templ->height;

	if (DiagDirToAxis(direction) == AXIS_Y) Swap(w, h);

	const byte *p = templ->data;

	if ((flag & 4) != 0) {
		/* This is only executed in secondary/tertiary loops to generate the terrain for arctic and tropic.
		 * It prevents the templates to be applied to certain parts of the map based on the flags, thus
		 * creating regions with different elevations/topography. */
		uint xw = x * Map::SizeY();
		uint yw = y * Map::SizeX();
		uint bias = (Map::SizeX() + Map::SizeY()) * 16;

		switch (flag & 3) {
			default: NOT_REACHED();
			case 0:
				if (xw + yw > Map::Size() - bias) return;
				break;

			case 1:
				if (yw < xw + bias) return;
				break;

			case 2:
				if (xw + yw < Map::Size() + bias) return;
				break;

			case 3:
				if (xw < yw + bias) return;
				break;
		}
	}

	/* Ensure the template does not overflow at the bottom edges of the map; upper edges were checked before. */
	if (x + w >= Map::MaxX()) return;
	if (y + h >= Map::MaxY()) return;

	TileIndex tile = TileXY(x, y);

	/* Get the template and overlay in a particular direction over the map's height from the given
	 * origin point (tile), and update the map's height everywhere where the height from the template
	 * is higher than the height of the map. In other words, this only raises the tile heights. */
	switch (direction) {
		default: NOT_REACHED();
		case DIAGDIR_NE:
			do {
				TileIndex tile_cur = tile;

				for (uint w_cur = w; w_cur != 0; --w_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur++;
				}
				tile += TileDiffXY(0, 1);
			} while (--h != 0);
			break;

		case DIAGDIR_SE:
			do {
				TileIndex tile_cur = tile;

				for (uint h_cur = h; h_cur != 0; --h_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur += TileDiffXY(0, 1);
				}
				tile += TileDiffXY(1, 0);
			} while (--w != 0);
			break;

		case DIAGDIR_SW:
			tile += TileDiffXY(w - 1, 0);
			do {
				TileIndex tile_cur = tile;

				for (uint w_cur = w; w_cur != 0; --w_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur--;
				}
				tile += TileDiffXY(0, 1);
			} while (--h != 0);
			break;

		case DIAGDIR_NW:
			tile += TileDiffXY(0, h - 1);
			do {
				TileIndex tile_cur = tile;

				for (uint h_cur = h; h_cur != 0; --h_cur) {
					if (GB(*p, 0, 4) >= TileHeight(tile_cur)) SetTileHeight(tile_cur, GB(*p, 0, 4));
					p++;
					tile_cur -= TileDiffXY(0, 1);
				}
				tile += TileDiffXY(1, 0);
			} while (--w != 0);
			break;
	}
}


#include "table/genland.h"

static void CreateDesertOrRainForest(uint desert_tropic_line)
{
	uint update_freq = Map::Size() / 4;
	const TileIndexDiffC *data;

	for (TileIndex tile = 0; tile != Map::Size(); ++tile) {
		if ((tile.base() % update_freq) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		if (!IsValidTile(tile)) continue;

		for (data = _make_desert_or_rainforest_data;
				data != endof(_make_desert_or_rainforest_data); ++data) {
			TileIndex t = AddTileIndexDiffCWrap(tile, *data);
			if (t != INVALID_TILE && (TileHeight(t) >= desert_tropic_line || IsTileType(t, MP_WATER))) break;
		}
		if (data == endof(_make_desert_or_rainforest_data)) {
			SetTropicZone(tile, TROPICZONE_DESERT);
		}
	}

	for (uint i = 0; i != 256; i++) {
		if ((i % 64) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		RunTileLoop();
	}

	for (TileIndex tile = 0; tile != Map::Size(); ++tile) {
		if ((tile.base() % update_freq) == 0) IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

		if (!IsValidTile(tile)) continue;

		for (data = _make_desert_or_rainforest_data;
				data != endof(_make_desert_or_rainforest_data); ++data) {
			TileIndex t = AddTileIndexDiffCWrap(tile, *data);
			if (t != INVALID_TILE && IsTileType(t, MP_CLEAR) && IsClearGround(t, CLEAR_DESERT)) break;
		}
		if (data == endof(_make_desert_or_rainforest_data)) {
			SetTropicZone(tile, TROPICZONE_RAINFOREST);
		}
	}
}

/**
 * Find the spring of a river.
 * @param tile The tile to consider for being the spring.
 * @return True iff it is suitable as a spring.
 */
static bool FindSpring(TileIndex tile, void *)
{
	int referenceHeight;
	if (!IsTileFlat(tile, &referenceHeight) || IsWaterTile(tile)) return false;

	/* In the tropics rivers start in the rainforest. */
	if (_settings_game.game_creation.landscape == LT_TROPIC && GetTropicZone(tile) != TROPICZONE_RAINFOREST) return false;

	/* Are there enough higher tiles to warrant a 'spring'? */
	uint num = 0;
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			TileIndex t = TileAddWrap(tile, dx, dy);
			if (t != INVALID_TILE && GetTileMaxZ(t) > referenceHeight) num++;
		}
	}

	if (num < 4) return false;

	/* Are we near the top of a hill? */
	for (int dx = -16; dx <= 16; dx++) {
		for (int dy = -16; dy <= 16; dy++) {
			TileIndex t = TileAddWrap(tile, dx, dy);
			if (t != INVALID_TILE && GetTileMaxZ(t) > referenceHeight + 2) return false;
		}
	}

	return true;
}

/**
 * Make a connected lake; fill all tiles in the circular tile search that are connected.
 * @param tile The tile to consider for lake making.
 * @param user_data The height of the lake.
 * @return Always false, so it continues searching.
 */
static bool MakeLake(TileIndex tile, void *user_data)
{
	uint height = *(uint*)user_data;
	if (!IsValidTile(tile) || TileHeight(tile) != height || !IsTileFlat(tile)) return false;
	if (_settings_game.game_creation.landscape == LT_TROPIC && GetTropicZone(tile) == TROPICZONE_DESERT) return false;

	for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; d++) {
		TileIndex t2 = tile + TileOffsByDiagDir(d);
		if (IsWaterTile(t2)) {
			MakeRiverAndModifyDesertZoneAround(tile);
			return false;
		}
	}

	return false;
}

/**
 * Widen a river by expanding into adjacent tiles via circular tile search.
 * @param tile The tile to try expanding the river into.
 * @param data The tile to try surrounding the river around.
 * @return Always false, so it continues searching.
 */
static bool RiverMakeWider(TileIndex tile, void *data)
{
	/* Don't expand into void tiles. */
	if (!IsValidTile(tile)) return false;

	/* If the tile is already sea or river, don't expand. */
	if (IsWaterTile(tile)) return false;

	/* If the tile is at height 0 after terraforming but the ocean hasn't flooded yet, don't build river. */
	if (GetTileMaxZ(tile) == 0) return false;

	TileIndex origin_tile = *(TileIndex *)data;
	Slope cur_slope = GetTileSlope(tile);
	Slope desired_slope = GetTileSlope(origin_tile); // Initialize matching the origin tile as a shortcut if no terraforming is needed.

	/* Never flow uphill. */
	if (GetTileMaxZ(tile) > GetTileMaxZ(origin_tile)) return false;

	/* If the new tile can't hold a river tile, try terraforming. */
	if (!IsTileFlat(tile) && !IsInclinedSlope(cur_slope)) {
		/* Don't try to terraform steep slopes. */
		if (IsSteepSlope(cur_slope)) return false;

		bool flat_river_found = false;
		bool sloped_river_found = false;

		/* There are two common possibilities:
		 * 1. River flat, adjacent tile has one corner lowered.
		 * 2. River descending, adjacent tile has either one or three corners raised.
		 */

		/* First, determine the desired slope based on adjacent river tiles. This doesn't necessarily match the origin tile for the CircularTileSearch. */
		for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; d++) {
			TileIndex other_tile = TileAddByDiagDir(tile, d);
			Slope other_slope = GetTileSlope(other_tile);

			/* Only consider river tiles. */
			if (IsWaterTile(other_tile) && IsRiver(other_tile)) {
				/* If the adjacent river tile flows downhill, we need to check where we are relative to the slope. */
				if (IsInclinedSlope(other_slope) && GetTileMaxZ(tile) == GetTileMaxZ(other_tile)) {
					/* Check for a parallel slope. If we don't find one, we're above or below the slope instead. */
					if (GetInclinedSlopeDirection(other_slope) == ChangeDiagDir(d, DIAGDIRDIFF_90RIGHT) ||
							GetInclinedSlopeDirection(other_slope) == ChangeDiagDir(d, DIAGDIRDIFF_90LEFT)) {
						desired_slope = other_slope;
						sloped_river_found = true;
						break;
					}
				}
				/* If we find an adjacent river tile, remember it. We'll terraform to match it later if we don't find a slope. */
				if (IsTileFlat(other_tile)) flat_river_found = true;
			}
		}
		/* We didn't find either an inclined or flat river, so we're climbing the wrong slope. Bail out. */
		if (!sloped_river_found && !flat_river_found) return false;

		/* We didn't find an inclined river, but there is a flat river. */
		if (!sloped_river_found && flat_river_found) desired_slope = SLOPE_FLAT;

		/* Now that we know the desired slope, it's time to terraform! */

		/* If the river is flat and the adjacent tile has one corner lowered, we want to raise it. */
		if (desired_slope == SLOPE_FLAT && IsSlopeWithThreeCornersRaised(cur_slope)) {
			/* Make sure we're not affecting an existing river slope tile. */
			for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; d++) {
				TileIndex other_tile = TileAddByDiagDir(tile, d);
				if (IsInclinedSlope(GetTileSlope(other_tile)) && IsWaterTile(other_tile)) return false;
			}
			Command<CMD_TERRAFORM_LAND>::Do(DC_EXEC | DC_AUTO, tile, ComplementSlope(cur_slope), true);

		/* If the river is descending and the adjacent tile has either one or three corners raised, we want to make it match the slope. */
		} else if (IsInclinedSlope(desired_slope)) {
			/* Don't break existing flat river tiles by terraforming under them. */
			DiagDirection river_direction = ReverseDiagDir(GetInclinedSlopeDirection(desired_slope));

			for (DiagDirDiff d = DIAGDIRDIFF_BEGIN; d < DIAGDIRDIFF_END; d++) {
				/* We don't care about downstream or upstream tiles, just the riverbanks. */
				if (d == DIAGDIRDIFF_SAME || d == DIAGDIRDIFF_REVERSE) continue;

				TileIndex other_tile = (TileAddByDiagDir(tile, ChangeDiagDir(river_direction, d)));
				if (IsWaterTile(other_tile) && IsRiver(other_tile) && IsTileFlat(other_tile)) return false;
			}

			/* Get the corners which are different between the current and desired slope. */
			Slope to_change = cur_slope ^ desired_slope;

			/* Lower unwanted corners first. If only one corner is raised, no corners need lowering. */
			if (!IsSlopeWithOneCornerRaised(cur_slope)) {
				to_change = to_change & ComplementSlope(desired_slope);
				Command<CMD_TERRAFORM_LAND>::Do(DC_EXEC | DC_AUTO, tile, to_change, false);
			}

			/* Now check the match and raise any corners needed. */
			cur_slope = GetTileSlope(tile);
			if (cur_slope != desired_slope && IsSlopeWithOneCornerRaised(cur_slope)) {
				to_change = cur_slope ^ desired_slope;
				Command<CMD_TERRAFORM_LAND>::Do(DC_EXEC | DC_AUTO, tile, to_change, true);
			}
		}
		/* Update cur_slope after possibly terraforming. */
		cur_slope = GetTileSlope(tile);
	}

	/* Sloped rivers need water both upstream and downstream. */
	if (IsInclinedSlope(cur_slope)) {
		DiagDirection slope_direction = GetInclinedSlopeDirection(cur_slope);

		TileIndex upstream_tile = TileAddByDiagDir(tile, slope_direction);
		TileIndex downstream_tile = TileAddByDiagDir(tile, ReverseDiagDir(slope_direction));

		/* Don't look outside the map. */
		if (!IsValidTile(upstream_tile) || !IsValidTile(downstream_tile)) return false;

		/* Downstream might be new ocean created by our terraforming, and it hasn't flooded yet. */
		bool downstream_is_ocean = GetTileZ(downstream_tile) == 0 && (GetTileSlope(downstream_tile) == SLOPE_FLAT || IsSlopeWithOneCornerRaised(GetTileSlope(downstream_tile)));

		/* If downstream is dry, flat, and not ocean, try making it a river tile. */
		if (!IsWaterTile(downstream_tile) && !downstream_is_ocean) {
			/* If the tile upstream isn't flat, don't bother. */
			if (GetTileSlope(downstream_tile) != SLOPE_FLAT) return false;

			MakeRiverAndModifyDesertZoneAround(downstream_tile);
		}

		/* If upstream is dry and flat, try making it a river tile. */
		if (!IsWaterTile(upstream_tile)) {
			/* If the tile upstream isn't flat, don't bother. */
			if (GetTileSlope(upstream_tile) != SLOPE_FLAT) return false;

			MakeRiverAndModifyDesertZoneAround(upstream_tile);
		}
	}

	/* If the tile slope matches the desired slope, add a river tile. */
	if (cur_slope == desired_slope) {
		MakeRiverAndModifyDesertZoneAround(tile);
	}

	/* Always return false to keep searching. */
	return false;
}

/**
 * Check whether a river at begin could (logically) flow down to end.
 * @param begin The origin of the flow.
 * @param end The destination of the flow.
 * @return True iff the water can be flowing down.
 */
static bool FlowsDown(TileIndex begin, TileIndex end)
{
	assert(DistanceManhattan(begin, end) == 1);

	int heightBegin;
	int heightEnd;
	Slope slopeBegin = GetTileSlope(begin, &heightBegin);
	Slope slopeEnd   = GetTileSlope(end, &heightEnd);

	return heightEnd <= heightBegin &&
			/* Slope either is inclined or flat; rivers don't support other slopes. */
			(slopeEnd == SLOPE_FLAT || IsInclinedSlope(slopeEnd)) &&
			/* Slope continues, then it must be lower... or either end must be flat. */
			((slopeEnd == slopeBegin && heightEnd < heightBegin) || slopeEnd == SLOPE_FLAT || slopeBegin == SLOPE_FLAT);
}

/** Parameters for river generation to pass as AyStar user data. */
struct River_UserData {
	TileIndex spring; ///< The current spring during river generation.
	bool main_river;  ///< Whether the current river is a big river that others flow into.
};

/* AyStar callback for checking whether we reached our destination. */
static int32_t River_EndNodeCheck(const AyStar *aystar, const OpenListNode *current)
{
	return current->path.node.tile == *(TileIndex*)aystar->user_target ? AYSTAR_FOUND_END_NODE : AYSTAR_DONE;
}

/* AyStar callback for getting the cost of the current node. */
static int32_t River_CalculateG(AyStar *, AyStarNode *, OpenListNode *)
{
	return 1 + RandomRange(_settings_game.game_creation.river_route_random);
}

/* AyStar callback for getting the estimated cost to the destination. */
static int32_t River_CalculateH(AyStar *aystar, AyStarNode *current, OpenListNode *)
{
	return DistanceManhattan(*(TileIndex*)aystar->user_target, current->tile);
}

/* AyStar callback for getting the neighbouring nodes of the given node. */
static void River_GetNeighbours(AyStar *aystar, OpenListNode *current)
{
	TileIndex tile = current->path.node.tile;

	aystar->num_neighbours = 0;
	for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; d++) {
		TileIndex t2 = tile + TileOffsByDiagDir(d);
		if (IsValidTile(t2) && FlowsDown(tile, t2)) {
			aystar->neighbours[aystar->num_neighbours].tile = t2;
			aystar->neighbours[aystar->num_neighbours].direction = INVALID_TRACKDIR;
			aystar->num_neighbours++;
		}
	}
}

/* AyStar callback when an route has been found. */
static void River_FoundEndNode(AyStar *aystar, OpenListNode *current)
{
	River_UserData *data = (River_UserData *)aystar->user_data;

	/* First, build the river without worrying about its width. */
	uint cur_pos = 0;
	for (PathNode *path = &current->path; path != nullptr; path = path->parent, cur_pos++) {
		TileIndex tile = path->node.tile;
		if (!IsWaterTile(tile)) {
			MakeRiverAndModifyDesertZoneAround(tile);
		}
	}

	/* If the river is a main river, go back along the path to widen it.
	 * Don't make wide rivers if we're using the original landscape generator.
	 */
	if (_settings_game.game_creation.land_generator != LG_ORIGINAL && data->main_river) {
		const uint long_river_length = _settings_game.game_creation.min_river_length * 4;
		uint current_river_length;
		uint radius;

		cur_pos = 0;
		for (PathNode *path = &current->path; path != nullptr; path = path->parent, cur_pos++) {
			TileIndex tile = path->node.tile;

			/* Check if we should widen river depending on how far we are away from the source. */
			current_river_length = DistanceManhattan(data->spring, tile);
			radius = std::min(3u, (current_river_length / (long_river_length / 3u)) + 1u);

			if (radius > 1) CircularTileSearch(&tile, radius, RiverMakeWider, (void *)&path->node.tile);
		}
	}
}

static const uint RIVER_HASH_SIZE = 8; ///< The number of bits the hash for river finding should have.

/**
 * Simple hash function for river tiles to be used by AyStar.
 * @param tile The tile to hash.
 * @return The hash for the tile.
 */
static uint River_Hash(TileIndex tile, Trackdir)
{
	return GB(TileHash(TileX(tile), TileY(tile)), 0, RIVER_HASH_SIZE);
}

/**
 * Actually build the river between the begin and end tiles using AyStar.
 * @param begin The begin of the river.
 * @param end The end of the river.
 * @param spring The springing point of the river.
 * @param main_river Whether the current river is a big river that others flow into.
 */
static void BuildRiver(TileIndex begin, TileIndex end, TileIndex spring, bool main_river)
{
	River_UserData user_data = { spring, main_river };

	AyStar finder = {};
	finder.CalculateG = River_CalculateG;
	finder.CalculateH = River_CalculateH;
	finder.GetNeighbours = River_GetNeighbours;
	finder.EndNodeCheck = River_EndNodeCheck;
	finder.FoundEndNode = River_FoundEndNode;
	finder.user_target = &end;
	finder.user_data = &user_data;

	finder.Init(River_Hash, 1 << RIVER_HASH_SIZE);

	AyStarNode start;
	start.tile = begin;
	start.direction = INVALID_TRACKDIR;
	finder.AddStartNode(&start, 0);
	finder.Main();
	finder.Free();
}

/**
 * Try to flow the river down from a given begin.
 * @param spring The springing point of the river.
 * @param begin  The begin point we are looking from; somewhere down hill from the spring.
 * @param min_river_length The minimum length for the river.
 * @return First element: True iff a river could/has been built, otherwise false; second element: River ends at sea.
 */
static std::tuple<bool, bool> FlowRiver(TileIndex spring, TileIndex begin, uint min_river_length)
{
#	define SET_MARK(x) marks.insert(x)
#	define IS_MARKED(x) (marks.find(x) != marks.end())

	uint height = TileHeight(begin);

	if (IsWaterTile(begin)) {
		return { DistanceManhattan(spring, begin) > min_river_length, GetTileZ(begin) == 0 };
	}

	std::set<TileIndex> marks;
	SET_MARK(begin);

	/* Breadth first search for the closest tile we can flow down to. */
	std::list<TileIndex> queue;
	queue.push_back(begin);

	bool found = false;
	uint count = 0; // Number of tiles considered; to be used for lake location guessing.
	TileIndex end;
	do {
		end = queue.front();
		queue.pop_front();

		uint height2 = TileHeight(end);
		if (IsTileFlat(end) && (height2 < height || (height2 == height && IsWaterTile(end)))) {
			found = true;
			break;
		}

		for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; d++) {
			TileIndex t2 = end + TileOffsByDiagDir(d);
			if (IsValidTile(t2) && !IS_MARKED(t2) && FlowsDown(end, t2)) {
				SET_MARK(t2);
				count++;
				queue.push_back(t2);
			}
		}
	} while (!queue.empty());

	bool main_river = false;
	if (found) {
		/* Flow further down hill. */
		std::tie(found, main_river) = FlowRiver(spring, end, min_river_length);
	} else if (count > 32) {
		/* Maybe we can make a lake. Find the Nth of the considered tiles. */
		std::set<TileIndex>::const_iterator cit = marks.cbegin();
		std::advance(cit, RandomRange(count - 1));
		TileIndex lakeCenter = *cit;

		if (IsValidTile(lakeCenter) &&
				/* A river, or lake, can only be built on flat slopes. */
				IsTileFlat(lakeCenter) &&
				/* We want the lake to be built at the height of the river. */
				TileHeight(begin) == TileHeight(lakeCenter) &&
				/* We don't want the lake at the entry of the valley. */
				lakeCenter != begin &&
				/* We don't want lakes in the desert. */
				(_settings_game.game_creation.landscape != LT_TROPIC || GetTropicZone(lakeCenter) != TROPICZONE_DESERT) &&
				/* We only want a lake if the river is long enough. */
				DistanceManhattan(spring, lakeCenter) > min_river_length) {
			end = lakeCenter;
			MakeRiverAndModifyDesertZoneAround(lakeCenter);
			uint range = RandomRange(8) + 3;
			CircularTileSearch(&lakeCenter, range, MakeLake, &height);
			/* Call the search a second time so artefacts from going circular in one direction get (mostly) hidden. */
			lakeCenter = end;
			CircularTileSearch(&lakeCenter, range, MakeLake, &height);
			found = true;
		}
	}

	marks.clear();
	if (found) BuildRiver(begin, end, spring, main_river);
	return { found, main_river };
}

/**
 * Actually (try to) create some rivers.
 */
static void CreateRivers()
{
	int amount = _settings_game.game_creation.amount_of_rivers;
	if (amount == 0) return;

	uint wells = Map::ScaleBySize(4 << _settings_game.game_creation.amount_of_rivers);
	const uint num_short_rivers = wells - std::max(1u, wells / 10);
	SetGeneratingWorldProgress(GWP_RIVER, wells + 256 / 64); // Include the tile loop calls below.

	/* Try to create long rivers. */
	for (; wells > num_short_rivers; wells--) {
		IncreaseGeneratingWorldProgress(GWP_RIVER);
		for (int tries = 0; tries < 512; tries++) {
			TileIndex t = RandomTile();
			if (!CircularTileSearch(&t, 8, FindSpring, nullptr)) continue;
			if (std::get<0>(FlowRiver(t, t, _settings_game.game_creation.min_river_length * 4))) break;
		}
	}

	/* Try to create short rivers. */
	for (; wells != 0; wells--) {
		IncreaseGeneratingWorldProgress(GWP_RIVER);
		for (int tries = 0; tries < 128; tries++) {
			TileIndex t = RandomTile();
			if (!CircularTileSearch(&t, 8, FindSpring, nullptr)) continue;
			if (std::get<0>(FlowRiver(t, t, _settings_game.game_creation.min_river_length))) break;
		}
	}

	/* Widening rivers may have left some tiles requiring to be watered. */
	ConvertGroundTilesIntoWaterTiles();

	/* Run tile loop to update the ground density. */
	for (uint i = 0; i != 256; i++) {
		if (i % 64 == 0) IncreaseGeneratingWorldProgress(GWP_RIVER);
		RunTileLoop();
	}
}

/**
 * Calculate what height would be needed to cover N% of the landmass.
 *
 * The function allows both snow and desert/tropic line to be calculated. It
 * tries to find the closests height which covers N% of the landmass; it can
 * be below or above it.
 *
 * Tropic has a mechanism where water and tropic tiles in mountains grow
 * inside the desert. To better approximate the requested coverage, this is
 * taken into account via an edge histogram, which tells how many neighbouring
 * tiles are lower than the tiles of that height. The multiplier indicates how
 * severe this has to be taken into account.
 *
 * @param coverage A value between 0 and 100 indicating a percentage of landmass that should be covered.
 * @param edge_multiplier How much effect neighbouring tiles that are of a lower height level have on the score.
 * @return The estimated best height to use to cover N% of the landmass.
 */
static uint CalculateCoverageLine(uint coverage, uint edge_multiplier)
{
	const DiagDirection neighbour_dir[] = {
		DIAGDIR_NE,
		DIAGDIR_SE,
		DIAGDIR_SW,
		DIAGDIR_NW,
	};

	/* Histogram of how many tiles per height level exist. */
	std::array<int, MAX_TILE_HEIGHT + 1> histogram = {};
	/* Histogram of how many neighbour tiles are lower than the tiles of the height level. */
	std::array<int, MAX_TILE_HEIGHT + 1> edge_histogram = {};

	/* Build a histogram of the map height. */
	for (TileIndex tile = 0; tile < Map::Size(); tile++) {
		uint h = TileHeight(tile);
		histogram[h]++;

		if (edge_multiplier != 0) {
			/* Check if any of our neighbours is below us. */
			for (auto dir : neighbour_dir) {
				TileIndex neighbour_tile = AddTileIndexDiffCWrap(tile, TileIndexDiffCByDiagDir(dir));
				if (IsValidTile(neighbour_tile) && TileHeight(neighbour_tile) < h) {
					edge_histogram[h]++;
				}
			}
		}
	}

	/* The amount of land we have is the map size minus the first (sea) layer. */
	uint land_tiles = Map::Size() - histogram[0];
	int best_score = land_tiles;

	/* Our goal is the coverage amount of the land-mass. */
	int goal_tiles = land_tiles * coverage / 100;

	/* We scan from top to bottom. */
	uint h = MAX_TILE_HEIGHT;
	uint best_h = h;

	int current_tiles = 0;
	for (; h > 0; h--) {
		current_tiles += histogram[h];
		int current_score = goal_tiles - current_tiles;

		/* Tropic grows from water and mountains into the desert. This is a
		 * great visual, but it also means we* need to take into account how
		 * much less desert tiles are being created if we are on this
		 * height-level. We estimate this based on how many neighbouring
		 * tiles are below us for a given length, assuming that is where
		 * tropic is growing from.
		 */
		if (edge_multiplier != 0 && h > 1) {
			/* From water tropic tiles grow for a few tiles land inward. */
			current_score -= edge_histogram[1] * edge_multiplier;
			/* Tropic tiles grow into the desert for a few tiles. */
			current_score -= edge_histogram[h] * edge_multiplier;
		}

		if (std::abs(current_score) < std::abs(best_score)) {
			best_score = current_score;
			best_h = h;
		}

		/* Always scan all height-levels, as h == 1 might give a better
		 * score than any before. This is true for example with 0% desert
		 * coverage. */
	}

	return best_h;
}

/**
 * Calculate the line from which snow begins.
 */
static void CalculateSnowLine()
{
	/* We do not have snow sprites on coastal tiles, so never allow "1" as height. */
	_settings_game.game_creation.snow_line_height = std::max(CalculateCoverageLine(_settings_game.game_creation.snow_coverage, 0), 2u);
}

/**
 * Calculate the line (in height) between desert and tropic.
 * @return The height of the line between desert and tropic.
 */
static uint8_t CalculateDesertLine()
{
	/* CalculateCoverageLine() runs from top to bottom, so we need to invert the coverage. */
	return CalculateCoverageLine(100 - _settings_game.game_creation.desert_coverage, 4);
}

void GenerateLandscape(byte mode)
{
	/** Number of steps of landscape generation */
	enum GenLandscapeSteps {
		GLS_HEIGHTMAP    =  3, ///< Loading a heightmap
		GLS_TERRAGENESIS =  5, ///< Terragenesis generator
		GLS_ORIGINAL     =  2, ///< Original generator
		GLS_TROPIC       = 12, ///< Extra steps needed for tropic landscape
		GLS_OTHER        =  0, ///< Extra steps for other landscapes
	};
	uint steps = (_settings_game.game_creation.landscape == LT_TROPIC) ? GLS_TROPIC : GLS_OTHER;

	if (mode == GWM_HEIGHTMAP) {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, steps + GLS_HEIGHTMAP);
		LoadHeightmap(_file_to_saveload.detail_ftype, _file_to_saveload.name.c_str());
		IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);
	} else if (_settings_game.game_creation.land_generator == LG_TERRAGENESIS) {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, steps + GLS_TERRAGENESIS);
		GenerateTerrainPerlin();
	} else {
		SetGeneratingWorldProgress(GWP_LANDSCAPE, steps + GLS_ORIGINAL);
		if (_settings_game.construction.freeform_edges) {
			for (uint x = 0; x < Map::SizeX(); x++) MakeVoid(TileXY(x, 0));
			for (uint y = 0; y < Map::SizeY(); y++) MakeVoid(TileXY(0, y));
		}
		switch (_settings_game.game_creation.landscape) {
			case LT_ARCTIC: {
				uint32_t r = Random();

				for (uint i = Map::ScaleBySize(GB(r, 0, 7) + 950); i != 0; --i) {
					GenerateTerrain(2, 0);
				}

				uint flag = GB(r, 7, 2) | 4;
				for (uint i = Map::ScaleBySize(GB(r, 9, 7) + 450); i != 0; --i) {
					GenerateTerrain(4, flag);
				}
				break;
			}

			case LT_TROPIC: {
				uint32_t r = Random();

				for (uint i = Map::ScaleBySize(GB(r, 0, 7) + 170); i != 0; --i) {
					GenerateTerrain(0, 0);
				}

				uint flag = GB(r, 7, 2) | 4;
				for (uint i = Map::ScaleBySize(GB(r, 9, 8) + 1700); i != 0; --i) {
					GenerateTerrain(0, flag);
				}

				flag ^= 2;

				for (uint i = Map::ScaleBySize(GB(r, 17, 7) + 410); i != 0; --i) {
					GenerateTerrain(3, flag);
				}
				break;
			}

			default: {
				uint32_t r = Random();

				assert(_settings_game.difficulty.quantity_sea_lakes != CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY);
				uint i = Map::ScaleBySize(GB(r, 0, 7) + (3 - _settings_game.difficulty.quantity_sea_lakes) * 256 + 100);
				for (; i != 0; --i) {
					/* Make sure we do not overflow. */
					GenerateTerrain(Clamp(_settings_game.difficulty.terrain_type, 0, 3), 0);
				}
				break;
			}
		}
	}

	/* Do not call IncreaseGeneratingWorldProgress() before FixSlopes(),
	 * it allows screen redraw. Drawing of broken slopes crashes the game */
	FixSlopes();
	MarkWholeScreenDirty();
	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	ConvertGroundTilesIntoWaterTiles();
	MarkWholeScreenDirty();
	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	switch (_settings_game.game_creation.landscape) {
		case LT_ARCTIC:
			CalculateSnowLine();
			break;

		case LT_TROPIC: {
			uint desert_tropic_line = CalculateDesertLine();
			CreateDesertOrRainForest(desert_tropic_line);
			break;
		}

		default:
			break;
	}

	CreateRivers();
}

void OnTick_Town();
void OnTick_Trees();
void OnTick_Station();
void OnTick_Industry();

void OnTick_Companies();
void OnTick_LinkGraph();

void CallLandscapeTick()
{
	{
		PerformanceAccumulator framerate(PFE_GL_LANDSCAPE);

		OnTick_Town();
		OnTick_Trees();
		OnTick_Station();
		OnTick_Industry();
	}

	OnTick_Companies();
	OnTick_LinkGraph();
}
