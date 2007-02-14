/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "debug.h"
#include "functions.h"
#include "npf.h"
#include "aystar.h"
#include "macros.h"
#include "pathfind.h"
#include "station.h"
#include "station_map.h"
#include "tile.h"
#include "depot.h"
#include "tunnel_map.h"
#include "network/network.h"
#include "water_map.h"

static AyStar _npf_aystar;

/* The cost of each trackdir. A diagonal piece is the full NPF_TILE_LENGTH,
 * the shorter piece is sqrt(2)/2*NPF_TILE_LENGTH =~ 0.7071
 */
#define NPF_STRAIGHT_LENGTH (uint)(NPF_TILE_LENGTH * STRAIGHT_TRACK_LENGTH)
static const uint _trackdir_length[TRACKDIR_END] = {
	NPF_TILE_LENGTH, NPF_TILE_LENGTH, NPF_STRAIGHT_LENGTH, NPF_STRAIGHT_LENGTH, NPF_STRAIGHT_LENGTH, NPF_STRAIGHT_LENGTH,
	0, 0,
	NPF_TILE_LENGTH, NPF_TILE_LENGTH, NPF_STRAIGHT_LENGTH, NPF_STRAIGHT_LENGTH, NPF_STRAIGHT_LENGTH, NPF_STRAIGHT_LENGTH
};

/**
 * Calculates the minimum distance traveled to get from t0 to t1 when only
 * using tracks (ie, only making 45 degree turns). Returns the distance in the
 * NPF scale, ie the number of full tiles multiplied by NPF_TILE_LENGTH to
 * prevent rounding.
 */
static uint NPFDistanceTrack(TileIndex t0, TileIndex t1)
{
	const uint dx = delta(TileX(t0), TileX(t1));
	const uint dy = delta(TileY(t0), TileY(t1));

	const uint straightTracks = 2 * min(dx, dy); /* The number of straight (not full length) tracks */
	/* OPTIMISATION:
	 * Original: diagTracks = max(dx, dy) - min(dx,dy);
	 * Proof:
	 * (dx+dy) - straightTracks  == (min + max) - straightTracks = min + max - 2 * min = max - min */
	const uint diagTracks = dx + dy - straightTracks; /* The number of diagonal (full tile length) tracks. */

	/* Don't factor out NPF_TILE_LENGTH below, this will round values and lose
	 * precision */
	return diagTracks * NPF_TILE_LENGTH + straightTracks * NPF_TILE_LENGTH * STRAIGHT_TRACK_LENGTH;
}


#if 0
static uint NTPHash(uint key1, uint key2)
{
	/* This function uses the old hash, which is fixed on 10 bits (1024 buckets) */
	return PATHFIND_HASH_TILE(key1);
}
#endif

/**
 * Calculates a hash value for use in the NPF.
 * @param key1 The TileIndex of the tile to hash
 * @param key2 The Trackdir of the track on the tile.
 *
 * @todo Think of a better hash.
 */
static uint NPFHash(uint key1, uint key2)
{
	/* TODO: think of a better hash? */
	uint part1 = TileX(key1) & NPF_HASH_HALFMASK;
	uint part2 = TileY(key1) & NPF_HASH_HALFMASK;

	assert(IsValidTrackdir((Trackdir)key2));
	assert(IsValidTile(key1));
	return ((part1 << NPF_HASH_HALFBITS | part2) + (NPF_HASH_SIZE * key2 / TRACKDIR_END)) % NPF_HASH_SIZE;
}

static int32 NPFCalcZero(AyStar* as, AyStarNode* current, OpenListNode* parent)
{
	return 0;
}

/* Calcs the tile of given station that is closest to a given tile
 * for this we assume the station is a rectangle,
 * as defined by its top tile (st->train_tile) and its width/height (st->trainst_w, st->trainst_h)
 */
static TileIndex CalcClosestStationTile(StationID station, TileIndex tile)
{
	const Station* st = GetStation(station);

	uint minx = TileX(st->train_tile);  // topmost corner of station
	uint miny = TileY(st->train_tile);
	uint maxx = minx + st->trainst_w - 1; // lowermost corner of station
	uint maxy = miny + st->trainst_h - 1;
	uint x;
	uint y;

	// we are going the aim for the x coordinate of the closest corner
	// but if we are between those coordinates, we will aim for our own x coordinate
	x = clamp(TileX(tile), minx, maxx);

	// same for y coordinate, see above comment
	y = clamp(TileY(tile), miny, maxy);

	// return the tile of our target coordinates
	return TileXY(x, y);
}

/* Calcs the heuristic to the target station or tile. For train stations, it
 * takes into account the direction of approach.
 */
static int32 NPFCalcStationOrTileHeuristic(AyStar* as, AyStarNode* current, OpenListNode* parent)
{
	NPFFindStationOrTileData* fstd = (NPFFindStationOrTileData*)as->user_target;
	NPFFoundTargetData* ftd = (NPFFoundTargetData*)as->user_path;
	TileIndex from = current->tile;
	TileIndex to = fstd->dest_coords;
	uint dist;

	// for train-stations, we are going to aim for the closest station tile
	if (as->user_data[NPF_TYPE] == TRANSPORT_RAIL && fstd->station_index != INVALID_STATION)
		to = CalcClosestStationTile(fstd->station_index, from);

	if (as->user_data[NPF_TYPE] == TRANSPORT_ROAD) {
		/* Since roads only have diagonal pieces, we use manhattan distance here */
		dist = DistanceManhattan(from, to) * NPF_TILE_LENGTH;
	} else {
		/* Ships and trains can also go diagonal, so the minimum distance is shorter */
		dist = NPFDistanceTrack(from, to);
	}

	DEBUG(npf, 4, "Calculating H for: (%d, %d). Result: %d", TileX(current->tile), TileY(current->tile), dist);

	if (dist < ftd->best_bird_dist) {
		ftd->best_bird_dist = dist;
		ftd->best_trackdir = (Trackdir)current->user_data[NPF_TRACKDIR_CHOICE];
	}
	return dist;
}


/* Fills AyStarNode.user_data[NPF_TRACKDIRCHOICE] with the chosen direction to
 * get here, either getting it from the current choice or from the parent's
 * choice */
static void NPFFillTrackdirChoice(AyStarNode* current, OpenListNode* parent)
{
	if (parent->path.parent == NULL) {
		Trackdir trackdir = (Trackdir)current->direction;
		/* This is a first order decision, so we'd better save the
		 * direction we chose */
		current->user_data[NPF_TRACKDIR_CHOICE] = trackdir;
		DEBUG(npf, 6, "Saving trackdir: 0x%X", trackdir);
	} else {
		/* We've already made the decision, so just save our parent's decision */
		current->user_data[NPF_TRACKDIR_CHOICE] = parent->path.node.user_data[NPF_TRACKDIR_CHOICE];
	}
}

/* Will return the cost of the tunnel. If it is an entry, it will return the
 * cost of that tile. If the tile is an exit, it will return the tunnel length
 * including the exit tile. Requires that this is a Tunnel tile */
static uint NPFTunnelCost(AyStarNode* current)
{
	DiagDirection exitdir = TrackdirToExitdir((Trackdir)current->direction);
	TileIndex tile = current->tile;
	if (GetTunnelDirection(tile) == ReverseDiagDir(exitdir)) {
		/* We just popped out if this tunnel, since were
		 * facing the tunnel exit */
		FindLengthOfTunnelResult flotr;
		flotr = FindLengthOfTunnel(tile, ReverseDiagDir(exitdir));
		return flotr.length * NPF_TILE_LENGTH;
		//TODO: Penalty for tunnels?
	} else {
		/* We are entering the tunnel, the enter tile is just a
		 * straight track */
		return NPF_TILE_LENGTH;
	}
}

static inline uint NPFBridgeCost(AyStarNode *current)
{
	return NPF_TILE_LENGTH * GetBridgeLength(current->tile, GetOtherBridgeEnd(current->tile));
}

static uint NPFSlopeCost(AyStarNode* current)
{
	TileIndex next = current->tile + TileOffsByDiagDir(TrackdirToExitdir((Trackdir)current->direction));
	int x,y;
	int8 z1,z2;

	x = TileX(current->tile) * TILE_SIZE;
	y = TileY(current->tile) * TILE_SIZE;
	/* get the height of the center of the current tile */
	z1 = GetSlopeZ(x + TILE_SIZE / 2, y + TILE_SIZE / 2);

	x = TileX(next) * TILE_SIZE;
	y = TileY(next) * TILE_SIZE;
	/* get the height of the center of the next tile */
	z2 = GetSlopeZ(x + TILE_SIZE / 2, y + TILE_SIZE / 2);

	if (z2 - z1 > 1) {
		/* Slope up */
		return _patches.npf_rail_slope_penalty;
	}
	return 0;
	/* Should we give a bonus for slope down? Probably not, we
	 * could just substract that bonus from the penalty, because
	 * there is only one level of steepness... */
}

/**
 * Mark tiles by mowing the grass when npf debug level >= 1.
 * Will not work for multiplayer games, since it can (will) cause desyncs.
 */
static void NPFMarkTile(TileIndex tile)
{
#ifndef NO_DEBUG_MESSAGES
	if (_debug_npf_level < 1 || _networking) return;
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			/* DEBUG: mark visited tiles by mowing the grass under them ;-) */
			if (!IsTileDepotType(tile, TRANSPORT_RAIL)) {
				SetRailGroundType(tile, RAIL_GROUND_BARREN);
				MarkTileDirtyByTile(tile);
			}
			break;

		case MP_STREET:
			if (!IsTileDepotType(tile, TRANSPORT_ROAD)) {
				SetRoadside(tile, ROADSIDE_BARREN);
				MarkTileDirtyByTile(tile);
			}
			break;

		default:
			break;
	}
#endif
}

static int32 NPFWaterPathCost(AyStar* as, AyStarNode* current, OpenListNode* parent)
{
	//TileIndex tile = current->tile;
	int32 cost = 0;
	Trackdir trackdir = (Trackdir)current->direction;

	cost = _trackdir_length[trackdir]; /* Should be different for diagonal tracks */

	if (IsBuoyTile(current->tile) && IsDiagonalTrackdir(trackdir))
		cost += _patches.npf_buoy_penalty; /* A small penalty for going over buoys */

	if (current->direction != NextTrackdir((Trackdir)parent->path.node.direction))
		cost += _patches.npf_water_curve_penalty;

	/* TODO More penalties? */

	return cost;
}

/* Determine the cost of this node, for road tracks */
static int32 NPFRoadPathCost(AyStar* as, AyStarNode* current, OpenListNode* parent)
{
	TileIndex tile = current->tile;
	int32 cost = 0;

	/* Determine base length */
	switch (GetTileType(tile)) {
		case MP_TUNNELBRIDGE:
			cost = IsTunnel(tile) ? NPFTunnelCost(current) : NPFBridgeCost(current);
			break;

		case MP_STREET:
			cost = NPF_TILE_LENGTH;
			/* Increase the cost for level crossings */
			if (IsLevelCrossing(tile)) cost += _patches.npf_crossing_penalty;
			break;

		case MP_STATION:
			cost = NPF_TILE_LENGTH;
			/* Increase the cost for drive-through road stops */
			if (IsDriveThroughStopTile(tile)) cost += _patches.npf_road_drive_through_penalty;
			break;

		default:
			break;
	}

	/* Determine extra costs */

	/* Check for slope */
	cost += NPFSlopeCost(current);

	/* Check for turns. Road vehicles only really drive diagonal, turns are
	 * represented by non-diagonal tracks */
	if (!IsDiagonalTrackdir((Trackdir)current->direction))
		cost += _patches.npf_road_curve_penalty;

	NPFMarkTile(tile);
	DEBUG(npf, 4, "Calculating G for: (%d, %d). Result: %d", TileX(current->tile), TileY(current->tile), cost);
	return cost;
}


/* Determine the cost of this node, for railway tracks */
static int32 NPFRailPathCost(AyStar* as, AyStarNode* current, OpenListNode* parent)
{
	TileIndex tile = current->tile;
	Trackdir trackdir = (Trackdir)current->direction;
	int32 cost = 0;
	/* HACK: We create a OpenListNode manually, so we can call EndNodeCheck */
	OpenListNode new_node;

	/* Determine base length */
	switch (GetTileType(tile)) {
		case MP_TUNNELBRIDGE:
			cost = IsTunnel(tile) ? NPFTunnelCost(current) : NPFBridgeCost(current);
			break;

		case MP_RAILWAY:
			cost = _trackdir_length[trackdir]; /* Should be different for diagonal tracks */
			break;

		case MP_STREET: /* Railway crossing */
			cost = NPF_TILE_LENGTH;
			break;

		case MP_STATION:
			/* We give a station tile a penalty. Logically we would only want to give
			 * station tiles that are not our destination this penalty. This would
			 * discourage trains to drive through busy stations. But, we can just
			 * give any station tile a penalty, because every possible route will get
			 * this penalty exactly once, on its end tile (if it's a station) and it
			 * will therefore not make a difference. */
			cost = NPF_TILE_LENGTH + _patches.npf_rail_station_penalty;
			break;

		default:
			break;
	}

	/* Determine extra costs */

	/* Check for signals */
	if (IsTileType(tile, MP_RAILWAY) && HasSignalOnTrackdir(tile, trackdir)) {
		/* Ordinary track with signals */
		if (GetSignalStateByTrackdir(tile, trackdir) == SIGNAL_STATE_RED) {
			/* Signal facing us is red */
			if (!NPFGetFlag(current, NPF_FLAG_SEEN_SIGNAL)) {
				/* Penalize the first signal we
				 * encounter, if it is red */

				/* Is this a presignal exit or combo? */
				SignalType sigtype = GetSignalType(tile);
				if (sigtype == SIGTYPE_EXIT || sigtype == SIGTYPE_COMBO) {
					/* Penalise exit and combo signals differently (heavier) */
					cost += _patches.npf_rail_firstred_exit_penalty;
				} else {
					cost += _patches.npf_rail_firstred_penalty;
				}
			}
			/* Record the state of this signal */
			NPFSetFlag(current, NPF_FLAG_LAST_SIGNAL_RED, true);
		} else {
			/* Record the state of this signal */
			NPFSetFlag(current, NPF_FLAG_LAST_SIGNAL_RED, false);
		}
		NPFSetFlag(current, NPF_FLAG_SEEN_SIGNAL, true);
	}

	/* Penalise the tile if it is a target tile and the last signal was
	 * red */
	/* HACK: We create a new_node here so we can call EndNodeCheck. Ugly as hell
	 * of course... */
	new_node.path.node = *current;
	if (as->EndNodeCheck(as, &new_node) == AYSTAR_FOUND_END_NODE && NPFGetFlag(current, NPF_FLAG_LAST_SIGNAL_RED))
		cost += _patches.npf_rail_lastred_penalty;

	/* Check for slope */
	cost += NPFSlopeCost(current);

	/* Check for turns */
	if (current->direction != NextTrackdir((Trackdir)parent->path.node.direction))
		cost += _patches.npf_rail_curve_penalty;
	//TODO, with realistic acceleration, also the amount of straight track between
	//      curves should be taken into account, as this affects the speed limit.

	/* Check for reverse in depot */
	if (IsTileDepotType(tile, TRANSPORT_RAIL) && as->EndNodeCheck(as, &new_node) != AYSTAR_FOUND_END_NODE) {
		/* Penalise any depot tile that is not the last tile in the path. This
		 * _should_ penalise every occurence of reversing in a depot (and only
		 * that) */
		cost += _patches.npf_rail_depot_reverse_penalty;
	}

	/* Check for occupied track */
	//TODO

	NPFMarkTile(tile);
	DEBUG(npf, 4, "Calculating G for: (%d, %d). Result: %d", TileX(current->tile), TileY(current->tile), cost);
	return cost;
}

/* Will find any depot */
static int32 NPFFindDepot(AyStar* as, OpenListNode *current)
{
	/* It's not worth caching the result with NPF_FLAG_IS_TARGET here as below,
	 * since checking the cache not that much faster than the actual check */
	return IsTileDepotType(current->path.node.tile, (TransportType)as->user_data[NPF_TYPE]) ?
		AYSTAR_FOUND_END_NODE : AYSTAR_DONE;
}

/* Will find a station identified using the NPFFindStationOrTileData */
static int32 NPFFindStationOrTile(AyStar* as, OpenListNode *current)
{
	NPFFindStationOrTileData* fstd = (NPFFindStationOrTileData*)as->user_target;
	AyStarNode *node = &current->path.node;
	TileIndex tile = node->tile;

	/* If GetNeighbours said we could get here, we assume the station type
	 * is correct */
	if (
		(fstd->station_index == INVALID_STATION && tile == fstd->dest_coords) || /* We've found the tile, or */
		(IsTileType(tile, MP_STATION) && GetStationIndex(tile) == fstd->station_index) /* the station */
	) {
		return AYSTAR_FOUND_END_NODE;
	} else {
		return AYSTAR_DONE;
	}
}

/* To be called when current contains the (shortest route to) the target node.
 * Will fill the contents of the NPFFoundTargetData using
 * AyStarNode[NPF_TRACKDIR_CHOICE].
 */
static void NPFSaveTargetData(AyStar* as, OpenListNode* current)
{
	NPFFoundTargetData* ftd = (NPFFoundTargetData*)as->user_path;
	ftd->best_trackdir = (Trackdir)current->path.node.user_data[NPF_TRACKDIR_CHOICE];
	ftd->best_path_dist = current->g;
	ftd->best_bird_dist = 0;
	ftd->node = current->path.node;
}

/**
 * Finds out if a given player's vehicles are allowed to enter a given tile.
 * @param owner    The owner of the vehicle.
 * @param tile     The tile that is about to be entered.
 * @param enterdir The direction from which the vehicle wants to enter the tile.
 * @return         true if the vehicle can enter the tile.
 * @todo           This function should be used in other places than just NPF,
 *                 maybe moved to another file too.
 */
static bool VehicleMayEnterTile(Owner owner, TileIndex tile, DiagDirection enterdir)
{
	if (IsTileType(tile, MP_RAILWAY) ||           /* Rail tile (also rail depot) */
			IsRailwayStationTile(tile) ||               /* Rail station tile */
			IsTileDepotType(tile, TRANSPORT_ROAD) ||  /* Road depot tile */
			IsStandardRoadStopTile(tile) || /* Road station tile (but not drive-through stops) */
			IsTileDepotType(tile, TRANSPORT_WATER)) { /* Water depot tile */
		return IsTileOwner(tile, owner); /* You need to own these tiles entirely to use them */
	}

	switch (GetTileType(tile)) {
		case MP_STREET:
			/* rail-road crossing : are we looking at the railway part? */
			if (IsLevelCrossing(tile) &&
					DiagDirToAxis(enterdir) != GetCrossingRoadAxis(tile)) {
				return IsTileOwner(tile, owner); /* Railway needs owner check, while the street is public */
			}
			break;

		case MP_TUNNELBRIDGE:
			if ((IsTunnel(tile) && GetTunnelTransportType(tile) == TRANSPORT_RAIL) ||
					(IsBridge(tile) && GetBridgeTransportType(tile) == TRANSPORT_RAIL)) {
				return IsTileOwner(tile, owner);
			}
			break;

		default:
			break;
	}

	return true; /* no need to check */
}


/**
 * Returns the direction the exit of the depot on the given tile is facing.
 */
static DiagDirection GetDepotDirection(TileIndex tile, TransportType type)
{
	assert(IsTileDepotType(tile, type));

	switch (type) {
		case TRANSPORT_RAIL:  return GetRailDepotDirection(tile);
		case TRANSPORT_ROAD:  return GetRoadDepotDirection(tile);
		case TRANSPORT_WATER: return GetShipDepotDirection(tile);
		default: return INVALID_DIAGDIR; /* Not reached */
	}
}


/* Will just follow the results of GetTileTrackStatus concerning where we can
 * go and where not. Uses AyStar.user_data[NPF_TYPE] as the transport type and
 * an argument to GetTileTrackStatus. Will skip tunnels, meaning that the
 * entry and exit are neighbours. Will fill
 * AyStarNode.user_data[NPF_TRACKDIR_CHOICE] with an appropriate value, and
 * copy AyStarNode.user_data[NPF_NODE_FLAGS] from the parent */
static void NPFFollowTrack(AyStar* aystar, OpenListNode* current)
{
	Trackdir src_trackdir = (Trackdir)current->path.node.direction;
	TileIndex src_tile = current->path.node.tile;
	DiagDirection src_exitdir = TrackdirToExitdir(src_trackdir);
	TileIndex dst_tile = INVALID_TILE;
	int i;
	uint32 ts;
	TrackdirBits trackdirbits;
	TransportType type = (TransportType)aystar->user_data[NPF_TYPE];
	bool override_dst_check = false;
	/* Initialize to 0, so we can jump out (return) somewhere an have no neighbours */
	aystar->num_neighbours = 0;
	DEBUG(npf, 4, "Expanding: (%d, %d, %d) [%d]", TileX(src_tile), TileY(src_tile), src_trackdir, src_tile);

	/* Find dest tile */
	if (IsTunnelTile(src_tile) && GetTunnelDirection(src_tile) == src_exitdir) {
		/* This is a tunnel. We know this tunnel is our type,
		 * otherwise we wouldn't have got here. It is also facing us,
		 * so we should skip it's body */
		dst_tile = GetOtherTunnelEnd(src_tile);
		override_dst_check = true;
	} else if (IsBridgeTile(src_tile) && GetBridgeRampDirection(src_tile) == src_exitdir) {
		dst_tile = GetOtherBridgeEnd(src_tile);
		override_dst_check = true;
	} else if (type != TRANSPORT_WATER && (IsStandardRoadStopTile(src_tile) || IsTileDepotType(src_tile, type))) {
		/* This is a road station (non drive-through) or a train or road depot. We can enter and exit
		 * those from one side only. Trackdirs don't support that (yet), so we'll
		 * do this here. */

		DiagDirection exitdir;
		/* Find out the exit direction first */
		if (IsRoadStopTile(src_tile)) {
			exitdir = GetRoadStopDir(src_tile);
		} else { /* Train or road depot */
			exitdir = GetDepotDirection(src_tile, type);
		}

		/* Let's see if were headed the right way into the depot */
		if (src_trackdir == DiagdirToDiagTrackdir(ReverseDiagDir(exitdir))) {
			/* We are headed inwards. We cannot go through the back of the depot.
			 * For rail, we can now reverse. Reversing for road vehicles is never
			 * useful, since you cannot take paths you couldn't take before
			 * reversing (as with rail). */
			if (type == TRANSPORT_RAIL) {
				/* We can only reverse here, so we'll not consider this direction, but
				 * jump ahead to the reverse direction.  It would be nicer to return
				 * one neighbour here (the reverse trackdir of the one we are
				 * considering now) and then considering that one to return the tracks
				 * outside of the depot. But, because the code layout is cleaner this
				 * way, we will just pretend we are reversed already */
				src_trackdir = ReverseTrackdir(src_trackdir);
				dst_tile = AddTileIndexDiffCWrap(src_tile, TileIndexDiffCByDiagDir(exitdir));
			} else {
				dst_tile = INVALID_TILE; /* Road vehicle heading inwards: dead end */
			}
		} else {
			dst_tile = AddTileIndexDiffCWrap(src_tile, TileIndexDiffCByDiagDir(exitdir));
		}
	} else {
		/* This a normal tile, a bridge, a tunnel exit, etc. */
		dst_tile = AddTileIndexDiffCWrap(src_tile, TileIndexDiffCByDiagDir(TrackdirToExitdir(src_trackdir)));
	}
	if (dst_tile == INVALID_TILE) {
		/* We reached the border of the map */
		/* TODO Nicer control flow for this */
		return;
	}

	/* I can't enter a tunnel entry/exit tile from a tile above the tunnel. Note
	 * that I can enter the tunnel from a tile below the tunnel entrance. This
	 * solves the problem of vehicles wanting to drive off a tunnel entrance */
	if (!override_dst_check) {
		if (IsTileType(dst_tile, MP_TUNNELBRIDGE)) {
			if (IsTunnel(dst_tile)) {
				if (GetTunnelDirection(dst_tile) != src_exitdir) return;
			} else {
				if (GetBridgeRampDirection(dst_tile) != src_exitdir) return;
			}
		}
	}

	/* check correct rail type (mono, maglev, etc) */
	if (type == TRANSPORT_RAIL) {
		RailType dst_type = GetTileRailType(dst_tile, TrackdirToTrack(src_trackdir));
		if (!HASBIT(aystar->user_data[NPF_RAILTYPES], dst_type))
			return;
	}

	/* Check the owner of the tile */
	if (!VehicleMayEnterTile((Owner)aystar->user_data[NPF_OWNER], dst_tile, TrackdirToExitdir(src_trackdir))) {
		return;
	}

	/* Determine available tracks */
	if (type != TRANSPORT_WATER && (IsStandardRoadStopTile(dst_tile) || IsTileDepotType(dst_tile, type))){
		/* Road stations and road and train depots return 0 on GTTS, so we have to do this by hand... */
		DiagDirection exitdir;
		if (IsRoadStopTile(dst_tile)) {
			exitdir = GetRoadStopDir(dst_tile);
		} else { /* Road or train depot */
			exitdir = GetDepotDirection(dst_tile, type);
		}
		/* Find the trackdirs that are available for a depot or station with this
		 * orientation. They are only "inwards", since we are reaching this tile
		 * from some other tile. This prevents vehicles driving into depots from
		 * the back */
		ts = TrackdirToTrackdirBits(DiagdirToDiagTrackdir(ReverseDiagDir(exitdir)));
	} else {
		ts = GetTileTrackStatus(dst_tile, type);
	}
	trackdirbits = (TrackdirBits)(ts & TRACKDIR_BIT_MASK); /* Filter out signal status and the unused bits */

	DEBUG(npf, 4, "Next node: (%d, %d) [%d], possible trackdirs: 0x%X", TileX(dst_tile), TileY(dst_tile), dst_tile, trackdirbits);
	/* Select only trackdirs we can reach from our current trackdir */
	trackdirbits &= TrackdirReachesTrackdirs(src_trackdir);
	if (_patches.forbid_90_deg && (type == TRANSPORT_RAIL || type == TRANSPORT_WATER)) /* Filter out trackdirs that would make 90 deg turns for trains */
		trackdirbits &= ~TrackdirCrossesTrackdirs(src_trackdir);

	DEBUG(npf, 6, "After filtering: (%d, %d), possible trackdirs: 0x%X", TileX(dst_tile), TileY(dst_tile), trackdirbits);

	i = 0;
	/* Enumerate possible track */
	while (trackdirbits != 0) {
		Trackdir dst_trackdir = RemoveFirstTrackdir(&trackdirbits);
		DEBUG(npf, 5, "Expanded into trackdir: %d, remaining trackdirs: 0x%X", dst_trackdir, trackdirbits);

		/* Check for oneway signal against us */
		if (IsTileType(dst_tile, MP_RAILWAY) && GetRailTileType(dst_tile) == RAIL_TILE_SIGNALS) {
			if (HasSignalOnTrackdir(dst_tile, ReverseTrackdir(dst_trackdir)) && !HasSignalOnTrackdir(dst_tile, dst_trackdir))
				// if one way signal not pointing towards us, stop going in this direction.
				break;
		}
		{
			/* We've found ourselves a neighbour :-) */
			AyStarNode* neighbour = &aystar->neighbours[i];
			neighbour->tile = dst_tile;
			neighbour->direction = dst_trackdir;
			/* Save user data */
			neighbour->user_data[NPF_NODE_FLAGS] = current->path.node.user_data[NPF_NODE_FLAGS];
			NPFFillTrackdirChoice(neighbour, current);
		}
		i++;
	}
	aystar->num_neighbours = i;
}

/*
 * Plan a route to the specified target (which is checked by target_proc),
 * from start1 and if not NULL, from start2 as well. The type of transport we
 * are checking is in type. reverse_penalty is applied to all routes that
 * originate from the second start node.
 * When we are looking for one specific target (optionally multiple tiles), we
 * should use a good heuristic to perform aystar search. When we search for
 * multiple targets that are spread around, we should perform a breadth first
 * search by specifiying CalcZero as our heuristic.
 */
static NPFFoundTargetData NPFRouteInternal(AyStarNode* start1, AyStarNode* start2, NPFFindStationOrTileData* target, AyStar_EndNodeCheck target_proc, AyStar_CalculateH heuristic_proc, TransportType type, Owner owner, RailTypeMask railtypes, uint reverse_penalty)
{
	int r;
	NPFFoundTargetData result;

	/* Initialize procs */
	_npf_aystar.CalculateH = heuristic_proc;
	_npf_aystar.EndNodeCheck = target_proc;
	_npf_aystar.FoundEndNode = NPFSaveTargetData;
	_npf_aystar.GetNeighbours = NPFFollowTrack;
	switch (type) {
		default: NOT_REACHED();
		case TRANSPORT_RAIL:  _npf_aystar.CalculateG = NPFRailPathCost;  break;
		case TRANSPORT_ROAD:  _npf_aystar.CalculateG = NPFRoadPathCost;  break;
		case TRANSPORT_WATER: _npf_aystar.CalculateG = NPFWaterPathCost; break;
	}

	/* Initialize Start Node(s) */
	start1->user_data[NPF_TRACKDIR_CHOICE] = INVALID_TRACKDIR;
	start1->user_data[NPF_NODE_FLAGS] = 0;
	_npf_aystar.addstart(&_npf_aystar, start1, 0);
	if (start2) {
		start2->user_data[NPF_TRACKDIR_CHOICE] = INVALID_TRACKDIR;
		start2->user_data[NPF_NODE_FLAGS] = 0;
		NPFSetFlag(start2, NPF_FLAG_REVERSE, true);
		_npf_aystar.addstart(&_npf_aystar, start2, reverse_penalty);
	}

	/* Initialize result */
	result.best_bird_dist = (uint)-1;
	result.best_path_dist = (uint)-1;
	result.best_trackdir = INVALID_TRACKDIR;
	_npf_aystar.user_path = &result;

	/* Initialize target */
	_npf_aystar.user_target = target;

	/* Initialize user_data */
	_npf_aystar.user_data[NPF_TYPE] = type;
	_npf_aystar.user_data[NPF_OWNER] = owner;
	_npf_aystar.user_data[NPF_RAILTYPES] = railtypes;

	/* GO! */
	r = AyStarMain_Main(&_npf_aystar);
	assert(r != AYSTAR_STILL_BUSY);

	if (result.best_bird_dist != 0) {
		if (target != NULL) {
			DEBUG(npf, 1, "Could not find route to tile 0x%X from 0x%X.", target->dest_coords, start1->tile);
		} else {
			/* Assumption: target == NULL, so we are looking for a depot */
			DEBUG(npf, 1, "Could not find route to a depot from tile 0x%X.", start1->tile);
		}

	}
	return result;
}

NPFFoundTargetData NPFRouteToStationOrTileTwoWay(TileIndex tile1, Trackdir trackdir1, TileIndex tile2, Trackdir trackdir2, NPFFindStationOrTileData* target, TransportType type, Owner owner, RailTypeMask railtypes)
{
	AyStarNode start1;
	AyStarNode start2;

	start1.tile = tile1;
	start2.tile = tile2;
	/* We set this in case the target is also the start tile, we will just
	 * return a not found then */
	start1.user_data[NPF_TRACKDIR_CHOICE] = INVALID_TRACKDIR;
	start1.direction = trackdir1;
	start2.direction = trackdir2;
	start2.user_data[NPF_TRACKDIR_CHOICE] = INVALID_TRACKDIR;

	return NPFRouteInternal(&start1, (IsValidTile(tile2) ? &start2 : NULL), target, NPFFindStationOrTile, NPFCalcStationOrTileHeuristic, type, owner, railtypes, 0);
}

NPFFoundTargetData NPFRouteToStationOrTile(TileIndex tile, Trackdir trackdir, NPFFindStationOrTileData* target, TransportType type, Owner owner, RailTypeMask railtypes)
{
	return NPFRouteToStationOrTileTwoWay(tile, trackdir, INVALID_TILE, INVALID_TRACKDIR, target, type, owner, railtypes);
}

NPFFoundTargetData NPFRouteToDepotBreadthFirstTwoWay(TileIndex tile1, Trackdir trackdir1, TileIndex tile2, Trackdir trackdir2, TransportType type, Owner owner, RailTypeMask railtypes, uint reverse_penalty)
{
	AyStarNode start1;
	AyStarNode start2;

	start1.tile = tile1;
	start2.tile = tile2;
	/* We set this in case the target is also the start tile, we will just
	 * return a not found then */
	start1.user_data[NPF_TRACKDIR_CHOICE] = INVALID_TRACKDIR;
	start1.direction = trackdir1;
	start2.direction = trackdir2;
	start2.user_data[NPF_TRACKDIR_CHOICE] = INVALID_TRACKDIR;

	/* perform a breadth first search. Target is NULL,
	 * since we are just looking for any depot...*/
	return NPFRouteInternal(&start1, (IsValidTile(tile2) ? &start2 : NULL), NULL, NPFFindDepot, NPFCalcZero, type, owner, railtypes, reverse_penalty);
}

NPFFoundTargetData NPFRouteToDepotBreadthFirst(TileIndex tile, Trackdir trackdir, TransportType type, Owner owner, RailTypeMask railtypes)
{
	return NPFRouteToDepotBreadthFirstTwoWay(tile, trackdir, INVALID_TILE, INVALID_TRACKDIR, type, owner, railtypes, 0);
}

NPFFoundTargetData NPFRouteToDepotTrialError(TileIndex tile, Trackdir trackdir, TransportType type, Owner owner, RailTypeMask railtypes)
{
	/* Okay, what we're gonna do. First, we look at all depots, calculate
	 * the manhatten distance to get to each depot. We then sort them by
	 * distance. We start by trying to plan a route to the closest, then
	 * the next closest, etc. We stop when the best route we have found so
	 * far, is shorter than the manhattan distance. This will obviously
	 * always find the closest depot. It will probably be most efficient
	 * for ships, since the heuristic will not be to far off then. I hope.
	 */
	Queue depots;
	int r;
	NPFFoundTargetData best_result = {(uint)-1, (uint)-1, INVALID_TRACKDIR, {INVALID_TILE, 0, {0, 0}}};
	NPFFoundTargetData result;
	NPFFindStationOrTileData target;
	AyStarNode start;
	Depot* current;
	Depot *depot;

	init_InsSort(&depots);
	/* Okay, let's find all depots that we can use first */
	FOR_ALL_DEPOTS(depot) {
		/* Check if this is really a valid depot, it is of the needed type and
		 * owner */
		if (IsTileDepotType(depot->xy, type) && IsTileOwner(depot->xy, owner))
			/* If so, let's add it to the queue, sorted by distance */
			depots.push(&depots, depot, DistanceManhattan(tile, depot->xy));
	}

	/* Now, let's initialise the aystar */

	/* Initialize procs */
	_npf_aystar.CalculateH = NPFCalcStationOrTileHeuristic;
	_npf_aystar.EndNodeCheck = NPFFindStationOrTile;
	_npf_aystar.FoundEndNode = NPFSaveTargetData;
	_npf_aystar.GetNeighbours = NPFFollowTrack;
	switch (type) {
		default: NOT_REACHED();
		case TRANSPORT_RAIL:  _npf_aystar.CalculateG = NPFRailPathCost;  break;
		case TRANSPORT_ROAD:  _npf_aystar.CalculateG = NPFRoadPathCost;  break;
		case TRANSPORT_WATER: _npf_aystar.CalculateG = NPFWaterPathCost; break;
	}

	/* Initialize target */
	target.station_index = INVALID_STATION; /* We will initialize dest_coords inside the loop below */
	_npf_aystar.user_target = &target;

	/* Initialize user_data */
	_npf_aystar.user_data[NPF_TYPE] = type;
	_npf_aystar.user_data[NPF_OWNER] = owner;

	/* Initialize Start Node */
	start.tile = tile;
	start.direction = trackdir; /* We will initialize user_data inside the loop below */

	/* Initialize Result */
	_npf_aystar.user_path = &result;
	best_result.best_path_dist = (uint)-1;
	best_result.best_bird_dist = (uint)-1;

	/* Just iterate the depots in order of increasing distance */
	while ((current = (Depot*)depots.pop(&depots))) {
		/* Check to see if we already have a path shorter than this
		 * depot's manhattan distance. HACK: We call DistanceManhattan
		 * again, we should probably modify the queue to give us that
		 * value... */
		if ( DistanceManhattan(tile, current->xy * NPF_TILE_LENGTH) > best_result.best_path_dist)
			break;

		/* Initialize Start Node */
		/* We set this in case the target is also the start tile, we will just
		 * return a not found then */
		start.user_data[NPF_TRACKDIR_CHOICE] = INVALID_TRACKDIR;
		start.user_data[NPF_NODE_FLAGS] = 0;
		_npf_aystar.addstart(&_npf_aystar, &start, 0);

		/* Initialize result */
		result.best_bird_dist = (uint)-1;
		result.best_path_dist = (uint)-1;
		result.best_trackdir = INVALID_TRACKDIR;

		/* Initialize target */
		target.dest_coords = current->xy;

		/* GO! */
		r = AyStarMain_Main(&_npf_aystar);
		assert(r != AYSTAR_STILL_BUSY);

		/* This depot is closer */
		if (result.best_path_dist < best_result.best_path_dist)
			best_result = result;
	}
	if (result.best_bird_dist != 0) {
		DEBUG(npf, 1, "Could not find route to any depot from tile 0x%X.", tile);
	}
	return best_result;
}

void InitializeNPF(void)
{
	init_AyStar(&_npf_aystar, NPFHash, NPF_HASH_SIZE);
	_npf_aystar.loops_per_tick = 0;
	_npf_aystar.max_path_cost = 0;
	//_npf_aystar.max_search_nodes = 0;
	/* We will limit the number of nodes for now, until we have a better
	 * solution to really fix performance */
	_npf_aystar.max_search_nodes = _patches.npf_max_search_nodes;
}

void NPFFillWithOrderData(NPFFindStationOrTileData* fstd, Vehicle* v)
{
	/* Ships don't really reach their stations, but the tile in front. So don't
	 * save the station id for ships. For roadvehs we don't store it either,
	 * because multistop depends on vehicles actually reaching the exact
	 * dest_tile, not just any stop of that station.
	 * So only for train orders to stations we fill fstd->station_index, for all
	 * others only dest_coords */
	if (v->current_order.type == OT_GOTO_STATION && v->type == VEH_Train) {
		fstd->station_index = v->current_order.dest;
		/* Let's take the closest tile of the station as our target for trains */
		fstd->dest_coords = CalcClosestStationTile(v->current_order.dest, v->tile);
	} else {
		fstd->dest_coords = v->dest_tile;
		fstd->station_index = INVALID_STATION;
	}
}
