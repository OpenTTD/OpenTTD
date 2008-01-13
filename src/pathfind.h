/* $Id$ */

/** @file pathfind.h */

#ifndef PATHFIND_H
#define PATHFIND_H

#include "direction_type.h"

enum {
	STR_FACTOR  = 2,
	DIAG_FACTOR = 3
};

//#define PF_BENCH // perform simple benchmarks on the train pathfinder (not
//supported on all archs)

struct TrackPathFinder;
typedef bool TPFEnumProc(TileIndex tile, void *data, Trackdir trackdir, uint length, byte *state);
typedef void TPFAfterProc(TrackPathFinder *tpf);

typedef bool NTPEnumProc(TileIndex tile, void *data, int track, uint length);

#define PATHFIND_GET_LINK_OFFS(tpf, link) ((byte*)(link) - (byte*)tpf->links)
#define PATHFIND_GET_LINK_PTR(tpf, link_offs) (TrackPathFinderLink*)((byte*)tpf->links + (link_offs))

/* y7 y6 y5 y4 y3 y2 y1 y0 x7 x6 x5 x4 x3 x2 x1 x0
 * y7 y6 y5 y4 y3 y2 y1 y0 x4 x3 x2 x1 x0  0  0  0
 *  0  0 y7 y6 y5 y4 y3 y2 y1 y0 x4 x3 x2 x1 x0  0
 *  0  0  0  0 y5 y4 y3 y2 y1 y0 x4 x3 x2 x1 x0  0
 */
#define PATHFIND_HASH_TILE(tile) (TileX(tile) & 0x1F) + ((TileY(tile) & 0x1F) << 5)

struct TrackPathFinderLink {
	TileIndex tile;
	uint16 flags;
	uint16 next;
};

struct RememberData {
	uint16 cur_length;
	byte depth;
	byte pft_var6;
};

struct TrackPathFinder {
	int num_links_left;
	TrackPathFinderLink *new_link;

	TPFEnumProc *enum_proc;

	void *userdata;

	RememberData rd;

	TrackdirByte the_dir;

	TransportType tracktype;
	uint sub_type;

	byte var2;
	bool disable_tile_hash;
	bool hasbit_13;

	uint16 hash_head[0x400];
	TileIndex hash_tile[0x400];       ///< stores the link index when multi link.

	TrackPathFinderLink links[0x400]; ///< hopefully, this is enough.
};

void FollowTrack(TileIndex tile, uint16 flags, uint sub_type, DiagDirection direction, TPFEnumProc* enum_proc, TPFAfterProc* after_proc, void* data);

struct FindLengthOfTunnelResult {
	TileIndex tile;
	int length;
};
FindLengthOfTunnelResult FindLengthOfTunnel(TileIndex tile, DiagDirection direction);

void NewTrainPathfind(TileIndex tile, TileIndex dest, RailTypes railtypes, DiagDirection direction, NTPEnumProc* enum_proc, void* data);

#endif /* PATHFIND_H */
