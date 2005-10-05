/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "pathfind.h"
#include "engine.h"
#include "town.h"
#include "sound.h"
#include "station.h"
#include "sprite.h"
#include "depot.h"
#include "pbs.h"
#include "waypoint.h"
#include "npf.h"
#include "rail.h"
#include "railtypes.h" // include table for railtypes

extern uint16 _custom_sprites_base;

const byte _track_sloped_sprites[14] = {
	14, 15, 22, 13,
	 0, 21, 17, 12,
	23,  0, 18, 20,
	19, 16
};

void ShowTrainDepotWindow(TileIndex tile);

/* Format of rail map5 byte.
 * 00 abcdef  => Normal rail
 * 01 abcdef  => Rail with signals
 * 10 ??????  => Unused
 * 11 ????dd  => Depot
 *
 * abcdef is a bitmask, which contains ones for all present tracks. Below the
 * value for each track is given.
 */

/*         4
 *     ---------
 *    |\       /|
 *    | \    1/ |
 *    |  \   /  |
 *    |   \ /   |
 *  16|    \    |32
 *    |   / \2  |
 *    |  /   \  |
 *    | /     \ |
 *    |/       \|
 *     ---------
 *         8
 */


// Constants for lower part of Map2 byte.
enum RailMap2Lower4 {
	RAIL_MAP2LO_GROUND_MASK = 0xF,
	RAIL_GROUND_BROWN = 0,
	RAIL_GROUND_GREEN = 1,
	RAIL_GROUND_FENCE_NW = 2,
	RAIL_GROUND_FENCE_SE = 3,
	RAIL_GROUND_FENCE_SENW = 4,
	RAIL_GROUND_FENCE_NE = 5,
	RAIL_GROUND_FENCE_SW = 6,
	RAIL_GROUND_FENCE_NESW = 7,
	RAIL_GROUND_FENCE_VERT1 = 8,
	RAIL_GROUND_FENCE_VERT2 = 9,
	RAIL_GROUND_FENCE_HORIZ1 = 10,
	RAIL_GROUND_FENCE_HORIZ2 = 11,
	RAIL_GROUND_ICE_DESERT = 12,
};


/* MAP2 byte:    abcd???? => Signal On? Same coding as map3lo
 * MAP3LO byte:  abcd???? => Signal Exists?
 *				 a and b are for diagonals, upper and left,
 *				 one for each direction. (ie a == NE->SW, b ==
 *				 SW->NE, or v.v., I don't know. b and c are
 *				 similar for lower and right.
 * MAP2 byte:    ????abcd => Type of ground.
 * MAP3LO byte:  ????abcd => Type of rail.
 * MAP5:         00abcdef => rail
 *               01abcdef => rail w/ signals
 *               10uuuuuu => unused
 *               11uuuudd => rail depot
 */

static bool CheckTrackCombination(TileIndex tile, TrackBits to_build, uint flags)
{
	RailTileType type = GetRailTileType(tile);
	TrackBits current; /* The current track layout */
	TrackBits future; /* The track layout we want to build */
	_error_message = STR_1001_IMPOSSIBLE_TRACK_COMBINATION;

	if (type != RAIL_TYPE_NORMAL && type != RAIL_TYPE_SIGNALS)
		return false; /* Cannot build anything on depots and checkpoints */

	/* So, we have a tile with tracks on it (and possibly signals). Let's see
	 * what tracks first */
	current = GetTrackBits(tile);
	future = current | to_build;

	/* Are we really building something new? */
	if (current == future) {
		/* Nothing new is being built */
		_error_message = STR_1007_ALREADY_BUILT;
		return false;
	}

	/* Let's see if we may build this */
	if ((flags & DC_NO_RAIL_OVERLAP) || type == RAIL_TYPE_SIGNALS) {
		/* If we are not allowed to overlap (flag is on for ai players or we have
		 * signals on the tile), check that */
		return
			future == (TRACK_BIT_UPPER | TRACK_BIT_LOWER) ||
			future == (TRACK_BIT_LEFT  | TRACK_BIT_RIGHT);
	} else {
		/* Normally, we may overlap and any combination is valid */
		return true;
	}
}


static const byte _valid_tileh_slopes[4][15] = {

// set of normal ones
{
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	TRACK_BIT_RIGHT,
	TRACK_BIT_UPPER,
	TRACK_BIT_DIAG1,

	TRACK_BIT_LEFT,
	0,
	TRACK_BIT_DIAG2,
	TRACK_BIT_LOWER,

	TRACK_BIT_LOWER,
	TRACK_BIT_DIAG2,
	0,
	TRACK_BIT_LEFT,

	TRACK_BIT_DIAG1,
	TRACK_BIT_UPPER,
	TRACK_BIT_RIGHT,
},

// allowed rail for an evenly raised platform
{
	0,
	TRACK_BIT_LEFT,
	TRACK_BIT_LOWER,
	TRACK_BIT_DIAG2 | TRACK_BIT_LOWER | TRACK_BIT_LEFT,

	TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1 | TRACK_BIT_LOWER | TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,

	TRACK_BIT_UPPER,
	TRACK_BIT_DIAG1 | TRACK_BIT_UPPER | TRACK_BIT_LEFT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,

	TRACK_BIT_DIAG2 | TRACK_BIT_UPPER | TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
},

// allowed rail on coast tile
{
	0,
	TRACK_BIT_LEFT,
	TRACK_BIT_LOWER,
	TRACK_BIT_DIAG2|TRACK_BIT_LEFT|TRACK_BIT_LOWER,

	TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_RIGHT|TRACK_BIT_LOWER,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,

	TRACK_BIT_UPPER,
	TRACK_BIT_DIAG1|TRACK_BIT_LEFT|TRACK_BIT_UPPER,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,

	TRACK_BIT_DIAG2|TRACK_BIT_RIGHT|TRACK_BIT_UPPER,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	TRACK_BIT_DIAG1|TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LOWER|TRACK_BIT_LEFT|TRACK_BIT_RIGHT,
	},

	// valid railway crossings on slopes
	{
		1, 0, 0, // 0, 1, 2
		0, 0, 1, // 3, 4, 5
		0, 1, 0, // 6, 7, 8
		0, 1, 1, // 9, 10, 11
		0, 1, 1, // 12, 13, 14
	}
};

uint GetRailFoundation(uint tileh, uint bits)
{
	int i;

	if ((~_valid_tileh_slopes[0][tileh] & bits) == 0)
		return 0;

	if ((~_valid_tileh_slopes[1][tileh] & bits) == 0)
		return tileh;

	if ( ((i=0, tileh == 1) || (i+=2, tileh == 2) || (i+=2, tileh == 4) || (i+=2, tileh == 8)) && (bits == TRACK_BIT_DIAG1 || (i++, bits == TRACK_BIT_DIAG2)))
		return i + 15;

	return 0;
}

//
static uint32 CheckRailSlope(uint tileh, TrackBits rail_bits, TrackBits existing, TileIndex tile)
{
	// never allow building on top of steep tiles
	if (!IsSteepTileh(tileh)) {
		rail_bits |= existing;

		// don't allow building on the lower side of a coast
		if (IsTileType(tile, MP_WATER) &&
				~_valid_tileh_slopes[2][tileh] & rail_bits) {
			return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);
		}

		// no special foundation
		if ((~_valid_tileh_slopes[0][tileh] & rail_bits) == 0)
			return 0;

		if ((~_valid_tileh_slopes[1][tileh] & rail_bits) == 0 || ( // whole tile is leveled up
					(rail_bits == TRACK_BIT_DIAG1 || rail_bits == TRACK_BIT_DIAG2) &&
					(tileh == 1 || tileh == 2 || tileh == 4 || tileh == 8)
				)) { // partly up
			if (existing != 0) {
				return 0;
			} else if (!_patches.build_on_slopes || _is_old_ai_player) {
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			} else {
				return _price.terraform;
			}
		}
	}
	return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
}

/* Validate functions for rail building */
static inline bool ValParamTrackOrientation(Track track) {return IsValidTrack(track);}

/** Build a single piece of rail
 * @param x,y coordinates on where to build
 * @param p1 railtype of being built piece (normal, mono, maglev)
 * @param p2 rail track to build
 */
int32 CmdBuildSingleRail(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile;
	uint tileh;
	uint m5; /* XXX: Used only as a cache, should probably be removed? */
	Track track = (Track)p2;
	TrackBits trackbit;
	int32 cost = 0;
	int32 ret;

	if (!ValParamRailtype(p1) || !ValParamTrackOrientation(track)) return CMD_ERROR;

	tile = TileVirtXY(x, y);
	tileh = GetTileSlope(tile, NULL);
	m5 = _m[tile].m5;
	trackbit = TrackToTrackBits(track);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	switch (GetTileType(tile)) {
		case MP_TUNNELBRIDGE:
			if ((m5 & 0xC0) != 0xC0 || // not bridge middle part?
					(m5 & 0x01 ? TRACK_BIT_DIAG1 : TRACK_BIT_DIAG2) != trackbit) { // wrong direction?
				// Get detailed error message
				return DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}

			switch (m5 & 0x38) { // what's under the bridge?
				case 0x00: // clear land
					ret = CheckRailSlope(tileh, trackbit, 0, tile);
					if (CmdFailed(ret)) return ret;
					cost += ret;

					if (flags & DC_EXEC) {
						SetTileOwner(tile, _current_player);
						SB(_m[tile].m3, 0, 4, p1);
						_m[tile].m5 = (m5 & 0xC7) | 0x20; // railroad under bridge
					}
					break;

				case 0x20: // rail already there
					return_cmd_error(STR_1007_ALREADY_BUILT);

				default:
					// Get detailed error message
					return DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
			break;

		case MP_RAILWAY:
			if (!CheckTrackCombination(tile, trackbit, flags) ||
					!EnsureNoVehicle(tile)) {
				return CMD_ERROR;
			}
			if (m5 & RAIL_TYPE_SPECIAL ||
					!IsTileOwner(tile, _current_player) ||
					GB(_m[tile].m3, 0, 4) != p1) {
				// Get detailed error message
				return DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}

			ret = CheckRailSlope(tileh, trackbit, GetTrackBits(tile), tile);
			if (CmdFailed(ret)) return ret;
			cost += ret;

			if (flags & DC_EXEC) {
				_m[tile].m2 &= ~RAIL_MAP2LO_GROUND_MASK; // Bare land
				_m[tile].m5 = m5 | trackbit;
			}
			break;

		case MP_STREET:
			if (!_valid_tileh_slopes[3][tileh]) // prevent certain slopes
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			if (!EnsureNoVehicle(tile)) return CMD_ERROR;

			if ((m5 & 0xF0) == 0 && ( // normal road?
						(track == TRACK_DIAG1 && m5 == 0x05) ||
						(track == TRACK_DIAG2 && m5 == 0x0A) // correct direction?
					)) {
				if (flags & DC_EXEC) {
					_m[tile].m3 = GetTileOwner(tile);
					SetTileOwner(tile, _current_player);
					_m[tile].m4 = p1;
					_m[tile].m5 = 0x10 | (track == TRACK_DIAG1 ? 0x08 : 0x00); // level crossing
				}
				break;
			}

			if (IsLevelCrossing(tile) && (m5 & 0x08 ? TRACK_DIAG1 : TRACK_DIAG2) == track)
				return_cmd_error(STR_1007_ALREADY_BUILT);
			/* FALLTHROUGH */

		default:
			ret = CheckRailSlope(tileh, trackbit, 0, tile);
			if (CmdFailed(ret)) return ret;
			cost += ret;

			ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return ret;
			cost += ret;

			if (flags & DC_EXEC) {
				SetTileType(tile, MP_RAILWAY);
				SetTileOwner(tile, _current_player);
				_m[tile].m2 = 0; // Bare land
				_m[tile].m3 = p1; // No signals, rail type
				_m[tile].m5 = trackbit;
			}
			break;
	}

	if (flags & DC_EXEC) {
		MarkTileDirtyByTile(tile);
		SetSignalsOnBothDir(tile, track);
	}

	return cost + _price.build_rail;
}

static const byte _signals_table[] = {
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20, 0, 0, // direction 1
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10, 0, 0  // direction 2
};

static const byte _signals_table_other[] = {
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10, 0, 0, // direction 1
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20, 0, 0  // direction 2
};

static const byte _signals_table_both[] = {
	0xC0, 0xC0, 0xC0, 0x30, 0xC0, 0x30, 0, 0,	// both directions combined
	0xC0, 0xC0, 0xC0, 0x30, 0xC0, 0x30, 0, 0
};


/** Remove a single piece of track
 * @param x,y coordinates for removal of track
 * @param p1 unused
 * @param p2 rail orientation
 */
int32 CmdRemoveSingleRail(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Track track = (Track)p2;
	TrackBits trackbit;
	uint tileh;
	TileIndex tile;
	byte m5;
	int32 cost = _price.remove_rail;

	if (!ValParamTrackOrientation(p2)) return CMD_ERROR;
	trackbit = TrackToTrackBits(track);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TileVirtXY(x, y);
	tileh = GetTileSlope(tile, NULL);

	if (!IsTileType(tile, MP_TUNNELBRIDGE) && !IsTileType(tile, MP_STREET) && !IsTileType(tile, MP_RAILWAY))
		return CMD_ERROR;

	if (_current_player != OWNER_WATER && !CheckTileOwnership(tile))
		return CMD_ERROR;

	// allow building rail under bridge
	if (!IsTileType(tile, MP_TUNNELBRIDGE) && !EnsureNoVehicle(tile))
		return CMD_ERROR;

	switch(GetTileType(tile))
	{
		case MP_TUNNELBRIDGE:
			if (!EnsureNoVehicleZ(tile, TilePixelHeight(tile)))
				return CMD_ERROR;

			if ((_m[tile].m5 & 0xF8) != 0xE0)
				return CMD_ERROR;

			if ((_m[tile].m5 & 1 ? TRACK_BIT_DIAG1 : TRACK_BIT_DIAG2) != trackbit)
				return CMD_ERROR;

			if (!(flags & DC_EXEC))
				return _price.remove_rail;

		SetTileOwner(tile, OWNER_NONE);
			_m[tile].m5 = _m[tile].m5 & 0xC7;
			break;
		case MP_STREET:
			if (!IsLevelCrossing(tile)) return CMD_ERROR;

			/* This is a crossing, let's check if the direction is correct */
			if (_m[tile].m5 & 8) {
				m5 = 5;
				if (track != TRACK_DIAG1)
					return CMD_ERROR;
			} else {
				m5 = 10;
				if (track != TRACK_DIAG2)
					return CMD_ERROR;
			}

			if (!(flags & DC_EXEC))
				return _price.remove_rail;

			_m[tile].m5 = m5;
			SetTileOwner(tile, _m[tile].m3);
			_m[tile].m2 = 0;
			break;
		case MP_RAILWAY:

			if (!IsPlainRailTile(tile))
				return CMD_ERROR;

			/* See if the track to remove is actually there */
			if (!(GetTrackBits(tile) & trackbit))
				return CMD_ERROR;

			/* Charge extra to remove signals on the track, if they are there */
			if (HasSignalOnTrack(tile, track))
				cost += DoCommand(x, y, track, 0, flags, CMD_REMOVE_SIGNALS);

			if (!(flags & DC_EXEC))
				return cost;

			/* We remove the trackbit here. */
			_m[tile].m5 &= ~trackbit;

			/* Unreserve track for PBS */
			if (PBSTileReserved(tile) & trackbit)
				PBSClearTrack(tile, track);

			if (GetTrackBits(tile)  == 0) {
				/* The tile has no tracks left, it is no longer a rail tile */
				DoClearSquare(tile);
				/* XXX: This is an optimisation, right? Is it really worth the ugly goto? */
				goto skip_mark_dirty;
			}
			break;
		default:
			assert(0);
	}

	/* mark_dirty */
	MarkTileDirtyByTile(tile);

skip_mark_dirty:;

	SetSignalsOnBothDir(tile, track);

	return cost;
}

static const struct {
	int8 xinc[16];
	int8 yinc[16];
} _railbit = {{
//  0   1   2   3   4   5
	-16,  0,-16,  0, 16,  0,    0,  0,
	 16,  0,  0, 16,  0,-16,    0,  0,
},{
	  0, 16,  0, 16,  0, 16,    0,  0,
	  0,-16,-16,  0,-16,  0,    0,  0,
}};

static int32 ValidateAutoDrag(Trackdir *trackdir, int x, int y, int ex, int ey)
{
	int dx, dy, trdx, trdy;

	if (!ValParamTrackOrientation(*trackdir)) return CMD_ERROR;

	// calculate delta x,y from start to end tile
	dx = ex - x;
	dy = ey - y;

	// calculate delta x,y for the first direction
	trdx = _railbit.xinc[*trackdir];
	trdy = _railbit.yinc[*trackdir];

	if (!IsDiagonalTrackdir(*trackdir)) {
		trdx += _railbit.xinc[*trackdir ^ 1];
		trdy += _railbit.yinc[*trackdir ^ 1];
	}

	// validate the direction
	while (((trdx <= 0) && (dx > 0)) || ((trdx >= 0) && (dx < 0)) ||
	       ((trdy <= 0) && (dy > 0)) || ((trdy >= 0) && (dy < 0))) {
		if (!HASBIT(*trackdir, 3)) { // first direction is invalid, try the other
			SETBIT(*trackdir, 3); // reverse the direction
			trdx = -trdx;
			trdy = -trdy;
		} else // other direction is invalid too, invalid drag
			return CMD_ERROR;
	}

	// (for diagonal tracks, this is already made sure of by above test), but:
	// for non-diagonal tracks, check if the start and end tile are on 1 line
	if (!IsDiagonalTrackdir(*trackdir)) {
		trdx = _railbit.xinc[*trackdir];
		trdy = _railbit.yinc[*trackdir];
		if ((abs(dx) != abs(dy)) && (abs(dx) + abs(trdy) != abs(dy) + abs(trdx)))
			return CMD_ERROR;
	}

	return 0;
}

/** Build a stretch of railroad tracks.
 * @param x,y start tile of drag
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-3) - railroad type normal/maglev (0 = normal, 1 = mono, 2 = maglev)
 * - p2 = (bit 4-6) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit 7)   - 0 = build, 1 = remove tracks
 */
static int32 CmdRailTrackHelper(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int ex, ey;
	int32 ret, total_cost = 0;
	Track track = (Track)GB(p2, 4, 3);
	Trackdir trackdir;
	byte mode = HASBIT(p2, 7);

	if (!ValParamRailtype(p2 & 0x3) || !ValParamTrackOrientation(track)) return CMD_ERROR;
	if (p1 > MapSize()) return CMD_ERROR;
	trackdir = TrackToTrackdir(track);

	/* unpack end point */
	ex = TileX(p1) * TILE_SIZE;
	ey = TileY(p1) * TILE_SIZE;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (CmdFailed(ValidateAutoDrag(&trackdir, x, y, ex, ey))) return CMD_ERROR;

	if (flags & DC_EXEC) SndPlayTileFx(SND_20_SPLAT_2, TileVirtXY(x, y));

	for(;;) {
		ret = DoCommand(x, y, p2 & 0x3, TrackdirToTrack(trackdir), flags, (mode == 0) ? CMD_BUILD_SINGLE_RAIL : CMD_REMOVE_SINGLE_RAIL);

		if (CmdFailed(ret)) {
			if ((_error_message != STR_1007_ALREADY_BUILT) && (mode == 0))
				break;
		} else
			total_cost += ret;

		if (x == ex && y == ey)
			break;

		x += _railbit.xinc[trackdir];
		y += _railbit.yinc[trackdir];

		// toggle railbit for the non-diagonal tracks
		if (!IsDiagonalTrackdir(trackdir)) trackdir ^= 1;
	}

	return (total_cost == 0) ? CMD_ERROR : total_cost;
}

/** Build rail on a stretch of track.
 * Stub for the unified rail builder/remover
 * @see CmdRailTrackHelper
 */
int32 CmdBuildRailroadTrack(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdRailTrackHelper(x, y, flags, p1, CLRBIT(p2, 7));
}

/** Build rail on a stretch of track.
 * Stub for the unified rail builder/remover
 * @see CmdRailTrackHelper
 */
int32 CmdRemoveRailroadTrack(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdRailTrackHelper(x, y, flags, p1, SETBIT(p2, 7));
}

/** Build a train depot
 * @param x,y position of the train depot
 * @param p1 rail type
 * @param p2 depot direction (0 through 3), where 0 is NE, 1 is SE, 2 is SW, 3 is NW
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
int32 CmdBuildTrainDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Depot *d;
	TileIndex tile = TileVirtXY(x, y);
	int32 cost, ret;
	uint tileh;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;
	/* check railtype and valid direction for depot (0 through 3), 4 in total */
	if (!ValParamRailtype(p1) || p2 > 3) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);

	/* Prohibit construction if
		The tile is non-flat AND
		1) The AI is "old-school"
		2) build-on-slopes is disabled
		3) the tile is steep i.e. spans two height levels
		4) the exit points in the wrong direction

	*/

	if (tileh != 0 && (
			_is_old_ai_player ||
			!_patches.build_on_slopes ||
			IsSteepTileh(tileh) ||
			!CanBuildDepotByTileh(p2, tileh)
		)
	) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	cost = ret;

	d = AllocateDepot();
	if (d == NULL)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (IsLocalPlayer()) _last_built_train_depot_tile = tile;

		ModifyTile(tile,
			MP_SETTYPE(MP_RAILWAY) |
			MP_MAP3LO | MP_MAPOWNER_CURRENT | MP_MAP5,
			p1, /* map3_lo */
			p2 | RAIL_TYPE_DEPOT_WAYPOINT /* map5 */
		);

		d->xy = tile;
		d->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		SetSignalsOnBothDir(tile, (p2&1) ? 2 : 1);

	}

	return cost + _price.build_train_depot;
}

/** Build signals, alternate between double/single, signal/semaphore,
 * pre/exit/combo-signals, and what-else not
 * @param x,y coordinates where signals is being built
 * @param p1 various bitstuffed elements
 * - p1 = (bit 0-2) - track-orientation, valid values: 0-5 (Track enum)
 * - p1 = (bit 3)   - choose semaphores/signals or cycle normal/pre/exit/combo depending on context
 * @param p2 used for CmdBuildManySignals() to copy direction of first signal
 * TODO: p2 should be replaced by two bits for "along" and "against" the track.
 */
int32 CmdBuildSingleSignal(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile = TileVirtXY(x, y);
	bool semaphore;
	bool pre_signal;
	Track track = (Track)(p1 & 0x7);
	byte m5;
	int32 cost;

	// Same bit, used in different contexts
	semaphore = pre_signal = HASBIT(p1, 3);

	if (!ValParamTrackOrientation(track) || !IsTileType(tile, MP_RAILWAY) || !EnsureNoVehicle(tile))
		return CMD_ERROR;

	/* Protect against invalid signal copying */
	if (p2 != 0 && (p2 & SignalOnTrack(track)) == 0) return CMD_ERROR;

	m5 = _m[tile].m5;

	/* You can only build signals on plain rail tiles, and the selected track must exist */
	if (!IsPlainRailTile(tile) || !HasTrack(tile, track)) return CMD_ERROR;

	if (!CheckTileOwnership(tile)) return CMD_ERROR;

	_error_message = STR_1005_NO_SUITABLE_RAILROAD_TRACK;

	{
 		/* See if this is a valid track combination for signals, (ie, no overlap) */
 		TrackBits trackbits = GetTrackBits(tile);
		if (KILL_FIRST_BIT(trackbits) != 0 && /* More than one track present */
				trackbits != (TRACK_BIT_UPPER | TRACK_BIT_LOWER) && /* Horizontal parallel, non-intersecting tracks */
				trackbits != (TRACK_BIT_LEFT | TRACK_BIT_RIGHT) /* Vertical parallel, non-intersecting tracks */
		)
			return CMD_ERROR;
	}

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!HasSignalOnTrack(tile, track)) {
		// build new signals
		cost = _price.build_signals;
	} else {
		if (p2 != 0 && semaphore != HasSemaphores(tile, track)) {
			// convert signals <-> semaphores
			cost = _price.build_signals + _price.remove_signals;
		} else {
			// it is free to change orientation/pre-exit-combo signals
			cost = 0;
		}
	}

	if (flags & DC_EXEC) {
		if (GetRailTileType(tile) != RAIL_TYPE_SIGNALS) {
			// there are no signals at all on this tile yet
			_m[tile].m5 |= RAIL_TYPE_SIGNALS; // change into signals
			_m[tile].m2 |= 0xF0;              // all signals are on
			_m[tile].m3 &= ~0xF0;          // no signals built by default
			_m[tile].m4 = semaphore ? 0x08 : 0;
		}

		if (p2 == 0) {
			if (!HasSignalOnTrack(tile, track)) {
				// build new signals
				_m[tile].m3 |= SignalOnTrack(track);
			} else {
				if (pre_signal) {
					// cycle between normal -> pre -> exit -> combo -> pbs ->...
					byte type = ((GetSignalType(tile, track) + 1) % 5);
					SB(_m[tile].m4, 0, 3, type);
				} else {
					// cycle between two-way -> one-way -> one-way -> ...
					/* TODO: Rewrite switch into something more general */
					switch (track) {
						case TRACK_LOWER:
						case TRACK_RIGHT: {
							byte signal = (_m[tile].m3 - 0x10) & 0x30;
							if (signal == 0) signal = 0x30;
							_m[tile].m3 &= ~0x30;
							_m[tile].m3 |= signal;
							break;
						}

						default: {
							byte signal = (_m[tile].m3 - 0x40) & 0xC0;
							if (signal == 0) signal = 0xC0;
							_m[tile].m3 &= ~0xC0;
							_m[tile].m3 |= signal;
							break;
						}
					}
				}
			}
		} else {
			/* If CmdBuildManySignals is called with copying signals, just copy the
			 * direction of the first signal given as parameter by CmdBuildManySignals */
			_m[tile].m3 &= ~SignalOnTrack(track);
			_m[tile].m3 |= p2 & SignalOnTrack(track);
			// convert between signal<->semaphores when dragging
			if (semaphore)
				SETBIT(_m[tile].m4, 3);
			else
				CLRBIT(_m[tile].m4, 3);
		}

		MarkTileDirtyByTile(tile);
		SetSignalsOnBothDir(tile, track);
	}

	return cost;
}

/**	Build many signals by dragging; AutoSignals
 * @param x,y start tile of drag
 * @param p1  end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0)    - 0 = build, 1 = remove signals
 * - p2 = (bit  3)    - 0 = signals, 1 = semaphores
 * - p2 = (bit  4- 6) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit 24-31) - user defined signals_density
 */
static int32 CmdSignalTrackHelper(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int ex, ey;
	int32 ret, total_cost, signal_ctr;
	byte signals;
	TileIndex tile = TileVirtXY(x, y);
	bool error = true;

	int mode = p2 & 0x1;
	Track track = GB(p2, 4, 3);
	Trackdir trackdir = TrackToTrackdir(track);
	byte semaphores = (HASBIT(p2, 3)) ? 8 : 0;
	byte signal_density = (p2 >> 24);

	if (p1 > MapSize()) return CMD_ERROR;
	if (signal_density == 0 || signal_density > 20) return CMD_ERROR;

	if (!IsTileType(tile, MP_RAILWAY))
		return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* for vertical/horizontal tracks, double the given signals density
	* since the original amount will be too dense (shorter tracks) */
	if (!IsDiagonalTrack(track))
		signal_density *= 2;

	// unpack end tile
	ex = TileX(p1) * TILE_SIZE;
	ey = TileY(p1) * TILE_SIZE;

	if (CmdFailed(ValidateAutoDrag(&trackdir, x, y, ex, ey))) return CMD_ERROR;

	track = TrackdirToTrack(trackdir); /* trackdir might have changed, keep track in sync */

	// copy the signal-style of the first rail-piece if existing
	if (GetRailTileType(tile) == RAIL_TYPE_SIGNALS && GetTrackBits(tile) != 0) { /* XXX: GetTrackBits check useless? */
		signals = _m[tile].m3 & SignalOnTrack(track);
		if (signals == 0) signals = SignalOnTrack(track); /* Can this actually occur? */

		semaphores = (HasSemaphores(tile, track) ? 8 : 0); // copy signal/semaphores style (independent of CTRL)
	} else // no signals exist, drag a two-way signal stretch
		signals = SignalOnTrack(track);

	/* signal_ctr         - amount of tiles already processed
	 * signals_density    - patch setting to put signal on every Nth tile (double space on |, -- tracks)
	 **********
	 * trackdir   - trackdir to build with autorail
	 * semaphores - semaphores or signals
	 * signals    - is there a signal/semaphore on the first tile, copy its style (two-way/single-way)
	                and convert all others to semaphore/signal
	 * mode       - 1 remove signals, 0 build signals */
	signal_ctr = total_cost = 0;
	for (;;) {
		// only build/remove signals with the specified density
		if ((signal_ctr % signal_density) == 0 ) {
			ret = DoCommand(x, y, TrackdirToTrack(trackdir) | semaphores, signals, flags, (mode == 1) ? CMD_REMOVE_SIGNALS : CMD_BUILD_SIGNALS);

			/* Abort placement for any other error than NOT_SUITABLE_TRACK
			 * This includes vehicles on track, competitor's tracks, etc. */
			if (CmdFailed(ret)) {
				if (_error_message != STR_1005_NO_SUITABLE_RAILROAD_TRACK && mode != 1) return CMD_ERROR;
			} else {
				error = false;
				total_cost += ret;
			}
		}

		if (ex == x && ey == y) break; // reached end of drag

		x += _railbit.xinc[trackdir];
		y += _railbit.yinc[trackdir];
		signal_ctr++;

		// toggle railbit for the non-diagonal tracks (|, -- tracks)
		if (!IsDiagonalTrackdir(trackdir)) trackdir ^= 1;
	}

	return (error) ? CMD_ERROR : total_cost;
}

/** Build signals on a stretch of track.
 * Stub for the unified signal builder/remover
 * @see CmdSignalTrackHelper
 */
int32 CmdBuildSignalTrack(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdSignalTrackHelper(x, y, flags, p1, p2);
}

/** Remove signals
 * @param x,y coordinates where signal is being deleted from
 * @param p1 track to remove signal from (Track enum)
 */
int32 CmdRemoveSingleSignal(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile = TileVirtXY(x, y);
	Track track = (Track)(p1 & 0x7);

	if (!ValParamTrackOrientation(track) || !IsTileType(tile, MP_RAILWAY) || !EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (!HasSignalOnTrack(tile, track)) // no signals on track?
		return CMD_ERROR;

	/* Only water can remove signals from anyone */
	if (_current_player != OWNER_WATER && !CheckTileOwnership(tile)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Do it? */
	if (flags & DC_EXEC) {
		_m[tile].m3 &= ~SignalOnTrack(track);

		/* removed last signal from tile? */
		if (GB(_m[tile].m3, 4, 4) == 0) {
			SB(_m[tile].m2, 4, 4, 0);
			SB(_m[tile].m5, 6, 2, RAIL_TYPE_NORMAL >> 6); // XXX >> because the constant is meant for direct application, not use with SB
			CLRBIT(_m[tile].m4, 3); // remove any possible semaphores
		}

		SetSignalsOnBothDir(tile, track);

		MarkTileDirtyByTile(tile);
	}

	return _price.remove_signals;
}

/** Remove signals on a stretch of track.
 * Stub for the unified signal builder/remover
 * @see CmdSignalTrackHelper
 */
int32 CmdRemoveSignalTrack(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdSignalTrackHelper(x, y, flags, p1, SETBIT(p2, 0));
}

typedef int32 DoConvertRailProc(TileIndex tile, uint totype, bool exec);

static int32 DoConvertRail(TileIndex tile, uint totype, bool exec)
{
	if (!CheckTileOwnership(tile) || !EnsureNoVehicle(tile))
		return CMD_ERROR;

	// tile is already of requested type?
	if ( GetRailType(tile) == totype)
		return CMD_ERROR;

	// change type.
	if (exec) {
		SB(_m[tile].m3, 4, 4, totype);
		MarkTileDirtyByTile(tile);
	}

	return _price.build_rail / 2;
}

extern int32 DoConvertStationRail(TileIndex tile, uint totype, bool exec);
extern int32 DoConvertStreetRail(TileIndex tile, uint totype, bool exec);
extern int32 DoConvertTunnelBridgeRail(TileIndex tile, uint totype, bool exec);

/** Convert one rail type to the other. You can convert normal rail to
 * monorail/maglev easily or vice-versa.
 * @param ex,ey end tile of rail conversion drag
 * @param p1 start tile of drag
 * @param p2 new railtype to convert to
 */
int32 CmdConvertRail(int ex, int ey, uint32 flags, uint32 p1, uint32 p2)
{
	int32 ret, cost, money;
	int sx, sy, x, y;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!ValParamRailtype(p2)) return CMD_ERROR;
	if (p1 > MapSize()) return CMD_ERROR;

	// make sure sx,sy are smaller than ex,ey
	sx = TileX(p1) * TILE_SIZE;
	sy = TileY(p1) * TILE_SIZE;
	if (ex < sx) intswap(ex, sx);
	if (ey < sy) intswap(ey, sy);

	money = GetAvailableMoneyForCommand();
	cost = 0;

	for (x = sx; x <= ex; x += TILE_SIZE) {
		for (y = sy; y <= ey; y += TILE_SIZE) {
			TileIndex tile = TileVirtXY(x, y);
			DoConvertRailProc *proc;

			if (IsTileType(tile, MP_RAILWAY)) proc = DoConvertRail;
			else if (IsTileType(tile, MP_STATION)) proc = DoConvertStationRail;
			else if (IsTileType(tile, MP_STREET)) proc = DoConvertStreetRail;
			else if (IsTileType(tile, MP_TUNNELBRIDGE)) proc = DoConvertTunnelBridgeRail;
			else continue;

			ret = proc(tile, p2, false);
			if (CmdFailed(ret)) continue;
			cost += ret;

			if (flags & DC_EXEC) {
				if ( (money -= ret) < 0) { _additional_cash_required = ret; return cost - ret; }
				proc(tile, p2, true);
			}
		}
	}

	return (cost == 0) ? CMD_ERROR : cost;
}

static int32 RemoveTrainDepot(TileIndex tile, uint32 flags)
{
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		int track = TrackdirToTrack(DiagdirToDiagTrackdir(GetDepotDirection(tile, TRANSPORT_RAIL)));

		DoDeleteDepot(tile);
		SetSignalsOnBothDir(tile, track);
	}

	return _price.remove_train_depot;
}

static int32 ClearTile_Track(TileIndex tile, byte flags)
{
	int32 cost;
	int32 ret;
	byte m5;

	m5 = _m[tile].m5;

	if (flags & DC_AUTO) {
		if (m5 & RAIL_TYPE_SPECIAL)
			return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);

		if (!IsTileOwner(tile, _current_player))
			return_cmd_error(STR_1024_AREA_IS_OWNED_BY_ANOTHER);

		return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);
	}

	cost = 0;

	switch (GetRailTileType(tile)) {
		case RAIL_TYPE_SIGNALS:
			if (_m[tile].m3 & _signals_table_both[0]) {
				ret = DoCommandByTile(tile, 0, 0, flags, CMD_REMOVE_SIGNALS);
				if (ret == CMD_ERROR) return CMD_ERROR;
				cost += ret;
			}
			if (_m[tile].m3 & _signals_table_both[3]) {
				ret = DoCommandByTile(tile, 3, 0, flags, CMD_REMOVE_SIGNALS);
				if (ret == CMD_ERROR) return CMD_ERROR;
				cost += ret;
			}

			m5 &= TRACK_BIT_MASK;
			if (!(flags & DC_EXEC)) {
				for (; m5 != 0; m5 >>= 1) if (m5 & 1) cost += _price.remove_rail;
				return cost;
			}
			/* FALLTHROUGH */

		case RAIL_TYPE_NORMAL: {
			uint i;

			for (i = 0; m5 != 0; i++, m5 >>= 1) {
				if (m5 & 1) {
					ret = DoCommandByTile(tile, 0, i, flags, CMD_REMOVE_SINGLE_RAIL);
					if (ret == CMD_ERROR) return CMD_ERROR;
					cost += ret;
				}
			}
			return cost;
		}

		case RAIL_TYPE_DEPOT_WAYPOINT:
			switch (m5 & RAIL_SUBTYPE_MASK) {
				case RAIL_SUBTYPE_DEPOT:
					return RemoveTrainDepot(tile, flags);

				case RAIL_SUBTYPE_WAYPOINT:
					return RemoveTrainWaypoint(tile, flags, false);

				default:
					return CMD_ERROR;
			}

		default:
			return CMD_ERROR;
	}
}



#include "table/track_land.h"

// used for presignals
static const SpriteID _signal_base_sprites[32] = {
	0x4FB,
	0x1323,
	0x1333,
	0x1343,

	// pbs signals
	0x1393,
	0x13A3,  // not used (yet?)
	0x13B3,  // not used (yet?)
	0x13C3,  // not used (yet?)

	// semaphores
	0x1353,
	0x1363,
	0x1373,
	0x1383,

	// pbs semaphores
	0x13D3,
	0x13E3,  // not used (yet?)
	0x13F3,  // not used (yet?)
	0x1403,  // not used (yet?)


	// mirrored versions
	0x4FB,
	0x1323,
	0x1333,
	0x1343,

	// pbs signals
	0x1393,
	0x13A3,  // not used (yet?)
	0x13B3,  // not used (yet?)
	0x13C3,  // not used (yet?)

	// semaphores
	0x1446,
	0x1456,
	0x1466,
	0x1476,

	// pbs semaphores
	0x14C6,
	0x14D6,  // not used (yet?)
	0x14E6,  // not used (yet?)
	0x14F6,  // not used (yet?)
};

// used to determine the side of the road for the signal
static const byte _signal_position[24] = {
	/* original: left side position */
	0x58,0x1E,0xE1,0xB9,0x01,0xA3,0x4B,0xEE,0x3B,0xD4,0x43,0xBD,
	/* patch: ride side position */
	0x1E,0xAC,0x64,0xE1,0x4A,0x10,0xEE,0xC5,0xDB,0x34,0x4D,0xB3
};

static void DrawSignalHelper(const TileInfo *ti, byte condition, uint32 image_and_pos)
{
	bool otherside = _opt.road_side & _patches.signal_side;

	uint v = _signal_position[(image_and_pos & 0xF) + (otherside ? 12 : 0)];
	uint x = ti->x | (v&0xF);
	uint y = ti->y | (v>>4);
	uint sprite = _signal_base_sprites[(_m[ti->tile].m4 & 0xF) + (otherside ? 0x10 : 0)] + (image_and_pos>>4) + ((condition != 0) ? 1 : 0);
	AddSortableSpriteToDraw(sprite, x, y, 1, 1, 10, GetSlopeZ(x,y));
}

static uint32 _drawtile_track_palette;


static void DrawTrackFence_NW(const TileInfo *ti)
{
	uint32 image = 0x515;
	if (ti->tileh != 0) {
		image = 0x519;
		if (!(ti->tileh & 2)) {
			image = 0x51B;
		}
	}
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x, ti->y+1, 16, 1, 4, ti->z);
}

static void DrawTrackFence_SE(const TileInfo *ti)
{
	uint32 image = 0x515;
	if (ti->tileh != 0) {
		image = 0x519;
		if (!(ti->tileh & 2)) {
			image = 0x51B;
		}
	}
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x, ti->y+15, 16, 1, 4, ti->z);
}

static void DrawTrackFence_NW_SE(const TileInfo *ti)
{
	DrawTrackFence_NW(ti);
	DrawTrackFence_SE(ti);
}

static void DrawTrackFence_NE(const TileInfo *ti)
{
	uint32 image = 0x516;
	if (ti->tileh != 0) {
		image = 0x51A;
		if (!(ti->tileh & 2)) {
			image = 0x51C;
		}
	}
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x+1, ti->y, 1, 16, 4, ti->z);
}

static void DrawTrackFence_SW(const TileInfo *ti)
{
	uint32 image = 0x516;
	if (ti->tileh != 0) {
		image = 0x51A;
		if (!(ti->tileh & 2)) {
			image = 0x51C;
		}
	}
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x+15, ti->y, 1, 16, 4, ti->z);
}

static void DrawTrackFence_NE_SW(const TileInfo *ti)
{
	DrawTrackFence_NE(ti);
	DrawTrackFence_SW(ti);
}

static void DrawTrackFence_NS_1(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & 1)
		z += 8;
	AddSortableSpriteToDraw(0x517 | _drawtile_track_palette,
		ti->x + 8, ti->y + 8, 1, 1, 4, z);
}

static void DrawTrackFence_NS_2(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & 4)
		z += 8;
	AddSortableSpriteToDraw(0x517 | _drawtile_track_palette,
		ti->x + 8, ti->y + 8, 1, 1, 4, z);
}

static void DrawTrackFence_WE_1(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & 8)
		z += 8;
	AddSortableSpriteToDraw(0x518 | _drawtile_track_palette,
		ti->x + 8, ti->y + 8, 1, 1, 4, z);
}

static void DrawTrackFence_WE_2(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & 2)
		z += 8;
	AddSortableSpriteToDraw(0x518 | _drawtile_track_palette,
		ti->x + 8, ti->y + 8, 1, 1, 4, z);
}

static void DetTrackDrawProc_Null(const TileInfo *ti)
{
	/* nothing should be here */
}

typedef void DetailedTrackProc(const TileInfo *ti);
DetailedTrackProc * const _detailed_track_proc[16] = {
	DetTrackDrawProc_Null,
	DetTrackDrawProc_Null,

	DrawTrackFence_NW,
	DrawTrackFence_SE,
	DrawTrackFence_NW_SE,

	DrawTrackFence_NE,
	DrawTrackFence_SW,
	DrawTrackFence_NE_SW,

	DrawTrackFence_NS_1,
	DrawTrackFence_NS_2,

	DrawTrackFence_WE_1,
	DrawTrackFence_WE_2,

	DetTrackDrawProc_Null,
	DetTrackDrawProc_Null,
	DetTrackDrawProc_Null,
	DetTrackDrawProc_Null,
};

static void DrawSpecialBuilding(uint32 image, uint32 offset,
                                TileInfo *ti,
                                byte x, byte y, byte z,
                                byte xsize, byte ysize, byte zsize)
{
	if (image & PALETTE_MODIFIER_COLOR)
		image |= _drawtile_track_palette;
	image += offset;
	if (_display_opt & DO_TRANS_BUILDINGS) // show transparent depots
		MAKE_TRANSPARENT(image);
	AddSortableSpriteToDraw(image, ti->x + x, ti->y + y, xsize, ysize, zsize, ti->z + z);
}

/**
 * Draw ground sprite and track bits
 * @param ti TileInfo
 * @param track TrackBits to draw
 * @param earth Draw as earth
 * @param snow Draw as snow
 * @param flat Always draw foundation
 */
void DrawTrackBits(TileInfo *ti, TrackBits track, bool earth, bool snow, bool flat)
{
	const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
	PalSpriteID image;
	bool junction = false;

	// Select the sprite to use.
	(image = rti->base_sprites.track_y, track == TRACK_BIT_DIAG2) ||
	(image++,                           track == TRACK_BIT_DIAG1) ||
	(image++,                           track == TRACK_BIT_UPPER) ||
	(image++,                           track == TRACK_BIT_LOWER) ||
	(image++,                           track == TRACK_BIT_RIGHT) ||
	(image++,                           track == TRACK_BIT_LEFT) ||
	(image++,                           track == (TRACK_BIT_DIAG1 | TRACK_BIT_DIAG2)) ||

	(image = rti->base_sprites.track_ns, track == (TRACK_BIT_UPPER | TRACK_BIT_LOWER)) ||
	(image++,                            track == (TRACK_BIT_LEFT | TRACK_BIT_RIGHT)) ||

	(junction = true, false) ||
	(image = rti->base_sprites.ground, !(track & (TRACK_BIT_RIGHT | TRACK_BIT_UPPER | TRACK_BIT_DIAG1))) ||
	(image++,                          !(track & (TRACK_BIT_LEFT | TRACK_BIT_LOWER | TRACK_BIT_DIAG1))) ||
	(image++,                          !(track & (TRACK_BIT_LEFT | TRACK_BIT_UPPER | TRACK_BIT_DIAG2))) ||
	(image++,                          !(track & (TRACK_BIT_RIGHT | TRACK_BIT_LOWER | TRACK_BIT_DIAG2))) ||
	(image++, true);

	if (ti->tileh != 0) {
		int foundation;

		if (flat) {
			foundation = ti->tileh;
		} else {
			foundation = GetRailFoundation(ti->tileh, track);
		}

		if (foundation != 0)
			DrawFoundation(ti, foundation);

		// DrawFoundation() modifies ti.
		// Default sloped sprites..
		if (ti->tileh != 0)
			image = _track_sloped_sprites[ti->tileh - 1] + rti->base_sprites.track_y;
	}

	if (earth) {
		image = (image & SPRITE_MASK) | PALETTE_TO_BARE_LAND; // Use brown palette
	} else if (snow) {
		image += rti->snow_offset;
	}

	DrawGroundSprite(image);

	// Draw track pieces individually for junction tiles
	if (junction) {
		if (track & TRACK_BIT_DIAG1) DrawGroundSprite(rti->base_sprites.single_y);
		if (track & TRACK_BIT_DIAG2) DrawGroundSprite(rti->base_sprites.single_x);
		if (track & TRACK_BIT_UPPER) DrawGroundSprite(rti->base_sprites.single_n);
		if (track & TRACK_BIT_LOWER) DrawGroundSprite(rti->base_sprites.single_s);
		if (track & TRACK_BIT_LEFT)  DrawGroundSprite(rti->base_sprites.single_w);
		if (track & TRACK_BIT_RIGHT) DrawGroundSprite(rti->base_sprites.single_e);
	}

	if (_debug_pbs_level >= 1) {
		byte pbs = PBSTileReserved(ti->tile) & track;
		if (pbs & TRACK_BIT_DIAG1) DrawGroundSprite(rti->base_sprites.single_y | PALETTE_CRASH);
		if (pbs & TRACK_BIT_DIAG2) DrawGroundSprite(rti->base_sprites.single_x | PALETTE_CRASH);
		if (pbs & TRACK_BIT_UPPER) DrawGroundSprite(rti->base_sprites.single_n | PALETTE_CRASH);
		if (pbs & TRACK_BIT_LOWER) DrawGroundSprite(rti->base_sprites.single_s | PALETTE_CRASH);
		if (pbs & TRACK_BIT_LEFT)  DrawGroundSprite(rti->base_sprites.single_w | PALETTE_CRASH);
		if (pbs & TRACK_BIT_RIGHT) DrawGroundSprite(rti->base_sprites.single_e | PALETTE_CRASH);
	}
}

static void DrawTile_Track(TileInfo *ti)
{
	byte m5;
	const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
	PalSpriteID image;

	_drawtile_track_palette = SPRITE_PALETTE(PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)));

	m5 = (byte)ti->map5;
	if (!(m5 & RAIL_TYPE_SPECIAL)) {
		bool earth = (_m[ti->tile].m2 & RAIL_MAP2LO_GROUND_MASK) == RAIL_GROUND_BROWN;
		bool snow = (_m[ti->tile].m2 & RAIL_MAP2LO_GROUND_MASK) == RAIL_GROUND_ICE_DESERT;

		DrawTrackBits(ti, m5 & TRACK_BIT_MASK, earth, snow, false);

		if (_display_opt & DO_FULL_DETAIL) {
			_detailed_track_proc[_m[ti->tile].m2 & RAIL_MAP2LO_GROUND_MASK](ti);
		}

		/* draw signals also? */
		if (!(ti->map5 & RAIL_TYPE_SIGNALS))
			return;

		{
			byte m23;

			m23 = (_m[ti->tile].m3 >> 4) | (_m[ti->tile].m2 & 0xF0);

#define HAS_SIGNAL(x) (m23 & (byte)(0x1 << (x)))
#define ISON_SIGNAL(x) (m23 & (byte)(0x10 << (x)))
#define MAYBE_DRAW_SIGNAL(x,y,z) if (HAS_SIGNAL(x)) DrawSignalHelper(ti, ISON_SIGNAL(x), ((y-0x4FB) << 4)|(z))

		if (!(m5 & TRACK_BIT_DIAG2)) {
			if (!(m5 & TRACK_BIT_DIAG1)) {
				if (m5 & TRACK_BIT_LEFT) {
					MAYBE_DRAW_SIGNAL(2, 0x509, 0);
					MAYBE_DRAW_SIGNAL(3, 0x507, 1);
				}
				if (m5 & TRACK_BIT_RIGHT) {
					MAYBE_DRAW_SIGNAL(0, 0x509, 2);
					MAYBE_DRAW_SIGNAL(1, 0x507, 3);
				}
				if (m5 & TRACK_BIT_UPPER) {
					MAYBE_DRAW_SIGNAL(3, 0x505, 4);
					MAYBE_DRAW_SIGNAL(2, 0x503, 5);
				}
				if (m5 & TRACK_BIT_LOWER) {
					MAYBE_DRAW_SIGNAL(1, 0x505, 6);
					MAYBE_DRAW_SIGNAL(0, 0x503, 7);
				}
			} else {
				MAYBE_DRAW_SIGNAL(3, 0x4FB, 8);
				MAYBE_DRAW_SIGNAL(2, 0x4FD, 9);
			}
		} else {
			MAYBE_DRAW_SIGNAL(3, 0x4FF, 10);
			MAYBE_DRAW_SIGNAL(2, 0x501, 11);
		}
		}
	} else {
		/* draw depots / waypoints */
		const DrawTrackSeqStruct *drss;
		byte type = m5 & 0x3F; // 0-3: depots, 4-5: waypoints

		if (!(m5 & (RAIL_TILE_TYPE_MASK&~RAIL_TYPE_SPECIAL)))
			/* XXX: There used to be "return;" here, but since I could not find out
			 * why this would ever occur, I put assert(0) here. Let's see if someone
			 * complains about it. If not, we'll remove this check. (Matthijs). */
			 assert(0);

		if (ti->tileh != 0) { DrawFoundation(ti, ti->tileh); }

		if (IsRailWaypoint(m5) && HASBIT(_m[ti->tile].m3, 4)) {
			// look for customization
			StationSpec *stat = GetCustomStation(STAT_CLASS_WAYP, _m[ti->tile].m4);

			if (stat) {
				DrawTileSeqStruct const *seq;
				// emulate station tile - open with building
				DrawTileSprites *cust = &stat->renderdata[2 + (m5 & 0x1)];
				uint32 relocation = GetCustomStationRelocation(stat, ComposeWaypointStation(ti->tile), 0);

				/* We don't touch the 0x8000 bit. In all this
				 * waypoint code, it is used to indicate that
				 * we should offset by railtype, but we always
				 * do that for custom ground sprites and never
				 * for station sprites. And in the drawing
				 * code, it is used to indicate that the sprite
				 * should be drawn in company colors, and it's
				 * up to the GRF file to decide that. */

				image = cust->ground_sprite;
				image += (image < _custom_sprites_base) ? rti->total_offset : GetRailType(ti->tile);

				DrawGroundSprite(image);

				foreach_draw_tile_seq(seq, cust->seq) {
					uint32 image = seq->image + relocation;
					DrawSpecialBuilding(image, 0, ti,
					                    seq->delta_x, seq->delta_y, seq->delta_z,
					                    seq->width, seq->height, seq->unk);
				}
				return;
			}
		}

		drss = _track_depot_layout_table[type];

		image = drss++->image;
		/* @note This is kind of an ugly hack, as the PALETTE_MODIFIER_COLOR indicates
	 	 * whether the sprite is railtype dependent. Rewrite this asap */
		if (image & PALETTE_MODIFIER_COLOR) image = (image & SPRITE_MASK) + rti->total_offset;

		// adjust ground tile for desert
		// (don't adjust for arctic depots, because snow in depots looks weird)
		// type >= 4 means waypoints
		if ((_m[ti->tile].m2 & RAIL_MAP2LO_GROUND_MASK) == RAIL_GROUND_ICE_DESERT && (_opt.landscape == LT_DESERT || type >= 4)) {
			if (image != SPR_FLAT_GRASS_TILE)
				image += rti->snow_offset; // tile with tracks
			else
				image = SPR_FLAT_SNOWY_TILE; // flat ground
		}

		DrawGroundSprite(image);

		if (_debug_pbs_level >= 1) {
			byte pbs = PBSTileReserved(ti->tile);
			if (pbs & TRACK_BIT_DIAG1) DrawGroundSprite(rti->base_sprites.single_y | PALETTE_CRASH);
			if (pbs & TRACK_BIT_DIAG2) DrawGroundSprite(rti->base_sprites.single_x | PALETTE_CRASH);
			if (pbs & TRACK_BIT_UPPER) DrawGroundSprite(rti->base_sprites.single_n | PALETTE_CRASH);
			if (pbs & TRACK_BIT_LOWER) DrawGroundSprite(rti->base_sprites.single_s | PALETTE_CRASH);
			if (pbs & TRACK_BIT_LEFT)  DrawGroundSprite(rti->base_sprites.single_w | PALETTE_CRASH);
			if (pbs & TRACK_BIT_RIGHT) DrawGroundSprite(rti->base_sprites.single_e | PALETTE_CRASH);
		}

		while ((image = drss->image) != 0) {
			DrawSpecialBuilding(image, type < 4 ? rti->total_offset : 0, ti,
			                    drss->subcoord_x, drss->subcoord_y, 0,
			                    drss->width, drss->height, 0x17);
			drss++;
		}
	}
}

void DrawTrainDepotSprite(int x, int y, int image, int railtype)
{
	uint32 ormod, img;
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	const DrawTrackSeqStruct *dtss;

	ormod = PLAYER_SPRITE_COLOR(_local_player);

	dtss = _track_depot_layout_table[image];

	x+=33;
	y+=17;

	img = dtss++->image;
	/* @note This is kind of an ugly hack, as the PALETTE_MODIFIER_COLOR indicates
	 * whether the sprite is railtype dependent. Rewrite this asap */
	if (img & PALETTE_MODIFIER_COLOR) img = (img & SPRITE_MASK) + rti->total_offset;
	DrawSprite(img, x, y);

	for (; dtss->image != 0; dtss++) {
		Point pt = RemapCoords(dtss->subcoord_x, dtss->subcoord_y, 0);
		image = dtss->image;
		if (image & PALETTE_MODIFIER_COLOR) image |= ormod;
		DrawSprite(image + rti->total_offset, x + pt.x, y + pt.y);
	}
}

void DrawDefaultWaypointSprite(int x, int y, int railtype)
{
	const DrawTrackSeqStruct *dtss = _track_depot_layout_table[4];
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	uint32 img;

	img = dtss++->image;
	if (img & PALETTE_MODIFIER_COLOR) img = (img & SPRITE_MASK) + rti->total_offset;
	DrawSprite(img, x, y);

	for (; dtss->image != 0; dtss++) {
		Point pt = RemapCoords(dtss->subcoord_x, dtss->subcoord_y, 0);
		img = dtss->image;
		if (img & PALETTE_MODIFIER_COLOR) img |= PLAYER_SPRITE_COLOR(_local_player);
		DrawSprite(img, x + pt.x, y + pt.y);
	}
}

typedef struct SetSignalsData {
	int cur;
	int cur_stack;
	bool stop;
	bool has_presignal;

	bool has_pbssignal;
		// lowest 2 bits = amount of pbs signals in the block, clamped at 2
		// bit 2 = there is a pbs entry signal in this block
		// bit 3 = there is a pbs exit signal in this block

	// presignal info
	int presignal_exits;
	int presignal_exits_free;

	// these are used to keep track of the signals that change.
	byte bit[NUM_SSD_ENTRY];
	TileIndex tile[NUM_SSD_ENTRY];

	int pbs_cur;
	// these are used to keep track of all signals in the block
	TileIndex pbs_tile[NUM_SSD_ENTRY];

	// these are used to keep track of the stack that modifies presignals recursively
	TileIndex next_tile[NUM_SSD_STACK];
	byte next_dir[NUM_SSD_STACK];

} SetSignalsData;

static bool SetSignalsEnumProc(TileIndex tile, SetSignalsData *ssd, int track, uint length, byte *state)
{
	// the tile has signals?
	if (IsTileType(tile, MP_RAILWAY)) {
		if (HasSignalOnTrack(tile, TrackdirToTrack(track))) {
			if ((_m[tile].m3 & _signals_table[track]) != 0) {
				// yes, add the signal to the list of signals
				if (ssd->cur != NUM_SSD_ENTRY) {
					ssd->tile[ssd->cur] = tile; // remember the tile index
					ssd->bit[ssd->cur] = track; // and the controlling bit number
					ssd->cur++;
				}

			if (PBSIsPbsSignal(tile, ReverseTrackdir(track)))
				SETBIT(ssd->has_pbssignal, 2);

				// remember if this block has a presignal.
				ssd->has_presignal |= (_m[tile].m4&1);
			}

			if (PBSIsPbsSignal(tile, ReverseTrackdir(track)) || PBSIsPbsSignal(tile, track)) {
				byte num = ssd->has_pbssignal & 3;
				num = clamp(num + 1, 0, 2);
				ssd->has_pbssignal &= ~3;
				ssd->has_pbssignal |= num;
			}

			if ((_m[tile].m3 & _signals_table_both[track]) != 0) {
				ssd->pbs_tile[ssd->pbs_cur] = tile; // remember the tile index
				ssd->pbs_cur++;
			}

			if (_m[tile].m3&_signals_table_other[track]) {
				if (_m[tile].m4&2) {
					// this is an exit signal that points out from the segment
					ssd->presignal_exits++;
					if ((_m[tile].m2&_signals_table_other[track]) != 0)
						ssd->presignal_exits_free++;
				}
				if (PBSIsPbsSignal(tile, track))
					SETBIT(ssd->has_pbssignal, 3);
			}

			return true;
		} else if (IsTileDepotType(tile, TRANSPORT_RAIL))
			return true; // don't look further if the tile is a depot
	}
	return false;
}

/* Struct to parse data from VehicleFromPos to SignalVehicleCheckProc */
typedef struct SignalVehicleCheckStruct {
	TileIndex tile;
	uint track;
} SignalVehicleCheckStruct;

static void *SignalVehicleCheckProc(Vehicle *v, void *data)
{
	SignalVehicleCheckStruct *dest = data;
	TileIndex tile;

	if (v->type != VEH_Train)
		return NULL;

	/* Find the tile outside the tunnel, for signalling */
	if (v->u.rail.track == 0x40)
		tile = GetVehicleOutOfTunnelTile(v);
	else
		tile = v->tile;

	/* Wrong tile, or no train? Not a match */
	if (tile != dest->tile)
		return NULL;

	/* Are we on the same piece of track? */
	if (dest->track & (v->u.rail.track + (v->u.rail.track<<8)))
		return v;

	return NULL;
}

/* Special check for SetSignalsAfterProc, to see if there is a vehicle on this tile */
bool SignalVehicleCheck(TileIndex tile, uint track)
{
	SignalVehicleCheckStruct dest;

	dest.tile = tile;
	dest.track = track;

	/** @todo "Hackish" fix for the tunnel problems. This is needed because a tunnel
	 * is some kind of invisible black hole, and there is some special magic going
	 * on in there. This 'workaround' can be removed once the maprewrite is done.
	 */
	if (GetTileType(tile) == MP_TUNNELBRIDGE && GB(_m[tile].m5, 4, 4) == 0) {
		// It is a tunnel we're checking, we need to do some special stuff
		// because VehicleFromPos will not find the vihicle otherwise
		byte direction = GB(_m[tile].m5, 0, 2);
		FindLengthOfTunnelResult flotr;
		flotr = FindLengthOfTunnel(tile, direction);
		dest.track = 1 << (direction & 1); // get the trackbit the vehicle would have if it has not entered the tunnel yet (ie is still visible)

		// check for a vehicle with that trackdir on the start tile of the tunnel
		if (VehicleFromPos(tile, &dest, SignalVehicleCheckProc) != NULL) return true;

		// check for a vehicle with that trackdir on the end tile of the tunnel
		if (VehicleFromPos(flotr.tile, &dest, SignalVehicleCheckProc) != NULL) return true;

		// now check all tiles from start to end for a "hidden" vehicle
		// NOTE: the hashes for tiles may overlap, so this could maybe be optimised a bit by not checking every tile?
		dest.track = 0x40; // trackbit for vehicles "hidden" inside a tunnel
		for (; tile != flotr.tile; tile += TileOffsByDir(direction)) {
			if (VehicleFromPos(tile, &dest, SignalVehicleCheckProc) != NULL)
				return true;
		}

		// no vehicle found
		return false;
	}

	return VehicleFromPos(tile, &dest, SignalVehicleCheckProc) != NULL;
}

static void SetSignalsAfterProc(TrackPathFinder *tpf)
{
	SetSignalsData *ssd = tpf->userdata;
	TrackPathFinderLink *link;
	uint offs;
	uint i;

	ssd->stop = false;

	/* Go through all the PF tiles */
	for (i = 0; i < lengthof(tpf->hash_head); i++) {
		/* Empty hash item */
		if (tpf->hash_head[i] == 0)
			continue;

		/* If 0x8000 is not set, there is only 1 item */
		if (!(tpf->hash_head[i] & 0x8000)) {
			/* Check if there is a vehicle on this tile */
			if (SignalVehicleCheck(tpf->hash_tile[i], tpf->hash_head[i])) {
				ssd->stop = true;
				return;
			}
		} else {
			/* There are multiple items, where hash_tile points to the first item in the list */
			offs = tpf->hash_tile[i];
			do {
				/* Find the next item */
				link = PATHFIND_GET_LINK_PTR(tpf, offs);
				/* Check if there is a vehicle on this tile */
				if (SignalVehicleCheck(link->tile, link->flags)) {
					ssd->stop = true;
					return;
				}
				/* Goto the next item */
			} while ((offs=link->next) != 0xFFFF);
		}
	}
}

static const byte _dir_from_track[14] = {
	0,1,0,1,2,1, 0,0,
	2,3,3,2,3,0,
};


static void ChangeSignalStates(SetSignalsData *ssd)
{
	int i;

	// thinking about presignals...
	// the presignal is green if,
	//   if no train is in the segment AND
	//   there is at least one green exit signal OR
	//   there are no exit signals in the segment

	// convert the block to pbs, if needed
	if (_patches.auto_pbs_placement && !(ssd->stop) && (ssd->has_pbssignal == 0xE) && !ssd->has_presignal && (ssd->presignal_exits == 0)) // 0xE means at least 2 pbs signals, and at least 1 entry and 1 exit, see comments ssd->has_pbssignal
	for(i=0; i!=ssd->pbs_cur; i++) {
		TileIndex tile = ssd->pbs_tile[i];
		SB(_m[tile].m4, 0, 3, SIGTYPE_PBS);
		MarkTileDirtyByTile(tile);
	};

	// then mark the signals in the segment accordingly
	for(i=0; i!=ssd->cur; i++) {
		TileIndex tile = ssd->tile[i];
		byte bit = _signals_table[ssd->bit[i]];
		uint16 m2 = _m[tile].m2;

		// presignals don't turn green if there is at least one presignal exit and none are free
		if (_m[tile].m4 & 1) {
			int ex = ssd->presignal_exits, exfree = ssd->presignal_exits_free;

			// subtract for dual combo signals so they don't count themselves
			if (_m[tile].m4&2 && _m[tile].m3&_signals_table_other[ssd->bit[i]]) {
				ex--;
				if ((_m[tile].m2&_signals_table_other[ssd->bit[i]]) != 0) exfree--;
			}

			// if we have exits and none are free, make red.
			if (ex && !exfree) goto make_red;
		}

		// check if the signal is unaffected.
		if (ssd->stop) {
make_red:
			// turn red
			if ( (bit&m2) == 0 )
				continue;
		} else {
			// turn green
			if ( (bit&m2) != 0 )
				continue;
		}

		/* Update signals on the other side of this exit-combo signal; it changed. */
		if (_m[tile].m4 & 2 ) {
			if (ssd->cur_stack != NUM_SSD_STACK) {
				ssd->next_tile[ssd->cur_stack] = tile;
				ssd->next_dir[ssd->cur_stack] = _dir_from_track[ssd->bit[i]];
				ssd->cur_stack++;
			} else {
				printf("NUM_SSD_STACK too small\n"); /// @todo WTF is this???
			}
		}

		// it changed, so toggle it
		_m[tile].m2 = m2 ^ bit;
		MarkTileDirtyByTile(tile);
	}
}


bool UpdateSignalsOnSegment(TileIndex tile, byte direction)
{
	SetSignalsData ssd;
	int result = -1;

	ssd.cur_stack = 0;
	direction>>=1;

	for(;;) {
		// go through one segment and update all signals pointing into that segment.
		ssd.cur = ssd.pbs_cur = ssd.presignal_exits = ssd.presignal_exits_free = 0;
		ssd.has_presignal = false;
		ssd.has_pbssignal = false;

		FollowTrack(tile, 0xC000 | TRANSPORT_RAIL, direction, (TPFEnumProc*)SetSignalsEnumProc, SetSignalsAfterProc, &ssd);
		ChangeSignalStates(&ssd);

		// remember the result only for the first iteration.
		if (result < 0) result = ssd.stop;

		// if any exit signals were changed, we need to keep going to modify the stuff behind those.
		if(!ssd.cur_stack)
			break;

		// one or more exit signals were changed, so we need to update another segment too.
		tile = ssd.next_tile[--ssd.cur_stack];
		direction = ssd.next_dir[ssd.cur_stack];
	}

	return (bool)result;
}

void SetSignalsOnBothDir(TileIndex tile, byte track)
{
	static const byte _search_dir_1[6] = {1, 3, 1, 3, 5, 3};
	static const byte _search_dir_2[6] = {5, 7, 7, 5, 7, 1};

	UpdateSignalsOnSegment(tile, _search_dir_1[track]);
	UpdateSignalsOnSegment(tile, _search_dir_2[track]);
}

static uint GetSlopeZ_Track(TileInfo *ti)
{
	uint z = ti->z;
	int th = ti->tileh;

	// check if it's a foundation
	if (ti->tileh != 0) {
		if ((ti->map5 & 0x80) == 0) {
			uint f = GetRailFoundation(ti->tileh, ti->map5 & 0x3F);
			if (f != 0) {
				if (f < 15) {
					// leveled foundation
					return z + 8;
				}
				// inclined foundation
				th = _inclined_tileh[f - 15];
			}
		} else if ((ti->map5 & 0xC0) == 0xC0) {
			// depot or waypoint
			return z + 8;
		}
		return GetPartialZ(ti->x&0xF, ti->y&0xF, th) + z;
	}
	return z;
}

static uint GetSlopeTileh_Track(const TileInfo *ti)
{
	// check if it's a foundation
	if (ti->tileh != 0) {
		if ((ti->map5 & 0x80) == 0) {
			uint f = GetRailFoundation(ti->tileh, ti->map5 & 0x3F);
			if (f != 0) {
				if (f < 15) {
					// leveled foundation
					return 0;
				}
				// inclined foundation
				return _inclined_tileh[f - 15];
			}
		} else if ((ti->map5 & 0xC0) == 0xC0) {
			// depot or waypoint
			return 0;
		}
	}
	return ti->tileh;
}

static void GetAcceptedCargo_Track(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void AnimateTile_Track(TileIndex tile)
{
	/* not used */
}

static void TileLoop_Track(TileIndex tile)
{
	byte a2;
	byte rail;
	uint16 m2;
	byte owner;

	m2 = GB(_m[tile].m2, 0, 4);

	/* special code for alps landscape */
	if (_opt.landscape == LT_HILLY) {
		/* convert into snow? */
		if (GetTileZ(tile) > _opt.snow_line) {
			a2 = RAIL_GROUND_ICE_DESERT;
			goto modify_me;
		}

	/* special code for desert landscape */
	} else if (_opt.landscape == LT_DESERT) {
		/* convert into desert? */
		if (GetMapExtraBits(tile) == 1) {
			a2 = RAIL_GROUND_ICE_DESERT;
			goto modify_me;
		}
	}

	// Don't continue tile loop for depots
	if (_m[tile].m5 & RAIL_TYPE_SPECIAL)
		return;

	a2 = RAIL_GROUND_GREEN;

	if (m2 != RAIL_GROUND_BROWN) { /* wait until bottom is green */
		/* determine direction of fence */
		rail = _m[tile].m5 & TRACK_BIT_MASK;

		if (rail == TRACK_BIT_UPPER) {
			a2 = RAIL_GROUND_FENCE_HORIZ1;
		} else if (rail == TRACK_BIT_LOWER) {
			a2 = RAIL_GROUND_FENCE_HORIZ2;
		} else if (rail == TRACK_BIT_LEFT) {
			a2 = RAIL_GROUND_FENCE_VERT1;
		} else if (rail == TRACK_BIT_RIGHT) {
			a2 = RAIL_GROUND_FENCE_VERT2;
		} else {
			owner = GetTileOwner(tile);

			if ( (!(rail&(TRACK_BIT_DIAG2|TRACK_BIT_UPPER|TRACK_BIT_LEFT)) && (rail&TRACK_BIT_DIAG1)) || rail==(TRACK_BIT_LOWER|TRACK_BIT_RIGHT)) {
				if (!IsTileType(tile + TileDiffXY(0, -1), MP_RAILWAY) ||
						!IsTileOwner(tile + TileDiffXY(0, -1), owner) ||
						(_m[tile + TileDiffXY(0, -1)].m5 == TRACK_BIT_UPPER || _m[tile + TileDiffXY(0, -1)].m5 == TRACK_BIT_LEFT))
							a2 = RAIL_GROUND_FENCE_NW;
			}

			if ( (!(rail&(TRACK_BIT_DIAG2|TRACK_BIT_LOWER|TRACK_BIT_RIGHT)) && (rail&TRACK_BIT_DIAG1)) || rail==(TRACK_BIT_UPPER|TRACK_BIT_LEFT)) {
				if (!IsTileType(tile + TileDiffXY(0, 1), MP_RAILWAY) ||
						!IsTileOwner(tile + TileDiffXY(0, 1), owner) ||
						(_m[tile + TileDiffXY(0, 1)].m5 == TRACK_BIT_LOWER || _m[tile + TileDiffXY(0, 1)].m5 == TRACK_BIT_RIGHT))
							a2 = (a2 == RAIL_GROUND_FENCE_NW) ? RAIL_GROUND_FENCE_SENW : RAIL_GROUND_FENCE_SE;
			}

			if ( (!(rail&(TRACK_BIT_DIAG1|TRACK_BIT_UPPER|TRACK_BIT_RIGHT)) && (rail&TRACK_BIT_DIAG2)) || rail==(TRACK_BIT_LOWER|TRACK_BIT_LEFT)) {
				if (!IsTileType(tile + TileDiffXY(-1, 0), MP_RAILWAY) ||
						!IsTileOwner(tile + TileDiffXY(-1, 0), owner) ||
						(_m[tile + TileDiffXY(-1, 0)].m5 == TRACK_BIT_UPPER || _m[tile + TileDiffXY(-1, 0)].m5 == TRACK_BIT_RIGHT))
							a2 = RAIL_GROUND_FENCE_NE;
			}

			if ( (!(rail&(TRACK_BIT_DIAG1|TRACK_BIT_LOWER|TRACK_BIT_LEFT)) && (rail&TRACK_BIT_DIAG2)) || rail==(TRACK_BIT_UPPER|TRACK_BIT_RIGHT)) {
				if (!IsTileType(tile + TileDiffXY(1, 0), MP_RAILWAY) ||
						!IsTileOwner(tile + TileDiffXY(1, 0), owner) ||
						(_m[tile + TileDiffXY(1, 0)].m5 == TRACK_BIT_LOWER || _m[tile + TileDiffXY(1, 0)].m5 == TRACK_BIT_LEFT))
							a2 = (a2 == RAIL_GROUND_FENCE_NE) ? RAIL_GROUND_FENCE_NESW : RAIL_GROUND_FENCE_SW;
			}
		}
	}

modify_me:;
	/* tile changed? */
	if ( m2 != a2) {
		_m[tile].m2 = (_m[tile].m2 & ~RAIL_MAP2LO_GROUND_MASK) | a2;
		MarkTileDirtyByTile(tile);
	}
}


static uint32 GetTileTrackStatus_Track(TileIndex tile, TransportType mode)
{
	byte m5, a;
	uint16 b;
	uint32 ret;

	if (mode != TRANSPORT_RAIL)
		return 0;

	m5 = _m[tile].m5;

	if (!(m5 & RAIL_TYPE_SPECIAL)) {
		ret = (m5 | (m5 << 8)) & 0x3F3F;
		if (!(m5 & RAIL_TYPE_SIGNALS)) {
			if ( (ret & 0xFF) == 3)
			/* Diagonal crossing? */
				ret |= 0x40;
		} else {
			/* has_signals */

			a = _m[tile].m3;
			b = _m[tile].m2;

			b &= a;

			/* When signals are not present (in neither
			 * direction), we pretend them to be green. (So if
			 * signals are only one way, the other way will
			 * implicitely become `red' */
			if ((a & 0xC0) == 0) { b |= 0xC0; }
			if ((a & 0x30) == 0) { b |= 0x30; }

			if ( (b & 0x80) == 0)	ret |= 0x10070000;
			if ( (b & 0x40) == 0)	ret |= 0x7100000;
			if ( (b & 0x20) == 0)	ret |= 0x20080000;
			if ( (b & 0x10) == 0)	ret |= 0x8200000;
		}
	} else if (m5 & 0x40) {
		static const byte _train_spec_tracks[6] = {1,2,1,2,1,2};
		m5 = _train_spec_tracks[m5 & 0x3F];
		ret = (m5 << 8) + m5;
	} else
		return 0;
	return ret;
}

static void ClickTile_Track(TileIndex tile)
{
	if (IsTileDepotType(tile, TRANSPORT_RAIL))
		ShowTrainDepotWindow(tile);
	else if (IsRailWaypoint(_m[tile].m5))
		ShowRenameWaypointWindow(GetWaypointByTile(tile));

}

static void GetTileDesc_Track(TileIndex tile, TileDesc *td)
{
	td->owner = GetTileOwner(tile);
	switch (GetRailTileType(tile)) {
		case RAIL_TYPE_NORMAL:
			td->str = STR_1021_RAILROAD_TRACK;
			break;

		case RAIL_TYPE_SIGNALS: {
			const StringID signal_type[7] = {
				STR_RAILROAD_TRACK_WITH_NORMAL_SIGNALS,
				STR_RAILROAD_TRACK_WITH_PRESIGNALS,
				STR_RAILROAD_TRACK_WITH_EXITSIGNALS,
				STR_RAILROAD_TRACK_WITH_COMBOSIGNALS,
				STR_RAILROAD_TRACK_WITH_PBSSIGNALS,
				STR_NULL, STR_NULL
			};

			td->str = signal_type[GB(_m[tile].m4, 0, 3)];
			break;
		}

		case RAIL_TYPE_DEPOT_WAYPOINT:
		default:
			td->str = ((_m[tile].m5 & RAIL_SUBTYPE_MASK) == RAIL_SUBTYPE_DEPOT) ?
				STR_1023_RAILROAD_TRAIN_DEPOT : STR_LANDINFO_WAYPOINT;
			break;
	}
}

static void ChangeTileOwner_Track(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != 255) {
		SetTileOwner(tile, new_player);
	}	else {
		DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static const byte _fractcoords_behind[4] = { 0x8F, 0x8, 0x80, 0xF8 };
static const byte _fractcoords_enter[4] = { 0x8A, 0x48, 0x84, 0xA8 };
static const byte _deltacoord_leaveoffset[8] = {
	-1,  0,  1,  0, /* x */
	 0,  1,  0, -1  /* y */
};
static const byte _enter_directions[4] = {5, 7, 1, 3};
static const byte _leave_directions[4] = {1, 3, 5, 7};
static const byte _depot_track_mask[4] = {1, 2, 1, 2};

static uint32 VehicleEnter_Track(Vehicle *v, TileIndex tile, int x, int y)
{
	byte fract_coord;
	byte fract_coord_leave;
	int dir;
	int length;

	// this routine applies only to trains in depot tiles
	if (v->type != VEH_Train || !IsTileDepotType(tile, TRANSPORT_RAIL))
		return 0;

	/* depot direction */
	dir = GetDepotDirection(tile, TRANSPORT_RAIL);

	/* calculate the point where the following wagon should be activated */
	/* this depends on the length of the current vehicle */
	length = v->u.rail.cached_veh_length;

	fract_coord_leave =
		((_fractcoords_enter[dir] & 0x0F) +				// x
			(length + 1) * _deltacoord_leaveoffset[dir]) +
		(((_fractcoords_enter[dir] >> 4) +				// y
			((length + 1) * _deltacoord_leaveoffset[dir+4])) << 4);

	fract_coord = (x & 0xF) + ((y & 0xF) << 4);

	if (_fractcoords_behind[dir] == fract_coord) {
		/* make sure a train is not entering the tile from behind */
		return 8;
	} else if (_fractcoords_enter[dir] == fract_coord) {
		if (_enter_directions[dir] == v->direction) {
			/* enter the depot */
			if (v->next == NULL)
				PBSClearTrack(v->tile, FIND_FIRST_BIT(v->u.rail.track));
			v->u.rail.track = 0x80,
			v->vehstatus |= VS_HIDDEN; /* hide it */
			v->direction ^= 4;
			if (v->next == NULL)
				TrainEnterDepot(v, tile);
			v->tile = tile;
			InvalidateWindow(WC_VEHICLE_DEPOT, tile);
			return 4;
		}
	} else if (fract_coord_leave == fract_coord) {
		if (_leave_directions[dir] == v->direction) {
			/* leave the depot? */
			if ((v=v->next) != NULL) {
				v->vehstatus &= ~VS_HIDDEN;
				v->u.rail.track = _depot_track_mask[dir];
				assert(v->u.rail.track);
			}
		}
	}

	return 0;
}

void InitializeRail(void)
{
	_last_built_train_depot_tile = 0;
}

const TileTypeProcs _tile_type_rail_procs = {
	DrawTile_Track,						/* draw_tile_proc */
	GetSlopeZ_Track,					/* get_slope_z_proc */
	ClearTile_Track,					/* clear_tile_proc */
	GetAcceptedCargo_Track,		/* get_accepted_cargo_proc */
	GetTileDesc_Track,				/* get_tile_desc_proc */
	GetTileTrackStatus_Track,	/* get_tile_track_status_proc */
	ClickTile_Track,					/* click_tile_proc */
	AnimateTile_Track,				/* animate_tile_proc */
	TileLoop_Track,						/* tile_loop_clear */
	ChangeTileOwner_Track,		/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	VehicleEnter_Track,				/* vehicle_enter_tile_proc */
	NULL,											/* vehicle_leave_tile_proc */
	GetSlopeTileh_Track,			/* get_slope_tileh_proc */
};
