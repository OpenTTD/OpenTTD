#ifndef NPF_H
#define NPF_H

#include "ttd.h"
#include "aystar.h"
#include "vehicle.h"

//mowing grass
enum {
	NPF_HASH_BITS = 12, /* The size of the hash used in pathfinding. Just changing this value should be sufficient to change the hash size. Should be an even value. */
	/* Do no change below values */
	NPF_HASH_SIZE = 1 << NPF_HASH_BITS,
	NPF_HASH_HALFBITS = NPF_HASH_BITS / 2,
	NPF_HASH_HALFMASK = (1 << NPF_HASH_HALFBITS) - 1
};

enum {
	/** This penalty is the equivalent of "inifite", which means that paths that
	 * get this penalty will be chosen, but only if there is no other route
	 * without it. Be careful with not applying this penalty to often, or the
	 * total path cost might overflow..
	 * For now, this is just a Very Big Penalty, we might actually implement
	 * this in a nicer way :-)
	 */
	NPF_INFINITE_PENALTY = 1000 * NPF_TILE_LENGTH
};

typedef struct NPFFindStationOrTileData { /* Meant to be stored in AyStar.targetdata */
	TileIndex dest_coords; /* An indication of where the station is, for heuristic purposes, or the target tile */
	int station_index; /* station index we're heading for, or -1 when we're heading for a tile */
} NPFFindStationOrTileData;

enum { /* Indices into AyStar.userdata[] */
	NPF_TYPE = 0, /* Contains a TransportTypes value */
	NPF_OWNER, /* Contains an Owner value */
};

enum { /* Indices into AyStarNode.userdata[] */
	NPF_TRACKDIR_CHOICE = 0, /* The trackdir chosen to get here */
	NPF_NODE_FLAGS,
};
typedef enum { /* Flags for AyStarNode.userdata[NPF_NODE_FLAGS]. Use NPFGetBit() and NPFGetBit() to use them. */
	NPF_FLAG_SEEN_SIGNAL, /* Used to mark that a signal was seen on the way, for rail only */
	NPF_FLAG_REVERSE, /* Used to mark that this node was reached from the second start node, if applicable */
	NPF_FLAG_LAST_SIGNAL_RED, /* Used to mark that the last signal on this path was red */
} NPFNodeFlag;

typedef struct NPFFoundTargetData { /* Meant to be stored in AyStar.userpath */
	uint best_bird_dist; /* The best heuristic found. Is 0 if the target was found */
	uint best_path_dist; /* The shortest path. Is (uint)-1 if no path is found */
	byte best_trackdir; /* The trackdir that leads to the shortest path/closest birds dist */
	AyStarNode node; /* The node within the target the search led us to */
} NPFFoundTargetData;

/* These functions below are _not_ re-entrant, in favor of speed! */

/* Will search from the given tile and direction, for a route to the given
 * station for the given transport type. See the declaration of
 * NPFFoundTargetData above for the meaning of the result. */
NPFFoundTargetData NPFRouteToStationOrTile(TileIndex tile, byte trackdir, NPFFindStationOrTileData* target, TransportType type, Owner owner);
/* Will search as above, but with two start nodes, the second being the
 * reverse. Look at the NPF_FLAG_REVERSE flag in the result node to see which
 * direction was taken (NPFGetBit(result.node, NPF_FLAG_REVERSE)) */
NPFFoundTargetData NPFRouteToStationOrTileTwoWay(TileIndex tile1, byte trackdir1, TileIndex tile2, byte trackdir2, NPFFindStationOrTileData* target, TransportType type, Owner owner);

/* Will search a route to the closest depot. */

/* Search using breadth first. Good for little track choice and inaccurate
 * heuristic, such as railway/road.*/
NPFFoundTargetData NPFRouteToDepotBreadthFirst(TileIndex tile, byte trackdir, TransportType type, Owner owner);
/* Same as above but with two start nodes, the second being the reverse. Call
 * NPFGetBit(result.node, NPF_FLAG_REVERSE) to see from which node the path
 * orginated. All pathfs from the second node will have the given
 * reverse_penalty applied (NPF_TILE_LENGTH is the equivalent of one full
 * tile).
 */
NPFFoundTargetData NPFRouteToDepotBreadthFirstTwoWay(TileIndex tile1, byte trackdir1, TileIndex tile2, byte trackdir2, TransportType type, Owner owner, uint reverse_penalty);
/* Search by trying each depot in order of Manhattan Distance. Good for lots
 * of choices and accurate heuristics, such as water. */
NPFFoundTargetData NPFRouteToDepotTrialError(TileIndex tile, byte trackdir, TransportType type, Owner owner);

void NPFFillWithOrderData(NPFFindStationOrTileData* fstd, Vehicle* v);


/*
 * Functions to manipulate the various NPF related flags on an AyStarNode.
 */

/**
 * Returns the current value of the given flag on the given AyStarNode.
 */
static inline bool NPFGetFlag(const AyStarNode* node, NPFNodeFlag flag)
{
	return HASBIT(node->user_data[NPF_NODE_FLAGS], flag);
}

/**
 * Sets the given flag on the given AyStarNode to the given value.
 */
static inline void NPFSetFlag(AyStarNode* node, NPFNodeFlag flag, bool value)
{
	if (value)
		SETBIT(node->user_data[NPF_NODE_FLAGS], flag);
	else
		CLRBIT(node->user_data[NPF_NODE_FLAGS], flag);
}

/*
 * Some tables considering tracks, directions and signals.
 * XXX: Better place to but these?
 */

/**
 * Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction along with the trackdir.
 */
const byte _signal_along_trackdir[14];

/**
 * Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction against the trackdir.
 */
const byte _signal_against_trackdir[14];

/**
 * Maps a trackdir to the trackdirs that can be reached from it (ie, when
 * entering the next tile.
 */
const uint16 _trackdir_reaches_trackdirs[14];

/**
 * Maps a trackdir to the trackdir that you will end up on if you go straight
 * ahead. This will be the same trackdir for diagonal trackdirs, but a
 * different (alternating) one for straight trackdirs */
const uint16 _next_trackdir[14];
/**
 * Maps a trackdir to all trackdirs that make 90 deg turns with it.
 */
const uint16 _trackdir_crosses_trackdirs[14];

/**
 * Maps a track to all tracks that make 90 deg turns with it.
 */
const byte _track_crosses_tracks[6];

/**
 * Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir.
 */
const byte _trackdir_to_exitdir[14];

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the exit in the given direction.
 */
const byte _track_exitdir_to_trackdir[6][4];

/**
 * Maps a track and a full (8-way) direction to the trackdir that represents
 * the track running in the given direction.
 */
const byte _track_direction_to_trackdir[6][8];

/**
 * Maps a (4-way) direction to the diagonal track that runs in that
 * direction.
 */
const byte _dir_to_diag_trackdir[4];

/**
 * Maps a (4-way) direction to the reverse.
 */
const byte _reverse_dir[4];

/**
 * Maps a trackdir to the reverse trackdir.
 */
const byte _reverse_trackdir[14];

/* Returns the Track that a given Trackdir represents */
static inline byte TrackdirToTrack(byte trackdir) { return trackdir & 0x7; }

/* Returns a Trackdir for the given Track. Since every Track corresponds to
 * two Trackdirs, we choose the one which points between N and SE.
 * Note that the actual implementation is quite futile, but this might change
 * in the future.
 */
static inline byte TrackToTrackdir(byte track) { return track; }

/* Checks if a given Track is diagonal */
static inline bool IsDiagonalTrack(byte track) { return track == 0x0 || track == 0x1; }

/* Checks if a given Trackdir is diagonal. */
static inline bool IsDiagonalTrackdir(byte trackdir) { return IsDiagonalTrack(TrackdirToTrack(trackdir)); }


#define REVERSE_TRACKDIR(trackdir) (trackdir ^ 0x8)

#endif // NPF_H
