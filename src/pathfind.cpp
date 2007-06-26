/* $Id$ */

/** @file pathfind.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "station_map.h"
#include "depot.h"
#include "functions.h"
#include "landscape.h"
#include "map.h"
#include "tile.h"
#include "pathfind.h"
#include "rail.h"
#include "debug.h"
#include "tunnel_map.h"
#include "variables.h"
#include "depot.h"

/* remember which tiles we have already visited so we don't visit them again. */
static bool TPFSetTileBit(TrackPathFinder *tpf, TileIndex tile, int dir)
{
	uint hash, val, offs;
	TrackPathFinderLink *link, *new_link;
	uint bits = 1 << dir;

	if (tpf->disable_tile_hash)
		return true;

	hash = PATHFIND_HASH_TILE(tile);

	val = tpf->hash_head[hash];

	if (val == 0) {
		/* unused hash entry, set the appropriate bit in it and return true
		 * to indicate that a bit was set. */
		tpf->hash_head[hash] = bits;
		tpf->hash_tile[hash] = tile;
		return true;
	} else if (!(val & 0x8000)) {
		/* single tile */

		if (tile == tpf->hash_tile[hash]) {
			/* found another bit for the same tile,
			 * check if this bit is already set, if so, return false */
			if (val & bits)
				return false;

			/* otherwise set the bit and return true to indicate that the bit
			 * was set */
			tpf->hash_head[hash] = val | bits;
			return true;
		} else {
			/* two tiles with the same hash, need to make a link */

			/* allocate a link. if out of links, handle this by returning
			 * that a tile was already visisted. */
			if (tpf->num_links_left == 0) {
				return false;
			}
			tpf->num_links_left--;
			link = tpf->new_link++;

			/* move the data that was previously in the hash_??? variables
			 * to the link struct, and let the hash variables point to the link */
			link->tile = tpf->hash_tile[hash];
			tpf->hash_tile[hash] = PATHFIND_GET_LINK_OFFS(tpf, link);

			link->flags = tpf->hash_head[hash];
			tpf->hash_head[hash] = 0xFFFF; // multi link

			link->next = 0xFFFF;
		}
	} else {
		/* a linked list of many tiles,
		 * find the one corresponding to the tile, if it exists.
		 * otherwise make a new link */

		offs = tpf->hash_tile[hash];
		do {
			link = PATHFIND_GET_LINK_PTR(tpf, offs);
			if (tile == link->tile) {
				/* found the tile in the link list,
				 * check if the bit was alrady set, if so return false to indicate that the
				 * bit was already set */
				if (link->flags & bits)
					return false;
				link->flags |= bits;
				return true;
			}
		} while ((offs=link->next) != 0xFFFF);
	}

	/* get here if we need to add a new link to link,
	 * first, allocate a new link, in the same way as before */
	if (tpf->num_links_left == 0) {
			return false;
	}
	tpf->num_links_left--;
	new_link = tpf->new_link++;

	/* then fill the link with the new info, and establish a ptr from the old
	 * link to the new one */
	new_link->tile = tile;
	new_link->flags = bits;
	new_link->next = 0xFFFF;

	link->next = PATHFIND_GET_LINK_OFFS(tpf, new_link);
	return true;
}

static const byte _bits_mask[4] = {
	0x19,
	0x16,
	0x25,
	0x2A,
};

static const DiagDirection _tpf_new_direction[14] = {
	DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE,
	INVALID_DIAGDIR, INVALID_DIAGDIR,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE,
};

static const DiagDirection _tpf_prev_direction[14] = {
	DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW,
	INVALID_DIAGDIR, INVALID_DIAGDIR,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE, DIAGDIR_NW,
};


static const byte _otherdir_mask[4] = {
	0x10,
	0,
	0x5,
	0x2A,
};

static void TPFMode2(TrackPathFinder* tpf, TileIndex tile, DiagDirection direction)
{
	uint bits;
	int i;
	RememberData rd;

	assert(tpf->tracktype == TRANSPORT_WATER);

	/* This addition will sometimes overflow by a single tile.
	 * The use of TILE_MASK here makes sure that we still point at a valid
	 * tile, and then this tile will be in the sentinel row/col, so GetTileTrackStatus will fail. */
	tile = TILE_MASK(tile + TileOffsByDiagDir(direction));

	if (++tpf->rd.cur_length > 50)
		return;

	bits = GetTileTrackStatus(tile, tpf->tracktype, tpf->sub_type);
	bits = (byte)((bits | (bits >> 8)) & _bits_mask[direction]);
	if (bits == 0)
		return;

	assert(TileX(tile) != MapMaxX() && TileY(tile) != MapMaxY());

	if ( (bits & (bits - 1)) == 0 ) {
		/* only one direction */
		i = 0;
		while (!(bits & 1))
			i++, bits >>= 1;

		rd = tpf->rd;
		goto continue_here;
	}
	/* several directions */
	i=0;
	do {
		if (!(bits & 1)) continue;
		rd = tpf->rd;

		/* Change direction 4 times only */
		if ((byte)i != tpf->rd.pft_var6) {
			if (++tpf->rd.depth > 4) {
				tpf->rd = rd;
				return;
			}
			tpf->rd.pft_var6 = (byte)i;
		}

continue_here:;
		tpf->the_dir = (Trackdir)(i + (HASBIT(_otherdir_mask[direction], i) ? 8 : 0));

		if (!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length, NULL)) {
			TPFMode2(tpf, tile, _tpf_new_direction[tpf->the_dir]);
		}

		tpf->rd = rd;
	} while (++i, bits >>= 1);

}


/* Returns the end tile and the length of a tunnel. The length does not
 * include the starting tile (entry), it does include the end tile (exit).
 */
FindLengthOfTunnelResult FindLengthOfTunnel(TileIndex tile, DiagDirection dir)
{
	TileIndexDiff delta = TileOffsByDiagDir(dir);
	uint z = GetTileZ(tile);
	FindLengthOfTunnelResult flotr;

	flotr.length = 0;

	dir = ReverseDiagDir(dir);
	do {
		flotr.length++;
		tile += delta;
	} while(
		!IsTunnelTile(tile) ||
		GetTunnelDirection(tile) != dir ||
		GetTileZ(tile) != z
	);

	flotr.tile = tile;
	return flotr;
}

static const uint16 _tpfmode1_and[4] = { 0x1009, 0x16, 0x520, 0x2A00 };

static uint SkipToEndOfTunnel(TrackPathFinder* tpf, TileIndex tile, DiagDirection direction)
{
	FindLengthOfTunnelResult flotr;
	TPFSetTileBit(tpf, tile, 14);
	flotr = FindLengthOfTunnel(tile, direction);
	tpf->rd.cur_length += flotr.length;
	TPFSetTileBit(tpf, flotr.tile, 14);
	return flotr.tile;
}

const byte _ffb_64[128] = {
 0,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,
 4,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,
 5,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,
 4,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,

 0,  0,  0,  2,  0,  4,  4,  6,
 0,  8,  8, 10,  8, 12, 12, 14,
 0, 16, 16, 18, 16, 20, 20, 22,
16, 24, 24, 26, 24, 28, 28, 30,
 0, 32, 32, 34, 32, 36, 36, 38,
32, 40, 40, 42, 40, 44, 44, 46,
32, 48, 48, 50, 48, 52, 52, 54,
48, 56, 56, 58, 56, 60, 60, 62,
};

static void TPFMode1(TrackPathFinder* tpf, TileIndex tile, DiagDirection direction);

/** Most code of the "Normal" case of TPF Mode 1; for signals special tricks
 * have to be done, but those happen in TPFMode1; this is just to prevent
 * gotos ;). */
static inline void TPFMode1_NormalCase(TrackPathFinder* tpf, TileIndex tile, TileIndex tile_org, DiagDirection direction)
{
	/* Check in case of rail if the owner is the same */
	if (tpf->tracktype == TRANSPORT_RAIL) {
		/* don't enter train depot from the back */
		if (IsTileDepotType(tile, TRANSPORT_RAIL) && GetRailDepotDirection(tile) == direction) return;

		if (IsTileType(tile_org, MP_RAILWAY) || IsTileType(tile_org, MP_STATION) || IsTileType(tile_org, MP_TUNNELBRIDGE))
			if (IsTileType(tile, MP_RAILWAY) || IsTileType(tile, MP_STATION) || IsTileType(tile, MP_TUNNELBRIDGE))
				if (GetTileOwner(tile_org) != GetTileOwner(tile)) return;
	}

	/* check if the new tile can be entered from that direction */
	if (tpf->tracktype == TRANSPORT_ROAD) {
		/* road stops and depots now have a track (r4419)
		 * don't enter road stop from the back */
		if (IsStandardRoadStopTile(tile) && ReverseDiagDir(GetRoadStopDir(tile)) != direction) return;
		/* don't enter road depot from the back */
		if (IsTileDepotType(tile, TRANSPORT_ROAD) && ReverseDiagDir(GetRoadDepotDirection(tile)) != direction) return;
	}

	/* Check if the new tile is a tunnel or bridge head and that the direction
	 * and transport type match */
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (IsTunnel(tile)) {
			if (GetTunnelDirection(tile) != direction ||
					GetTunnelTransportType(tile) != tpf->tracktype) {
				return;
			}
		} else if (IsBridge(tile)) {
			if (GetBridgeRampDirection(tile) != direction ||
					GetBridgeTransportType(tile) != tpf->tracktype) {
				return;
			}
		}
	}

	tpf->rd.cur_length++;

	uint bits = GetTileTrackStatus(tile, tpf->tracktype, tpf->sub_type);

	if ((byte)bits != tpf->var2) {
		bits &= _tpfmode1_and[direction];
		bits = bits | (bits >> 8);
	}
	bits &= 0xBF;

	if (bits != 0) {
		if (!tpf->disable_tile_hash || (tpf->rd.cur_length <= 64 && (KILL_FIRST_BIT(bits) == 0 || ++tpf->rd.depth <= 7))) {
			do {
				int i = FIND_FIRST_BIT(bits);
				bits = KILL_FIRST_BIT(bits);

				tpf->the_dir = (Trackdir)((_otherdir_mask[direction] & (byte)(1 << i)) ? (i + 8) : i);
				RememberData rd = tpf->rd;

				if (TPFSetTileBit(tpf, tile, tpf->the_dir) &&
						!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length, &tpf->rd.pft_var6) ) {
					TPFMode1(tpf, tile, _tpf_new_direction[tpf->the_dir]);
				}
				tpf->rd = rd;
			} while (bits != 0);
		}
	}
}

static void TPFMode1(TrackPathFinder* tpf, TileIndex tile, DiagDirection direction)
{
	TileIndex tile_org = tile;

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (IsTunnel(tile)) {
			if (GetTunnelDirection(tile) != direction ||
					GetTunnelTransportType(tile) != tpf->tracktype) {
				return;
			}
			tile = SkipToEndOfTunnel(tpf, tile, direction);
		} else {
			TileIndex tile_end;
			if (GetBridgeRampDirection(tile) != direction ||
					GetBridgeTransportType(tile) != tpf->tracktype) {
				return;
			}
			//fprintf(stderr, "%s: Planning over bridge\n", __func__);
			// TODO doesn't work - WHAT doesn't work?
			TPFSetTileBit(tpf, tile, 14);
			tile_end = GetOtherBridgeEnd(tile);
			tpf->rd.cur_length += DistanceManhattan(tile, tile_end);
			tile = tile_end;
			TPFSetTileBit(tpf, tile, 14);
		}
	}
	tile += TileOffsByDiagDir(direction);

	TPFMode1_NormalCase(tpf, tile, tile_org, direction);

	/* the next is only used when signals are checked.
	 * seems to go in 2 directions simultaneously */

	/* if i can get rid of this, tail end recursion can be used to minimize
	 * stack space dramatically. */

	/* If we are doing signal setting, we must reverse at evere tile, so we
	 * iterate all the tracks in a signal block, even when a normal train would
	 * not reach it (for example, when two lines merge */
	if (tpf->hasbit_13)
		return;

	direction = ReverseDiagDir(direction);
	tile += TileOffsByDiagDir(direction);

	uint bits = GetTileTrackStatus(tile, tpf->tracktype, tpf->sub_type);
	bits |= (bits >> 8);

	if ( (byte)bits != tpf->var2) {
		bits &= _bits_mask[direction];
	}

	bits &= 0xBF;
	if (bits == 0)
		return;

	do {
		uint i = FIND_FIRST_BIT(bits);
		bits = KILL_FIRST_BIT(bits);

		tpf->the_dir = (Trackdir)((_otherdir_mask[direction] & (byte)(1 << i)) ? (i + 8) : i);
		RememberData rd = tpf->rd;
		if (TPFSetTileBit(tpf, tile, tpf->the_dir) &&
				!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length, &tpf->rd.pft_var6) ) {
			TPFMode1(tpf, tile, _tpf_new_direction[tpf->the_dir]);
		}
		tpf->rd = rd;
	} while (bits != 0);
}

void FollowTrack(TileIndex tile, uint16 flags, uint sub_type, DiagDirection direction, TPFEnumProc *enum_proc, TPFAfterProc *after_proc, void *data)
{
	TrackPathFinder tpf;

	assert(direction < 4);

	/* initialize path finder variables */
	tpf.userdata = data;
	tpf.enum_proc = enum_proc;
	tpf.new_link = tpf.links;
	tpf.num_links_left = lengthof(tpf.links);

	tpf.rd.cur_length = 0;
	tpf.rd.depth = 0;
	tpf.rd.pft_var6 = 0;

	tpf.var2 = HASBIT(flags, 15) ? 0x43 : 0xFF; // 0x8000

	tpf.disable_tile_hash = HASBIT(flags, 12);  // 0x1000
	tpf.hasbit_13         = HASBIT(flags, 13);  // 0x2000


	tpf.tracktype = (TransportType)(flags & 0xFF);
	tpf.sub_type = sub_type;

	if (HASBIT(flags, 11)) {
		tpf.rd.pft_var6 = 0xFF;
		tpf.enum_proc(tile, data, INVALID_TRACKDIR, 0, 0);
		TPFMode2(&tpf, tile, direction);
	} else {
		/* clear the hash_heads */
		memset(tpf.hash_head, 0, sizeof(tpf.hash_head));
		TPFMode1(&tpf, tile, direction);
	}

	if (after_proc != NULL)
		after_proc(&tpf);
}

struct StackedItem {
	TileIndex tile;
	uint16 cur_length; ///< This is the current length to this tile.
	uint16 priority;   ///< This is the current length + estimated length to the goal.
	TrackdirByte track;
	byte depth;
	byte state;
	byte first_track;
};

static const Trackdir _new_trackdir[6][4] = {
{TRACKDIR_X_NE,    INVALID_TRACKDIR, TRACKDIR_X_SW,    INVALID_TRACKDIR,},
{INVALID_TRACKDIR, TRACKDIR_Y_SE,    INVALID_TRACKDIR, TRACKDIR_Y_NW,},
{INVALID_TRACKDIR, TRACKDIR_UPPER_E, TRACKDIR_UPPER_W, INVALID_TRACKDIR,},
{TRACKDIR_LOWER_E, INVALID_TRACKDIR, INVALID_TRACKDIR, TRACKDIR_LOWER_W,},
{TRACKDIR_LEFT_N,  TRACKDIR_LEFT_S,  INVALID_TRACKDIR, INVALID_TRACKDIR,},
{INVALID_TRACKDIR, INVALID_TRACKDIR, TRACKDIR_RIGHT_S, TRACKDIR_RIGHT_N,},
};

struct HashLink {
	TileIndex tile;
	uint16 typelength;
	uint16 next;
};

struct NewTrackPathFinder {
	NTPEnumProc *enum_proc;
	void *userdata;
	TileIndex dest;

	TransportType tracktype;
	RailTypeMask railtypes;
	uint maxlength;

	HashLink *new_link;
	uint num_links_left;

	uint nstack;
	StackedItem stack[256];     ///< priority queue of stacked items

	uint16 hash_head[0x400];    ///< hash heads. 0 means unused. 0xFFFC = length, 0x3 = dir
	TileIndex hash_tile[0x400]; ///< tiles. or links.

	HashLink links[0x400];      ///< hash links

};
#define NTP_GET_LINK_OFFS(tpf, link) ((byte*)(link) - (byte*)tpf->links)
#define NTP_GET_LINK_PTR(tpf, link_offs) (HashLink*)((byte*)tpf->links + (link_offs))

#define ARR(i) tpf->stack[(i)-1]

/** called after a new element was added in the queue at the last index.
 * move it down to the proper position */
static inline void HeapifyUp(NewTrackPathFinder *tpf)
{
	StackedItem si;
	int i = ++tpf->nstack;

	while (i != 1 && ARR(i).priority < ARR(i>>1).priority) {
		/* the child element is larger than the parent item.
		 * swap the child item and the parent item. */
		si = ARR(i); ARR(i) = ARR(i >> 1); ARR(i >> 1) = si;
		i >>= 1;
	}
}

/** called after the element 0 was eaten. fill it with a new element */
static inline void HeapifyDown(NewTrackPathFinder *tpf)
{
	StackedItem si;
	int i = 1, j;
	int n;

	assert(tpf->nstack > 0);
	n = --tpf->nstack;

	if (n == 0) return; // heap is empty so nothing to do?

	/* copy the last item to index 0. we use it as base for heapify. */
	ARR(1) = ARR(n + 1);

	while ((j = i * 2) <= n) {
		/* figure out which is smaller of the children. */
		if (j != n && ARR(j).priority > ARR(j + 1).priority)
			j++; // right item is smaller

		assert(i <= n && j <= n);
		if (ARR(i).priority <= ARR(j).priority)
			break; // base elem smaller than smallest, done!

		/* swap parent with the child */
		si = ARR(i); ARR(i) = ARR(j); ARR(j) = si;
		i = j;
	}
}

/** mark a tile as visited and store the length of the path.
 * if we already had a better path to this tile, return false.
 * otherwise return true. */
static bool NtpVisit(NewTrackPathFinder* tpf, TileIndex tile, DiagDirection dir, uint length)
{
	uint hash,head;
	HashLink *link, *new_link;

	assert(length < 16384-1);

	hash = PATHFIND_HASH_TILE(tile);

	/* never visited before? */
	if ((head=tpf->hash_head[hash]) == 0) {
		tpf->hash_tile[hash] = tile;
		tpf->hash_head[hash] = dir | (length << 2);
		return true;
	}

	if (head != 0xffff) {
		if (tile == tpf->hash_tile[hash] && (head & 0x3) == (uint)dir) {

			/* longer length */
			if (length >= (head >> 2)) return false;

			tpf->hash_head[hash] = dir | (length << 2);
			return true;
		}
		/* two tiles with the same hash, need to make a link
		 * allocate a link. if out of links, handle this by returning
		 * that a tile was already visisted. */
		if (tpf->num_links_left == 0) {
			DEBUG(ntp, 1, "No links left");
			return false;
		}

		tpf->num_links_left--;
		link = tpf->new_link++;

		/* move the data that was previously in the hash_??? variables
		 * to the link struct, and let the hash variables point to the link */
		link->tile = tpf->hash_tile[hash];
		tpf->hash_tile[hash] = NTP_GET_LINK_OFFS(tpf, link);

		link->typelength = tpf->hash_head[hash];
		tpf->hash_head[hash] = 0xFFFF; // multi link
		link->next = 0xFFFF;
	} else {
		/* a linked list of many tiles,
		 * find the one corresponding to the tile, if it exists.
		 * otherwise make a new link */

		uint offs = tpf->hash_tile[hash];
		do {
			link = NTP_GET_LINK_PTR(tpf, offs);
			if (tile == link->tile && (link->typelength & 0x3U) == (uint)dir) {
				if (length >= (uint)(link->typelength >> 2)) return false;
				link->typelength = dir | (length << 2);
				return true;
			}
		} while ((offs = link->next) != 0xFFFF);
	}

	/* get here if we need to add a new link to link,
	 * first, allocate a new link, in the same way as before */
	if (tpf->num_links_left == 0) {
		DEBUG(ntp, 1, "No links left");
		return false;
	}
	tpf->num_links_left--;
	new_link = tpf->new_link++;

	/* then fill the link with the new info, and establish a ptr from the old
	 * link to the new one */
	new_link->tile = tile;
	new_link->typelength = dir | (length << 2);
	new_link->next = 0xFFFF;

	link->next = NTP_GET_LINK_OFFS(tpf, new_link);
	return true;
}

/**
 * Checks if the shortest path to the given tile/dir so far is still the given
 * length.
 * @return true if the length is still the same
 * @pre    The given tile/dir combination should be present in the hash, by a
 *         previous call to NtpVisit().
 */
static bool NtpCheck(NewTrackPathFinder *tpf, TileIndex tile, uint dir, uint length)
{
	uint hash,head,offs;
	HashLink *link;

	hash = PATHFIND_HASH_TILE(tile);
	head=tpf->hash_head[hash];
	assert(head);

	if (head != 0xffff) {
		assert( tpf->hash_tile[hash] == tile && (head & 3) == dir);
		assert( (head >> 2) <= length);
		return length == (head >> 2);
	}

	/* else it's a linked list of many tiles */
	offs = tpf->hash_tile[hash];
	for (;;) {
		link = NTP_GET_LINK_PTR(tpf, offs);
		if (tile == link->tile && (link->typelength & 0x3U) == dir) {
			assert((uint)(link->typelength >> 2) <= length);
			return length == (uint)(link->typelength >> 2);
		}
		offs = link->next;
		assert(offs != 0xffff);
	}
}


static const uint16 _is_upwards_slope[15] = {
	0,                                           ///< no tileh
	(1 << TRACKDIR_X_SW) | (1 << TRACKDIR_Y_NW), ///< 1
	(1 << TRACKDIR_X_SW) | (1 << TRACKDIR_Y_SE), ///< 2
	(1 << TRACKDIR_X_SW),                        ///< 3
	(1 << TRACKDIR_X_NE) | (1 << TRACKDIR_Y_SE), ///< 4
	0,                                           ///< 5
	(1 << TRACKDIR_Y_SE),                        ///< 6
	0,                                           ///< 7
	(1 << TRACKDIR_X_NE) | (1 << TRACKDIR_Y_NW), ///< 8,
	(1 << TRACKDIR_Y_NW),                        ///< 9
	0,                                           ///< 10
	0,                                           ///< 11,
	(1 << TRACKDIR_X_NE),                        ///< 12
	0,                                           ///< 13
	0,                                           ///< 14
};

static uint DistanceMoo(TileIndex t0, TileIndex t1)
{
	const uint dx = delta(TileX(t0), TileX(t1));
	const uint dy = delta(TileY(t0), TileY(t1));

	const uint straightTracks = 2 * min(dx, dy); // The number of straight (not full length) tracks
	/* OPTIMISATION:
	 * Original: diagTracks = max(dx, dy) - min(dx,dy);
	 * Proof:
	 * (dx-dy) - straightTracks  == (min + max) - straightTracks = min + // max - 2 * min = max - min */
	const uint diagTracks = dx + dy - straightTracks; // The number of diagonal (full tile length) tracks.

	return diagTracks*DIAG_FACTOR + straightTracks*STR_FACTOR;
}

/* These has to be small cause the max length of a track
 * is currently limited to 16384 */

static const byte _length_of_track[16] = {
	DIAG_FACTOR, DIAG_FACTOR, STR_FACTOR, STR_FACTOR, STR_FACTOR, STR_FACTOR, 0, 0,
	DIAG_FACTOR, DIAG_FACTOR, STR_FACTOR, STR_FACTOR, STR_FACTOR, STR_FACTOR, 0, 0
};

/* new more optimized pathfinder for trains...
 * Tile is the tile the train is at.
 * direction is the tile the train is moving towards. */

static void NTPEnum(NewTrackPathFinder* tpf, TileIndex tile, DiagDirection direction)
{
	TrackBits bits, allbits;
	Trackdir track;
	TileIndex tile_org;
	StackedItem si;
	int estimation;



	/* Need to have a special case for the start.
	 * We shouldn't call the callback for the current tile. */
	si.cur_length = 1; // Need to start at 1 cause 0 is a reserved value.
	si.depth = 0;
	si.state = 0;
	si.first_track = 0xFF;
	goto start_at;

	for (;;) {
		/* Get the next item to search from from the priority queue */
		do {
			if (tpf->nstack == 0)
				return; // nothing left? then we're done!
			si = tpf->stack[0];
			tile = si.tile;

			HeapifyDown(tpf);
			/* Make sure we havn't already visited this tile. */
		} while (!NtpCheck(tpf, tile, _tpf_prev_direction[si.track], si.cur_length));

		/* Add the length of this track. */
		si.cur_length += _length_of_track[si.track];

callback_and_continue:
		if (tpf->enum_proc(tile, tpf->userdata, si.first_track, si.cur_length))
			return;

		assert(si.track <= 13);
		direction = _tpf_new_direction[si.track];

start_at:
		/* If the tile is the entry tile of a tunnel, and we're not going out of the tunnel,
		 *   need to find the exit of the tunnel. */
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			if (IsTunnel(tile)) {
				if (GetTunnelDirection(tile) != ReverseDiagDir(direction)) {
					FindLengthOfTunnelResult flotr;

					/* We are not just driving out of the tunnel */
					if (GetTunnelDirection(tile) != direction ||
							GetTunnelTransportType(tile) != tpf->tracktype) {
						/* We are not driving into the tunnel, or it is an invalid tunnel */
						continue;
					}
					if (!HASBIT(tpf->railtypes, GetRailType(tile))) {
						bits = TRACK_BIT_NONE;
						break;
					}
					flotr = FindLengthOfTunnel(tile, direction);
					si.cur_length += flotr.length * DIAG_FACTOR;
					tile = flotr.tile;
					/* tile now points to the exit tile of the tunnel */
				}
			} else {
				TileIndex tile_end;
				if (GetBridgeRampDirection(tile) != ReverseDiagDir(direction)) {
					/* We are not just leaving the bridge */
					if (GetBridgeRampDirection(tile) != direction ||
							GetBridgeTransportType(tile) != tpf->tracktype) {
						/* Not entering the bridge or not compatible */
						continue;
					}
				}
				tile_end = GetOtherBridgeEnd(tile);
				si.cur_length += DistanceManhattan(tile, tile_end) * DIAG_FACTOR;
				tile = tile_end;
			}
		}

		/* This is a special loop used to go through
		 * a rail net and find the first intersection */
		tile_org = tile;
		for (;;) {
			assert(direction <= 3);
			tile += TileOffsByDiagDir(direction);

			/* too long search length? bail out. */
			if (si.cur_length >= tpf->maxlength) {
				DEBUG(ntp, 1, "Cur_length too big");
				bits = TRACK_BIT_NONE;
				break;
			}

			/* Not a regular rail tile?
			 * Then we can't use the code below, but revert to more general code. */
			if (!IsTileType(tile, MP_RAILWAY) || !IsPlainRailTile(tile)) {
				/* We found a tile which is not a normal railway tile.
				 * Determine which tracks that exist on this tile. */
				uint32 ts = GetTileTrackStatus(tile, TRANSPORT_RAIL, 0) & _tpfmode1_and[direction];
				bits = TrackdirBitsToTrackBits((TrackdirBits)(ts & TRACKDIR_BIT_MASK));

				/* Check that the tile contains exactly one track */
				if (bits == 0 || KILL_FIRST_BIT(bits) != 0) break;

				if (!HASBIT(tpf->railtypes, GetRailType(tile))) {
					bits = TRACK_BIT_NONE;
					break;
				}

				/*******************
				 * If we reach here, the tile has exactly one track.
				 *   tile - index to a tile that is not rail tile, but still straight (with optional signals)
				 *   bits - bitmask of which track that exist on the tile (exactly one bit is set)
				 *   direction - which direction are we moving in?
				 *******************/
				si.track = _new_trackdir[FIND_FIRST_BIT(bits)][direction];
				si.cur_length += _length_of_track[si.track];
				goto callback_and_continue;
			}

			/* Regular rail tile, determine which tracks exist. */
			allbits = GetTrackBits(tile);
			/* Which tracks are reachable? */
			bits = allbits & DiagdirReachesTracks(direction);

			/* The tile has no reachable tracks => End of rail segment
			 * or Intersection => End of rail segment. We check this agains all the
			 * bits, not just reachable ones, to prevent infinite loops. */
			if (bits == TRACK_BIT_NONE || TracksOverlap(allbits)) break;

			if (!HASBIT(tpf->railtypes, GetRailType(tile))) {
				bits = TRACK_BIT_NONE;
				break;
			}

			/* If we reach here, the tile has exactly one track, and this
			 track is reachable = > Rail segment continues */

			track = _new_trackdir[FIND_FIRST_BIT(bits)][direction];
			assert(track != INVALID_TRACKDIR);

			si.cur_length += _length_of_track[track];

			/* Check if this rail is an upwards slope. If it is, then add a penalty.
			 * Small optimization here.. if (track&7)>1 then it can't be a slope so we avoid calling GetTileSlope */
			if ((track & 7) <= 1 && (_is_upwards_slope[GetTileSlope(tile, NULL)] & (1 << track)) ) {
				// upwards slope. add some penalty.
				si.cur_length += 4 * DIAG_FACTOR;
			}

			/* railway tile with signals..? */
			if (HasSignals(tile)) {
				if (!HasSignalOnTrackdir(tile, track)) {
					/* if one way signal not pointing towards us, stop going in this direction => End of rail segment. */
					if (HasSignalOnTrackdir(tile, ReverseTrackdir(track))) {
						bits = TRACK_BIT_NONE;
						break;
					}
				} else if (GetSignalStateByTrackdir(tile, track) == SIGNAL_STATE_GREEN) {
					/* green signal in our direction. either one way or two way. */
					si.state |= 3;
				} else {
					/* reached a red signal. */
					if (HasSignalOnTrackdir(tile, ReverseTrackdir(track))) {
						/* two way red signal. unless we passed another green signal on the way,
						 * stop going in this direction => End of rail segment.
						 * this is to prevent us from going into a full platform. */
						if (!(si.state & 1)) {
							bits = TRACK_BIT_NONE;
							break;
						}
					}
					if (!(si.state & 2)) {
						/* Is this the first signal we see? And it's red... add penalty */
						si.cur_length += 10 * DIAG_FACTOR;
						si.state += 2; // remember that we added penalty.
						/* Because we added a penalty, we can't just continue as usual.
						 * Need to get out and let A* do it's job with
						 * possibly finding an even shorter path. */
						break;
					}
				}

				if (tpf->enum_proc(tile, tpf->userdata, si.first_track, si.cur_length))
					return; // Don't process this tile any further
			}

			/* continue with the next track */
			direction = _tpf_new_direction[track];

			/* safety check if we're running around chasing our tail... (infinite loop) */
			if (tile == tile_org) {
				bits = TRACK_BIT_NONE;
				break;
			}
		}

		/* There are no tracks to choose between.
		 * Stop searching in this direction */
		if (bits == TRACK_BIT_NONE)
			continue;

		/****************
		 * We got multiple tracks to choose between (intersection).
		 * Branch the search space into several branches.
		 ****************/

		/* Check if we've already visited this intersection.
		 * If we've already visited it with a better length, then
		 * there's no point in visiting it again. */
		if (!NtpVisit(tpf, tile, direction, si.cur_length))
			continue;

		/* Push all possible alternatives that we can reach from here
		 * onto the priority heap.
		 * 'bits' contains the tracks that we can choose between. */

		/* First compute the estimated distance to the target.
		 * This is used to implement A* */
		estimation = 0;
		if (tpf->dest != 0)
			estimation = DistanceMoo(tile, tpf->dest);

		si.depth++;
		if (si.depth == 0)
			continue; // We overflowed our depth. No more searching in this direction.
		si.tile = tile;
		while (bits != TRACK_BIT_NONE) {
			Track track = RemoveFirstTrack(&bits);
			si.track = _new_trackdir[track][direction];
			assert(si.track != 0xFF);
			si.priority = si.cur_length + estimation;

			/* out of stack items, bail out? */
			if (tpf->nstack >= lengthof(tpf->stack)) {
				DEBUG(ntp, 1, "Out of stack");
				break;
			}

			tpf->stack[tpf->nstack] = si;
			HeapifyUp(tpf);
		};

		/* If this is the first intersection, we need to fill the first_track member.
		 * so the code outside knows which path is better.
		 * also randomize the order in which we search through them. */
		if (si.depth == 1) {
			assert(tpf->nstack == 1 || tpf->nstack == 2 || tpf->nstack == 3);
			if (tpf->nstack != 1) {
				uint32 r = Random();
				if (r & 1) Swap(tpf->stack[0].track, tpf->stack[1].track);
				if (tpf->nstack != 2) {
					TrackdirByte t = tpf->stack[2].track;
					if (r & 2) Swap(tpf->stack[0].track, t);
					if (r & 4) Swap(tpf->stack[1].track, t);
					tpf->stack[2].first_track = tpf->stack[2].track = t;
				}
				tpf->stack[0].first_track = tpf->stack[0].track;
				tpf->stack[1].first_track = tpf->stack[1].track;
			}
		}

		/* Continue with the next from the queue... */
	}
}


/** new pathfinder for trains. better and faster. */
void NewTrainPathfind(TileIndex tile, TileIndex dest, RailTypeMask railtypes, DiagDirection direction, NTPEnumProc* enum_proc, void* data)
{
	NewTrackPathFinder tpf;

	tpf.dest = dest;
	tpf.userdata = data;
	tpf.enum_proc = enum_proc;
	tpf.tracktype = TRANSPORT_RAIL;
	tpf.railtypes = railtypes;
	tpf.maxlength = min(_patches.pf_maxlength * 3, 10000);
	tpf.nstack = 0;
	tpf.new_link = tpf.links;
	tpf.num_links_left = lengthof(tpf.links);
	memset(tpf.hash_head, 0, sizeof(tpf.hash_head));

	NTPEnum(&tpf, tile, direction);
}
