/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "pbs.h"
#include "debug.h"
#include "map.h"
#include "tile.h"
#include "npf.h"
#include "pathfind.h"
#include "depot.h"

/** @file pbs.c Path-Based-Signalling implementation file
 *  @see pbs.h */

/* reserved track encoding:
 normal railway tracks:
   map3hi bits 4..6 = 'Track'number of reserved track + 1, if this is zero it means nothing is reserved on this tile
   map3hi bit  7    = if this is set, then the opposite track ('Track'number^1) is also reserved
 waypoints/stations:
   map3lo bit 6 set = track is reserved
 tunnels/bridges:
   map3hi bit 0 set = track with 'Track'number 0 is reserved
   map3hi bit 1 set = track with 'Track'number 1 is reserved
 level crossings:
   map5 bit 0 set = the rail track is reserved
*/

/**
 * maps an encoded reserved track (from map3lo bits 4..7)
 * to the tracks that are reserved.
 * 0xFF are invalid entries and should never be accessed.
 */
static const byte encrt_to_reserved[16] = {
	0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0xFF,
	0xFF, 0xFF, 0xFF, 0x0C, 0x0C, 0x30, 0x30, 0xFF
};

/**
 * maps an encoded reserved track (from map3lo bits 4..7)
 * to the track(dir)s that are unavailable due to reservations.
 * 0xFFFF are invalid entries and should never be accessed.
 */
static const uint16 encrt_to_unavail[16] = {
	0x0000, 0x3F3F, 0x3F3F, 0x3737, 0x3B3B, 0x1F1F, 0x2F2F, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0x3F3F, 0x3F3F, 0x3F3F, 0x3F3F, 0xFFFF
};

void PBSReserveTrack(TileIndex tile, Track track) {
	assert(IsValidTile(tile));
	assert(track <= 5);
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if ((_m[tile].m5 & ~1) == 0xC4) {
				// waypoint
				SETBIT(_m[tile].m3, 6);
			} else {
				// normal rail track
				byte encrt = GB(_m[tile].m4, 4, 4); // get current encoded info (see comments at top of file)

				if (encrt == 0) // nothing reserved before
					encrt = track + 1;
				else if (encrt == (track^1) + 1) // opposite track reserved before
					encrt |= 8;

				SB(_m[tile].m4, 4, 4, encrt);
			}
			break;
		case MP_TUNNELBRIDGE:
			_m[tile].m4 |= (1 << track) & 3;
			break;
		case MP_STATION:
			SETBIT(_m[tile].m3, 6);
			break;
		case MP_STREET:
			// make sure it is a railroad crossing
			if (!IsLevelCrossing(tile)) return;
			SETBIT(_m[tile].m5, 0);
			break;
		default:
			return;
	}
	// if debugging, mark tile dirty to show reserved status
	if (_debug_pbs_level >= 1)
		MarkTileDirtyByTile(tile);
}

byte PBSTileReserved(TileIndex tile) {
	assert(IsValidTile(tile));
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if ((_m[tile].m5 & ~1) == 0xC4) {
				// waypoint
				// check if its reserved
				if (!HASBIT(_m[tile].m3, 6)) return 0;
				// return the track for the correct direction
				return HASBIT(_m[tile].m5, 0) ? 2 : 1;
			} else {
				// normal track
				byte res = encrt_to_reserved[GB(_m[tile].m4, 4, 4)];
				assert(res != 0xFF);
				return res;
			}
		case MP_TUNNELBRIDGE:
			return GB(_m[tile].m4, 0, 2);
		case MP_STATION:
			// check if its reserved
			if (!HASBIT(_m[tile].m3, 6)) return 0;
			// return the track for the correct direction
			return HASBIT(_m[tile].m5, 0) ? 2 : 1;
		case MP_STREET:
			// make sure its a railroad crossing
			if (!IsLevelCrossing(tile)) return 0;
			// check if its reserved
			if (!HASBIT(_m[tile].m5, 0)) return 0;
			// return the track for the correct direction
			return HASBIT(_m[tile].m5, 3) ? 1 : 2;
		default:
			return 0;
	}
}

uint16 PBSTileUnavail(TileIndex tile) {
	assert(IsValidTile(tile));
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if ((_m[tile].m5 & ~1) == 0xC4) {
				// waypoint
				return HASBIT(_m[tile].m3, 6) ? TRACKDIR_BIT_MASK : 0;
			} else {
				// normal track
				uint16 res = encrt_to_unavail[GB(_m[tile].m4, 4, 4)];
				assert(res != 0xFFFF);
				return res;
			}
		case MP_TUNNELBRIDGE:
			return GB(_m[tile].m4, 0, 2) | (GB(_m[tile].m4, 0, 2) << 8);
		case MP_STATION:
			return HASBIT(_m[tile].m3, 6) ? TRACKDIR_BIT_MASK : 0;
		case MP_STREET:
			// make sure its a railroad crossing
			if (!IsLevelCrossing(tile)) return 0;
			// check if its reserved
			return (HASBIT(_m[tile].m5, 0)) ? TRACKDIR_BIT_MASK : 0;
		default:
			return 0;
	}
}

void PBSClearTrack(TileIndex tile, Track track) {
	assert(IsValidTile(tile));
	assert(track <= 5);
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if ((_m[tile].m5 & ~1) == 0xC4) {
				// waypoint
				CLRBIT(_m[tile].m3, 6);
			} else {
				// normal rail track
				byte encrt = GB(_m[tile].m4, 4, 4);

				if (encrt == track + 1)
					encrt = 0;
				else if (encrt == track + 1 + 8)
					encrt = (track^1) + 1;
				else if (encrt == (track^1) + 1 + 8)
					encrt &= 7;

				SB(_m[tile].m4, 4, 4, encrt);
			}
			break;
		case MP_TUNNELBRIDGE:
			_m[tile].m4 &= ~((1 << track) & 3);
			break;
		case MP_STATION:
			CLRBIT(_m[tile].m3, 6);
			break;
		case MP_STREET:
			// make sure it is a railroad crossing
			if (!IsLevelCrossing(tile)) return;
			CLRBIT(_m[tile].m5, 0);
			break;
		default:
			return;
	}
	// if debugging, mark tile dirty to show reserved status
	if (_debug_pbs_level >= 1)
		MarkTileDirtyByTile(tile);
}

void PBSClearPath(TileIndex tile, Trackdir trackdir, TileIndex end_tile, Trackdir end_trackdir) {
	uint16 res;
	FindLengthOfTunnelResult flotr;
	assert(IsValidTile(tile));
	assert(IsValidTrackdir(trackdir));

	do {
		PBSClearTrack(tile, TrackdirToTrack(trackdir));

		if (tile == end_tile && TrackdirToTrack(trackdir) == TrackdirToTrack(end_trackdir))
			return;

		if (IsTileType(tile, MP_TUNNELBRIDGE) &&
				GB(_m[tile].m5, 4, 4) == 0 &&
				GB(_m[tile].m5, 0, 2) == TrackdirToExitdir(trackdir)) {
			// this is a tunnel
			flotr = FindLengthOfTunnel(tile, TrackdirToExitdir(trackdir));

			tile = flotr.tile;
		} else {
			byte exitdir = TrackdirToExitdir(trackdir);
			tile = AddTileIndexDiffCWrap(tile, TileIndexDiffCByDir(exitdir));
		}

		res = PBSTileReserved(tile);
		res |= res << 8;
		res &= TrackdirReachesTrackdirs(trackdir);
		trackdir = FindFirstBit2x64(res);

	} while (res != 0);
}

bool PBSIsPbsSignal(TileIndex tile, Trackdir trackdir)
{
	assert(IsValidTile(tile));
	assert(IsValidTrackdir(trackdir));

	if (!_patches.new_pathfinding_all)
		return false;

	if (!IsTileType(tile, MP_RAILWAY))
		return false;

	if (GetRailTileType(tile) != RAIL_TYPE_SIGNALS)
		return false;

	if (!HasSignalOnTrackdir(tile, trackdir))
		return false;

	if (GetSignalType(tile, TrackdirToTrack(trackdir)) == 4)
		return true;
	else
		return false;
}

typedef struct SetSignalsDataPbs {
	int cur;

	// these are used to keep track of the signals.
	byte bit[NUM_SSD_ENTRY];
	TileIndex tile[NUM_SSD_ENTRY];
} SetSignalsDataPbs;

// This function stores the signals inside the SetSignalsDataPbs struct, passed as callback to FollowTrack() in the PBSIsPbsSegment() function below
static bool SetSignalsEnumProcPBS(uint tile, SetSignalsDataPbs *ssd, int trackdir, uint length, byte *state)
{
	// the tile has signals?
	if (IsTileType(tile, MP_RAILWAY)) {
		if (HasSignalOnTrack(tile, TrackdirToTrack(trackdir))) {

				if (ssd->cur != NUM_SSD_ENTRY) {
					ssd->tile[ssd->cur] = tile; // remember the tile index
					ssd->bit[ssd->cur] = TrackdirToTrack(trackdir); // and the controlling bit number
					ssd->cur++;
				}
				return true;
		} else if (IsTileDepotType(tile, TRANSPORT_RAIL))
			return true; // don't look further if the tile is a depot
	}
	return false;
}

bool PBSIsPbsSegment(uint tile, Trackdir trackdir)
{
	SetSignalsDataPbs ssd;
	bool result = PBSIsPbsSignal(tile, trackdir);
	DiagDirection direction = TrackdirToExitdir(trackdir);//GetDepotDirection(tile,TRANSPORT_RAIL);
	int i;

	ssd.cur = 0;

	FollowTrack(tile, 0xC000 | TRANSPORT_RAIL, direction, (TPFEnumProc*)SetSignalsEnumProcPBS, NULL, &ssd);
	for(i=0; i!=ssd.cur; i++) {
		uint tile = ssd.tile[i];
		byte bit = ssd.bit[i];
		if (!PBSIsPbsSignal(tile, bit) && !PBSIsPbsSignal(tile, bit | 8))
			return false;
		result = true;
	}

	return result;
}
