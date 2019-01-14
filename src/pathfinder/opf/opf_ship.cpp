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
#include "../pathfinder_type.h"
#include "../../depot_base.h"

#include "../../safeguards.h"

static const uint OPF_MAX_LENGTH = 50; ///< The default maximum path length

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
	uint max_path_length;
	RememberData rd;
	TrackdirByte the_dir;
	Owner ship_owner;
};

static bool ShipTrackFollower(TileIndex tile, TrackPathFinder *pfs, uint length, bool depot)
{
	if (!depot) {
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
	} else {
		/* Found depot? */
		if (IsShipDepotTile(tile) && GetShipDepotPart(tile) == DEPOT_PART_NORTH && IsTileOwner(tile, pfs->ship_owner)) {
			pfs->best_bird_dist = 0;
			pfs->dest_coords = tile;
			pfs->best_length = minu(pfs->best_length, length);
			return true;
		}

		return false;
	}
}

static void TPFModeShip(TrackPathFinder *tpf, TileIndex tile, DiagDirection direction, bool depot)
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

	/* Finish if we already exceeded the maximum path cost */
	if (++tpf->rd.cur_length > tpf->max_path_length) return;

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

		if (!ShipTrackFollower(tile, tpf, tpf->rd.cur_length, depot)) {
			TPFModeShip(tpf, tile, TrackdirToExitdir(tpf->the_dir), depot);
		}

		tpf->rd = rd;
	} while (bits != TRACK_BIT_NONE);
}

static void OPFShipFollowTrack(TileIndex tile, DiagDirection direction, TrackPathFinder *tpf, bool depot)
{
	assert(IsValidDiagDirection(direction));

	/* initialize path finder variables */
	tpf->rd.cur_length = 0;
	tpf->rd.depth = 0;
	tpf->rd.last_choosen_track = INVALID_TRACK;

	ShipTrackFollower(tile, tpf, 0, depot);
	TPFModeShip(tpf, tile, direction, depot);
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

struct ShipTrackData {
	Track best_track;
	uint best_bird_dist;
	uint best_length;
	TileIndex dest_coords;
};

static void FindShipTrack(const Ship *v, TileIndex tile, DiagDirection dir, TrackBits bits, TileIndex skiptile, ShipTrackData *fstd, uint max_path_length = OPF_MAX_LENGTH, bool depot = false)
{
	fstd->best_track = INVALID_TRACK;
	fstd->best_bird_dist = 0;
	fstd->best_length = 0;
	fstd->dest_coords = depot ? INVALID_TILE : v->dest_tile;
	byte ship_dir = v->direction & 3;

	TrackPathFinder pfs;
	pfs.dest_coords = fstd->dest_coords;
	pfs.skiptile = skiptile;
	pfs.max_path_length = max_path_length;
	pfs.ship_owner = v->owner;

	assert(bits != TRACK_BIT_NONE);
	do {
		Track i = RemoveFirstTrack(&bits);

		pfs.best_bird_dist = UINT_MAX;
		pfs.best_length = UINT_MAX;

		OPFShipFollowTrack(tile, _ship_search_directions[i][dir], &pfs, depot);

		if (fstd->best_track != INVALID_TRACK) {
			if (pfs.best_bird_dist != 0) {
				/* neither reached the destination, pick the one with the smallest bird dist */
				if (pfs.best_bird_dist > fstd->best_bird_dist) goto bad;
				if (pfs.best_bird_dist < fstd->best_bird_dist) goto good;
			} else {
				if (pfs.best_length > fstd->best_length) goto bad;
				if (pfs.best_length < fstd->best_length) goto good;
			}

			/* if we reach this position, there's two paths of equal value so far.
			 * pick one randomly. */
			uint r = GB(Random(), 0, 8);
			if (_pick_shiptrack_table[i] == ship_dir) r += 80;
			if (_pick_shiptrack_table[fstd->best_track] == ship_dir) r -= 80;
			if (r <= 127) goto bad;
		}
good:;
		fstd->best_track = i;
		fstd->best_bird_dist = pfs.best_bird_dist;
		fstd->best_length = pfs.best_length;
		fstd->dest_coords = pfs.dest_coords;
bad:;

	} while (bits != TRACK_BIT_NONE);
}

/**
 * Finds the best track to choose on the next tile and
 * returns INVALID_TRACK when it is better to reverse.
 * @param v The ship.
 * @param tile The tile we are about to enter.
 * @param enterdir The direction entering the tile.
 * @param tracks The tracks available on new tile.
 * @param[out] path_found Whether a path has been found.
 * @return Best track on next tile or INVALID_TRACK when better to reverse.
 */
Track OPFShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found)
{
	assert(IsValidDiagDirection(enterdir));

	TileIndex tile2 = TILE_ADD(tile, -TileOffsByDiagDir(enterdir));
	ShipTrackData fstd;

	/* Let's find out how far it would be if we would reverse first */
	uint rev_dist = UINT_MAX; // distance if we reverse
	Track cur_track = TrackdirToTrack(v->GetVehicleTrackdir()); // track on the current tile
	DiagDirection rev_enterdir = ReverseDiagDir(enterdir);
	TrackBits rev_tracks = TrackStatusToTrackBits(GetTileTrackStatus(tile2, TRANSPORT_WATER, 0)) &
			DiagdirReachesTracks(rev_enterdir);

	if (HasTrack(rev_tracks, cur_track)) {
		FindShipTrack(v, tile2, rev_enterdir, TrackToTrackBits(cur_track), tile, &fstd);
		rev_dist = fstd.best_bird_dist;
		if (rev_dist != UINT_MAX) rev_dist++; // penalty for reversing
	}

	/* And if we would not reverse? */
	FindShipTrack(v, tile, enterdir, tracks, 0, &fstd);
	uint dist = fstd.best_bird_dist;

	/* Due to the way this pathfinder works we cannot determine whether we're lost or not. */
	path_found = true;
	if (dist <= rev_dist) return fstd.best_track;
	return INVALID_TRACK; // We could better reverse
}

FindDepotData OPFShipFindNearestDepot(const Ship *v, int max_distance)
{
	FindDepotData fdd;

	Trackdir trackdir = v->GetVehicleTrackdir();

	/* Argument values for FindShipTrack below, for the current ship direction */
	DiagDirection enterdir = TrackdirToExitdir(trackdir);
	TileIndex tile = TILE_ADD(v->tile, TileOffsByDiagDir(enterdir));
	TrackBits tracks = TrackdirBitsToTrackBits(DiagdirReachesTrackdirs(enterdir));

	/* Argument values for FindShipTrack below, for the reversed ship direction */
	DiagDirection enterdir_rev = ReverseDiagDir(enterdir);
	TileIndex tile_rev = v->tile;
	TrackBits tracks_rev = TrackStatusToTrackBits(GetTileTrackStatus(tile_rev, TRANSPORT_WATER, 0)) & DiagdirReachesTracks(enterdir_rev) & TrackdirBitsToTrackBits(TrackdirToTrackdirBits(trackdir));

	ShipTrackData fstd;
	if (max_distance == 0) max_distance = OPF_MAX_LENGTH;

	/* Let's find the length it would be if we would reverse first */
	uint length_rev = UINT_MAX;
	TileIndex dest_tile_rev = INVALID_TILE;
	if (tracks_rev != 0) {
		FindShipTrack(v, tile_rev, enterdir_rev, tracks_rev, tile, &fstd, max_distance, true);
		length_rev = fstd.best_length;
		tile_rev = fstd.dest_coords;
		if (length_rev != UINT_MAX) length_rev++; // penalty for reversing
	}

	/* And if we would not reverse? */
	FindShipTrack(v, tile, enterdir, tracks, 0, &fstd, max_distance, true);
	uint length = fstd.best_length;
	TileIndex dest_tile = fstd.dest_coords;

	if (IsValidTile(dest_tile_rev)) {
		if (IsValidTile(dest_tile)) {
			fdd.tile = length_rev < length ? dest_tile_rev : dest_tile; // tile location of ship depot
			fdd.best_length = DistanceManhattan(v->tile, fdd.tile); // distance manhattan from ship to depot
			fdd.reverse = !(length <= length_rev);
		}
		fdd.tile = dest_tile_rev;
		fdd.best_length = DistanceManhattan(v->tile, fdd.tile); // distance manhattan from ship to depot
		fdd.reverse = true;
	} else {
		if (IsValidTile(dest_tile)) {
			fdd.tile = dest_tile;
			fdd.best_length = DistanceManhattan(v->tile, fdd.tile); // distance manhattan from ship to depot
			fdd.reverse = false;
		}
	}

	return fdd;
}
