/* $Id$ */

/** @file rail_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "bridge.h"
#include "cmd_helper.h"
#include "debug.h"
#include "functions.h"
#include "rail_map.h"
#include "road_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "landscape.h"
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
#include "newgrf_engine.h"
#include "newgrf_callbacks.h"
#include "newgrf_station.h"
#include "train.h"
#include "misc/autoptr.hpp"
#include "autoslope.h"

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

/** Struct used in EnsureNoTrainOnTrack() */
struct TrainOnTrackData {
	TileIndex tile;       ///< tile to check
	uint z;               ///< tile max Z
	TrackBits rail_bits;  ///< trackbits of interest
};

static void *EnsureNoTrainOnTrackProc(Vehicle *v, void *data)
{
	const TrainOnTrackData *info = (const TrainOnTrackData *)data;

	if (v->tile != info->tile || v->type != VEH_TRAIN) return NULL;
	if (v->z_pos > info->z) return NULL;

	if ((v->u.rail.track != info->rail_bits) && !TracksOverlap(v->u.rail.track | info->rail_bits)) return NULL;

	_error_message = VehicleInTheWayErrMsg(v);
	return v;
}

/**
 * Tests if a vehicle interacts with the specified track.
 * All track bits interact except parallel TRACK_BIT_HORZ or TRACK_BIT_VERT.
 *
 * @param tile The tile.
 * @param track The track.
 */
static bool EnsureNoTrainOnTrack(TileIndex tile, Track track)
{
	TrainOnTrackData info;

	info.tile = tile;
	info.z = GetTileMaxZ(tile);
	info.rail_bits = TrackToTrackBits(track);

	return VehicleFromPos(tile, &info, EnsureNoTrainOnTrackProc) == NULL;
}

static bool CheckTrackCombination(TileIndex tile, TrackBits to_build, uint flags)
{
	TrackBits current; // The current track layout
	TrackBits future;  // The track layout we want to build
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


/** Valid TrackBits on a specific (non-steep)-slope without foundation */
static const TrackBits _valid_tracks_without_foundation[15] = {
	TRACK_BIT_ALL,
	TRACK_BIT_RIGHT,
	TRACK_BIT_UPPER,
	TRACK_BIT_X,

	TRACK_BIT_LEFT,
	TRACK_BIT_NONE,
	TRACK_BIT_Y,
	TRACK_BIT_LOWER,

	TRACK_BIT_LOWER,
	TRACK_BIT_Y,
	TRACK_BIT_NONE,
	TRACK_BIT_LEFT,

	TRACK_BIT_X,
	TRACK_BIT_UPPER,
	TRACK_BIT_RIGHT,
};

/** Valid TrackBits on a specific (non-steep)-slope with leveled foundation */
static const TrackBits _valid_tracks_on_leveled_foundation[15] = {
	TRACK_BIT_NONE,
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
};

/**
 * Checks if a track combination is valid on a specific slope and returns the needed foundation.
 *
 * @param tileh Tile slope.
 * @param bits  Trackbits.
 * @return Needed foundation or FOUNDATION_INVALID if track/slope combination is not allowed.
 */
Foundation GetRailFoundation(Slope tileh, TrackBits bits)
{
	if (bits == TRACK_BIT_NONE) return FOUNDATION_NONE;

	if (IsSteepSlope(tileh)) {
		/* Test for inclined foundations */
		if (bits == TRACK_BIT_X) return FOUNDATION_INCLINED_X;
		if (bits == TRACK_BIT_Y) return FOUNDATION_INCLINED_Y;

		/* Get higher track */
		Corner highest_corner = GetHighestSlopeCorner(tileh);
		TrackBits higher_track = CornerToTrackBits(highest_corner);

		/* Only higher track? */
		if (bits == higher_track) return HalftileFoundation(highest_corner);

		/* Overlap with higher track? */
		if (TracksOverlap(bits | higher_track)) return FOUNDATION_INVALID;

		/* either lower track or both higher and lower track */
		return ((bits & higher_track) != 0 ? FOUNDATION_STEEP_BOTH : FOUNDATION_STEEP_LOWER);
	} else {
		if ((~_valid_tracks_without_foundation[tileh] & bits) == 0) return FOUNDATION_NONE;

		bool valid_on_leveled = ((~_valid_tracks_on_leveled_foundation[tileh] & bits) == 0);

		Corner track_corner;
		switch (bits) {
			case TRACK_BIT_LEFT:  track_corner = CORNER_W; break;
			case TRACK_BIT_LOWER: track_corner = CORNER_S; break;
			case TRACK_BIT_RIGHT: track_corner = CORNER_E; break;
			case TRACK_BIT_UPPER: track_corner = CORNER_N; break;

			case TRACK_BIT_HORZ:
				if (tileh == SLOPE_N) return HalftileFoundation(CORNER_N);
				if (tileh == SLOPE_S) return HalftileFoundation(CORNER_S);
				return (valid_on_leveled ? FOUNDATION_LEVELED : FOUNDATION_INVALID);

			case TRACK_BIT_VERT:
				if (tileh == SLOPE_W) return HalftileFoundation(CORNER_W);
				if (tileh == SLOPE_E) return HalftileFoundation(CORNER_E);
				return (valid_on_leveled ? FOUNDATION_LEVELED : FOUNDATION_INVALID);

			case TRACK_BIT_X:
				if (HasSlopeHighestCorner(tileh)) return FOUNDATION_INCLINED_X;
				return (valid_on_leveled ? FOUNDATION_LEVELED : FOUNDATION_INVALID);

			case TRACK_BIT_Y:
				if (HasSlopeHighestCorner(tileh)) return FOUNDATION_INCLINED_Y;
				return (valid_on_leveled ? FOUNDATION_LEVELED : FOUNDATION_INVALID);

			default:
				return (valid_on_leveled ? FOUNDATION_LEVELED : FOUNDATION_INVALID);
		}
		/* Single diagonal track */

		/* Track must be at least valid on leveled foundation */
		if (!valid_on_leveled) return FOUNDATION_INVALID;

		/* If slope has three raised corners, build leveled foundation */
		if (HasSlopeHighestCorner(ComplementSlope(tileh))) return FOUNDATION_LEVELED;

		/* If neighboured corners of track_corner are lowered, build halftile foundation */
		if ((tileh & SlopeWithThreeCornersRaised(OppositeCorner(track_corner))) == SlopeWithOneCornerRaised(track_corner)) return HalftileFoundation(track_corner);

		/* else special anti-zig-zag foundation */
		return SpecialRailFoundation(track_corner);
	}
}


/**
 * Tests if a track can be build on a tile.
 *
 * @param tileh Tile slope.
 * @param rail_bits Tracks to build.
 * @param existing Tracks already built.
 * @param tile Tile (used for water test)
 * @return Error message or cost for foundation building.
 */
static CommandCost CheckRailSlope(Slope tileh, TrackBits rail_bits, TrackBits existing, TileIndex tile)
{
	/* don't allow building on the lower side of a coast */
	if (IsTileType(tile, MP_WATER) || (IsTileType(tile, MP_RAILWAY) && (GetRailGroundType(tile) == RAIL_GROUND_WATER))) {
		if (!IsSteepSlope(tileh) && ((~_valid_tracks_on_leveled_foundation[tileh] & (rail_bits | existing)) != 0)) return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);
	}

	Foundation f_new = GetRailFoundation(tileh, rail_bits | existing);

	/* check track/slope combination */
	if ((f_new == FOUNDATION_INVALID) ||
	    ((f_new != FOUNDATION_NONE) && (!_patches.build_on_slopes || _is_old_ai_player))
	   ) return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	Foundation f_old = GetRailFoundation(tileh, existing);
	return CommandCost(f_new != f_old ? _price.terraform : (Money)0);
}

/* Validate functions for rail building */
static inline bool ValParamTrackOrientation(Track track) {return IsValidTrack(track);}

/** Build a single piece of rail
 * @param tile tile  to build on
 * @param flags operation to perform
 * @param p1 railtype of being built piece (normal, mono, maglev)
 * @param p2 rail track to build
 */
CommandCost CmdBuildSingleRail(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Slope tileh;
	RailType railtype;
	Track track;
	TrackBits trackbit;
	CommandCost cost;
	CommandCost ret;

	if (!ValParamRailtype(p1) || !ValParamTrackOrientation((Track)p2)) return CMD_ERROR;
	railtype = (RailType)p1;
	track = (Track)p2;

	tileh = GetTileSlope(tile, NULL);
	trackbit = TrackToTrackBits(track);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (!CheckTrackCombination(tile, trackbit, flags) ||
					!EnsureNoTrainOnTrack(tile, track)) {
				return CMD_ERROR;
			}
			if (!IsTileOwner(tile, _current_player) ||
					!IsCompatibleRail(GetRailType(tile), railtype)) {
				/* Get detailed error message */
				return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}

			ret = CheckRailSlope(tileh, trackbit, GetTrackBits(tile), tile);
			if (CmdFailed(ret)) return ret;
			cost.AddCost(ret);

			/* If the rail types don't match, try to convert only if engines of
			 * the present rail type are powered on the new rail type. */
			if (GetRailType(tile) != railtype && HasPowerOnRail(GetRailType(tile), railtype)) {
				ret = DoCommand(tile, tile, railtype, flags, CMD_CONVERT_RAIL);
				if (CmdFailed(ret)) return ret;
				cost.AddCost(ret);
			}

			if (flags & DC_EXEC) {
				SetRailGroundType(tile, RAIL_GROUND_BARREN);
				SetTrackBits(tile, GetTrackBits(tile) | trackbit);
			}
			break;

		case MP_ROAD:
#define M(x) (1 << (x))
			/* Level crossings may only be built on these slopes */
			if (!HASBIT(M(SLOPE_SEN) | M(SLOPE_ENW) | M(SLOPE_NWS) | M(SLOPE_NS) | M(SLOPE_WSE) | M(SLOPE_EW) | M(SLOPE_FLAT), tileh)) {
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}
#undef M

			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

			if (GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
				if (HasRoadWorks(tile)) return_cmd_error(STR_ROAD_WORKS_IN_PROGRESS);

				RoadTypes roadtypes = GetRoadTypes(tile);
				RoadBits road = GetRoadBits(tile, ROADTYPE_ROAD);
				RoadBits tram = GetRoadBits(tile, ROADTYPE_TRAM);
				switch (roadtypes) {
					default: break;
					case ROADTYPES_TRAM:
						/* Tram crossings must always have road. */
						SetRoadOwner(tile, ROADTYPE_ROAD, _current_player);
						roadtypes |= ROADTYPES_ROAD;
						break;

					case ROADTYPES_ROADTRAM: if (road == tram) break;
						/* FALL THROUGH */
					case ROADTYPES_ROADHWAY: // Road and highway are incompatible in this case
					case ROADTYPES_TRAMHWAY: // Tram and highway are incompatible in this case
					case ROADTYPES_ALL:      // Also incompatible
						return CMD_ERROR;
				}

				road |= tram | GetRoadBits(tile, ROADTYPE_HWAY);

				if ((track == TRACK_X && road == ROAD_Y) ||
						(track == TRACK_Y && road == ROAD_X)) {
					if (flags & DC_EXEC) {
						MakeRoadCrossing(tile, GetRoadOwner(tile, ROADTYPE_ROAD), GetRoadOwner(tile, ROADTYPE_TRAM), GetRoadOwner(tile, ROADTYPE_HWAY), _current_player, (track == TRACK_X ? AXIS_Y : AXIS_X), railtype, roadtypes, GetTownIndex(tile));
					}
					break;
				}
			}

			if (IsLevelCrossing(tile) && GetCrossingRailBits(tile) == trackbit) {
				return_cmd_error(STR_1007_ALREADY_BUILT);
			}
			/* FALLTHROUGH */

		default:
			bool water_ground = IsTileType(tile, MP_WATER) && !IsSteepSlope(tileh) && HasSlopeHighestCorner(tileh);

			ret = CheckRailSlope(tileh, trackbit, TRACK_BIT_NONE, tile);
			if (CmdFailed(ret)) return ret;
			cost.AddCost(ret);

			ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return ret;
			cost.AddCost(ret);

			if (water_ground) {
				cost.AddCost(-_price.clear_water);
				cost.AddCost(_price.purchase_land);
			}

			if (flags & DC_EXEC) {
				MakeRailNormal(tile, _current_player, trackbit, railtype);
				if (water_ground) SetRailGroundType(tile, RAIL_GROUND_WATER);
			}
			break;
	}

	if (flags & DC_EXEC) {
		MarkTileDirtyByTile(tile);
		SetSignalsOnBothDir(tile, track);
		YapfNotifyTrackLayoutChange(tile, track);
	}

	return cost.AddCost(RailBuildCost(railtype));
}

/** Remove a single piece of track
 * @param tile tile to remove track from
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 rail orientation
 */
CommandCost CmdRemoveSingleRail(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Track track = (Track)p2;
	TrackBits trackbit;
	CommandCost cost(_price.remove_rail);
	bool crossing = false;

	if (!ValParamTrackOrientation((Track)p2)) return CMD_ERROR;
	trackbit = TrackToTrackBits(track);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	switch (GetTileType(tile)) {
		case MP_ROAD: {
			if (!IsLevelCrossing(tile) ||
					GetCrossingRailBits(tile) != trackbit ||
					(_current_player != OWNER_WATER && !CheckTileOwnership(tile)) ||
					!EnsureNoVehicleOnGround(tile)) {
				return CMD_ERROR;
			}

			if (flags & DC_EXEC) {
				MakeRoadNormal(tile, GetCrossingRoadBits(tile), GetRoadTypes(tile), GetTownIndex(tile), GetRoadOwner(tile, ROADTYPE_ROAD), GetRoadOwner(tile, ROADTYPE_TRAM), GetRoadOwner(tile, ROADTYPE_HWAY));
			}
			break;
		}

		case MP_RAILWAY: {
			TrackBits present;

			if (!IsPlainRailTile(tile) ||
					(_current_player != OWNER_WATER && !CheckTileOwnership(tile)) ||
					!EnsureNoTrainOnTrack(tile, track)) {
				return CMD_ERROR;
			}

			present = GetTrackBits(tile);
			if ((present & trackbit) == 0) return CMD_ERROR;
			if (present == (TRACK_BIT_X | TRACK_BIT_Y)) crossing = true;

			/* Charge extra to remove signals on the track, if they are there */
			if (HasSignalOnTrack(tile, track))
				cost.AddCost(DoCommand(tile, track, 0, flags, CMD_REMOVE_SIGNALS));

			if (flags & DC_EXEC) {
				present ^= trackbit;
				if (present == 0) {
					if (GetRailGroundType(tile) == RAIL_GROUND_WATER) {
						MakeShore(tile);
					} else {
						DoClearSquare(tile);
					}
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


/**
 * Called from water_cmd if a non-flat rail-tile gets flooded and should be converted to shore.
 * The function floods the lower halftile, if the tile has a halftile foundation.
 *
 * @param t The tile to flood.
 */
void FloodHalftile(TileIndex t)
{
	if (GetRailGroundType(t) == RAIL_GROUND_WATER) return;

	Slope tileh = GetTileSlope(t, NULL);
	TrackBits rail_bits = GetTrackBits(t);

	if (!IsSteepSlope(tileh) && HasSlopeHighestCorner(tileh)) {
		TrackBits lower_track = CornerToTrackBits(OppositeCorner(GetHighestSlopeCorner(tileh)));

		TrackBits to_remove = lower_track & rail_bits;
		if (to_remove != 0) {
			_current_player = OWNER_WATER;
			if (CmdFailed(DoCommand(t, 0, FIND_FIRST_BIT(to_remove), DC_EXEC, CMD_REMOVE_SINGLE_RAIL))) return; // not yet floodable
			rail_bits = rail_bits & ~to_remove;
			if (rail_bits == 0) {
				MakeShore(t);
				MarkTileDirtyByTile(t);
				return;
			}
		}

		if (IsNonContinuousFoundation(GetRailFoundation(tileh, rail_bits))) {
			SetRailGroundType(t, RAIL_GROUND_WATER);
			MarkTileDirtyByTile(t);
		}
	}
}

static const TileIndexDiffC _trackdelta[] = {
	{ -1,  0 }, {  0,  1 }, { -1,  0 }, {  0,  1 }, {  1,  0 }, {  0,  1 },
	{  0,  0 },
	{  0,  0 },
	{  1,  0 }, {  0, -1 }, {  0, -1 }, {  1,  0 }, {  0, -1 }, { -1,  0 },
	{  0,  0 },
	{  0,  0 }
};


static CommandCost ValidateAutoDrag(Trackdir *trackdir, TileIndex start, TileIndex end)
{
	int x = TileX(start);
	int y = TileY(start);
	int ex = TileX(end);
	int ey = TileY(end);
	int dx, dy, trdx, trdy;

	if (!ValParamTrackOrientation(TrackdirToTrack(*trackdir))) return CMD_ERROR;

	/* calculate delta x,y from start to end tile */
	dx = ex - x;
	dy = ey - y;

	/* calculate delta x,y for the first direction */
	trdx = _trackdelta[*trackdir].x;
	trdy = _trackdelta[*trackdir].y;

	if (!IsDiagonalTrackdir(*trackdir)) {
		trdx += _trackdelta[*trackdir ^ 1].x;
		trdy += _trackdelta[*trackdir ^ 1].y;
	}

	/* validate the direction */
	while (
		(trdx <= 0 && dx > 0) ||
		(trdx >= 0 && dx < 0) ||
		(trdy <= 0 && dy > 0) ||
		(trdy >= 0 && dy < 0)
	) {
		if (!HASBIT(*trackdir, 3)) { // first direction is invalid, try the other
			SetBitT(*trackdir, 3); // reverse the direction
			trdx = -trdx;
			trdy = -trdy;
		} else { // other direction is invalid too, invalid drag
			return CMD_ERROR;
		}
	}

	/* (for diagonal tracks, this is already made sure of by above test), but:
	 * for non-diagonal tracks, check if the start and end tile are on 1 line */
	if (!IsDiagonalTrackdir(*trackdir)) {
		trdx = _trackdelta[*trackdir].x;
		trdy = _trackdelta[*trackdir].y;
		if (abs(dx) != abs(dy) && abs(dx) + abs(trdy) != abs(dy) + abs(trdx))
			return CMD_ERROR;
	}

	return CommandCost();
}

/** Build a stretch of railroad tracks.
 * @param tile start tile of drag
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-3) - railroad type normal/maglev (0 = normal, 1 = mono, 2 = maglev)
 * - p2 = (bit 4-6) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit 7)   - 0 = build, 1 = remove tracks
 */
static CommandCost CmdRailTrackHelper(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost ret, total_cost;
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
			total_cost.AddCost(ret);
		}

		if (tile == end_tile) break;

		tile += ToTileIndexDiff(_trackdelta[trackdir]);

		/* toggle railbit for the non-diagonal tracks */
		if (!IsDiagonalTrackdir(trackdir)) ToggleBitT(trackdir, 0);
	}

	return (total_cost.GetCost() == 0) ? CMD_ERROR : total_cost;
}

/** Build rail on a stretch of track.
 * Stub for the unified rail builder/remover
 * @param tile start tile of drag
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-3) - railroad type normal/maglev (0 = normal, 1 = mono, 2 = maglev)
 * - p2 = (bit 4-6) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit 7)   - 0 = build, 1 = remove tracks
 * @see CmdRailTrackHelper
 */
CommandCost CmdBuildRailroadTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdRailTrackHelper(tile, flags, p1, CLRBIT(p2, 7));
}

/** Build rail on a stretch of track.
 * Stub for the unified rail builder/remover
 * @param tile start tile of drag
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-3) - railroad type normal/maglev (0 = normal, 1 = mono, 2 = maglev)
 * - p2 = (bit 4-6) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit 7)   - 0 = build, 1 = remove tracks
 * @see CmdRailTrackHelper
 */
CommandCost CmdRemoveRailroadTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdRailTrackHelper(tile, flags, p1, SETBIT(p2, 7));
}

/** Build a train depot
 * @param tile position of the train depot
 * @param flags operation to perform
 * @param p1 rail type
 * @param p2 bit 0..1 entrance direction (DiagDirection)
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
CommandCost CmdBuildTrainDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost;
	Slope tileh;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* check railtype and valid direction for depot (0 through 3), 4 in total */
	if (!ValParamRailtype(p1)) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);

	DiagDirection dir = Extract<DiagDirection, 0>(p2);

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
				!CanBuildDepotByTileh(dir, tileh)
			)) {
		return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	Depot *d = new Depot(tile);

	if (d == NULL) return CMD_ERROR;
	AutoPtrT<Depot> d_auto_delete = d;

	if (flags & DC_EXEC) {
		MakeRailDepot(tile, _current_player, dir, (RailType)p1);
		MarkTileDirtyByTile(tile);

		d->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		UpdateSignalsOnSegment(tile, dir);
		YapfNotifyTrackLayoutChange(tile, TrackdirToTrack(DiagdirToDiagTrackdir(dir)));
		d_auto_delete.Detach();
	}

	return cost.AddCost(_price.build_train_depot);
}

/** Build signals, alternate between double/single, signal/semaphore,
 * pre/exit/combo-signals, and what-else not. If the rail piece does not
 * have any signals, bit 4 (cycle signal-type) is ignored
 * @param tile tile where to build the signals
 * @param flags operation to perform
 * @param p1 various bitstuffed elements
 * - p1 = (bit 0-2) - track-orientation, valid values: 0-5 (Track enum)
 * - p1 = (bit 3)   - 1 = override signal/semaphore, or pre/exit/combo signal (CTRL-toggle)
 * - p1 = (bit 4)   - 0 = signals, 1 = semaphores
 * @param p2 used for CmdBuildManySignals() to copy direction of first signal
 * TODO: p2 should be replaced by two bits for "along" and "against" the track.
 */
CommandCost CmdBuildSingleSignal(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Track track = (Track)GB(p1, 0, 3);
	bool pre_signal = HASBIT(p1, 3);
	SignalVariant sigvar = (pre_signal ^ HASBIT(p1, 4)) ? SIG_SEMAPHORE : SIG_ELECTRIC;
	CommandCost cost;

	if (!ValParamTrackOrientation(track) || !IsTileType(tile, MP_RAILWAY) || !EnsureNoTrainOnTrack(tile, track))
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
		/* build new signals */
		cost = CommandCost(_price.build_signals);
	} else {
		if (p2 != 0 && sigvar != GetSignalVariant(tile, track)) {
			/* convert signals <-> semaphores */
			cost = CommandCost(_price.build_signals + _price.remove_signals);
		} else {
			/* it is free to change orientation/pre-exit-combo signals */
			cost = CommandCost();
		}
	}

	if (flags & DC_EXEC) {
		if (!HasSignals(tile)) {
			/* there are no signals at all on this tile yet */
			SetHasSignals(tile, true);
			SetSignalStates(tile, 0xF); // all signals are on
			SetPresentSignals(tile, 0); // no signals built by default
			SetSignalType(tile, track, SIGTYPE_NORMAL);
			SetSignalVariant(tile, track, sigvar);
		}

		if (p2 == 0) {
			if (!HasSignalOnTrack(tile, track)) {
				/* build new signals */
				SetPresentSignals(tile, GetPresentSignals(tile) | SignalOnTrack(track));
				SetSignalType(tile, track, SIGTYPE_NORMAL);
				SetSignalVariant(tile, track, sigvar);
			} else {
				if (pre_signal) {
					/* cycle between normal -> pre -> exit -> combo -> ... */
					SignalType type = GetSignalType(tile, track);

					SetSignalType(tile, track, type == SIGTYPE_COMBO ? SIGTYPE_NORMAL : (SignalType)(type + 1));
				} else {
					CycleSignalSide(tile, track);
				}
			}
		} else {
			/* If CmdBuildManySignals is called with copying signals, just copy the
			 * direction of the first signal given as parameter by CmdBuildManySignals */
			SetPresentSignals(tile, (GetPresentSignals(tile) & ~SignalOnTrack(track)) | (p2 & SignalOnTrack(track)));
			SetSignalVariant(tile, track, sigvar);
		}

		MarkTileDirtyByTile(tile);
		SetSignalsOnBothDir(tile, track);
		YapfNotifyTrackLayoutChange(tile, track);
	}

	return cost;
}

static bool CheckSignalAutoFill(TileIndex &tile, Trackdir &trackdir, int &signal_ctr, bool remove)
{
	tile = AddTileIndexDiffCWrap(tile, _trackdelta[trackdir]);
	if (tile == INVALID_TILE) return false;

	/* Check for track bits on the new tile */
	uint32 ts = GetTileTrackStatus(tile, TRANSPORT_RAIL, 0);
	TrackdirBits trackdirbits = (TrackdirBits)(ts & TRACKDIR_BIT_MASK);

	if (TracksOverlap(TrackdirBitsToTrackBits(trackdirbits))) return false;
	trackdirbits &= TrackdirReachesTrackdirs(trackdir);

	/* No track bits, must stop */
	if (trackdirbits == TRACKDIR_BIT_NONE) return false;

	/* Get the first track dir */
	trackdir = RemoveFirstTrackdir(&trackdirbits);

	/* Any left? It's a junction so we stop */
	if (trackdirbits != TRACKDIR_BIT_NONE) return false;

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (IsRailDepot(tile)) return false;
			if (!remove && HasSignalOnTrack(tile, TrackdirToTrack(trackdir))) return false;
			signal_ctr++;
			if (IsDiagonalTrackdir(trackdir)) {
				signal_ctr++;
				/* Ensure signal_ctr even so X and Y pieces get signals */
				CLRBIT(signal_ctr, 0);
			}
			return true;

		case MP_ROAD:
			if (!IsLevelCrossing(tile)) return false;
			signal_ctr += 2;
			return true;

		case MP_TUNNELBRIDGE: {
			TileIndex orig_tile = tile;
			/* Skip to end of tunnel or bridge */
			if (IsBridge(tile)) {
				if (GetBridgeTransportType(tile) != TRANSPORT_RAIL) return false;
				if (GetBridgeRampDirection(tile) != TrackdirToExitdir(trackdir)) return false;
				tile = GetOtherBridgeEnd(tile);
			} else {
				if (GetTunnelTransportType(tile) != TRANSPORT_RAIL) return false;
				if (GetTunnelDirection(tile) != TrackdirToExitdir(trackdir)) return false;
				tile = GetOtherTunnelEnd(tile);
			}
			signal_ctr += 2 + DistanceMax(orig_tile, tile) * 2;
			return true;
		}

		default: return false;
	}
}

/** Build many signals by dragging; AutoSignals
 * @param tile start tile of drag
 * @param flags operation to perform
 * @param p1  end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 2) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit  3)    - 1 = override signal/semaphore, or pre/exit/combo signal (CTRL-toggle)
 * - p2 = (bit  4)    - 0 = signals, 1 = semaphores
 * - p2 = (bit  5)    - 0 = build, 1 = remove signals
 * - p2 = (bit  6)    - 0 = selected stretch, 1 = auto fill
 * - p2 = (bit 24-31) - user defined signals_density
 */
static CommandCost CmdSignalTrackHelper(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost ret, total_cost;
	int signal_ctr;
	byte signals;
	bool error = true;
	TileIndex end_tile;
	TileIndex start_tile = tile;

	Track track = (Track)GB(p2, 0, 3);
	bool mode = HASBIT(p2, 3);
	bool semaphores = HASBIT(p2, 4);
	bool remove = HASBIT(p2, 5);
	bool autofill = HASBIT(p2, 6);
	Trackdir trackdir = TrackToTrackdir(track);
	byte signal_density = GB(p2, 24, 8);

	if (p1 >= MapSize()) return CMD_ERROR;
	end_tile = p1;
	if (signal_density == 0 || signal_density > 20) return CMD_ERROR;

	if (!IsTileType(tile, MP_RAILWAY)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* for vertical/horizontal tracks, double the given signals density
	 * since the original amount will be too dense (shorter tracks) */
	signal_density *= 2;

	if (CmdFailed(ValidateAutoDrag(&trackdir, tile, end_tile))) return CMD_ERROR;

	track = TrackdirToTrack(trackdir); /* trackdir might have changed, keep track in sync */
	Trackdir start_trackdir = trackdir;

	/* Autofill must start on a valid track to be able to avoid loops */
	if (autofill && !HasTrack(tile, track)) return CMD_ERROR;

	/* copy the signal-style of the first rail-piece if existing */
	if (HasSignals(tile)) {
		signals = GetPresentSignals(tile) & SignalOnTrack(track);
		if (signals == 0) signals = SignalOnTrack(track); /* Can this actually occur? */

		/* copy signal/semaphores style (independent of CTRL) */
		semaphores = GetSignalVariant(tile, track) != SIG_ELECTRIC;
	} else { // no signals exist, drag a two-way signal stretch
		signals = SignalOnTrack(track);
	}

	byte signal_dir = 0;
	if (signals & SignalAlongTrackdir(trackdir))   SETBIT(signal_dir, 0);
	if (signals & SignalAgainstTrackdir(trackdir)) SETBIT(signal_dir, 1);

	/* signal_ctr         - amount of tiles already processed
	 * signals_density    - patch setting to put signal on every Nth tile (double space on |, -- tracks)
	 **********
	 * trackdir   - trackdir to build with autorail
	 * semaphores - semaphores or signals
	 * signals    - is there a signal/semaphore on the first tile, copy its style (two-way/single-way)
	 *              and convert all others to semaphore/signal
	 * remove     - 1 remove signals, 0 build signals */
	signal_ctr = 0;
	for (;;) {
		/* only build/remove signals with the specified density */
		if ((remove && autofill) || signal_ctr % signal_density == 0) {
			uint32 p1 = GB(TrackdirToTrack(trackdir), 0, 3);
			SB(p1, 3, 1, mode);
			SB(p1, 4, 1, semaphores);

			/* Pick the correct orientation for the track direction */
			signals = 0;
			if (HASBIT(signal_dir, 0)) signals |= SignalAlongTrackdir(trackdir);
			if (HASBIT(signal_dir, 1)) signals |= SignalAgainstTrackdir(trackdir);

			ret = DoCommand(tile, p1, signals, flags, remove ? CMD_REMOVE_SIGNALS : CMD_BUILD_SIGNALS);

			/* Be user-friendly and try placing signals as much as possible */
			if (CmdSucceeded(ret)) {
				error = false;
				total_cost.AddCost(ret);
			}
		}

		if (autofill) {
			if (!CheckSignalAutoFill(tile, trackdir, signal_ctr, remove)) break;

			/* Prevent possible loops */
			if (tile == start_tile && trackdir == start_trackdir) break;
		} else {
			if (tile == end_tile) break;

			tile += ToTileIndexDiff(_trackdelta[trackdir]);
			signal_ctr++;

			/* toggle railbit for the non-diagonal tracks (|, -- tracks) */
			if (IsDiagonalTrackdir(trackdir)) {
				signal_ctr++;
			} else {
				ToggleBitT(trackdir, 0);
			}
		}
	}

	return error ? CMD_ERROR : total_cost;
}

/** Build signals on a stretch of track.
 * Stub for the unified signal builder/remover
 * @param tile start tile of drag
 * @param flags operation to perform
 * @param p1  end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 2) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit  3)    - 1 = override signal/semaphore, or pre/exit/combo signal (CTRL-toggle)
 * - p2 = (bit  4)    - 0 = signals, 1 = semaphores
 * - p2 = (bit  5)    - 0 = build, 1 = remove signals
 * - p2 = (bit  6)    - 0 = selected stretch, 1 = auto fill
 * - p2 = (bit 24-31) - user defined signals_density
 * @see CmdSignalTrackHelper
 */
CommandCost CmdBuildSignalTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdSignalTrackHelper(tile, flags, p1, p2);
}

/** Remove signals
 * @param tile coordinates where signal is being deleted from
 * @param flags operation to perform
 * @param p1 various bitstuffed elements, only track information is used
 *           - (bit  0- 2) - track-orientation, valid values: 0-5 (Track enum)
 *           - (bit  3)    - override signal/semaphore, or pre/exit/combo signal (CTRL-toggle)
 *           - (bit  4)    - 0 = signals, 1 = semaphores
 * @param p2 unused
 */
CommandCost CmdRemoveSingleSignal(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Track track = (Track)GB(p1, 0, 3);

	if (!ValParamTrackOrientation(track) ||
			!IsTileType(tile, MP_RAILWAY) ||
			!EnsureNoTrainOnTrack(tile, track) ||
			!HasSignalOnTrack(tile, track)) {
		return CMD_ERROR;
	}

	/* Only water can remove signals from anyone */
	if (_current_player != OWNER_WATER && !CheckTileOwnership(tile)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Do it? */
	if (flags & DC_EXEC) {
		SetPresentSignals(tile, GetPresentSignals(tile) & ~SignalOnTrack(track));

		/* removed last signal from tile? */
		if (GetPresentSignals(tile) == 0) {
			SetSignalStates(tile, 0);
			SetHasSignals(tile, false);
			SetSignalVariant(tile, INVALID_TRACK, SIG_ELECTRIC); // remove any possible semaphores
		}

		SetSignalsOnBothDir(tile, track);
		YapfNotifyTrackLayoutChange(tile, track);

		MarkTileDirtyByTile(tile);
	}

	return CommandCost(_price.remove_signals);
}

/** Remove signals on a stretch of track.
 * Stub for the unified signal builder/remover
 * @param tile start tile of drag
 * @param flags operation to perform
 * @param p1  end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 2) - track-orientation, valid values: 0-5 (Track enum)
 * - p2 = (bit  3)    - 1 = override signal/semaphore, or pre/exit/combo signal (CTRL-toggle)
 * - p2 = (bit  4)    - 0 = signals, 1 = semaphores
 * - p2 = (bit  5)    - 0 = build, 1 = remove signals
 * - p2 = (bit  6)    - 0 = selected stretch, 1 = auto fill
 * - p2 = (bit 24-31) - user defined signals_density
 * @see CmdSignalTrackHelper
 */
CommandCost CmdRemoveSignalTrack(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return CmdSignalTrackHelper(tile, flags, p1, SETBIT(p2, 5)); // bit 5 is remove bit
}

typedef CommandCost DoConvertRailProc(TileIndex tile, RailType totype, bool exec);

void *UpdateTrainPowerProc(Vehicle *v, void *data)
{
	/* Similiar checks as in TrainPowerChanged() */

	if (v->type == VEH_TRAIN && v->tile == *(TileIndex*)data && !IsArticulatedPart(v)) {
		const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
		if (GetVehicleProperty(v, 0x0B, rvi->power) != 0) TrainPowerChanged(v->First());
	}

	return NULL;
}

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
static CommandCost DoConvertRail(TileIndex tile, RailType totype, bool exec)
{
	/* change type. */
	if (exec) {
		SetRailType(tile, totype);
		MarkTileDirtyByTile(tile);

		/* notify YAPF about the track layout change */
		TrackBits tracks = GetTrackBits(tile);
		while (tracks != TRACK_BIT_NONE) {
			YapfNotifyTrackLayoutChange(tile, RemoveFirstTrack(&tracks));
		}

		if (IsTileDepotType(tile, TRANSPORT_RAIL)) {
			/* Update build vehicle window related to this depot */
			InvalidateWindowData(WC_VEHICLE_DEPOT, tile);
			InvalidateWindowData(WC_BUILD_VEHICLE, tile);
		}

		/* update power of train engines on this tile */
		VehicleFromPos(tile, &tile, UpdateTrainPowerProc);
	}

	return CommandCost(RailBuildCost(totype) / 2);
}

extern CommandCost DoConvertStationRail(TileIndex tile, RailType totype, bool exec);
extern CommandCost DoConvertStreetRail(TileIndex tile, RailType totype, bool exec);
extern CommandCost DoConvertTunnelBridgeRail(TileIndex tile, RailType totype, bool exec);

/** Convert one rail type to the other. You can convert normal rail to
 * monorail/maglev easily or vice-versa.
 * @param tile end tile of rail conversion drag
 * @param flags operation to perform
 * @param p1 start tile of drag
 * @param p2 new railtype to convert to
 */
CommandCost CmdConvertRail(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost ret, cost;
	Money money;
	int ex;
	int ey;
	int sx, sy, x, y;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!ValParamRailtype(p2)) return CMD_ERROR;
	if (p1 >= MapSize()) return CMD_ERROR;

	/* make sure sx,sy are smaller than ex,ey */
	ex = TileX(tile);
	ey = TileY(tile);
	sx = TileX(p1);
	sy = TileY(p1);
	if (ex < sx) Swap(ex, sx);
	if (ey < sy) Swap(ey, sy);

	money = GetAvailableMoneyForCommand();

	for (x = sx; x <= ex; ++x) {
		for (y = sy; y <= ey; ++y) {
			TileIndex tile = TileXY(x, y);
			DoConvertRailProc *proc;
			RailType totype = (RailType)p2;

			switch (GetTileType(tile)) {
				case MP_RAILWAY:      proc = DoConvertRail;             break;
				case MP_STATION:      proc = DoConvertStationRail;      break;
				case MP_ROAD:         proc = DoConvertStreetRail;       break;
				case MP_TUNNELBRIDGE: proc = DoConvertTunnelBridgeRail; break;
				default: continue;
			}

			/* It is possible that 'type' is invalid when there is no rail on the tile,
			 * but this situation will be detected in proc()
			 */
			RailType type = GetRailType(tile);

			/* Not own tile or track is already converted */
			if ((!CheckTileOwnership(tile) || type == totype) ||
				/* 'hidden' elrails can't be downgraded to normal rail when elrails are disabled */
				(_patches.disable_elrails && totype == RAILTYPE_RAIL && type == RAILTYPE_ELECTRIC) ||
				/* Vehicle on a tile while not converting Rail <-> ElRail */
				(!IsCompatibleRail(type, totype) && !EnsureNoVehicleOnGround(tile))) {
					ret = CMD_ERROR;
					continue;
			}

			ret = proc(tile, totype, false);
			if (CmdFailed(ret)) continue;

			if (flags & DC_EXEC) {
				money -= ret.GetCost();
				if (money < 0) {
					_additional_cash_required = ret.GetCost();
					return cost;
				}
				proc(tile, totype, true);
			}
			cost.AddCost(ret);
		}
	}

	return (cost.GetCost() == 0) ? ret : cost;
}

static CommandCost RemoveTrainDepot(TileIndex tile, uint32 flags)
{
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicleOnGround(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DiagDirection dir = GetRailDepotDirection(tile);

		DoClearSquare(tile);
		delete GetDepotByTile(tile);
		UpdateSignalsOnSegment(tile, dir);
		YapfNotifyTrackLayoutChange(tile, TrackdirToTrack(DiagdirToDiagTrackdir(dir)));
	}

	return CommandCost(_price.remove_train_depot);
}

static CommandCost ClearTile_Track(TileIndex tile, byte flags)
{
	CommandCost cost;
	CommandCost ret;

	if (flags & DC_AUTO) {
		if (!IsTileOwner(tile, _current_player))
			return_cmd_error(STR_1024_AREA_IS_OWNED_BY_ANOTHER);

		if (IsPlainRailTile(tile)) {
			return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);
		} else {
			return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
		}
	}

	switch (GetRailTileType(tile)) {
		case RAIL_TILE_SIGNALS:
		case RAIL_TILE_NORMAL: {
			bool water_ground = (GetRailGroundType(tile) == RAIL_GROUND_WATER);

			TrackBits tracks = GetTrackBits(tile);
			while (tracks != TRACK_BIT_NONE) {
				Track track = RemoveFirstTrack(&tracks);
				ret = DoCommand(tile, 0, track, flags, CMD_REMOVE_SINGLE_RAIL);
				if (CmdFailed(ret)) return CMD_ERROR;
				cost.AddCost(ret);
			}

			if (water_ground) {
				/* The track was removed, and left a coast tile. Now also clear the water. */
				if (flags & DC_EXEC) DoClearSquare(tile);
				cost.AddCost(_price.clear_water);
			}

			return cost;
		}

		case RAIL_TILE_DEPOT:
			return RemoveTrainDepot(tile, flags);

		case RAIL_TILE_WAYPOINT:
			return RemoveTrainWaypoint(tile, flags, false);

		default:
			return CMD_ERROR;
	}
}

#include "table/track_land.h"

/**
 * Get surface height in point (x,y)
 * On tiles with halftile foundations move (x,y) to a save point wrt. track
 */
static uint GetSaveSlopeZ(uint x, uint y, Track track)
{
	switch (track) {
		case TRACK_UPPER: x &= ~0xF; y &= ~0xF; break;
		case TRACK_LOWER: x |=  0xF; y |=  0xF; break;
		case TRACK_LEFT:  x |=  0xF; y &= ~0xF; break;
		case TRACK_RIGHT: x &= ~0xF; y |=  0xF; break;
		default: break;
	}
	return GetSlopeZ(x, y);
}

static void DrawSingleSignal(TileIndex tile, Track track, byte condition, uint image, uint pos)
{
	bool side = (_opt.road_side != 0) && _patches.signal_side;
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
	if (_signal_base == 0 || (GetSignalType(tile, track) == SIGTYPE_NORMAL && GetSignalVariant(tile, track) == SIG_ELECTRIC)) {
		sprite = SignalBase[side][GetSignalVariant(tile, track)][GetSignalType(tile, track)] + image + condition;
	} else {
		sprite = _signal_base + (GetSignalType(tile, track) - 1) * 16 + GetSignalVariant(tile, track) * 64 + image + condition;
	}

	AddSortableSpriteToDraw(sprite, PAL_NONE, x, y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, GetSaveSlopeZ(x, y, track));
}

static uint32 _drawtile_track_palette;


static void DrawTrackFence_NW(const TileInfo *ti)
{
	SpriteID image = SPR_TRACK_FENCE_FLAT_X;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? SPR_TRACK_FENCE_SLOPE_SW : SPR_TRACK_FENCE_SLOPE_NE;
	AddSortableSpriteToDraw(image, _drawtile_track_palette,
		ti->x, ti->y + 1, 16, 1, 4, ti->z);
}

static void DrawTrackFence_SE(const TileInfo *ti)
{
	SpriteID image = SPR_TRACK_FENCE_FLAT_X;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? SPR_TRACK_FENCE_SLOPE_SW : SPR_TRACK_FENCE_SLOPE_NE;
	AddSortableSpriteToDraw(image, _drawtile_track_palette,
		ti->x, ti->y + TILE_SIZE - 1, 16, 1, 4, ti->z);
}

static void DrawTrackFence_NW_SE(const TileInfo *ti)
{
	DrawTrackFence_NW(ti);
	DrawTrackFence_SE(ti);
}

static void DrawTrackFence_NE(const TileInfo *ti)
{
	SpriteID image = SPR_TRACK_FENCE_FLAT_Y;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? SPR_TRACK_FENCE_SLOPE_SE : SPR_TRACK_FENCE_SLOPE_NW;
	AddSortableSpriteToDraw(image, _drawtile_track_palette,
		ti->x + 1, ti->y, 1, 16, 4, ti->z);
}

static void DrawTrackFence_SW(const TileInfo *ti)
{
	SpriteID image = SPR_TRACK_FENCE_FLAT_Y;
	if (ti->tileh != SLOPE_FLAT) image = (ti->tileh & SLOPE_S) ? SPR_TRACK_FENCE_SLOPE_SE : SPR_TRACK_FENCE_SLOPE_NW;
	AddSortableSpriteToDraw(image, _drawtile_track_palette,
		ti->x + TILE_SIZE - 1, ti->y, 1, 16, 4, ti->z);
}

static void DrawTrackFence_NE_SW(const TileInfo *ti)
{
	DrawTrackFence_NE(ti);
	DrawTrackFence_SW(ti);
}

/**
 * Draw fence at eastern side of track.
 */
static void DrawTrackFence_NS_1(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_W) z += TILE_HEIGHT;
	if (IsSteepSlope(ti->tileh)) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(SPR_TRACK_FENCE_FLAT_VERT, _drawtile_track_palette,
		ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, 4, z);
}

/**
 * Draw fence at western side of track.
 */
static void DrawTrackFence_NS_2(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_E) z += TILE_HEIGHT;
	if (IsSteepSlope(ti->tileh)) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(SPR_TRACK_FENCE_FLAT_VERT, _drawtile_track_palette,
		ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, 4, z);
}

/**
 * Draw fence at southern side of track.
 */
static void DrawTrackFence_WE_1(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_N) z += TILE_HEIGHT;
	if (IsSteepSlope(ti->tileh)) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(SPR_TRACK_FENCE_FLAT_HORZ, _drawtile_track_palette,
		ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, 4, z);
}

/**
 * Draw fence at northern side of track.
 */
static void DrawTrackFence_WE_2(const TileInfo *ti)
{
	int z = ti->z;
	if (ti->tileh & SLOPE_S) z += TILE_HEIGHT;
	if (IsSteepSlope(ti->tileh)) z += TILE_HEIGHT;
	AddSortableSpriteToDraw(SPR_TRACK_FENCE_FLAT_HORZ, _drawtile_track_palette,
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
		case RAIL_GROUND_WATER:
			switch (GetHalftileSlopeCorner(ti->tileh)) {
				case CORNER_W: DrawTrackFence_NS_1(ti); break;
				case CORNER_S: DrawTrackFence_WE_2(ti); break;
				case CORNER_E: DrawTrackFence_NS_2(ti); break;
				case CORNER_N: DrawTrackFence_WE_1(ti); break;
				default: NOT_REACHED();
			}
			break;
		default: break;
	}
}


/**
 * Draw ground sprite and track bits
 * @param ti TileInfo
 * @param track TrackBits to draw
 */
static void DrawTrackBits(TileInfo* ti, TrackBits track)
{
	const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
	RailGroundType rgt = GetRailGroundType(ti->tile);
	Foundation f = GetRailFoundation(ti->tileh, track);
	Corner halftile_corner = CORNER_INVALID;

	if (IsNonContinuousFoundation(f)) {
		/* Save halftile corner */
		halftile_corner = (f == FOUNDATION_STEEP_BOTH ? GetHighestSlopeCorner(ti->tileh) : GetHalftileFoundationCorner(f));
		/* Draw lower part first */
		track &= ~CornerToTrackBits(halftile_corner);
		f = (f == FOUNDATION_STEEP_BOTH ? FOUNDATION_STEEP_LOWER : FOUNDATION_NONE);
	}

	DrawFoundation(ti, f);
	/* DrawFoundation modifies ti */

	SpriteID image;
	SpriteID pal = PAL_NONE;
	bool junction = false;

	/* Select the sprite to use. */
	if (track == 0) {
		/* Clear ground (only track on halftile foundation) */
		if (rgt == RAIL_GROUND_WATER) {
			image = SPR_FLAT_WATER_TILE;
		} else {
			switch (rgt) {
				case RAIL_GROUND_BARREN:     image = SPR_FLAT_BARE_LAND;  break;
				case RAIL_GROUND_ICE_DESERT: image = SPR_FLAT_SNOWY_TILE; break;
				default:                     image = SPR_FLAT_GRASS_TILE; break;
			}
			image += _tileh_to_sprite[ti->tileh];
		}
	} else {
		if (ti->tileh != SLOPE_FLAT) {
			/* track on non-flat ground */
			image = _track_sloped_sprites[ti->tileh - 1] + rti->base_sprites.track_y;
		} else {
			/* track on flat ground */
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
		}

		switch (rgt) {
			case RAIL_GROUND_BARREN:     pal = PALETTE_TO_BARE_LAND; break;
			case RAIL_GROUND_ICE_DESERT: image += rti->snow_offset;  break;
			case RAIL_GROUND_WATER:      NOT_REACHED();
			default: break;
		}
	}

	DrawGroundSprite(image, pal);

	/* Draw track pieces individually for junction tiles */
	if (junction) {
		if (track & TRACK_BIT_X)     DrawGroundSprite(rti->base_sprites.single_y, PAL_NONE);
		if (track & TRACK_BIT_Y)     DrawGroundSprite(rti->base_sprites.single_x, PAL_NONE);
		if (track & TRACK_BIT_UPPER) DrawGroundSprite(rti->base_sprites.single_n, PAL_NONE);
		if (track & TRACK_BIT_LOWER) DrawGroundSprite(rti->base_sprites.single_s, PAL_NONE);
		if (track & TRACK_BIT_LEFT)  DrawGroundSprite(rti->base_sprites.single_w, PAL_NONE);
		if (track & TRACK_BIT_RIGHT) DrawGroundSprite(rti->base_sprites.single_e, PAL_NONE);
	}

	if (IsValidCorner(halftile_corner)) {
		DrawFoundation(ti, HalftileFoundation(halftile_corner));

		/* Draw higher halftile-overlay: Use the sloped sprites with three corners raised. They probably best fit the lightning. */
		Slope fake_slope = SlopeWithThreeCornersRaised(OppositeCorner(halftile_corner));
		image = _track_sloped_sprites[fake_slope - 1] + rti->base_sprites.track_y;
		pal = PAL_NONE;
		switch (rgt) {
			case RAIL_GROUND_BARREN:     pal = PALETTE_TO_BARE_LAND; break;
			case RAIL_GROUND_ICE_DESERT: image += rti->snow_offset;  break;
			default: break;
		}

		static const int INF = 1000; // big number compared to tilesprite size
		static const SubSprite _halftile_sub_sprite[4] = {
			{ -INF    , -INF  , 32 - 33, INF     }, // CORNER_W, clip 33 pixels from right
			{ -INF    ,  0 + 7, INF    , INF     }, // CORNER_S, clip 7 pixels from top
			{ -31 + 33, -INF  , INF    , INF     }, // CORNER_E, clip 33 pixels from left
			{ -INF    , -INF  , INF    , 30 - 23 }  // CORNER_N, clip 23 pixels from bottom
		};

		DrawGroundSprite(image, pal, &(_halftile_sub_sprite[halftile_corner]));
	}
}

static void DrawSignals(TileIndex tile, TrackBits rails)
{
#define MAYBE_DRAW_SIGNAL(x,y,z,t) if (IsSignalPresent(tile, x)) DrawSingleSignal(tile, t, GetSingleSignalState(tile, x), y - 0x4FB, z)

	if (!(rails & TRACK_BIT_Y)) {
		if (!(rails & TRACK_BIT_X)) {
			if (rails & TRACK_BIT_LEFT) {
				MAYBE_DRAW_SIGNAL(2, 0x509, 0, TRACK_LEFT);
				MAYBE_DRAW_SIGNAL(3, 0x507, 1, TRACK_LEFT);
			}
			if (rails & TRACK_BIT_RIGHT) {
				MAYBE_DRAW_SIGNAL(0, 0x509, 2, TRACK_RIGHT);
				MAYBE_DRAW_SIGNAL(1, 0x507, 3, TRACK_RIGHT);
			}
			if (rails & TRACK_BIT_UPPER) {
				MAYBE_DRAW_SIGNAL(3, 0x505, 4, TRACK_UPPER);
				MAYBE_DRAW_SIGNAL(2, 0x503, 5, TRACK_UPPER);
			}
			if (rails & TRACK_BIT_LOWER) {
				MAYBE_DRAW_SIGNAL(1, 0x505, 6, TRACK_LOWER);
				MAYBE_DRAW_SIGNAL(0, 0x503, 7, TRACK_LOWER);
			}
		} else {
			MAYBE_DRAW_SIGNAL(3, 0x4FB, 8, TRACK_X);
			MAYBE_DRAW_SIGNAL(2, 0x4FD, 9, TRACK_X);
		}
	} else {
		MAYBE_DRAW_SIGNAL(3, 0x4FF, 10, TRACK_Y);
		MAYBE_DRAW_SIGNAL(2, 0x501, 11, TRACK_Y);
	}
}

static void DrawTile_Track(TileInfo *ti)
{
	const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));
	SpriteID image;

	_drawtile_track_palette = PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile));

	if (IsPlainRailTile(ti->tile)) {
		TrackBits rails = GetTrackBits(ti->tile);

		DrawTrackBits(ti, rails);

		if (HASBIT(_display_opt, DO_FULL_DETAIL)) DrawTrackDetails(ti);

		if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);

		if (HasSignals(ti->tile)) DrawSignals(ti->tile, rails);
	} else {
		/* draw depot/waypoint */
		const DrawTileSprites* dts;
		const DrawTileSeqStruct* dtss;
		uint32 relocation;

		if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

		if (IsRailDepot(ti->tile)) {
			dts = &_depot_gfx_table[GetRailDepotDirection(ti->tile)];

			relocation = rti->total_offset;

			image = dts->ground_sprite;
			if (image != SPR_FLAT_GRASS_TILE) image += rti->total_offset;

			/* adjust ground tile for desert
			 * don't adjust for snow, because snow in depots looks weird */
			if (IsSnowRailGround(ti->tile) && _opt.landscape == LT_TROPIC) {
				if (image != SPR_FLAT_GRASS_TILE) {
					image += rti->snow_offset; // tile with tracks
				} else {
					image = SPR_FLAT_SNOWY_TILE; // flat ground
				}
			}
		} else {
			/* look for customization */
			byte stat_id = GetWaypointByTile(ti->tile)->stat_id;
			const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, stat_id);

			if (statspec != NULL) {
				/* emulate station tile - open with building */
				const Station* st = ComposeWaypointStation(ti->tile);
				uint gfx = 2;

				if (HASBIT(statspec->callbackmask, CBM_STATION_SPRITE_LAYOUT)) {
					uint16 callback = GetStationCallback(CBID_STATION_SPRITE_LAYOUT, 0, 0, statspec, st, ti->tile);
					if (callback != CALLBACK_FAILED) gfx = callback;
				}

				if (statspec->renderdata == NULL) {
					dts = GetStationTileLayout(STATION_RAIL, gfx);
				} else {
					dts = &statspec->renderdata[(gfx < statspec->tiles ? gfx : 0) + GetWaypointAxis(ti->tile)];
				}

				if (dts != NULL && dts->seq != NULL) {
					relocation = GetCustomStationRelocation(statspec, st, ti->tile);

					image = dts->ground_sprite;
					if (HASBIT(image, SPRITE_MODIFIER_USE_OFFSET)) {
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
				/* There is no custom layout, fall back to the default graphics */
				dts = &_waypoint_gfx_table[GetWaypointAxis(ti->tile)];
				relocation = 0;
				image = dts->ground_sprite + rti->total_offset;
				if (IsSnowRailGround(ti->tile)) image += rti->snow_offset;
			}
		}

		DrawGroundSprite(image, PAL_NONE);

		if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);

		foreach_draw_tile_seq(dtss, dts->seq) {
			SpriteID image = dtss->image;
			SpriteID pal;

			/* Unlike stations, our default waypoint has no variation for
			 * different railtype, so don't use the railtype offset if
			 * no relocation is set */
			if (HASBIT(image, SPRITE_MODIFIER_USE_OFFSET)) {
				image += rti->total_offset;
			} else {
				image += relocation;
			}

			if (!HASBIT(_transparent_opt, TO_BUILDINGS) && HASBIT(image, PALETTE_MODIFIER_COLOR)) {
				pal = _drawtile_track_palette;
			} else {
				pal = dtss->pal;
			}

			if ((byte)dtss->delta_z != 0x80) {
				AddSortableSpriteToDraw(
					image, pal,
					ti->x + dtss->delta_x, ti->y + dtss->delta_y,
					dtss->size_x, dtss->size_y,
					dtss->size_z, ti->z + dtss->delta_z,
					HASBIT(_transparent_opt, TO_BUILDINGS)
				);
			} else {
				AddChildSpriteScreen(image, pal, dtss->delta_x, dtss->delta_y);
			}
		}
	}
	DrawBridgeMiddle(ti);
}


static void DrawTileSequence(int x, int y, SpriteID ground, const DrawTileSeqStruct* dtss, uint32 offset)
{
	SpriteID palette = PLAYER_SPRITE_COLOR(_local_player);

	DrawSprite(ground, PAL_NONE, x, y);
	for (; dtss->image != 0; dtss++) {
		Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
		SpriteID image = dtss->image + offset;

		DrawSprite(image, HASBIT(image, PALETTE_MODIFIER_COLOR) ? palette : PAL_NONE, x + pt.x, y + pt.y);
	}
}

void DrawTrainDepotSprite(int x, int y, int dir, RailType railtype)
{
	const DrawTileSprites* dts = &_depot_gfx_table[dir];
	SpriteID image = dts->ground_sprite;
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

struct SetSignalsData {
	int cur;
	int cur_stack;
	bool stop;
	bool has_presignal;

	/* presignal info */
	int presignal_exits;
	int presignal_exits_free;

	/* these are used to keep track of the signals that change. */
	TrackdirByte bit[NUM_SSD_ENTRY];
	TileIndex tile[NUM_SSD_ENTRY];

	/* these are used to keep track of the stack that modifies presignals recursively */
	TileIndex next_tile[NUM_SSD_STACK];
	DiagDirectionByte next_dir[NUM_SSD_STACK];

};

static bool SetSignalsEnumProc(TileIndex tile, void* data, Trackdir trackdir, uint length, byte* state)
{
	SetSignalsData* ssd = (SetSignalsData*)data;
	Track track = TrackdirToTrack(trackdir);

	if (!IsTileType(tile, MP_RAILWAY)) return false;

	/* the tile has signals? */
	if (HasSignalOnTrack(tile, track)) {
		if (HasSignalOnTrackdir(tile, ReverseTrackdir(trackdir))) {
			/* yes, add the signal to the list of signals */
			if (ssd->cur != NUM_SSD_ENTRY) {
				ssd->tile[ssd->cur] = tile; // remember the tile index
				ssd->bit[ssd->cur] = trackdir; // and the controlling bit number
				ssd->cur++;
			}

			/* remember if this block has a presignal. */
			ssd->has_presignal |= IsPresignalEntry(tile, track);
		}

		if (HasSignalOnTrackdir(tile, trackdir) && IsPresignalExit(tile, track)) {
			/* this is an exit signal that points out from the segment */
			ssd->presignal_exits++;
			if (GetSignalStateByTrackdir(tile, trackdir) != SIGNAL_STATE_RED)
				ssd->presignal_exits_free++;
		}

		return true;
	} else if (IsTileDepotType(tile, TRANSPORT_RAIL)) {
		return true; // don't look further if the tile is a depot
	}

	return false;
}

/* Struct to parse data from VehicleFromPos to SignalVehicleCheckProc */
struct SignalVehicleCheckStruct {
	TileIndex tile;
	uint track;
};

static void *SignalVehicleCheckProc(Vehicle *v, void *data)
{
	const SignalVehicleCheckStruct* dest = (SignalVehicleCheckStruct*)data;

	if (v->type != VEH_TRAIN) return NULL;

	/* Wrong tile, or no train? Not a match */
	if (v->tile != dest->tile) return NULL;

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

	/* Locate vehicles in tunnels or on bridges */
	if (IsTunnelTile(tile) || IsBridgeTile(tile)) {
		TileIndex end;
		DiagDirection direction;

		if (IsTunnelTile(tile)) {
			end = GetOtherTunnelEnd(tile);
			direction = GetTunnelDirection(tile);
		} else {
			end = GetOtherBridgeEnd(tile);
			direction = GetBridgeRampDirection(tile);
		}

		dest.track = 1 << (direction & 1); // get the trackbit the vehicle would have if it has not entered the tunnel yet (ie is still visible)

		/* check for a vehicle with that trackdir on the start tile of the tunnel */
		if (VehicleFromPos(tile, &dest, SignalVehicleCheckProc) != NULL) return true;

		/* check for a vehicle with that trackdir on the end tile of the tunnel */
		if (VehicleFromPos(end, &dest, SignalVehicleCheckProc) != NULL) return true;

		/* now check all tiles from start to end for a warping vehicle */
		dest.track = 0x40;   //Vehicle inside a tunnel or on a bridge
		if (VehicleFromPos(tile, &dest, SignalVehicleCheckProc) != NULL) return true;
		if (VehicleFromPos(end, &dest, SignalVehicleCheckProc) != NULL) return true;

		/* no vehicle found */
		return false;
	}

	return VehicleFromPos(tile, &dest, SignalVehicleCheckProc) != NULL;
}

static void SetSignalsAfterProc(TrackPathFinder *tpf)
{
	SetSignalsData *ssd = (SetSignalsData*)tpf->userdata;
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

static void ChangeSignalStates(SetSignalsData *ssd)
{
	int i;

	/* thinking about presignals...
	 * the presignal is green if,
	 *   if no train is in the segment AND
	 *   there is at least one green exit signal OR
	 *   there are no exit signals in the segment */

	/* then mark the signals in the segment accordingly */
	for (i = 0; i != ssd->cur; i++) {
		TileIndex tile = ssd->tile[i];
		byte bit = SignalAgainstTrackdir(ssd->bit[i]);
		uint signals = GetSignalStates(tile);
		Track track = TrackdirToTrack(ssd->bit[i]);

		/* presignals don't turn green if there is at least one presignal exit and none are free */
		if (IsPresignalEntry(tile, track)) {
			int ex = ssd->presignal_exits, exfree = ssd->presignal_exits_free;

			/* subtract for dual combo signals so they don't count themselves */
			if (IsPresignalExit(tile, track) && HasSignalOnTrackdir(tile, ssd->bit[i])) {
				ex--;
				if (GetSignalStateByTrackdir(tile, ssd->bit[i]) != SIGNAL_STATE_RED) exfree--;
			}

			/* if we have exits and none are free, make red. */
			if (ex && !exfree) goto make_red;
		}

		/* check if the signal is unaffected. */
		if (ssd->stop) {
make_red:
			/* turn red */
			if ((bit & signals) == 0) continue;
		} else {
			/* turn green */
			if ((bit & signals) != 0) continue;
		}

		/* Update signals on the other side of this exit-combo signal; it changed. */
		if (IsPresignalExit(tile, track)) {
			if (ssd->cur_stack != NUM_SSD_STACK) {
				ssd->next_tile[ssd->cur_stack] = tile;
				ssd->next_dir[ssd->cur_stack] = TrackdirToExitdir(ssd->bit[i]);
				ssd->cur_stack++;
			} else {
				DEBUG(misc, 0, "NUM_SSD_STACK too small"); /// @todo WTF is this???
			}
		}

		/* it changed, so toggle it */
		SetSignalStates(tile, signals ^ bit);
		MarkTileDirtyByTile(tile);
	}
}


bool UpdateSignalsOnSegment(TileIndex tile, DiagDirection direction)
{
	SetSignalsData ssd;
	int result = -1;

	ssd.cur_stack = 0;

	for (;;) {
		/* go through one segment and update all signals pointing into that segment. */
		ssd.cur = ssd.presignal_exits = ssd.presignal_exits_free = 0;
		ssd.has_presignal = false;

		FollowTrack(tile, 0xC000 | TRANSPORT_RAIL, 0, direction, SetSignalsEnumProc, SetSignalsAfterProc, &ssd);
		ChangeSignalStates(&ssd);

		/* remember the result only for the first iteration. */
		if (result < 0) {
			/* stay in depot while segment is occupied or while all presignal exits are blocked */
			result = ssd.stop || (ssd.presignal_exits > 0 && ssd.presignal_exits_free == 0);
		}

		/* if any exit signals were changed, we need to keep going to modify the stuff behind those. */
		if (ssd.cur_stack == 0) break;

		/* one or more exit signals were changed, so we need to update another segment too. */
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
		z += ApplyFoundationToSlope(GetRailFoundation(tileh, GetTrackBits(tile)), &tileh);
		return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
	} else {
		return z + TILE_HEIGHT;
	}
}

static Foundation GetFoundation_Track(TileIndex tile, Slope tileh)
{
	return IsPlainRailTile(tile) ? GetRailFoundation(tileh, GetTrackBits(tile)) : FlatteningFoundation(tileh);
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

	if (old_ground == RAIL_GROUND_WATER) {
		TileLoop_Water(tile);
		return;
	}

	switch (_opt.landscape) {
		case LT_ARCTIC:
			if (GetTileZ(tile) > GetSnowLine()) {
				new_ground = RAIL_GROUND_ICE_DESERT;
				goto set_ground;
			}
			break;

		case LT_TROPIC:
			if (GetTropicZone(tile) == TROPICZONE_DESERT) {
				new_ground = RAIL_GROUND_ICE_DESERT;
				goto set_ground;
			}
			break;
	}

	if (!IsPlainRailTile(tile)) return;

	new_ground = RAIL_GROUND_GRASS;

	if (old_ground != RAIL_GROUND_BARREN) { // wait until bottom is green
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


static uint32 GetTileTrackStatus_Track(TileIndex tile, TransportType mode, uint sub_mode)
{
	if (mode != TRANSPORT_RAIL) return 0;

	switch (GetRailTileType(tile)) {
		default: NOT_REACHED();
		case RAIL_TILE_NORMAL: {
			TrackBits rails = GetTrackBits(tile);
			uint32 ret = rails * 0x101;
			return (rails == TRACK_BIT_CROSS) ? ret | 0x40 : ret;
		}

		case RAIL_TILE_SIGNALS: {
			uint32 ret = GetTrackBits(tile) * 0x101;
			byte a = GetPresentSignals(tile);
			uint b = GetSignalStates(tile);

			b &= a;

			/* When signals are not present (in neither
			 * direction), we pretend them to be green. (So if
			 * signals are only one way, the other way will
			 * implicitely become `red' */
			if ((a & 0xC) == 0) b |= 0xC;
			if ((a & 0x3) == 0) b |= 0x3;

			if ((b & 0x8) == 0) ret |= 0x10070000;
			if ((b & 0x4) == 0) ret |= 0x07100000;
			if ((b & 0x2) == 0) ret |= 0x20080000;
			if ((b & 0x1) == 0) ret |= 0x08200000;

			return ret;
		}

		case RAIL_TILE_DEPOT:    return AxisToTrackBits(DiagDirToAxis(GetRailDepotDirection(tile))) * 0x101;
		case RAIL_TILE_WAYPOINT: return GetRailWaypointBits(tile) * 0x101;
	}
}

static void ClickTile_Track(TileIndex tile)
{
	switch (GetRailTileType(tile)) {
		case RAIL_TILE_DEPOT:    ShowDepotWindow(tile, VEH_TRAIN);                  break;
		case RAIL_TILE_WAYPOINT: ShowRenameWaypointWindow(GetWaypointByTile(tile)); break;
		default: break;
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
			const StringID signal_type[4][4] = {
				{
					STR_RAILROAD_TRACK_WITH_NORMAL_SIGNALS,
					STR_RAILROAD_TRACK_WITH_NORMAL_PRESIGNALS,
					STR_RAILROAD_TRACK_WITH_NORMAL_EXITSIGNALS,
					STR_RAILROAD_TRACK_WITH_NORMAL_COMBOSIGNALS
				},
				{
					STR_RAILROAD_TRACK_WITH_NORMAL_PRESIGNALS,
					STR_RAILROAD_TRACK_WITH_PRESIGNALS,
					STR_RAILROAD_TRACK_WITH_PRE_EXITSIGNALS,
					STR_RAILROAD_TRACK_WITH_PRE_COMBOSIGNALS
				},
				{
					STR_RAILROAD_TRACK_WITH_NORMAL_EXITSIGNALS,
					STR_RAILROAD_TRACK_WITH_PRE_EXITSIGNALS,
					STR_RAILROAD_TRACK_WITH_EXITSIGNALS,
					STR_RAILROAD_TRACK_WITH_EXIT_COMBOSIGNALS
				},
				{
					STR_RAILROAD_TRACK_WITH_NORMAL_COMBOSIGNALS,
					STR_RAILROAD_TRACK_WITH_PRE_COMBOSIGNALS,
					STR_RAILROAD_TRACK_WITH_EXIT_COMBOSIGNALS,
					STR_RAILROAD_TRACK_WITH_COMBOSIGNALS
				}
			};

			td->str = signal_type[GetSignalType(tile, TRACK_UPPER)][GetSignalType(tile, TRACK_LOWER)];
			break;
		}

		case RAIL_TILE_DEPOT:
			td->str = STR_1023_RAILROAD_TRAIN_DEPOT;
			break;

		case RAIL_TILE_WAYPOINT:
		default:
			td->str = STR_LANDINFO_WAYPOINT;
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
static const signed char _deltacoord_leaveoffset[8] = {
	-1,  0,  1,  0, /* x */
	 0,  1,  0, -1  /* y */
};

static uint32 VehicleEnter_Track(Vehicle *v, TileIndex tile, int x, int y)
{
	byte fract_coord;
	byte fract_coord_leave;
	DiagDirection dir;
	int length;

	/* this routine applies only to trains in depot tiles */
	if (v->type != VEH_TRAIN || !IsTileDepotType(tile, TRANSPORT_RAIL)) return VETSB_CONTINUE;

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
		return VETSB_CANNOT_ENTER;
	} else if (_fractcoords_enter[dir] == fract_coord) {
		if (DiagDirToDir(ReverseDiagDir(dir)) == v->direction) {
			/* enter the depot */
			v->u.rail.track = TRACK_BIT_DEPOT,
			v->vehstatus |= VS_HIDDEN; /* hide it */
			v->direction = ReverseDir(v->direction);
			if (v->Next() == NULL) VehicleEnterDepot(v);
			v->tile = tile;

			InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
			return VETSB_ENTERED_WORMHOLE;
		}
	} else if (fract_coord_leave == fract_coord) {
		if (DiagDirToDir(dir) == v->direction) {
			/* leave the depot? */
			if ((v = v->Next()) != NULL) {
				v->vehstatus &= ~VS_HIDDEN;
				v->u.rail.track = (DiagDirToAxis(dir) == AXIS_X ? TRACK_BIT_X : TRACK_BIT_Y);
			}
		}
	}

	return VETSB_CONTINUE;
}

/**
 * Tests if autoslope is allowed.
 *
 * @param tile The tile.
 * @param flags Terraform command flags.
 * @param z_old Old TileZ.
 * @param tileh_old Old TileSlope.
 * @param z_new New TileZ.
 * @param tileh_new New TileSlope.
 * @param rail_bits Trackbits.
 */
static CommandCost TestAutoslopeOnRailTile(TileIndex tile, uint flags, uint z_old, Slope tileh_old, uint z_new, Slope tileh_new, TrackBits rail_bits)
{
	if (!_patches.build_on_slopes || !AutoslopeEnabled()) return CMD_ERROR;

	/* Is the slope-rail_bits combination valid in general? I.e. is it save to call GetRailFoundation() ? */
	if (CmdFailed(CheckRailSlope(tileh_new, rail_bits, TRACK_BIT_NONE, tile))) return CMD_ERROR;

	/* Get the slopes on top of the foundations */
	z_old += ApplyFoundationToSlope(GetRailFoundation(tileh_old, rail_bits), &tileh_old);
	z_new += ApplyFoundationToSlope(GetRailFoundation(tileh_new, rail_bits), &tileh_new);

	Corner track_corner;
	switch (rail_bits) {
		case TRACK_BIT_LEFT:  track_corner = CORNER_W; break;
		case TRACK_BIT_LOWER: track_corner = CORNER_S; break;
		case TRACK_BIT_RIGHT: track_corner = CORNER_E; break;
		case TRACK_BIT_UPPER: track_corner = CORNER_N; break;

		/* Surface slope must not be changed */
		default: return (((z_old != z_new) || (tileh_old != tileh_new)) ? CMD_ERROR : _price.terraform);
	}

	/* The height of the track_corner must not be changed. The rest ensures GetRailFoundation() already. */
	z_old += GetSlopeZInCorner((Slope)(tileh_old & ~SLOPE_HALFTILE_MASK), track_corner);
	z_new += GetSlopeZInCorner((Slope)(tileh_new & ~SLOPE_HALFTILE_MASK), track_corner);
	if (z_old != z_new) return CMD_ERROR;

	CommandCost cost = CommandCost(_price.terraform);
	/* Make the ground dirty, if surface slope has changed */
	if (tileh_old != tileh_new) {
		if (GetRailGroundType(tile) == RAIL_GROUND_WATER) cost.AddCost(_price.clear_water);
		if ((flags & DC_EXEC) != 0) SetRailGroundType(tile, RAIL_GROUND_BARREN);
	}
	return  cost;
}

static CommandCost TerraformTile_Track(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	uint z_old;
	Slope tileh_old = GetTileSlope(tile, &z_old);
	if (IsPlainRailTile(tile)) {
		TrackBits rail_bits = GetTrackBits(tile);
		bool was_water = GetRailGroundType(tile) == RAIL_GROUND_WATER;

		_error_message = STR_1008_MUST_REMOVE_RAILROAD_TRACK;

		/* First test autoslope. However if it succeeds we still have to test the rest, because non-autoslope terraforming is cheaper. */
		CommandCost autoslope_result = TestAutoslopeOnRailTile(tile, flags, z_old, tileh_old, z_new, tileh_new, rail_bits);

		/* When there is only a single horizontal/vertical track, one corner can be terraformed. */
		Corner allowed_corner;
		switch (rail_bits) {
			case TRACK_BIT_RIGHT: allowed_corner = CORNER_W; break;
			case TRACK_BIT_UPPER: allowed_corner = CORNER_S; break;
			case TRACK_BIT_LEFT:  allowed_corner = CORNER_E; break;
			case TRACK_BIT_LOWER: allowed_corner = CORNER_N; break;
			default: return autoslope_result;
		}

		Foundation f_old = GetRailFoundation(tileh_old, rail_bits);

		/* Do not allow terraforming if allowed_corner is part of anti-zig-zag foundations */
		if (tileh_old != SLOPE_NS && tileh_old != SLOPE_EW && IsSpecialRailFoundation(f_old)) return autoslope_result;

		/* Everything is valid, which only changes allowed_corner */
		for (Corner corner = (Corner)0; corner < CORNER_END; corner = (Corner)(corner + 1)) {
			if (allowed_corner == corner) continue;
			if (z_old + GetSlopeZInCorner(tileh_old, corner) != z_new + GetSlopeZInCorner(tileh_new, corner)) return autoslope_result;
		}

		/* Make the ground dirty */
		if ((flags & DC_EXEC) != 0) SetRailGroundType(tile, RAIL_GROUND_BARREN);

		/* allow terraforming */
		return (was_water ? CommandCost(_price.clear_water) : CommandCost());
	} else {
		if (_patches.build_on_slopes && AutoslopeEnabled()) {
			switch (GetRailTileType(tile)) {
				case RAIL_TILE_WAYPOINT: {
					CommandCost cost = TestAutoslopeOnRailTile(tile, flags, z_old, tileh_old, z_new, tileh_new, GetRailWaypointBits(tile));
					if (!CmdFailed(cost)) return cost; // allow autoslope
					break;
				}

				case RAIL_TILE_DEPOT:
					if (AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, GetRailDepotDirection(tile))) return _price.terraform;
					break;

				default: NOT_REACHED();
			}
		}
	}
	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}


extern const TileTypeProcs _tile_type_rail_procs = {
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
	GetFoundation_Track,      /* get_foundation_proc */
	TerraformTile_Track,      /* terraform_tile_proc */
};
