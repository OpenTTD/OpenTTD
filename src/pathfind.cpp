/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pathfind.cpp Implementation of the oldest supported pathfinder. */

#include "stdafx.h"
#include "pathfind.h"
#include "debug.h"
#include "tunnelbridge_map.h"
#include "core/alloc_type.hpp"
#include "tunnelbridge.h"

struct RememberData {
	uint16 cur_length;
	byte depth;
	Track last_choosen_track;
};

struct TrackPathFinder {
	TPFEnumProc *enum_proc;
	void *userdata;
	RememberData rd;
	TrackdirByte the_dir;
};

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

	if (++tpf->rd.cur_length > 50)
		return;

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

		if (!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length)) {
			TPFModeShip(tpf, tile, TrackdirToExitdir(tpf->the_dir));
		}

		tpf->rd = rd;
	} while (bits != TRACK_BIT_NONE);

}

void OPFShipFollowTrack(TileIndex tile, DiagDirection direction, TPFEnumProc *enum_proc, void *data)
{
	assert(IsValidDiagDirection(direction));

	SmallStackSafeStackAlloc<TrackPathFinder, 1> tpf;

	/* initialize path finder variables */
	tpf->userdata = data;
	tpf->enum_proc = enum_proc;

	tpf->rd.cur_length = 0;
	tpf->rd.depth = 0;
	tpf->rd.last_choosen_track = INVALID_TRACK;

	tpf->enum_proc(tile, data, INVALID_TRACKDIR, 0);
	TPFModeShip(tpf, tile, direction);
}
