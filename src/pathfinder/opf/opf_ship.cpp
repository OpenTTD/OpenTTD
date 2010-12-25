/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opf_ship.cpp Implementation of the oldest supported ship pathfinder. */

#include "../../stdafx.h"
#include "../../tunnelbridge_map.h"
#include "../../tunnelbridge.h"
#include "../../ship.h"
#include "../../core/random_func.hpp"

struct RememberData {
	uint16 cur_length;
	byte depth;
	Track last_choosen_track;
};

struct TrackPathFinder {
	TileIndex skiptile;
	TileIndex dest_coords;
	uint best_bird_dist;
	uint best_length;
	RememberData rd;
	TrackdirByte the_dir;
};

static bool ShipTrackFollower(TileIndex tile, TrackPathFinder *pfs, uint length)
{
	/* Found dest? */
	if (tile == pfs->dest_coords) {
		pfs->best_bird_dist = 0;

		pfs->best_length = minu(pfs->best_length, length);
		return true;
	}

	/* Skip this tile in the calculation */
	if (tile != pfs->skiptile) {
		pfs->best_bird_dist = minu(pfs->best_bird_dist, DistanceMaxPlusManhattan(pfs->dest_coords, tile));
	}

	return false;
}

static void TPFModeShip(TrackPathFinder *tpf, TileIndex tile, DiagDirection direction)
{
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		/* wrong track type */
		if (GetTunnelBridgeTransportType(tile) != TRANSPORT_WATER) return;

		DiagDirection dir = GetTunnelBridgeDirection(tile);
		/* entering tunnel / bridge? */
		if (dir == direction) {
			TileIndex endtile = GetOtherTunnelBridgeEnd(tile);

			tpf->rd.cur_length += GetTunnelBridgeLength(tile, endtile) + 1;

			tile = endtile;
		} else {
			/* leaving tunnel / bridge? */
			if (ReverseDiagDir(dir) != direction) return;
		}
	}

	/* This addition will sometimes overflow by a single tile.
	 * The use of TILE_MASK here makes sure that we still point at a valid
	 * tile, and then this tile will be in the sentinel row/col, so GetTileTrackStatus will fail. */
	tile = TILE_MASK(tile + TileOffsByDiagDir(direction));

	if (++tpf->rd.cur_length > 50) return;

	TrackBits bits = TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_WATER, 0)) & DiagdirReachesTracks(direction);
	if (bits == TRACK_BIT_NONE) return;

	assert(TileX(tile) != MapMaxX() && TileY(tile) != MapMaxY());

	bool only_one_track = true;
	do {
		Track track = RemoveFirstTrack(&bits);
		if (bits != TRACK_BIT_NONE) only_one_track = false;
		RememberData rd = tpf->rd;

		/* Change direction 4 times only */
		if (!only_one_track && track != tpf->rd.last_choosen_track) {
			if (++tpf->rd.depth > 4) {
				tpf->rd = rd;
				return;
			}
			tpf->rd.last_choosen_track = track;
		}

		tpf->the_dir = TrackEnterdirToTrackdir(track, direction);

		if (!ShipTrackFollower(tile, tpf, tpf->rd.cur_length)) {
			TPFModeShip(tpf, tile, TrackdirToExitdir(tpf->the_dir));
		}

		tpf->rd = rd;
	} while (bits != TRACK_BIT_NONE);
}

static void OPFShipFollowTrack(TileIndex tile, DiagDirection direction, TrackPathFinder *tpf)
{
	assert(IsValidDiagDirection(direction));

	/* initialize path finder variables */
	tpf->rd.cur_length = 0;
	tpf->rd.depth = 0;
	tpf->rd.last_choosen_track = INVALID_TRACK;

	ShipTrackFollower(tile, tpf, 0);
	TPFModeShip(tpf, tile, direction);
}

/** Directions to search towards given track bits and the ship's enter direction. */
static const DiagDirection _ship_search_directions[6][4] = {
	{ DIAGDIR_NE,      INVALID_DIAGDIR, DIAGDIR_SW,      INVALID_DIAGDIR },
	{ INVALID_DIAGDIR, DIAGDIR_SE,      INVALID_DIAGDIR, DIAGDIR_NW      },
	{ INVALID_DIAGDIR, DIAGDIR_NE,      DIAGDIR_NW,      INVALID_DIAGDIR },
	{ DIAGDIR_SE,      INVALID_DIAGDIR, INVALID_DIAGDIR, DIAGDIR_SW      },
	{ DIAGDIR_NW,      DIAGDIR_SW,      INVALID_DIAGDIR, INVALID_DIAGDIR },
	{ INVALID_DIAGDIR, INVALID_DIAGDIR, DIAGDIR_SE,      DIAGDIR_NE      },
};

/** Track to "direction (& 3)" mapping. */
static const byte _pick_shiptrack_table[6] = {DIR_NE, DIR_SE, DIR_E, DIR_E, DIR_N, DIR_N};

static uint FindShipTrack(const Ship *v, TileIndex tile, DiagDirection dir, TrackBits bits, TileIndex skiptile, Track *track)
{
	TrackPathFinder pfs;
	uint best_bird_dist = 0;
	uint best_length    = 0;
	byte ship_dir = v->direction & 3;

	pfs.dest_coords = v->dest_tile;
	pfs.skiptile = skiptile;

	Track best_track = INVALID_TRACK;

	do {
		Track i = RemoveFirstTrack(&bits);

		pfs.best_bird_dist = UINT_MAX;
		pfs.best_length = UINT_MAX;

		OPFShipFollowTrack(tile, _ship_search_directions[i][dir], &pfs);

		if (best_track != INVALID_TRACK) {
			if (pfs.best_bird_dist != 0) {
				/* neither reached the destination, pick the one with the smallest bird dist */
				if (pfs.best_bird_dist > best_bird_dist) goto bad;
				if (pfs.best_bird_dist < best_bird_dist) goto good;
			} else {
				if (pfs.best_length > best_length) goto bad;
				if (pfs.best_length < best_length) goto good;
			}

			/* if we reach this position, there's two paths of equal value so far.
			 * pick one randomly. */
			uint r = GB(Random(), 0, 8);
			if (_pick_shiptrack_table[i] == ship_dir) r += 80;
			if (_pick_shiptrack_table[best_track] == ship_dir) r -= 80;
			if (r <= 127) goto bad;
		}
good:;
		best_track = i;
		best_bird_dist = pfs.best_bird_dist;
		best_length = pfs.best_length;
bad:;

	} while (bits != 0);

	*track = best_track;
	return best_bird_dist;
}

/**
 * returns the track to choose on the next tile, or -1 when it's better to
 * reverse. The tile given is the tile we are about to enter, enterdir is the
 * direction in which we are entering the tile
 */
Track OPFShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found)
{
	assert(IsValidDiagDirection(enterdir));

	TileIndex tile2 = TILE_ADD(tile, -TileOffsByDiagDir(enterdir));
	Track track;

	/* Let's find out how far it would be if we would reverse first */
	TrackBits b = TrackStatusToTrackBits(GetTileTrackStatus(tile2, TRANSPORT_WATER, 0)) & DiagdirReachesTracks(ReverseDiagDir(enterdir)) & v->state;

	uint distr = UINT_MAX; // distance if we reversed
	if (b != 0) {
		distr = FindShipTrack(v, tile2, ReverseDiagDir(enterdir), b, tile, &track);
		if (distr != UINT_MAX) distr++; // penalty for reversing
	}

	/* And if we would not reverse? */
	uint dist = FindShipTrack(v, tile, enterdir, tracks, 0, &track);

	/* If the dist equals zero, or distr equals one (the extra reversing penalty),
	 * then we found our destination and we are not lost.
	 * When we are not lost (yet) and the distance to our destination becomes
	 * less, then we aren't lost yet.
	 * So, we only become lost when the distance to our destination increases. */
	path_found = (dist == 0 || distr == 1 || (!(v->vehicle_flags & VF_PATHFINDER_LOST) && min(dist, distr) < DistanceManhattan(tile, v->dest_tile)));
	if (dist <= distr) return track;
	return INVALID_TRACK; // We could better reverse
}
