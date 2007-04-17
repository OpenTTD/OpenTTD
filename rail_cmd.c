/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "debug.h"
#include "functions.h"
#include "rail_map.h"
#include "road_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "town_map.h"
#include "tunnel_map.h"
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
#include "waypoint.h"
#include "window.h"
#include "rail.h"
#include "railtypes.h" // include table for railtypes
#include "newgrf.h"
#include "yapf/yapf.h"
#include "newgrf_callbacks.h"
#include "newgrf_station.h"
#include "train.h"

const byte _track_sloped_sprites[14] = {
	14, 15, 22, 13,
	 0, 21, 17, 12,
	23,  0, 18, 20,
	19, 16
};


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



/* MAP2 byte:    abcd???? => Signal On? Same coding as map3lo
 * MAP3LO byte:  abcd???? => Signal Exists?
 *               a and b are for diagonals, upper and left,
 *               one for each direction. (ie a == NE->SW, b ==
 *               SW->NE, or v.v., I don't know. b and c are
 *               similar for lower and right.
 * MAP2 byte:    ????abcd => Type of ground.
 * MAP3LO byte:  ????abcd => Type of rail.
 * MAP5:         00abcdef => rail
 *               01abcdef => rail w/ signals
 *               10uuuuuu => unused
 *               11uuuudd => rail depot
 */

static bool CheckTrackCombination(TileIndex tile, TrackBits to_build, uint flags)
{
	TrackBits current; /* The current track layout */
	TrackBits future; /* The track layout we want to build */
	_error_message = STR_1001_IMPOSSIBLE_TRACK_COMBINATION;

	if (!IsPlainRailTile(tile)) return false;

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
	if (flags & DC_NO_RAIL_OVERLAP || HasSignals(tile)) {
		/* If we are not allowed to overlap (flag is on for ai players or we have
		 * signals on the tile), check that */
		return future == TRACK_BIT_HORZ || future == TRACK_BIT_VERT;
	} else {
		/* Normally, we may overlap and any combination is valid */
		return true;
	}
}


static const TrackBits _valid_tileh_slopes[][15] = {

// set of normal ones
{
	TRACK_BIT_ALL,
	TRACK_BIT_RIGHT,
	TRACK_BIT_UPPER,
	TRACK_BIT_X,

	TRACK_BIT_LEFT,
	0,
	TRACK_BIT_Y,
	TRACK_BIT_LOWER,

	TRACK_BIT_LOWER,
	TRACK_BIT_Y,
	0,
	TRACK_BIT_LEFT,

	TRACK_BIT_X,
	TRACK_BIT_UPPER,
	TRACK_BIT_RIGHT,
},

// allowed rail for an evenly raised platform
{
	0,
	TRACK_BIT_LEFT,
	TRACK_BIT_LOWER,
	TRACK_BIT_Y | TRACK_BIT_LOWER | TRACK_BIT_LEFT,

	TRACK_BIT_RIGHT,
	TRACK_BIT_ALL,
	TRACK_BIT_X | TRACK_BIT_LOWER | TRACK_BIT_RIGHT,
	TRACK_BIT_ALL,

	TRACK_BIT_UPPER,
	TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_LEFT,
	TRACK_BIT_ALL,
	TRACK_BIT_ALL,

	TRACK_BIT_Y | TRACK_BIT_UPPER | TRACK_BIT_RIGHT,
	TRACK_BIT_ALL,
	TRACK_BIT_ALL
}
};

uint GetRailFoundation(Slope tileh, TrackBits bits)
{
	uint i;

	if (!IsSteepSlope(tileh)) {
		if ((~_valid_tileh_slopes[0][tileh] & bits) == 0) return 0;
		if ((~_valid_tileh_slopes[1][tileh] & bits) == 0) return tileh;
	}

	switch (bits) {
		default: NOT_REACHED();
		case TRACK_BIT_X: i = 0; break;
		case TRACK_BIT_Y: i = 1; break;
		case TRACK_BIT_LEFT:  return 15 + 8 + (tileh == SLOPE_STEEP_W ? 4 : 0);
		case TRACK_BIT_LOWER: return 15 + 8 + (tileh == SLOPE_STEEP_S ? 5 : 1);
		case TRACK_BIT_RIGHT: return 15 + 8 + (tileh == SLOPE_STEEP_E ? 6 : 2);
		case TRACK_BIT_UPPER: return 15 + 8 + (tileh == SLOPE_STEEP_N ? 7 : 3);
	}
	switch (tileh) {
		case SLOPE_W:
		case SLOPE_STEEP_W: i += 0; break;
		case SLOPE_S:
		case SLOPE_STEEP_S: i += 2; break;
		case SLOPE_E:
		case SLOPE_STEEP_E: i += 4; break;
		case SLOPE_N:
		case SLOPE_STEEP_N: i += 6; break;
		default: return 0;
	}
	return i + 15;
}


static uint32 CheckRailSlope(Slope tileh, TrackBits rail_bits, TrackBits existing, TileIndex tile)
{
	if (IsSteepSlope(tileh)) {
		if (_patches.build_on_slopes && existing == 0) {
			TrackBits valid = TRACK_BIT_CROSS | (HASBIT(1 << SLOPE_STEEP_W | 1 << SLOPE_STEEP_E, tileh) ? TRACK_BIT_VERT : TRACK_BIT_HORZ);
			if (valid & rail_bits) return _price.terraform;
		}
	} else {
		rail_bits |= existing;

		// don't allow building on the lower side of a coast
		if (IsTileType(tile, MP_WATER) &&
				~_valid_tileh_slopes[1][tileh] & rail_bits) {
			return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);
		}

		// no special foundation
		if ((~_valid_tileh_slopes[0][tileh] & rail_bits) == 0)
			return 0;

		if ((~_valid_tileh_slopes[1][tileh] & rail_bits) == 0 || ( // whole tile is leveled up
					(rail_bits == TRACK_BIT_X || rail_bits == TRACK_BIT_Y) &&
					(tileh == SLOPE_W || tileh == SLOPE_S || tileh == SLOPE_E || tileh == SLOPE_N)
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
 * @param tile tile  to build on
 * @param p1 railtype of being built piece (normal, mono, maglev)
 * @param p2 rail track to build
 */
int32 CmdBuildSingleRail(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Slope tileh;
	RailType railtype;
	Track track;
	TrackBits trackbit;
	int32 cost = 0;
	int32 ret;

	if (!ValParamRailtype(p1) || !ValParamTrackOrientation(p2)) return CMD_ERROR;
	railtype = (RailType)p1;
	track = (Track)p2;

	tileh = GetTileSlope(tile, NULL);
	trackbit = TrackToTrackBits(track);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	switch (GetTileType(tile)) {
		case MP_TUNNELBRIDGE:
			if (!IsBridge(tile) ||
					!IsBridgeMiddle(tile) ||
					AxisToTrackBits(OtherAxis(GetBridgeAxis(tile))) != trackbit) {
				// Get detailed error message
				return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}

			if (IsClearUnderBridge(tile)) {
				ret = CheckRailSlope(tileh, trackbit, 0, tile);
				if (CmdFailed(ret)) return ret;
				cost += ret;

				if (flags & DC_EXEC) SetRailUnderBridge(tile, _current_player, railtype);
			} else if (IsTransportUnderBridge(tile) &&
					GetTransportTypeUnderBridge(tile) == TRANSPORT_RAIL) {
				return_cmd_error(STR_1007_ALREADY_BUILT);
			} else {
				// Get detailed error message
				return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
			break;

		case MP_RAILWAY:
			if (!CheckTrackCombination(tile, trackbit, flags) ||
					!EnsureNoVehicleOnGround(tile)) {
				return CMD_ERROR;
			}
			if (!IsTileOwner(tile, _current_player) ||
					!IsCompatibleRail(GetRailType(tile), railtype)) {
				// Get detailed error message
				return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}

			ret = CheckRailSlope(tileh, trackbit, GetTrackBits(tile), tile);
			if (CmdFailed(ret)) return ret;
			cost += ret;

			/* If the rail types don't match, try to convert only if engines of
			 * the present rail type are powered on the new rail type. */
			if (GetRailType(tile) != railtype && HasPowerOnRail(GetRailType(tile), railtype)) {
				ret = DoCommand(tile, tile, railtype, flags, CMD_CONVERT_RAIL);
				if (CmdFailed(ret)) return ret;
				cost += ret;
			}

			if (flags & DC_EXEC) {
				SetRailGroundType(tile, RAIL_GROUND_BARREN);
				_m[tile].m5 |= trackbit;
			}
			break;

		case MP_STREET:
#define M(x) (1 << (x))
			/* Level crossings may only be built on these slopes */
			if (!HASBIT(M(SLOPE_SEN) | M(SLOPE_ENW) | M(SLOPE_NWS) | M(SLOPE_NS) | M(SLOPE_WSE) | M(SLOPE_EW) | M(SLOPE_FLAT), tileh)) {
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}
#undef M

			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

			if (GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
				if (HasRoadWorks(tile)) return_cmd_error(STR_ROAD_WORKS_IN_PROGRESS);

				if ((track == TRACK_X && GetRoadBits(tile) == ROAD_Y) ||
						(track == TRACK_Y && GetRoadBits(tile) == ROAD_X)) {
					if (flags & DC_EXEC) {
						MakeRoadCrossing(tile, GetTileOwner(tile), _current_player, (track == TRACK_X ? AXIS_Y : AXIS_X), railtype, GetTownIndex(tile));
					}
					break;
				}
			}

			if (IsLevelCrossing(tile) && GetCrossingRailBits(tile) == trackbit) {
				return_cmd_error(STR_1007_ALREADY_BUILT);
			}
			/* FALLTHROUGH */

		default:
			ret = CheckRailSlope(tileh, trackbit, 0, tile);
			if (CmdFailed(ret)) return ret;
			cost += ret;

			ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return ret;
			cost += ret;

			if (flags & DC_EXEC) MakeRailNormal(tile, _current_player, trackbit, railtype);
			break;
	}

	if (flags & DC_EXEC) {
		MarkTileDirtyByTile(tile);
		SetSignalsOnBothDir(tile, track);
		YapfNotifyTrackLayoutChange(tile, track);
	}

	return cost + _price.build_rail;
}

/** Remove a single piece of track
 * @param tile tile to remove track from
 * @param p1 unused
 * @param p2 rail orientation
 */
int32 CmdRemoveSingleRail(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Track track = (Track)p2;
	TrackBits trackbit;
	int32 cost = _price.remove_rail;
	bool crossing = false;

	if (!ValParamTrackOrientation(p2)) return CMD_ERROR;
	trackbit = TrackToTrackBits(track);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	switch (GetTileType(tile)) {
		case MP_TUNNELBRIDGE:
			if (!IsBridge(tile) ||
					!IsBridgeMiddle(tile) ||
					!IsTransportUnderBridge(tile) ||
					GetTransportTypeUnderBridge(tile) != TRANSPORT_RAIL ||
					GetRailBitsUnderBridge(tile) != trackbit ||
					(_current_player != OWNER_WATER && !CheckTileOwnership(tile)) ||
					!EnsureNoVehicleOnGround(tile)) {
				return CMD_ERROR;
			}

			if (flags & DC_EXEC) SetClearUnderBridge(tile);
			break;

		case MP_STREET: {
			if (!IsLevelCrossing(tile) ||
					GetCrossingRailBits(tile) != trackbit ||
					(_current_player != OWNER_WATER && !CheckTileOwnership(tile)) ||
					!EnsureNoVehicleOnGround(tile)) {
				return CMD_ERROR;
			}

			if (flags & DC_EXEC) {
				MakeRoadNormal(tile, GetCrossingRoadOwner(tile), GetCrossingRoadBits(tile), GetTownIndex(tile));
			}
			break;
		}

		case MP_RAILWAY: {
			TrackBits present;

			if (!IsPlainRailTile(tile) ||
					(_current_player != OWNER_WATER && !CheckTileOwnership(tile)) ||
					!EnsureNoVehicleOnGround(tile)) {
				return CMD_ERROR;
			}

			present = GetTrackBits(tile);
			if ((present & trackbit) == 0) return CMD_ERROR;
			if (present == (TRACK_BIT_X | TRACK_BIT_Y)) crossing = true;

			/* Charge extra to remove signals on the track, if they are there */
			if (HasSignalOnTrack(tile, track))
				cost += DoCommand(tile, track, 0, flags, CMD_REMOVE_SIGNALS);

			if (flags & DC_EXEC) {
				present ^= trackbit;
				if (present == 0) {
					DoClearSquare(tile);
				} else {
					SetTrackBits(tile, present);
				}
			}
			break;
		}

		default: return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		MarkTileDirtyByTile(tile);
		if (crossing) {
			/* crossing is set when only TRACK_BIT_X and TRACK_BIT_Y are set. As we
			 * are removing one of these pieces, we'll need to update signals for
			 * both directions explicitly, as after the track is removed it won't
			 * 'connect' with the other piece. */
			SetSignalsOnBothDir(tile, TRACK_X);
			SetSignalsOnBothDir(tile, TRACK_Y);
			YapfNotifyTrackLayoutChange(tile, TRACK_X);
			YapfNotifyTrackLayoutChange(tile, TRACK_Y);
		} else {
			SetSignalsOnBothDir(tile, track);
			YapfNotifyTrackLayoutChange(tile, track);
		}
	}

	return cost;
}


static const TileIndexDiffC _trackdelta[] = {
	{ -1,  0 }, {  0,  1 }, { -1,  0 }, {  0,  1 }, {  1,  0 }, {  0,  1 },
	{  0,  0 },
	{  0,  0 },
	{  1,  0 }, {  0, -1 }, {  0, -1 }, {  1,  0 }, {  0, -1 }, { -1,  0 },
	{  0,  0 },
	{  0,  0 }
};


static int32 ValidateAutoDrag(Trackdir *trackdir, TileIndex start, TileIndex end)
{
	int x = TileX(start);
	int y = TileY(start);
	int ex = TileX(end);
	int ey = TileY(end);
	int dx, dy, trdx, trdy;

	if (!ValParamTrackOrientation(*trackdir)) return CMD_ERROR;

	// calculate delta x,y from start to end tile
	dx = ex - x;
	dy = ey - y;

	// calculate delta x,y for the first direction
	trdx = _trackdelta[*trackdir].x;
	trdy = _trackdelta[*trackdir].y;

	if (!IsDiagonalTrackdir(*trackdir)) {
		trdx += _trackdelta[*trackdir ^ 1].x;
		trdy += _trackdelta[*trackdir ^ 1].y;
	}

	// validate the direction
	while (
		(trdx <= 0 && dx > 0) ||
		(trdx >= 0 && dx < 0) ||
		(trdy <= 0 && dy > 0) ||
		(trdy >= 0 && dy < 0)
	) {
		if (!HASBIT(*trackdir, 3)) { // first direction is invalid, try the other
			SETBIT(*trackdir, 3); // reverse the direction
			trdx = -trdx;
			trdy = -trdy;
		} else { // other direction is invalid too, invalid drag
			return CMD_ERROR;
		}
	}

	// (for diagonal tracks, this is already made sure of by above test), but:
	// for non-diagonal tracks, check if the start and end tile are on 1 line
	if (!IsDiagonalTrackdir(*trackdir)) {
		trdx = _trackdelta[*trackdir].x;
		trdy = _trackdelta[*trackdir].y;
		if (abs(dx) != abs(dy) && abs(dx) + abs(trdy) != abs(dy) + abs(trdx))
			return CMD_ERROR;
	}

	return 0;
}

/** Build a stretch of railroad tracks.
 * @param tile start tile of drag
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-3) - railroad type normal/maglev (0 = normal, 1 = mono, 2 = maglev)
 * - p2 = (bit 4-6) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit 7)   - 0 = build, 1 = remove tracks
 */
static int32 CmdRailTrackHelper(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 ret, total_cost = 0;
	Track track = (Track)GB(p2, 4, 3);
	Trackdir trackdir;
	byte mode = HASBIT(p2, 7);
	RailType railtype = (RailType)GB(p2, 0, 4);
	TileIndex end_tile;

	if (!ValParamRailtype(railtype) || !ValParamTrackOrientation(track)) return CMD_ERROR;
	if (p1 >= MapSize()) return CMD_ERROR;
	end_tile = p1;
	trackdir = TrackToTrackdir(track);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (CmdFailed(ValidateAutoDrag(&trackdir, tile, end_tile))) return CMD_ERROR;

	if (flags & DC_EXEC) SndPlayTileFx(SND_20_SPLAT_2, tile);

	for (;;) {
		ret = DoCommand(tile, railtype, TrackdirToTrack(trackdir), flags, (mode == 0) ? CMD_BUILD_SINGLE_RAIL : CMD_REMOVE_SINGLE_RAIL);

		if (CmdFailed(ret)) {
			if ((_error_message != STR_1007_ALREADY_BUILT) && (mode == 0)) break;
			_error_message = INVALID_STRING_ID;
		} else {
			total_cost += ret;
		}

		if (tile == end_tile) break;

		tile += ToTileIndexDiff(_trackdelta[trackdir]);

		// toggle railbit for the non-diagonal tracks
		if (!IsDiagonalTrackdir(trackdir)) trackdir ^= 1;
	}

	return (total_cost == 0) ? CMD_ERROR : total_cost;
}

/** Build rail on a stretch of track.
 * Stub for the unified rail builder/remover
 * @see CmdRailTrackHelper
 */
int32 CmdBuildRailroadTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdRailTrackHelper(tile, flags, p1, CLRBIT(p2, 7));
}

/** Build rail on a stretch of track.
 * Stub for the unified rail builder/remover
 * @see CmdRailTrackHelper
 */
int32 CmdRemoveRailroadTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdRailTrackHelper(tile, flags, p1, SETBIT(p2, 7));
}

/** Build a train depot
 * @param tile position of the train depot
 * @param p1 rail type
 * @param p2 entrance direction (DiagDirection)
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
int32 CmdBuildTrainDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Depot *d;
	int32 cost, ret;
	Slope tileh;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;
	/* check railtype and valid direction for depot (0 through 3), 4 in total */
	if (!ValParamRailtype(p1) || p2 > 3) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);

	/* Prohibit construction if
	 * The tile is non-flat AND
	 * 1) The AI is "old-school"
	 * 2) build-on-slopes is disabled
	 * 3) the tile is steep i.e. spans two height levels
	 * 4) the exit points in the wrong direction
	 */

	if (tileh != SLOPE_FLAT && (
				_is_old_ai_player ||
				!_patches.build_on_slopes ||
				IsSteepSlope(tileh) ||
				!CanBuildDepotByTileh(p2, tileh)
			)) {
		return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	cost = ret;

	d = AllocateDepot();
	if (d == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		MakeRailDepot(tile, _current_player, p2, p1);
		MarkTileDirtyByTile(tile);

		d->xy = tile;
		d->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		UpdateSignalsOnSegment(tile, p2);
		YapfNotifyTrackLayoutChange(tile, TrackdirToTrack(DiagdirToDiagTrackdir(p2)));
	}

	return cost + _price.build_train_depot;
}

/** Build signals, alternate between double/single, signal/semaphore,
 * pre/exit/combo-signals, and what-else not
 * @param tile tile where to build the signals
 * @param p1 various bitstuffed elements
 * - p1 = (bit 0-2) - track-orientation, valid values: 0-5 (Track enum)
 * - p1 = (bit 3)   - choose semaphores/signals or cycle normal/pre/exit/combo depending on context
 * @param p2 used for CmdBuildManySignals() to copy direction of first signal
 * TODO: p2 should be replaced by two bits for "along" and "against" the track.
 */
int32 CmdBuildSingleSignal(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	SignalVariant sigvar;
	bool pre_signal;
	Track track = (Track)(p1 & 0x7);
	int32 cost;

	// Same bit, used in different contexts
	sigvar = HASBIT(p1, 3) ? SIG_SEMAPHORE : SIG_ELECTRIC;
	pre_signal = HASBIT(p1, 3);

	if (!ValParamTrackOrientation(track) || !IsTileType(tile, MP_RAILWAY) || !EnsureNoVehicleOnGround(tile))
		return CMD_ERROR;

	/* Protect against invalid signal copying */
	if (p2 != 0 && (p2 & SignalOnTrack(track)) == 0) return CMD_ERROR;

	/* You can only build signals on plain rail tiles, and the selected track must exist */
	if (!IsPlainRailTile(tile) || !HasTrack(tile, track)) return CMD_ERROR;

	if (!CheckTileOwnership(tile)) return CMD_ERROR;

	_error_message = STR_1005_NO_SUITABLE_RAILROAD_TRACK;

	{
		/* See if this is a valid track combination for signals, (ie, no overlap) */
		TrackBits trackbits = GetTrackBits(tile);
		if (KILL_FIRST_BIT(trackbits) != 0 && /* More than one track present */
				trackbits != TRACK_BIT_HORZ &&
				trackbits != TRACK_BIT_VERT) {
			return CMD_ERROR;
		}
	}

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!HasSignalOnTrack(tile, track)) {
		// build new signals
		cost = _price.build_signals;
	} else {
		if (p2 != 0 && sigvar != GetSignalVariant(tile)) {
			// convert signals <-> semaphores
			cost = _price.build_signals + _price.remove_signals;
		} else {
			// it is free to change orientation/pre-exit-combo signals
			cost = 0;
		}
	}

	if (flags & DC_EXEC) {
		if (!HasSignals(tile)) {
			// there are no signals at all on this tile yet
			_m[tile].m5 |= RAIL_TILE_SIGNALS; // change into signals
			_m[tile].m2 |= 0xF0;              // all signals are on
			_m[tile].m3 &= ~0xF0;          // no signals built by default
			SetSignalType(tile, SIGTYPE_NORMAL);
			SetSignalVariant(tile, sigvar);
		}

		if (p2 == 0) {
			if (!HasSignalOnTrack(tile, track)) {
				// build new signals
				_m[tile].m3 |= SignalOnTrack(track);
			} else {
				if (pre_signal) {
					// cycle between normal -> pre -> exit -> combo -> ...
					SignalType type = GetSignalType(tile);

					SetSignalType(tile, type == SIGTYPE_COMBO ? SIGTYPE_NORMAL : type + 1);
				} else {
					CycleSignalSide(tile, track);
				}
			}
		} else {
			/* If CmdBuildManySignals is called with copying signals, just copy the
			 * direction of the first signal given as parameter by CmdBuildManySignals */
			_m[tile].m3 &= ~SignalOnTrack(track);
			_m[tile].m3 |= p2 & SignalOnTrack(track);
			SetSignalVariant(tile, sigvar);
		}

		MarkTileDirtyByTile(tile);
		SetSignalsOnBothDir(tile, track);
		YapfNotifyTrackLayoutChange(tile, track);
	}

	return cost;
}

/** Build many signals by dragging; AutoSignals
 * @param tile start tile of drag
 * @param p1  end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0)    - 0 = build, 1 = remove signals
 * - p2 = (bit  3)    - 0 = signals, 1 = semaphores
 * - p2 = (bit  4- 6) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit 24-31) - user defined signals_density
 */
static int32 CmdSignalTrackHelper(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 ret, total_cost, signal_ctr;
	byte signals;
	bool error = true;
	TileIndex end_tile;

	int mode = p2 & 0x1;
	Track track = GB(p2, 4, 3);
	Trackdir trackdir = TrackToTrackdir(track);
	byte semaphores = (HASBIT(p2, 3) ? 8 : 0);
	byte signal_density = (p2 >> 24);

	if (p1 >= MapSize()) return CMD_ERROR;
	end_tile = p1;
	if (signal_density == 0 || signal_density > 20) return CMD_ERROR;

	if (!IsTileType(tile, MP_RAILWAY)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* for vertical/horizontal tracks, double the given signals density
	 * since the original amount will be too dense (shorter tracks) */
	if (!IsDiagonalTrack(track)) signal_density *= 2;

	if (CmdFailed(ValidateAutoDrag(&trackdir, tile, end_tile))) return CMD_ERROR;

	track = TrackdirToTrack(trackdir); /* trackdir might have changed, keep track in sync */

	// copy the signal-style of the first rail-piece if existing
	if (HasSignals(tile)) {
		signals = _m[tile].m3 & SignalOnTrack(track);
		if (signals == 0) signals = SignalOnTrack(track); /* Can this actually occur? */

		// copy signal/semaphores style (independent of CTRL)
		semaphores = (GetSignalVariant(tile) == SIG_ELECTRIC ? 0 : 8);
	} else { // no signals exist, drag a two-way signal stretch
		signals = SignalOnTrack(track);
	}

	/* signal_ctr         - amount of tiles already processed
	 * signals_density    - patch setting to put signal on every Nth tile (double space on |, -- tracks)
	 **********
	 * trackdir   - trackdir to build with autorail
	 * semaphores - semaphores or signals
	 * signals    - is there a signal/semaphore on the first tile, copy its style (two-way/single-way)
	 *              and convert all others to semaphore/signal
	 * mode       - 1 remove signals, 0 build signals */
	signal_ctr = total_cost = 0;
	for (;;) {
		// only build/remove signals with the specified density
		if (signal_ctr % signal_density == 0) {
			ret = DoCommand(tile, TrackdirToTrack(trackdir) | semaphores, signals, flags, (mode == 1) ? CMD_REMOVE_SIGNALS : CMD_BUILD_SIGNALS);

			/* Be user-friendly and try placing signals as much as possible */
			if (!CmdFailed(ret)) {
				error = false;
				total_cost += ret;
			}
		}

		if (tile == end_tile) break;

		tile += ToTileIndexDiff(_trackdelta[trackdir]);
		signal_ctr++;

		// toggle railbit for the non-diagonal tracks (|, -- tracks)
		if (!IsDiagonalTrackdir(trackdir)) trackdir ^= 1;
	}

	return error ? CMD_ERROR : total_cost;
}

/** Build signals on a stretch of track.
 * Stub for the unified signal builder/remover
 * @see CmdSignalTrackHelper
 */
int32 CmdBuildSignalTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdSignalTrackHelper(tile, flags, p1, p2);
}

/** Remove signals
 * @param tile coordinates where signal is being deleted from
 * @param p1 track to remove signal from (Track enum)
 */
int32 CmdRemoveSingleSignal(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Track track = (Track)(p1 & 0x7);

	if (!ValParamTrackOrientation(track) ||
			!IsTileType(tile, MP_RAILWAY) ||
			!EnsureNoVehicleOnGround(tile) ||
			!HasSignalOnTrack(tile, track)) {
		return CMD_ERROR;
	}

	/* Only water can remove signals from anyone */
	if (_current_player != OWNER_WATER && !CheckTileOwnership(tile)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Do it? */
	if (flags & DC_EXEC) {
		_m[tile].m3 &= ~SignalOnTrack(track);

		/* removed last signal from tile? */
		if (GB(_m[tile].m3, 4, 4) == 0) {
			SB(_m[tile].m2, 4, 4, 0);
			SB(_m[tile].m5, 6, 2, RAIL_TILE_NORMAL >> 6); // XXX >> because the constant is meant for direct application, not use with SB
			SetSignalVariant(tile, SIG_ELECTRIC); // remove any possible semaphores
		}

		SetSignalsOnBothDir(tile, track);
		YapfNotifyTrackLayoutChange(tile, track);

		MarkTileDirtyByTile(tile);
	}

	return _price.remove_signals;
}

/** Remove signals on a stretch of track.
 * Stub for the unified signal builder/remover
 * @see CmdSignalTrackHelper
 */
int32 CmdRemoveSignalTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdSignalTrackHelper(tile, flags, p1, SETBIT(p2, 0));
}

typedef int32 DoConvertRailProc(TileIndex tile, RailType totype, bool exec);

/**
 * Switches the rail type.
 * Railtypes are stored on a per-tile basis, not on a per-track basis, so
 * all the tracks in the given tile will be converted.
 * @param tile        The tile on which the railtype is to be convert.
 * @param totype      The railtype we want to convert to
 * @param exec        Switches between test and execute mode
 * @return            The cost and state of the operation
 * @retval CMD_ERROR  An error occured during the operation.
 */
static int32 DoConvertRail(TileIndex tile, RailType totype, bool exec)
{
	if (!CheckTileOwnership(tile)) return CMD_ERROR;

	if (GetRailType(tile) == totype) return CMD_ERROR;

	if (!EnsureNoVehicleOnGround(tile) && (!IsCompatibleRail(GetRailType(tile), totype) || IsPlainRailTile(tile))) return CMD_ERROR;

	// 'hidden' elrails can't be downgraded to normal rail when elrails are disabled
	if (_patches.disable_elrails && totype == RAILTYPE_RAIL && GetRailType(tile) == RAILTYPE_ELECTRIC) return CMD_ERROR;

	// change type.
	if (exec) {
		TrackBits tracks;
		SetRailType(tile, totype);
		MarkTileDirtyByTile(tile);

		// notify YAPF about the track layout change
		for (tracks = GetTrackBits(tile); tracks != TRACK_BIT_NONE; tracks = KILL_FIRST_BIT(tracks))
			YapfNotifyTrackLayoutChange(tile, FIND_FIRST_BIT(tracks));

		if (IsTileDepotType(tile, TRANSPORT_RAIL)) {
			Vehicle *v;

			/* Update build vehicle window related to this depot */
			InvalidateWindowData(WC_BUILD_VEHICLE, tile);

			/* update power of trains in this depot */
			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_Train && IsFrontEngine(v) && v->tile == tile && v->u.rail.track == 0x80) {
					TrainPowerChanged(v);
				}
			}
		}
	}

	return _price.build_rail / 2;
}

extern int32 DoConvertStationRail(TileIndex tile, RailType totype, bool exec);
extern int32 DoConvertStreetRail(TileIndex tile, RailType totype, bool exec);
extern int32 DoConvertTunnelBridgeRail(TileIndex tile, RailType totype, bool exec);

/** Convert one rail type to the other. You can convert normal rail to
 * monorail/maglev easily or vice-versa.
 * @param tile end tile of rail conversion drag
 * @param p1 start tile of drag
 * @param p2 new railtype to convert to
 */
int32 CmdConvertRail(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 ret, cost, money;
	int ex;
	int ey;
	int sx, sy, x, y;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!ValParamRailtype(p2)) return CMD_ERROR;
	if (p1 >= MapSize()) return CMD_ERROR;

	// make sure sx,sy are smaller than ex,ey
	ex = TileX(tile);
	ey = TileY(tile);
	sx = TileX(p1);
	sy = TileY(p1);
	if (ex < sx) intswap(ex, sx);
	if (ey < sy) intswap(ey, sy);

	money = GetAvailableMoneyForCommand();
	cost = 0;
	ret = 0;

	for (x = sx; x <= ex; ++x) {
		for (y = sy; y <= ey; ++y) {
			TileIndex tile = TileXY(x, y);
			DoConvertRailProc* proc;

			switch (GetTileType(tile)) {
				case MP_RAILWAY:      proc = DoConvertRail;             break;
				case MP_STATION:      proc = DoConvertStationRail;      break;
				case MP_STREET:       proc = DoConvertStreetRail;       break;
				case MP_TUNNELBRIDGE: proc = DoConvertTunnelBridgeRail; break;
				default: continue;
			}

			ret = proc(tile, p2, false);
			if (CmdFailed(ret)) continue;
			cost += ret;

			if (flags & DC_EXEC) {
				money -= ret;
				if (money < 0) {
					_additional_cash_required = ret;
					return cost - ret;
				}
				proc(tile, p2, true);
			}
		}
	}

	return (cost == 0) ? ret : cost;
}

static int32 RemoveTrainDepot(TileIndex tile, uint32 flags)
{
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicleOnGround(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DiagDirection dir = GetRailDepotDirection(tile);

		DeleteDepot(GetDepotByTile(tile));
		UpdateSignalsOnSegment(tile, dir);
		YapfNotifyTrackLayoutChange(tile, TrackdirToTrack(DiagdirToDiagTrackdir(dir)));
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
		if (!IsTileOwner(tile, _current_player))
			return_cmd_error(STR_1024_AREA_IS_OWNED_BY_ANOTHER);

		if (IsPlainRailTile(tile)) {
			return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);
		} else {
			return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
		}
	}

	cost = 0;

	switch (GetRailTileType(tile)) {
		/* XXX: Why the fuck do we remove these thow signals first? */
		case RAIL_TILE_SIGNALS:
			if (HasSignalOnTrack(tile, TRACK_X)) {
				ret = DoCommand(tile, TRACK_X, 0, flags, CMD_REMOVE_SIGNALS);
				if (CmdFailed(ret)) return CMD_ERROR;
				cost += ret;
			}
			if (HasSignalOnTrack(tile, TRACK_LOWER)) {
				ret = DoCommand(tile, TRACK_LOWER, 0, flags, CMD_REMOVE_SIGNALS);
				if (CmdFailed(ret)) return CMD_ERROR;
				cost += ret;
			}

			m5 &= TRACK_BIT_MASK;
			if (!(flags & DC_EXEC)) {
				for (; m5 != 0; m5 >>= 1) if (m5 & 1) cost += _price.remove_rail;
				return cost;
			}
			/* FALLTHROUGH */

		case RAIL_TILE_NORMAL: {
			uint i;

			for (i = 0; m5 != 0; i++, m5 >>= 1) {
				if (m5 & 1) {
					ret = DoCommand(tile, 0, i, flags, CMD_REMOVE_SINGLE_RAIL);
					if (CmdFailed(ret)) return CMD_ERROR;
					cost += ret;
				}
			}
			return cost;
		}

		case RAIL_TILE_DEPOT_WAYPOINT:
			if (GetRailTileSubtype(tile) == RAIL_SUBTYPE_DEPOT) {
				return RemoveTrainDepot(tile, flags);
			} else {
				return RemoveTrainWaypoint(tile, flags, false);
			}

		default:
			return CMD_ERROR;
	}
}

#include "table/track_land.h"

static void DrawSingleSignal(TileIndex tile, byte condition, uint image, uint pos)
{
	bool side = _opt.road_side & _patches.signal_side;
	static const Point SignalPositions[2][12] = {
		{      /* Signals on the left side */
		/*  LEFT      LEFT      RIGHT     RIGHT     UPPER     UPPER */
			{ 8,  5}, {14,  1}, { 1, 14}, { 9, 11}, { 1,  0}, { 3, 10},
		/*  LOWER     LOWER     X         X         Y         Y     */
			{11,  4}, {14, 14}, {11,  3}, { 4, 13}, { 3,  4}, {11, 13}
		}, {   /* Signals on the right side */
		/*  LEFT      LEFT      RIGHT     RIGHT     UPPER     UPPER */
			{14,  1}, {12, 10}, { 4,  6}, { 1, 14}, {10,  4}, { 0,  1},
		/*  LOWER     LOWER     X         X         Y         Y     */
			{14, 14}, { 5, 12}, {11, 13}, { 4,  3}, {13,  4}, { 3, 11}
		}
	};

	static const SpriteID SignalBase[2][2][4] = {
		{    /* Signals on left side */
			{  0x4FB, 0x1323, 0x1333, 0x1343}, /* light signals */
			{ 0x1353, 0x1363, 0x1373, 0x1383}  /* semaphores    */
		}, { /* Signals on right side */
			{  0x4FB, 0x1323, 0x1333, 0x1343}, /* light signals */
			{ 0x1446, 0x1456, 0x1466, 0x1476}  /* semaphores    */
		/*         |       |       |       |     */
		/*    normal,  entry,   exit,  combo     */
		}
	};

	uint x = TileX(tile) * TILE_SIZE + SignalPositions[side][pos].x;
	uint y = TileY(tile) * TILE_SIZE + SignalPositions[side][pos].y;

	SpriteID sprite;

	/* _signal_base is set by our NewGRF Action 5 loader. If it is 0 then we
	 * just draw the standard signals, else we get the offset from _signal_base
	 * and draw that sprite. All the signal sprites are loaded sequentially. */
	if (_signal_base == 0 || (GetSignalType(tile) == 0 && GetSignalVariant(tile) == SIG_ELECTRIC)) {
		sprite = SignalBase[side][GetSignalVariant(tile)][GetSignalType(tile)] + image + condition;
	} else {
		sprite = _signal_base + (GetSignalType(tile) - 1) * 16 + GetSignalVariant(tile) * 64 + image + condition;
	}

	AddSortableSpriteToDraw(sprite, x, y, 1, 1, 10, GetSlopeZ(x,y));
}

static uint32 _drawtile_track_palette;


static void DrawTrackFence_NW(const TileInfo *ti)
{
	uint32 image = 0x515;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? 0x519 : 0x51B;
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x, ti->y + 1, 16, 1, 4, ti->z);
}

static void DrawTrackFence_SE(const TileInfo *ti)
{
	uint32 image = 0x515;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? 0x519 : 0x51B;
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x, ti->y + TILE_SIZE - 1, 16, 1, 4, ti->z);
}

static void DrawTrackFence_NW_SE(const TileInfo *ti)
{
	DrawTrackFence_NW(ti);
	DrawTrackFence_SE(ti);
}

static void DrawTrackFence_NE(const TileInfo *ti)
{
	uint32 image = 0x516;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? 0x51A : 0x51C;
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x + 1, ti->y, 1, 16, 4, ti->z);
}

static void DrawTrackFence_SW(const TileInfo *ti)
{
	uint32 image = 0x516;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? 0x51A : 0x51C;
	AddSortableSpriteToDraw(image | _drawtile_track_palette,
		ti->x + TILE_SIZE - 1, ti->y, 1, 16, 4, ti->z);
}

static void DrawTrackFence_NE_SW(const TileInfo *ti)
{
	DrawTrackFence_NE(ti);
	DrawTrackFence_SW(ti);
}

static void DrawTrackFence_NS_1(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_W) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(0x517 | _drawtile_track_palette,
		ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, 4, z);
}

static void DrawTrackFence_NS_2(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_E) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(0x517 | _drawtile_track_palette,
		ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, 4, z);
}

static void DrawTrackFence_WE_1(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_N) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(0x518 | _drawtile_track_palette,
		ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, 4, z);
}

static void DrawTrackFence_WE_2(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_S) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(0x518 | _drawtile_track_palette,
		ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, 4, z);
}


static void DrawTrackDetails(const TileInfo* ti)
{
	switch (GetRailGroundType(ti->tile)) {
		case RAIL_GROUND_FENCE_NW:     DrawTrackFence_NW(ti);    break;
		case RAIL_GROUND_FENCE_SE:     DrawTrackFence_SE(ti);    break;
		case RAIL_GROUND_FENCE_SENW:   DrawTrackFence_NW_SE(ti); break;
		case RAIL_GROUND_FENCE_NE:     DrawTrackFence_NE(ti);    break;
		case RAIL_GROUND_FENCE_SW:     DrawTrackFence_SW(ti);    break;
		case RAIL_GROUND_FENCE_NESW:   DrawTrackFence_NE_SW(ti); break;
		case RAIL_GROUND_FENCE_VERT1:  DrawTrackFence_NS_1(ti);  break;
		case RAIL_GROUND_FENCE_VERT2:  DrawTrackFence_NS_2(ti);  break;
		case RAIL_GROUND_FENCE_HORIZ1: DrawTrackFence_WE_1(ti);  break;
		case RAIL_GROUND_FENCE_HORIZ2: DrawTrackFence_WE_2(ti);  break;
		default: break;
	}
}


/**
 * Draw ground sprite and track bits
 * @param ti TileInfo
 * @param track TrackBits to draw
 * @param earth Draw as earth
 * @param snow Draw as snow
 * @param flat Always draw foundation
 */
static void DrawTrackBits(TileInfo* ti, TrackBits track)
{
	const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
	PalSpriteID image;
	bool junction = false;

	// Select the sprite to use.
	(image = rti->base_sprites.track_y, track == TRACK_BIT_Y) ||
	(image++,                           track == TRACK_BIT_X) ||
	(image++,                           track == TRACK_BIT_UPPER) ||
	(image++,                           track == TRACK_BIT_LOWER) ||
	(image++,                           track == TRACK_BIT_RIGHT) ||
	(image++,                           track == TRACK_BIT_LEFT) ||
	(image++,                           track == TRACK_BIT_CROSS) ||

	(image = rti->base_sprites.track_ns, track == TRACK_BIT_HORZ) ||
	(image++,                            track == TRACK_BIT_VERT) ||

	(junction = true, false) ||
	(image = rti->base_sprites.ground, (track & TRACK_BIT_3WAY_NE) == 0) ||
	(image++,                          (track & TRACK_BIT_3WAY_SW) == 0) ||
	(image++,                          (track & TRACK_BIT_3WAY_NW) == 0) ||
	(image++,                          (track & TRACK_BIT_3WAY_SE) == 0) ||
	(image++, true);

	if (ti->tileh != SLOPE_FLAT) {
		uint foundation = GetRailFoundation(ti->tileh, track);

		if (foundation != 0) DrawFoundation(ti, foundation);

		// DrawFoundation() modifies ti.
		// Default sloped sprites..
		if (ti->tileh != SLOPE_FLAT)
			image = _track_sloped_sprites[ti->tileh - 1] + rti->base_sprites.track_y;
	}

	switch (GetRailGroundType(ti->tile)) {
		case RAIL_GROUND_BARREN:     image |= PALETTE_TO_BARE_LAND; break;
		case RAIL_GROUND_ICE_DESERT: image += rti->snow_offset; break;
		default: break;
	}

	DrawGroundSprite(image);

	// Draw track pieces individually for junction tiles
	if (junction) {
		if (track & TRACK_BIT_X)     DrawGroundSprite(rti->base_sprites.single_y);
		if (track & TRACK_BIT_Y)     DrawGroundSprite(rti->base_sprites.single_x);
		if (track & TRACK_BIT_UPPER) DrawGroundSprite(rti->base_sprites.single_n);
		if (track & TRACK_BIT_LOWER) DrawGroundSprite(rti->base_sprites.single_s);
		if (track & TRACK_BIT_LEFT)  DrawGroundSprite(rti->base_sprites.single_w);
		if (track & TRACK_BIT_RIGHT) DrawGroundSprite(rti->base_sprites.single_e);
	}

	if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);

}

static void DrawSignals(TileIndex tile, TrackBits rails)
{
#define MAYBE_DRAW_SIGNAL(x,y,z) if (IsSignalPresent(tile, x)) DrawSingleSignal(tile, GetSingleSignalState(tile, x), y - 0x4FB, z)

	if (!(rails & TRACK_BIT_Y)) {
		if (!(rails & TRACK_BIT_X)) {
			if (rails & TRACK_BIT_LEFT) {
				MAYBE_DRAW_SIGNAL(2, 0x509, 0);
				MAYBE_DRAW_SIGNAL(3, 0x507, 1);
			}
			if (rails & TRACK_BIT_RIGHT) {
				MAYBE_DRAW_SIGNAL(0, 0x509, 2);
				MAYBE_DRAW_SIGNAL(1, 0x507, 3);
			}
			if (rails & TRACK_BIT_UPPER) {
				MAYBE_DRAW_SIGNAL(3, 0x505, 4);
				MAYBE_DRAW_SIGNAL(2, 0x503, 5);
			}
			if (rails & TRACK_BIT_LOWER) {
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

static void DrawTile_Track(TileInfo *ti)
{
	const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
	PalSpriteID image;

	_drawtile_track_palette = SPRITE_PALETTE(PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)));

	if (IsPlainRailTile(ti->tile)) {
		TrackBits rails = GetTrackBits(ti->tile);

		DrawTrackBits(ti, rails);

		if (_display_opt & DO_FULL_DETAIL) DrawTrackDetails(ti);

		if (HasSignals(ti->tile)) DrawSignals(ti->tile, rails);
	} else {
		// draw depot/waypoint
		const DrawTileSprites* dts;
		const DrawTileSeqStruct* dtss;
		uint32 relocation;

		if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, ti->tileh);

		if (GetRailTileSubtype(ti->tile) == RAIL_SUBTYPE_DEPOT) {
			dts = &_depot_gfx_table[GetRailDepotDirection(ti->tile)];

			relocation = rti->total_offset;

			image = dts->ground_sprite;
			if (image != SPR_FLAT_GRASS_TILE) image += rti->total_offset;

			// adjust ground tile for desert
			// don't adjust for snow, because snow in depots looks weird
			if (IsSnowRailGround(ti->tile) && _opt.landscape == LT_DESERT) {
				if (image != SPR_FLAT_GRASS_TILE) {
					image += rti->snow_offset; // tile with tracks
				} else {
					image = SPR_FLAT_SNOWY_TILE; // flat ground
				}
			}
		} else {
			// look for customization
			byte stat_id = GetWaypointByTile(ti->tile)->stat_id;
			const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, stat_id);

			if (statspec != NULL) {
				// emulate station tile - open with building
				const Station* st = ComposeWaypointStation(ti->tile);
				uint gfx = 2;

				if (HASBIT(statspec->callbackmask, CBM_CUSTOM_LAYOUT)) {
					uint16 callback = GetStationCallback(CBID_STATION_SPRITE_LAYOUT, 0, 0, statspec, st, ti->tile);
					if (callback != CALLBACK_FAILED) gfx = callback;
				}

				if (statspec->renderdata == NULL) {
					dts = GetStationTileLayout(gfx);
				} else {
					dts = &statspec->renderdata[(gfx < statspec->tiles ? gfx : 0) + GetWaypointAxis(ti->tile)];
				}

				if (dts != NULL && dts->seq != NULL) {
					relocation = GetCustomStationRelocation(statspec, st, ti->tile);

					image = dts->ground_sprite;
					if (HASBIT(image, 31)) {
						CLRBIT(image, 31);
						image += GetCustomStationGroundRelocation(statspec, st, ti->tile);
						image += rti->custom_ground_offset;
					} else {
						image += rti->total_offset;
					}
				} else {
					goto default_waypoint;
				}
			} else {
default_waypoint:
				// There is no custom layout, fall back to the default graphics
				dts = &_waypoint_gfx_table[GetWaypointAxis(ti->tile)];
				relocation = 0;
				image = dts->ground_sprite + rti->total_offset;
				if (IsSnowRailGround(ti->tile)) image += rti->snow_offset;
			}
		}

		DrawGroundSprite(image);

		if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);

		foreach_draw_tile_seq(dtss, dts->seq) {
			uint32 image = dtss->image + relocation;

			if (_display_opt & DO_TRANS_BUILDINGS) {
				MAKE_TRANSPARENT(image);
			} else if (image & PALETTE_MODIFIER_COLOR) {
				image |= _drawtile_track_palette;
			}
			AddSortableSpriteToDraw(
				image,
				ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->size_x, dtss->size_y,
				dtss->size_z, ti->z + dtss->delta_z
			);
		}
	}
}


static void DrawTileSequence(int x, int y, uint32 ground, const DrawTileSeqStruct* dtss, uint32 offset)
{
	uint32 palette = PLAYER_SPRITE_COLOR(_local_player);

	DrawSprite(ground, x, y);
	for (; dtss->image != 0; dtss++) {
		Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
		uint32 image = dtss->image + offset;

		if (image & PALETTE_MODIFIER_COLOR) image |= palette;
		DrawSprite(image, x + pt.x, y + pt.y);
	}
}

void DrawTrainDepotSprite(int x, int y, int dir, RailType railtype)
{
	const DrawTileSprites* dts = &_depot_gfx_table[dir];
	uint32 image = dts->ground_sprite;
	uint32 offset = GetRailTypeInfo(railtype)->total_offset;

	if (image != SPR_FLAT_GRASS_TILE) image += offset;
	DrawTileSequence(x + 33, y + 17, image, dts->seq, offset);
}

void DrawDefaultWaypointSprite(int x, int y, RailType railtype)
{
	uint32 offset = GetRailTypeInfo(railtype)->total_offset;
	const DrawTileSprites* dts = &_waypoint_gfx_table[AXIS_X];

	DrawTileSequence(x, y, dts->ground_sprite + offset, dts->seq, 0);
}

typedef struct SetSignalsData {
	int cur;
	int cur_stack;
	bool stop;
	bool has_presignal;

	// presignal info
	int presignal_exits;
	int presignal_exits_free;

	// these are used to keep track of the signals that change.
	byte bit[NUM_SSD_ENTRY];
	TileIndex tile[NUM_SSD_ENTRY];

	// these are used to keep track of the stack that modifies presignals recursively
	TileIndex next_tile[NUM_SSD_STACK];
	byte next_dir[NUM_SSD_STACK];

} SetSignalsData;

static bool SetSignalsEnumProc(TileIndex tile, void* data, int track, uint length, byte* state)
{
	SetSignalsData* ssd = data;

	if (!IsTileType(tile, MP_RAILWAY)) return false;

	// the tile has signals?
	if (HasSignalOnTrack(tile, TrackdirToTrack(track))) {
		if (HasSignalOnTrackdir(tile, ReverseTrackdir(track))) {
			// yes, add the signal to the list of signals
			if (ssd->cur != NUM_SSD_ENTRY) {
				ssd->tile[ssd->cur] = tile; // remember the tile index
				ssd->bit[ssd->cur] = track; // and the controlling bit number
				ssd->cur++;
			}

			// remember if this block has a presignal.
			ssd->has_presignal |= IsPresignalEntry(tile);
		}

		if (HasSignalOnTrackdir(tile, track) && IsPresignalExit(tile)) {
			// this is an exit signal that points out from the segment
			ssd->presignal_exits++;
			if (GetSignalStateByTrackdir(tile, track) != SIGNAL_STATE_RED)
				ssd->presignal_exits_free++;
		}

		return true;
	} else if (IsTileDepotType(tile, TRANSPORT_RAIL)) {
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
	const SignalVehicleCheckStruct* dest = data;
	TileIndex tile;

	if (v->type != VEH_Train) return NULL;

	/* Find the tile outside the tunnel, for signalling */
	if (v->u.rail.track == 0x40) {
		tile = GetVehicleOutOfTunnelTile(v);
	} else {
		tile = v->tile;
	}

	/* Wrong tile, or no train? Not a match */
	if (tile != dest->tile) return NULL;

	/* Are we on the same piece of track? */
	if (dest->track & v->u.rail.track * 0x101) return v;

	return NULL;
}

/* Special check for SetSignalsAfterProc, to see if there is a vehicle on this tile */
static bool SignalVehicleCheck(TileIndex tile, uint track)
{
	SignalVehicleCheckStruct dest;

	dest.tile = tile;
	dest.track = track;

	/** @todo "Hackish" fix for the tunnel problems. This is needed because a tunnel
	 * is some kind of invisible black hole, and there is some special magic going
	 * on in there. This 'workaround' can be removed once the maprewrite is done.
	 */
	if (IsTunnelTile(tile)) {
		// It is a tunnel we're checking, we need to do some special stuff
		// because VehicleFromPos will not find the vihicle otherwise
		TileIndex end = GetOtherTunnelEnd(tile);
		DiagDirection direction = GetTunnelDirection(tile);

		dest.track = 1 << (direction & 1); // get the trackbit the vehicle would have if it has not entered the tunnel yet (ie is still visible)

		// check for a vehicle with that trackdir on the start tile of the tunnel
		if (VehicleFromPos(tile, &dest, SignalVehicleCheckProc) != NULL) return true;

		// check for a vehicle with that trackdir on the end tile of the tunnel
		if (VehicleFromPos(end, &dest, SignalVehicleCheckProc) != NULL) return true;

		// now check all tiles from start to end for a "hidden" vehicle
		// NOTE: the hashes for tiles may overlap, so this could maybe be optimised a bit by not checking every tile?
		dest.track = 0x40; // trackbit for vehicles "hidden" inside a tunnel
		for (; tile != end; tile += TileOffsByDiagDir(direction)) {
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
	const TrackPathFinderLink* link;
	uint offs;
	uint i;

	ssd->stop = false;

	/* Go through all the PF tiles */
	for (i = 0; i < lengthof(tpf->hash_head); i++) {
		/* Empty hash item */
		if (tpf->hash_head[i] == 0) continue;

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
			} while ((offs = link->next) != 0xFFFF);
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

	// then mark the signals in the segment accordingly
	for (i = 0; i != ssd->cur; i++) {
		TileIndex tile = ssd->tile[i];
		byte bit = SignalAgainstTrackdir(ssd->bit[i]);
		uint16 m2 = _m[tile].m2;

		// presignals don't turn green if there is at least one presignal exit and none are free
		if (IsPresignalEntry(tile)) {
			int ex = ssd->presignal_exits, exfree = ssd->presignal_exits_free;

			// subtract for dual combo signals so they don't count themselves
			if (IsPresignalExit(tile) && HasSignalOnTrackdir(tile, ssd->bit[i])) {
				ex--;
				if (GetSignalStateByTrackdir(tile, ssd->bit[i]) != SIGNAL_STATE_RED) exfree--;
			}

			// if we have exits and none are free, make red.
			if (ex && !exfree) goto make_red;
		}

		// check if the signal is unaffected.
		if (ssd->stop) {
make_red:
			// turn red
			if ((bit & m2) == 0) continue;
		} else {
			// turn green
			if ((bit & m2) != 0) continue;
		}

		/* Update signals on the other side of this exit-combo signal; it changed. */
		if (IsPresignalExit(tile)) {
			if (ssd->cur_stack != NUM_SSD_STACK) {
				ssd->next_tile[ssd->cur_stack] = tile;
				ssd->next_dir[ssd->cur_stack] = _dir_from_track[ssd->bit[i]];
				ssd->cur_stack++;
			} else {
				DEBUG(misc, 0) ("NUM_SSD_STACK too small"); /// @todo WTF is this???
			}
		}

		// it changed, so toggle it
		_m[tile].m2 = m2 ^ bit;
		MarkTileDirtyByTile(tile);
	}
}


bool UpdateSignalsOnSegment(TileIndex tile, DiagDirection direction)
{
	SetSignalsData ssd;
	int result = -1;

	ssd.cur_stack = 0;

	for (;;) {
		// go through one segment and update all signals pointing into that segment.
		ssd.cur = ssd.presignal_exits = ssd.presignal_exits_free = 0;
		ssd.has_presignal = false;

		FollowTrack(tile, 0xC000 | TRANSPORT_RAIL, direction, SetSignalsEnumProc, SetSignalsAfterProc, &ssd);
		ChangeSignalStates(&ssd);

		// remember the result only for the first iteration.
		if (result < 0) {
			// stay in depot while segment is occupied or while all presignal exits are blocked
			result = ssd.stop || (ssd.presignal_exits > 0 && ssd.presignal_exits_free == 0);
		}

		// if any exit signals were changed, we need to keep going to modify the stuff behind those.
		if (ssd.cur_stack == 0) break;

		// one or more exit signals were changed, so we need to update another segment too.
		tile = ssd.next_tile[--ssd.cur_stack];
		direction = ssd.next_dir[ssd.cur_stack];
	}

	return result != 0;
}

void SetSignalsOnBothDir(TileIndex tile, byte track)
{
	static const DiagDirection _search_dir_1[] = {
		DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE
	};
	static const DiagDirection _search_dir_2[] = {
		DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE
	};

	UpdateSignalsOnSegment(tile, _search_dir_1[track]);
	UpdateSignalsOnSegment(tile, _search_dir_2[track]);
}

static uint GetSlopeZ_Track(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	if (tileh == SLOPE_FLAT) return z;
	if (IsPlainRailTile(tile)) {
		uint f = GetRailFoundation(tileh, GetTrackBits(tile));

		if (f != 0) {
			if (IsSteepSlope(tileh)) {
				z += TILE_HEIGHT;
			} else if (f < 15) {
				return z + TILE_HEIGHT; // leveled foundation
			}
			tileh = _inclined_tileh[f - 15]; // inclined foundation
		}
		return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
	} else {
		return z + TILE_HEIGHT;
	}
}

static Slope GetSlopeTileh_Track(TileIndex tile, Slope tileh)
{
	if (tileh == SLOPE_FLAT) return SLOPE_FLAT;
	if (IsPlainRailTile(tile)) {
		uint f = GetRailFoundation(tileh, GetTrackBits(tile));

		if (f == 0) return tileh;
		if (f < 15) return SLOPE_FLAT; // leveled foundation
		return _inclined_tileh[f - 15]; // inclined foundation
	} else {
		return SLOPE_FLAT;
	}
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
	RailGroundType old_ground = GetRailGroundType(tile);
	RailGroundType new_ground;

	switch (_opt.landscape) {
		case LT_HILLY:
			if (GetTileZ(tile) > _opt.snow_line) {
				new_ground = RAIL_GROUND_ICE_DESERT;
				goto set_ground;
			}
			break;

		case LT_DESERT:
			if (GetTropicZone(tile) == TROPICZONE_DESERT) {
				new_ground = RAIL_GROUND_ICE_DESERT;
				goto set_ground;
			}
			break;
	}

	if (!IsPlainRailTile(tile)) return;

	new_ground = RAIL_GROUND_GRASS;

	if (old_ground != RAIL_GROUND_BARREN) { /* wait until bottom is green */
		/* determine direction of fence */
		TrackBits rail = GetTrackBits(tile);

		switch (rail) {
			case TRACK_BIT_UPPER: new_ground = RAIL_GROUND_FENCE_HORIZ1; break;
			case TRACK_BIT_LOWER: new_ground = RAIL_GROUND_FENCE_HORIZ2; break;
			case TRACK_BIT_LEFT:  new_ground = RAIL_GROUND_FENCE_VERT1;  break;
			case TRACK_BIT_RIGHT: new_ground = RAIL_GROUND_FENCE_VERT2;  break;

			default: {
				PlayerID owner = GetTileOwner(tile);

				if (rail == (TRACK_BIT_LOWER | TRACK_BIT_RIGHT) || (
							(rail & TRACK_BIT_3WAY_NW) == 0 &&
							(rail & TRACK_BIT_X)
						)) {
					TileIndex n = tile + TileDiffXY(0, -1);
					TrackBits nrail = GetTrackBits(n);

					if (!IsTileType(n, MP_RAILWAY) ||
							!IsTileOwner(n, owner) ||
							nrail == TRACK_BIT_UPPER ||
							nrail == TRACK_BIT_LEFT) {
						new_ground = RAIL_GROUND_FENCE_NW;
					}
				}

				if (rail == (TRACK_BIT_UPPER | TRACK_BIT_LEFT) || (
							(rail & TRACK_BIT_3WAY_SE) == 0 &&
							(rail & TRACK_BIT_X)
						)) {
					TileIndex n = tile + TileDiffXY(0, 1);
					TrackBits nrail = GetTrackBits(n);

					if (!IsTileType(n, MP_RAILWAY) ||
							!IsTileOwner(n, owner) ||
							nrail == TRACK_BIT_LOWER ||
							nrail == TRACK_BIT_RIGHT) {
						new_ground = (new_ground == RAIL_GROUND_FENCE_NW) ?
							RAIL_GROUND_FENCE_SENW : RAIL_GROUND_FENCE_SE;
					}
				}

				if (rail == (TRACK_BIT_LOWER | TRACK_BIT_LEFT) || (
							(rail & TRACK_BIT_3WAY_NE) == 0 &&
							(rail & TRACK_BIT_Y)
						)) {
					TileIndex n = tile + TileDiffXY(-1, 0);
					TrackBits nrail = GetTrackBits(n);

					if (!IsTileType(n, MP_RAILWAY) ||
							!IsTileOwner(n, owner) ||
							nrail == TRACK_BIT_UPPER ||
							nrail == TRACK_BIT_RIGHT) {
						new_ground = RAIL_GROUND_FENCE_NE;
					}
				}

				if (rail == (TRACK_BIT_UPPER | TRACK_BIT_RIGHT) || (
							(rail & TRACK_BIT_3WAY_SW) == 0 &&
							(rail & TRACK_BIT_Y)
						)) {
					TileIndex n = tile + TileDiffXY(1, 0);
					TrackBits nrail = GetTrackBits(n);

					if (!IsTileType(n, MP_RAILWAY) ||
							!IsTileOwner(n, owner) ||
							nrail == TRACK_BIT_LOWER ||
							nrail == TRACK_BIT_LEFT) {
						new_ground = (new_ground == RAIL_GROUND_FENCE_NE) ?
							RAIL_GROUND_FENCE_NESW : RAIL_GROUND_FENCE_SW;
					}
				}
				break;
			}
		}
	}

set_ground:
	if (old_ground != new_ground) {
		SetRailGroundType(tile, new_ground);
		MarkTileDirtyByTile(tile);
	}
}


static uint32 GetTileTrackStatus_Track(TileIndex tile, TransportType mode)
{
	byte a;
	uint16 b;

	if (mode != TRANSPORT_RAIL) return 0;

	if (IsPlainRailTile(tile)) {
		TrackBits rails = GetTrackBits(tile);
		uint32 ret = rails * 0x101;

		if (HasSignals(tile)) {
			a = _m[tile].m3;
			b = _m[tile].m2;

			b &= a;

			/* When signals are not present (in neither
			 * direction), we pretend them to be green. (So if
			 * signals are only one way, the other way will
			 * implicitely become `red' */
			if ((a & 0xC0) == 0) b |= 0xC0;
			if ((a & 0x30) == 0) b |= 0x30;

			if ((b & 0x80) == 0) ret |= 0x10070000;
			if ((b & 0x40) == 0) ret |= 0x07100000;
			if ((b & 0x20) == 0) ret |= 0x20080000;
			if ((b & 0x10) == 0) ret |= 0x08200000;
		} else {
			if (rails == TRACK_BIT_CROSS) ret |= 0x40;
		}
		return ret;
	} else {
		if (GetRailTileSubtype(tile) == RAIL_SUBTYPE_DEPOT) {
			return AxisToTrackBits(DiagDirToAxis(GetRailDepotDirection(tile))) * 0x101;
		} else {
			return GetRailWaypointBits(tile) * 0x101;
		}
	}
}

static void ClickTile_Track(TileIndex tile)
{
	if (IsTileDepotType(tile, TRANSPORT_RAIL)) {
		ShowDepotWindow(tile, VEH_Train);
	} else if (IsRailWaypoint(tile)) {
		ShowRenameWaypointWindow(GetWaypointByTile(tile));
	}
}

static void GetTileDesc_Track(TileIndex tile, TileDesc *td)
{
	td->owner = GetTileOwner(tile);
	switch (GetRailTileType(tile)) {
		case RAIL_TILE_NORMAL:
			td->str = STR_1021_RAILROAD_TRACK;
			break;

		case RAIL_TILE_SIGNALS: {
			const StringID signal_type[] = {
				STR_RAILROAD_TRACK_WITH_NORMAL_SIGNALS,
				STR_RAILROAD_TRACK_WITH_PRESIGNALS,
				STR_RAILROAD_TRACK_WITH_EXITSIGNALS,
				STR_RAILROAD_TRACK_WITH_COMBOSIGNALS
			};

			td->str = signal_type[GetSignalType(tile)];
			break;
		}

		case RAIL_TILE_DEPOT_WAYPOINT:
		default:
			td->str = (GetRailTileSubtype(tile) == RAIL_SUBTYPE_DEPOT) ?
				STR_1023_RAILROAD_TRAIN_DEPOT : STR_LANDINFO_WAYPOINT;
			break;
	}
}

static void ChangeTileOwner_Track(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != PLAYER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	} else {
		DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static const byte _fractcoords_behind[4] = { 0x8F, 0x8, 0x80, 0xF8 };
static const byte _fractcoords_enter[4] = { 0x8A, 0x48, 0x84, 0xA8 };
static const byte _deltacoord_leaveoffset[8] = {
	-1,  0,  1,  0, /* x */
	 0,  1,  0, -1  /* y */
};

static uint32 VehicleEnter_Track(Vehicle *v, TileIndex tile, int x, int y)
{
	byte fract_coord;
	byte fract_coord_leave;
	DiagDirection dir;
	int length;

	// this routine applies only to trains in depot tiles
	if (v->type != VEH_Train || !IsTileDepotType(tile, TRANSPORT_RAIL)) return 0;

	/* depot direction */
	dir = GetRailDepotDirection(tile);

	/* calculate the point where the following wagon should be activated */
	/* this depends on the length of the current vehicle */
	length = v->u.rail.cached_veh_length;

	fract_coord_leave =
		((_fractcoords_enter[dir] & 0x0F) + // x
			(length + 1) * _deltacoord_leaveoffset[dir]) +
		(((_fractcoords_enter[dir] >> 4) +  // y
			((length + 1) * _deltacoord_leaveoffset[dir+4])) << 4);

	fract_coord = (x & 0xF) + ((y & 0xF) << 4);

	if (_fractcoords_behind[dir] == fract_coord) {
		/* make sure a train is not entering the tile from behind */
		return 8;
	} else if (_fractcoords_enter[dir] == fract_coord) {
		if (DiagDirToDir(ReverseDiagDir(dir)) == v->direction) {
			/* enter the depot */
			v->u.rail.track = 0x80,
			v->vehstatus |= VS_HIDDEN; /* hide it */
			v->direction = ReverseDir(v->direction);
			if (v->next == NULL) VehicleEnterDepot(v);
			v->tile = tile;

			InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
			return 4;
		}
	} else if (fract_coord_leave == fract_coord) {
		if (DiagDirToDir(dir) == v->direction) {
			/* leave the depot? */
			if ((v = v->next) != NULL) {
				v->vehstatus &= ~VS_HIDDEN;
				v->u.rail.track = (DiagDirToAxis(dir) == AXIS_X ? 1 : 2);
			}
		}
	}

	return 0;
}


const TileTypeProcs _tile_type_rail_procs = {
	DrawTile_Track,           /* draw_tile_proc */
	GetSlopeZ_Track,          /* get_slope_z_proc */
	ClearTile_Track,          /* clear_tile_proc */
	GetAcceptedCargo_Track,   /* get_accepted_cargo_proc */
	GetTileDesc_Track,        /* get_tile_desc_proc */
	GetTileTrackStatus_Track, /* get_tile_track_status_proc */
	ClickTile_Track,          /* click_tile_proc */
	AnimateTile_Track,        /* animate_tile_proc */
	TileLoop_Track,           /* tile_loop_clear */
	ChangeTileOwner_Track,    /* change_tile_owner_clear */
	NULL,                     /* get_produced_cargo_proc */
	VehicleEnter_Track,       /* vehicle_enter_tile_proc */
	GetSlopeTileh_Track,      /* get_slope_tileh_proc */
};
