/* $Id$ */

/** @file pathfind.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "station_map.h"
#include "depot.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "pathfind.h"
#include "rail_type.h"
#include "debug.h"
#include "tunnel_map.h"
#include "settings_type.h"
#include "depot.h"
#include "tunnelbridge_map.h"
#include "core/random_func.hpp"
#include "core/alloc_type.hpp"
#include "tunnelbridge.h"

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

static void TPFModeShip(TrackPathFinder* tpf, TileIndex tile, DiagDirection direction)
{
	RememberData rd;

	assert(tpf->tracktype == TRANSPORT_WATER);

	/* This addition will sometimes overflow by a single tile.
	 * The use of TILE_MASK here makes sure that we still point at a valid
	 * tile, and then this tile will be in the sentinel row/col, so GetTileTrackStatus will fail. */
	tile = TILE_MASK(tile + TileOffsByDiagDir(direction));

	if (++tpf->rd.cur_length > 50)
		return;

	TrackBits bits = TrackStatusToTrackBits(GetTileTrackStatus(tile, tpf->tracktype, tpf->sub_type)) & DiagdirReachesTracks(direction);
	if (bits == TRACK_BIT_NONE) return;

	assert(TileX(tile) != MapMaxX() && TileY(tile) != MapMaxY());

	bool only_one_track = true;
	do {
		Track track = RemoveFirstTrack(&bits);
		if (bits != TRACK_BIT_NONE) only_one_track = false;
		rd = tpf->rd;

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

/**
 * Checks if any vehicle can enter/leave tile in given diagdir
 * Checks only for rail/road depots and road non-drivethrough stations
 * @param tile tile to check
 * @param side side of tile we are trying to leave/enter
 * @param tracktype type of transport
 * @pre tile has trackbit at that diagdir
 * @return true iff vehicle can enter/leve the tile in given side
 */
static inline bool CanAccessTileInDir(TileIndex tile, DiagDirection side, TransportType tracktype)
{
	if (tracktype == TRANSPORT_RAIL) {
		/* depot from wrong side */
		if (IsTileDepotType(tile, TRANSPORT_RAIL) && GetRailDepotDirection(tile) != side) return false;
	} else if (tracktype == TRANSPORT_ROAD) {
		/* depot from wrong side */
		if (IsTileDepotType(tile, TRANSPORT_ROAD) && GetRoadDepotDirection(tile) != side) return false;
		/* non-driverthrough road station from wrong side */
		if (IsStandardRoadStopTile(tile) && GetRoadStopDir(tile) != side) return false;
	}

	return true;
}

static void TPFModeNormal(TrackPathFinder* tpf, TileIndex tile, DiagDirection direction)
{
	const TileIndex tile_org = tile;

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		/* wrong track type */
		if (GetTunnelBridgeTransportType(tile) != tpf->tracktype) return;

		DiagDirection dir = GetTunnelBridgeDirection(tile);
		/* entering tunnel / bridge? */
		if (dir == direction) {
			TileIndex endtile = GetOtherTunnelBridgeEnd(tile);

			tpf->rd.cur_length += GetTunnelBridgeLength(tile, endtile) + 1;

			TPFSetTileBit(tpf, tile, 14);
			TPFSetTileBit(tpf, endtile, 14);

			tile = endtile;
		} else {
			/* leaving tunnel / bridge? */
			if (ReverseDiagDir(dir) != direction) return;
		}
	} else {
		/* can we leave tile in this dir? */
		if (!CanAccessTileInDir(tile, direction, tpf->tracktype)) return;
	}

	tile += TileOffsByDiagDir(direction);

	/* can we enter tile in this dir? */
	if (!CanAccessTileInDir(tile, ReverseDiagDir(direction), tpf->tracktype)) return;

	/* Check if the new tile is a tunnel or bridge head and that the direction
	 * and transport type match */
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (GetTunnelBridgeDirection(tile) != direction ||
				GetTunnelBridgeTransportType(tile) != tpf->tracktype) {
			return;
		}
	}

	TrackdirBits trackdirbits = TrackStatusToTrackdirBits(GetTileTrackStatus(tile, tpf->tracktype, tpf->sub_type));

	/* Check in case of rail if the owner is the same */
	if (tpf->tracktype == TRANSPORT_RAIL) {
		if (trackdirbits != TRACKDIR_BIT_NONE && TrackStatusToTrackdirBits(GetTileTrackStatus(tile_org, TRANSPORT_RAIL, 0)) != TRACKDIR_BIT_NONE) {
			if (GetTileOwner(tile_org) != GetTileOwner(tile)) return;
		}
	}

	tpf->rd.cur_length++;

	trackdirbits &= DiagdirReachesTrackdirs(direction);
	TrackBits bits = TrackdirBitsToTrackBits(trackdirbits);

	if (bits != TRACK_BIT_NONE) {
		if (!tpf->disable_tile_hash || (tpf->rd.cur_length <= 64 && (KillFirstBit(bits) == 0 || ++tpf->rd.depth <= 7))) {
			do {
				Track track = RemoveFirstTrack(&bits);

				tpf->the_dir = TrackEnterdirToTrackdir(track, direction);
				RememberData rd = tpf->rd;

				/* make sure we are not leaving from invalid side */
				if (TPFSetTileBit(tpf, tile, tpf->the_dir) && CanAccessTileInDir(tile, TrackdirToExitdir(tpf->the_dir), tpf->tracktype) &&
						!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length) ) {
					TPFModeNormal(tpf, tile, TrackdirToExitdir(tpf->the_dir));
				}
				tpf->rd = rd;
			} while (bits != TRACK_BIT_NONE);
		}
	}
}

void FollowTrack(TileIndex tile, PathfindFlags flags, TransportType tt, uint sub_type, DiagDirection direction, TPFEnumProc *enum_proc, TPFAfterProc *after_proc, void *data)
{
	assert(IsValidDiagDirection(direction));

	SmallStackSafeStackAlloc<TrackPathFinder, 1> tpf;

	/* initialize path finder variables */
	tpf->userdata = data;
	tpf->enum_proc = enum_proc;
	tpf->new_link = tpf->links;
	tpf->num_links_left = lengthof(tpf->links);

	tpf->rd.cur_length = 0;
	tpf->rd.depth = 0;
	tpf->rd.last_choosen_track = INVALID_TRACK;

	tpf->disable_tile_hash = (flags & PATHFIND_FLAGS_DISABLE_TILE_HASH) != 0;

	tpf->tracktype = tt;
	tpf->sub_type = sub_type;

	if ((flags & PATHFIND_FLAGS_SHIP_MODE) != 0) {
		tpf->enum_proc(tile, data, INVALID_TRACKDIR, 0);
		TPFModeShip(tpf, tile, direction);
	} else {
		/* clear the hash_heads */
		memset(tpf->hash_head, 0, sizeof(tpf->hash_head));
		TPFModeNormal(tpf, tile, direction);
	}

	if (after_proc != NULL) after_proc(tpf);
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
	RailTypes railtypes;
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


static uint DistanceMoo(TileIndex t0, TileIndex t1)
{
	const uint dx = Delta(TileX(t0), TileX(t1));
	const uint dy = Delta(TileY(t0), TileY(t1));

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
		} while (!NtpCheck(tpf, tile, ReverseDiagDir(TrackdirToExitdir(ReverseTrackdir(si.track))), si.cur_length));

		/* Add the length of this track. */
		si.cur_length += _length_of_track[si.track];

callback_and_continue:
		if (tpf->enum_proc(tile, tpf->userdata, si.first_track, si.cur_length))
			return;

		assert(si.track <= 13);
		direction = TrackdirToExitdir(si.track);

start_at:
		/* If the tile is the entry tile of a tunnel, and we're not going out of the tunnel,
		 *   need to find the exit of the tunnel. */
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			if (GetTunnelBridgeDirection(tile) != ReverseDiagDir(direction)) {
				/* We are not just driving out of the tunnel/bridge */
				if (GetTunnelBridgeDirection(tile) != direction ||
						GetTunnelBridgeTransportType(tile) != tpf->tracktype) {
					/* We are not driving into the tunnel/bridge, or it is an invalid tunnel/bridge */
					continue;
				}
				if (!HasBit(tpf->railtypes, GetRailType(tile))) {
					bits = TRACK_BIT_NONE;
					break;
				}

				TileIndex endtile = GetOtherTunnelBridgeEnd(tile);
				si.cur_length += DIAG_FACTOR * (GetTunnelBridgeLength(tile, endtile) + 1);
				tile = endtile;
				/* tile now points to the exit tile of the tunnel/bridge */
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
				bits = TrackdirBitsToTrackBits(TrackStatusToTrackdirBits(GetTileTrackStatus(tile, TRANSPORT_RAIL, 0)) & DiagdirReachesTrackdirs(direction));

				/* Check that the tile contains exactly one track */
				if (bits == 0 || KillFirstBit(bits) != 0) break;

				if (!HasBit(tpf->railtypes, GetRailType(tile))) {
					bits = TRACK_BIT_NONE;
					break;
				}

				/*******************
				 * If we reach here, the tile has exactly one track.
				 *   tile - index to a tile that is not rail tile, but still straight (with optional signals)
				 *   bits - bitmask of which track that exist on the tile (exactly one bit is set)
				 *   direction - which direction are we moving in?
				 *******************/
				si.track = TrackEnterdirToTrackdir(FindFirstTrack(bits), direction);
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

			if (!HasBit(tpf->railtypes, GetRailType(tile))) {
				bits = TRACK_BIT_NONE;
				break;
			}

			/* If we reach here, the tile has exactly one track, and this
			 track is reachable = > Rail segment continues */

			track = TrackEnterdirToTrackdir(FindFirstTrack(bits), direction);
			assert(track != INVALID_TRACKDIR);

			si.cur_length += _length_of_track[track];

			/* Check if this rail is an upwards slope. If it is, then add a penalty. */
			if (IsDiagonalTrackdir(track) && IsUphillTrackdir(GetTileSlope(tile, NULL), track)) {
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
			direction = TrackdirToExitdir(track);

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
			si.track = TrackEnterdirToTrackdir(track, direction);
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
void NewTrainPathfind(TileIndex tile, TileIndex dest, RailTypes railtypes, DiagDirection direction, NTPEnumProc* enum_proc, void* data)
{
	SmallStackSafeStackAlloc<NewTrackPathFinder, 1> tpf;

	tpf->dest = dest;
	tpf->userdata = data;
	tpf->enum_proc = enum_proc;
	tpf->tracktype = TRANSPORT_RAIL;
	tpf->railtypes = railtypes;
	tpf->maxlength = min(_patches.pf_maxlength * 3, 10000);
	tpf->nstack = 0;
	tpf->new_link = tpf->links;
	tpf->num_links_left = lengthof(tpf->links);
	memset(tpf->hash_head, 0, sizeof(tpf->hash_head));

	NTPEnum(tpf, tile, direction);
}
