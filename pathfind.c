#include "stdafx.h"
#include "ttd.h"
#include "map.h"
#include "pathfind.h"

// remember which tiles we have already visited so we don't visit them again.
static bool TPFSetTileBit(TrackPathFinder *tpf, uint tile, int dir)
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
		tpf->hash_tile[hash] = (TileIndex)tile;
		return true;
	} else if (!(val & 0x8000)) {
		/* single tile */

		if ( (TileIndex)tile == tpf->hash_tile[hash] ) {
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
			if (tpf->num_links_left == 0)
				return false;
			tpf->num_links_left--;
			link = tpf->new_link++;

			/* move the data that was previously in the hash_??? variables
			 * to the link struct, and let the hash variables point to the link */
			link->tile = tpf->hash_tile[hash];
			tpf->hash_tile[hash] = PATHFIND_GET_LINK_OFFS(tpf, link);

			link->flags = tpf->hash_head[hash];
			tpf->hash_head[hash] = 0xFFFF; /* multi link */

			link->next = 0xFFFF;
		}
	} else {
		/* a linked list of many tiles,
		 * find the one corresponding to the tile, if it exists.
		 * otherwise make a new link */

		offs = tpf->hash_tile[hash];
		do {
			link = PATHFIND_GET_LINK_PTR(tpf, offs);
			if ( (TileIndex)tile == link->tile) {
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
	if (tpf->num_links_left == 0)
			return false;
	tpf->num_links_left--;
	new_link = tpf->new_link++;

	/* then fill the link with the new info, and establish a ptr from the old
	 * link to the new one */
	new_link->tile = (TileIndex)tile;
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

static const byte _tpf_new_direction[14] = {
	0,1,0,1,2,1, 0,0,
	2,3,3,2,3,0,
};

static const byte _tpf_prev_direction[14] = {
	0,1,1,0,1,2, 0,0,
	2,3,2,3,0,3,
};


static const byte _otherdir_mask[4] = {
	0x10,
	0,
	0x5,
	0x2A,
};

#ifdef DEBUG_TILE_PUSH
extern void dbg_push_tile(uint tile, int track);
extern void dbg_pop_tile();
#endif

static void TPFMode2(TrackPathFinder *tpf, uint tile, int direction)
{
	uint bits;
	int i;
	RememberData rd;
	int owner;

	if (tpf->tracktype == TRANSPORT_RAIL) {
		owner = _map_owner[tile];
		/* Check if we are on the middle of a bridge (has no owner) */
		if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) && (_map5[tile] & 0xC0) == 0xC0)
			owner = -1;
	}

	// This addition will sometimes overflow by a single tile.
	// The use of TILE_MASK here makes sure that we still point at a valid
	// tile, and then this tile will be in the sentinel row/col, so GetTileTrackStatus will fail.
	tile = TILE_MASK(tile + _tileoffs_by_dir[direction]);

	/* Check in case of rail if the owner is the same */
	if (tpf->tracktype == TRANSPORT_RAIL) {
		/* Check if we are on the middle of a bridge (has no owner) */
		if (!IS_TILETYPE(tile, MP_TUNNELBRIDGE) || (_map5[tile] & 0xC0) != 0xC0)
			if (owner != -1 && _map_owner[tile] != owner)
				return;
	}

	if (++tpf->rd.cur_length > 50)
		return;

	bits = GetTileTrackStatus(tile, tpf->tracktype);
	bits = (byte)((bits | (bits >> 8)) & _bits_mask[direction]);
	if (bits == 0)
		return;

	assert(GET_TILE_X(tile) != 255 && GET_TILE_Y(tile) != 255);

	if ( (bits & (bits - 1)) == 0 ) {
		/* only one direction */
		i = 0;
		while (!(bits&1))
			i++, bits>>=1;

		rd = tpf->rd;
		goto continue_here;
	}
	/* several directions */
	i=0;
	do {
		if (!(bits & 1)) continue;
		rd = tpf->rd;

		// Change direction 4 times only
		if ((byte)i != tpf->rd.pft_var6) {
			if(++tpf->rd.depth > 4) {
				tpf->rd = rd;
				return;
			}
			tpf->rd.pft_var6 = (byte)i;
		}

continue_here:;
		tpf->the_dir = HASBIT(_otherdir_mask[direction],i) ? (i+8) : i;

#ifdef DEBUG_TILE_PUSH
		dbg_push_tile(tile, tpf->the_dir);
#endif
		if (!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length, NULL)) {
			TPFMode2(tpf, tile, _tpf_new_direction[tpf->the_dir]);
		}
#ifdef DEBUG_TILE_PUSH
		dbg_pop_tile();
#endif

		tpf->rd = rd;
	} while (++i, bits>>=1);

}

static const int8 _get_tunlen_inc[5] = { -16, 0, 16, 0, -16 };

/* Returns the end tile and the length of a tunnel. The length does not
 * include the starting tile (entry), it does include the end tile (exit).
 */
FindLengthOfTunnelResult FindLengthOfTunnel(uint tile, int direction)
{
	FindLengthOfTunnelResult flotr;
	int x,y;
	byte z;

	flotr.length = 0;

	x = GET_TILE_X(tile) * 16;
	y = GET_TILE_Y(tile) * 16;

	z = GetSlopeZ(x+8, y+8);

	for(;;) {
		flotr.length++;

		x += _get_tunlen_inc[direction];
		y += _get_tunlen_inc[direction+1];

		tile = TILE_FROM_XY(x,y);

		if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) &&
				(_map5[tile] & 0xF0) == 0 &&					// tunnel entrance/exit
				//((_map5[tile]>>2)&3) == type &&		// rail/road-tunnel <-- This is not necesary to check, right?
				((_map5[tile] & 3)^2) == direction &&	// entrance towards: 0 = NE, 1 = SE, 2 = SW, 3 = NW
				GetSlopeZ(x+8, y+8) == z)
					break;
	}

	flotr.tile = tile;
	return flotr;
}

static const uint16 _tpfmode1_and[4] = { 0x1009, 0x16, 0x520, 0x2A00 };

static uint SkipToEndOfTunnel(TrackPathFinder *tpf, uint tile, int direction) {
	FindLengthOfTunnelResult flotr;
	TPFSetTileBit(tpf, tile, 14);
	flotr = FindLengthOfTunnel(tile, direction);
	tpf->rd.cur_length += flotr.length;
	TPFSetTileBit(tpf, flotr.tile, 14);
	return flotr.tile;
}

const byte _ffb_64[128] = {
0,0,1,0,2,0,1,0,
3,0,1,0,2,0,1,0,
4,0,1,0,2,0,1,0,
3,0,1,0,2,0,1,0,
5,0,1,0,2,0,1,0,
3,0,1,0,2,0,1,0,
4,0,1,0,2,0,1,0,
3,0,1,0,2,0,1,0,

0,0,0,2,0,4,4,6,
0,8,8,10,8,12,12,14,
0,16,16,18,16,20,20,22,
16,24,24,26,24,28,28,30,
0,32,32,34,32,36,36,38,
32,40,40,42,40,44,44,46,
32,48,48,50,48,52,52,54,
48,56,56,58,56,60,60,62,
};

static void TPFMode1(TrackPathFinder *tpf, uint tile, int direction)
{
	uint bits;
	int i;
	RememberData rd;
	uint tile_org = tile;

	if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) && (_map5[tile] & 0xF0)==0) {
		if ((_map5[tile] & 3) != direction || ((_map5[tile]>>2)&3) != tpf->tracktype)
			return;
		tile = SkipToEndOfTunnel(tpf, tile, direction);
	}
	tile += _tileoffs_by_dir[direction];

	/* Check in case of rail if the owner is the same */
	if (tpf->tracktype == TRANSPORT_RAIL) {
		/* Check if we are on a bridge (middle parts don't have an owner */
		if (!IS_TILETYPE(tile, MP_TUNNELBRIDGE) || (_map5[tile] & 0xC0) != 0xC0)
			if (!IS_TILETYPE(tile_org, MP_TUNNELBRIDGE) || (_map5[tile_org] & 0xC0) != 0xC0)
				if (_map_owner[tile_org] != _map_owner[tile])
					return;
	}

	tpf->rd.cur_length++;

	bits = GetTileTrackStatus(tile, tpf->tracktype);

	if ((byte)bits != tpf->var2) {
		bits &= _tpfmode1_and[direction];
		bits = bits | (bits>>8);
	}
	bits &= 0xBF;

	if (bits != 0) {
		if (!tpf->disable_tile_hash || (tpf->rd.cur_length <= 64 && (KILL_FIRST_BIT(bits) == 0 || ++tpf->rd.depth <= 7))) {
			do {
				i = FIND_FIRST_BIT(bits);
				bits = KILL_FIRST_BIT(bits);

				tpf->the_dir = (_otherdir_mask[direction] & (byte)(1 << i)) ? (i+8) : i;
				rd = tpf->rd;

#ifdef DEBUG_TILE_PUSH
		dbg_push_tile(tile, tpf->the_dir);
#endif
				if (TPFSetTileBit(tpf, tile, tpf->the_dir) &&
						!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length, &tpf->rd.pft_var6) ) {
					TPFMode1(tpf, tile, _tpf_new_direction[tpf->the_dir]);
				}
#ifdef DEBUG_TILE_PUSH
		dbg_pop_tile();
#endif
				tpf->rd = rd;
			} while (bits != 0);
		}
	}

	/* the next is only used when signals are checked.
	 * seems to go in 2 directions simultaneously */

	/* if i can get rid of this, tail end recursion can be used to minimize
	 * stack space dramatically. */
	if (tpf->hasbit_13)
		return;

	tile = tile_org;
	direction ^= 2;

	bits = GetTileTrackStatus(tile, tpf->tracktype);
	bits |= (bits >> 8);

	if ( (byte)bits != tpf->var2) {
		bits &= _bits_mask[direction];
	}

	bits &= 0xBF;
	if (bits == 0)
		return;

	do {
		i = FIND_FIRST_BIT(bits);
		bits = KILL_FIRST_BIT(bits);

		tpf->the_dir = (_otherdir_mask[direction] & (byte)(1 << i)) ? (i+8) : i;
		rd = tpf->rd;
		if (TPFSetTileBit(tpf, tile, tpf->the_dir) &&
				!tpf->enum_proc(tile, tpf->userdata, tpf->the_dir, tpf->rd.cur_length, &tpf->rd.pft_var6) ) {
			TPFMode1(tpf, tile, _tpf_new_direction[tpf->the_dir]);
		}
		tpf->rd = rd;
	} while (bits != 0);
}

void FollowTrack(uint tile, uint16 flags, byte direction, TPFEnumProc *enum_proc, TPFAfterProc *after_proc, void *data)
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

	tpf.var2 = HASBIT(flags, 15) ? 0x43 : 0xFF; /* 0x8000 */

	tpf.disable_tile_hash = HASBIT(flags, 12) != 0;     /* 0x1000 */
	tpf.hasbit_13 = HASBIT(flags, 13) != 0;		 /* 0x2000 */


	tpf.tracktype = (byte)flags;

	if (HASBIT(flags, 11)) {
		tpf.rd.pft_var6 = 0xFF;
		tpf.enum_proc(tile, data, 0, 0, 0);
		TPFMode2(&tpf, tile, direction);
	} else {
		/* clear the hash_heads */
		memset(tpf.hash_head, 0, sizeof(tpf.hash_head));
		TPFMode1(&tpf, tile, direction);
	}

	if (after_proc != NULL)
		after_proc(&tpf);
}

typedef struct {
	TileIndex tile;
	uint16 cur_length;
	byte track;
	byte depth;
	byte state;
	byte first_track;
} StackedItem;

static const byte _new_dir[6][4] = {
{0,0xff,2,0xff,},
{0xff,1,0xff,3,},
{0xff,0,3,0xff,},
{1,0xff,0xff,2,},
{3,2,0xff,0xff,},
{0xff,0xff,1,0,},
};

static const byte _new_track[6][4] = {
{0,0xff,8,0xff,},
{0xff,1,0xff,9,},
{0xff,2,10,0xff,},
{3,0xff,0xff,11,},
{12,4,0xff,0xff,},
{0xff,0xff,5,13,},
};

typedef struct HashLink {
	TileIndex tile;
	uint16 typelength;
	uint16 next;
} HashLink;

typedef struct {
	TPFEnumProc *enum_proc;
	void *userdata;

	byte tracktype;
	uint maxlength;

	HashLink *new_link;
	uint num_links_left;

	int nstack;
	StackedItem stack[256]; // priority queue of stacked items

	uint16 hash_head[0x400]; // hash heads. 0 means unused. 0xFFC0 = length, 0x3F = type
	TileIndex hash_tile[0x400]; // tiles. or links.

	HashLink links[0x400]; // hash links

} NewTrackPathFinder;
#define NTP_GET_LINK_OFFS(tpf, link) ((byte*)(link) - (byte*)tpf->links)
#define NTP_GET_LINK_PTR(tpf, link_offs) (HashLink*)((byte*)tpf->links + (link_offs))

#define ARR(i) tpf->stack[(i)-1]

// called after a new element was added in the queue at the last index.
// move it down to the proper position
static inline void HeapifyUp(NewTrackPathFinder *tpf)
{
	StackedItem si;
	int i = ++tpf->nstack;

	while (i != 1 && ARR(i).cur_length < ARR(i>>1).cur_length) {
		// the child element is larger than the parent item.
		// swap the child item and the parent item.
		si = ARR(i); ARR(i) = ARR(i>>1); ARR(i>>1) = si;
		i>>=1;
	}
}

// called after the element 0 was eaten. fill it with a new element
static inline void HeapifyDown(NewTrackPathFinder *tpf)
{
	StackedItem si;
	int i = 1, j;
	int n = --tpf->nstack;

	if (n == 0) return; // heap is empty so nothing to do?

	// copy the last item to index 0. we use it as base for heapify.
	ARR(1) = ARR(n+1);

	while ((j=i*2) <= n) {
		// figure out which is smaller of the children.
		if (j != n && ARR(j).cur_length > ARR(j+1).cur_length)
			j++; // right item is smaller

		assert(i <= n && j <= n);
		if (ARR(i).cur_length <= ARR(j).cur_length)
			break; // base elem smaller than smallest, done!

		// swap parent with the child
		si = ARR(i); ARR(i) = ARR(j); ARR(j) = si;
		i = j;
	}
}

// mark a tile as visited and store the length of the path.
// if we already had a better path to this tile, return false.
// otherwise return true.
static bool NtpVisit(NewTrackPathFinder *tpf, uint tile, uint dir, uint length)
{
	uint hash,head;
	HashLink *link, *new_link;

	assert(length < 1024);

	hash = PATHFIND_HASH_TILE(tile);

	// never visited before?
	if ((head=tpf->hash_head[hash]) == 0) {
		tpf->hash_tile[hash] = tile;
		tpf->hash_head[hash] = dir | (length << 2);
		return true;
	}

	if (head != 0xffff) {
		if ( (TileIndex)tile == tpf->hash_tile[hash] && (head & 0x3) == dir ) {

			// longer length
			if (length >= (head >> 2)) return false;

			tpf->hash_head[hash] = dir | (length << 2);
			return true;
		}
		// two tiles with the same hash, need to make a link
		// allocate a link. if out of links, handle this by returning
		// that a tile was already visisted.
		if (tpf->num_links_left == 0)
			return false;
		tpf->num_links_left--;
		link = tpf->new_link++;

		/* move the data that was previously in the hash_??? variables
		 * to the link struct, and let the hash variables point to the link */
		link->tile = tpf->hash_tile[hash];
		tpf->hash_tile[hash] = NTP_GET_LINK_OFFS(tpf, link);

		link->typelength = tpf->hash_head[hash];
		tpf->hash_head[hash] = 0xFFFF; /* multi link */
		link->next = 0xFFFF;
	} else {
		// a linked list of many tiles,
		// find the one corresponding to the tile, if it exists.
		// otherwise make a new link

		uint offs = tpf->hash_tile[hash];
		do {
			link = NTP_GET_LINK_PTR(tpf, offs);
			if ( (TileIndex)tile == link->tile && (uint)(link->typelength & 0x3) == dir) {
				if (length >= (uint)(link->typelength >> 2)) return false;
				link->typelength = dir | (length << 2);
				return true;
			}
		} while ((offs=link->next) != 0xFFFF);
	}

	/* get here if we need to add a new link to link,
	 * first, allocate a new link, in the same way as before */
	if (tpf->num_links_left == 0)
			return false;
	tpf->num_links_left--;
	new_link = tpf->new_link++;

	/* then fill the link with the new info, and establish a ptr from the old
	 * link to the new one */
	new_link->tile = (TileIndex)tile;
	new_link->typelength = dir | (length << 2);
	new_link->next = 0xFFFF;

	link->next = NTP_GET_LINK_OFFS(tpf, new_link);
	return true;
}

static bool NtpCheck(NewTrackPathFinder *tpf, uint tile, uint dir, uint length)
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

	// else it's a linked list of many tiles
	offs = tpf->hash_tile[hash];
	for(;;) {
		link = NTP_GET_LINK_PTR(tpf, offs);
		if ( (TileIndex)tile == link->tile && (uint)(link->typelength & 0x3) == dir) {
			assert( (uint)(link->typelength >> 2) <= length);
			return length == (uint)(link->typelength >> 2);
		}
		offs = link->next;
		assert(offs != 0xffff);
	}
}


// new more optimized pathfinder for trains...
static void NTPEnum(NewTrackPathFinder *tpf, uint tile, uint direction)
{
	uint bits, tile_org;
	int i;
	StackedItem si;
	FindLengthOfTunnelResult flotr;

	si.cur_length = 0;
	si.depth = 0;
	si.state = 0;

restart:
	if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) && (_map5[tile] & 0xF0)==0) {
		/* This is a tunnel tile */
		if ( (uint)(_map5[tile] & 3) != (direction ^ 2)) { /* ^ 2 is reversing the direction */
			/* We are not just driving out of the tunnel */
			if ( (uint)(_map5[tile] & 3) != direction || ((_map5[tile]>>1)&6) != tpf->tracktype)
				/* We are not driving into the tunnel, or it
				 * is an invalid tunnel */
				goto popnext;
			flotr = FindLengthOfTunnel(tile, direction);
			si.cur_length += flotr.length;
			tile = flotr.tile;
		}
	}

	// remember the start tile so we know if we're in an inf loop.
	tile_org = tile;

	for(;;) {
		tile += _tileoffs_by_dir[direction];

		// too long search length? bail out.
		if (++si.cur_length >= tpf->maxlength)
			goto popnext;

		// not a regular rail tile?
		if (!IS_TILETYPE(tile, MP_RAILWAY) || (bits = _map5[tile]) & 0xC0) {
			bits = GetTileTrackStatus(tile, TRANSPORT_RAIL) & _tpfmode1_and[direction];
			bits = (bits | (bits >> 8)) & 0x3F;
			break;
		}

		// regular rail tile, determine the tracks that are actually reachable.
		bits &= _bits_mask[direction];
		if (bits == 0) goto popnext; // no tracks there? stop searching.

		// complex tile?, let the generic handler handle that..
		if (KILL_FIRST_BIT(bits) != 0) break;

		// don't bother calling the callback when we have regular tracks only.
		// it's usually not needed anyway. that will speed up things.
		direction = _new_dir[FIND_FIRST_BIT(bits)][direction];
		assert(direction != 0xFF);
		if (tile == tile_org) goto popnext; // detect infinite loop..
	}

	if (!bits) goto popnext;

	// if only one reachable track, use tail recursion optimization.
	if (KILL_FIRST_BIT(bits) == 0) {
		i = _new_track[FIND_FIRST_BIT(bits)][direction];
		// call the callback
		if (tpf->enum_proc(tile, tpf->userdata, i, si.cur_length, &si.state))
			goto popnext; // we should stop searching in this direction.

		// we should continue searching. determine new direction.
		direction = _tpf_new_direction[i];
		goto restart; // use tail recursion optimization.
	}

	// too high recursion depth.. bail out..
	if (si.depth >= _patches.pf_maxdepth)
		goto popnext;

	si.depth++; // increase recursion depth.

	// see if this tile was already visited..?
	if (NtpVisit(tpf, tile, direction, si.cur_length)) {
		// push all possible alternatives
		si.tile = tile;
		do {
			si.track = _new_track[FIND_FIRST_BIT(bits)][direction];

			// out of stack items, bail out?
			if (tpf->nstack >= lengthof(tpf->stack))
				break;
			tpf->stack[tpf->nstack] = si;
			HeapifyUp(tpf);
		} while ((bits = KILL_FIRST_BIT(bits)) != 0);

		// if this is the first recursion step, we need to fill the first_track member.
		// so the code outside knows which path is better.
		// also randomize the order in which we search through them.
		if (si.depth == 1) {
			uint32 r = Random();
			assert(tpf->nstack == 2 || tpf->nstack == 3);
			if (r&1) swap_byte(&tpf->stack[0].track, &tpf->stack[1].track);
			if (tpf->nstack != 2) {
				byte t = tpf->stack[2].track;
				if (r&2) swap_byte(&tpf->stack[0].track, &t);
				if (r&4) swap_byte(&tpf->stack[1].track, &t);
				tpf->stack[2].first_track = tpf->stack[2].track = t;
			}
			tpf->stack[0].first_track = tpf->stack[0].track;
			tpf->stack[1].first_track = tpf->stack[1].track;
		}
	}

popnext:
	// where to continue.
	do {
		if (tpf->nstack == 0) return; // nothing left?
		si = tpf->stack[0];
		tile = si.tile;
		HeapifyDown(tpf);
	} while (
		!NtpCheck(tpf, tile, _tpf_prev_direction[si.track], si.cur_length) || // already have better path to that tile?
		tpf->enum_proc(tile, tpf->userdata, si.track, si.cur_length, &si.state)
	);

	direction = _tpf_new_direction[si.track];
	goto restart;
}


// new pathfinder for trains. better and faster.
void NewTrainPathfind(uint tile, byte direction, TPFEnumProc *enum_proc, void *data, byte *cache)
{
	if (!_patches.new_pathfinding) {
		FollowTrack(tile, 0x3000 | TRANSPORT_RAIL, direction, enum_proc, NULL, data);
	} else {
		NewTrackPathFinder tpf;
		tpf.userdata = data;
		tpf.enum_proc = enum_proc;
		tpf.tracktype = 0;
		tpf.maxlength = _patches.pf_maxlength;
		tpf.nstack = 0;
		tpf.new_link = tpf.links;
		tpf.num_links_left = lengthof(tpf.links);
		memset(tpf.hash_head, 0, sizeof(tpf.hash_head));

		NTPEnum(&tpf, tile, direction);
	}
}

