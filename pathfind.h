#ifndef PATHFIND_H
#define PATHFIND_H

typedef struct TrackPathFinder TrackPathFinder;
typedef bool TPFEnumProc(uint tile, void *data, int track, uint length, byte *state);
typedef void TPFAfterProc(TrackPathFinder *tpf);


#define PATHFIND_GET_LINK_OFFS(tpf, link) ((byte*)(link) - (byte*)tpf->links)
#define PATHFIND_GET_LINK_PTR(tpf, link_offs) (TrackPathFinderLink*)((byte*)tpf->links + (link_offs))

/* y7 y6 y5 y4 y3 y2 y1 y0 x7 x6 x5 x4 x3 x2 x1 x0
 * y7 y6 y5 y4 y3 y2 y1 y0 x4 x3 x2 x1 x0  0  0  0
 *  0  0 y7 y6 y5 y4 y3 y2 y1 y0 x4 x3 x2 x1 x0  0
 *  0  0  0  0 y5 y4 y3 y2 y1 y0 x4 x3 x2 x1 x0  0
 */
#define PATHFIND_HASH_TILE(tile) (TileX(tile) & 0x1F) + ((TileY(tile) & 0x1F) << 5)

typedef struct TrackPathFinderLink {
	TileIndex tile;
	uint16 flags;
	uint16 next;
} TrackPathFinderLink;

typedef struct RememberData {
	uint16 cur_length;
	byte depth;
	byte pft_var6;
} RememberData;

struct TrackPathFinder {

	int num_links_left;
	TrackPathFinderLink *new_link;

	TPFEnumProc *enum_proc;

	void *userdata;

	RememberData rd;

	int the_dir;

	byte tracktype;
	byte var2;
	bool disable_tile_hash;
	bool hasbit_13;

	uint16 hash_head[0x400];
	TileIndex hash_tile[0x400]; /* stores the link index when multi link. */

	TrackPathFinderLink links[0x400]; /* hopefully, this is enough. */
};

void FollowTrack(uint tile, uint16 flags, byte direction, TPFEnumProc *enum_proc, TPFAfterProc *after_proc, void *data);

typedef struct {
	uint tile;
	int length;
} FindLengthOfTunnelResult;
FindLengthOfTunnelResult FindLengthOfTunnel(uint tile, int direction);

void NewTrainPathfind(uint tile, byte direction, TPFEnumProc *enum_proc, void *data, byte *cache);

#endif /* PATHFIND_H */
