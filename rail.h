/* $Id$ */

/** @file rail.h */

#ifndef RAIL_H
#define RAIL_H

#include "tile.h"

/*
 * Some enums for accesing the map bytes for rail tiles
 */

/** These types are used in the map5 byte for rail tiles. Use GetRailTileType() to
 * get these values */
typedef enum RailTileTypes {
	RAIL_TYPE_NORMAL         = 0x0,
	RAIL_TYPE_SIGNALS        = 0x40,
	RAIL_TYPE_UNUSED         = 0x80, /* XXX: Maybe this could become waypoints? */
	RAIL_TYPE_DEPOT_WAYPOINT = 0xC0, /* Is really depots and waypoints... */
	RAIL_TILE_TYPE_MASK      = 0xC0,
} RailTileType;

enum { /* DEPRECATED TODO: Rewrite all uses of this */
	RAIL_TYPE_SPECIAL = 0x80, /* This used to say "If this bit is set, then it's
														 * not a regular track.", but currently, you
														 * should rather view map5[6..7] as one type,
														 * containing a value from RailTileTypes above.
														 * This value is only maintained for backwards
														 * compatibility */

	/* There used to be RAIL_BIT_* enums here, they moved to (for now) npf.c as
	 * TRACK_BIT_* */
};

/** These subtypes are used in the map5 byte when the main rail type is
 * RAIL_TYPE_DEPOT_WAYPOINT */
typedef enum RailTileSubtypes {
	RAIL_SUBTYPE_DEPOT    = 0x00,
	RAIL_SUBTYPE_WAYPOINT = 0x04,
	RAIL_SUBTYPE_MASK     = 0x3C,
} RailTileSubtype;

typedef enum SignalTypes {
	/* Stored in m4[0..1] for MP_RAILWAY */
  SIGTYPE_NORMAL  = 0,        // normal signal
  SIGTYPE_ENTRY   = 1,        // presignal block entry
  SIGTYPE_EXIT    = 2,        // presignal block exit
  SIGTYPE_COMBO   = 3,        // presignal inter-block
	SIGTYPE_PBS     = 4,        // pbs signal
	SIGTYPE_END,
	SIGTYPE_MASK    = 7,
} SignalType;

typedef enum RailTypes {
	RAILTYPE_RAIL   = 0,
	RAILTYPE_MONO   = 1,
	RAILTYPE_MAGLEV = 2,
	RAILTYPE_END,
	RAILTYPE_MASK   = 0x3,
	INVALID_RAILTYPE = 0xFF,
} RailType;

enum {
	SIG_SEMAPHORE_MASK = 1 << 3,
};

/** These are used to specify a single track. Can be translated to a trackbit
 * with TrackToTrackbit */
typedef enum Tracks {
  TRACK_DIAG1 = 0,
  TRACK_DIAG2 = 1,
  TRACK_UPPER = 2,
  TRACK_LOWER = 3,
  TRACK_LEFT  = 4,
  TRACK_RIGHT = 5,
  TRACK_END,
  INVALID_TRACK = 0xFF,
} Track;

/** These are the bitfield variants of the above */
typedef enum TrackBits {
  TRACK_BIT_DIAG1 = 1,  // 0
  TRACK_BIT_DIAG2 = 2,  // 1
  TRACK_BIT_UPPER = 4,  // 2
  TRACK_BIT_LOWER = 8,  // 3
  TRACK_BIT_LEFT  = 16, // 4
  TRACK_BIT_RIGHT = 32, // 5
	TRACK_BIT_MASK  = 0x3F,
} TrackBits;

/** These are a combination of tracks and directions. Values are 0-5 in one
direction (corresponding to the Track enum) and 8-13 in the other direction. */
typedef enum Trackdirs {
  TRACKDIR_DIAG1_NE = 0,
  TRACKDIR_DIAG2_SE = 1,
  TRACKDIR_UPPER_E  = 2,
  TRACKDIR_LOWER_E  = 3,
  TRACKDIR_LEFT_S   = 4,
  TRACKDIR_RIGHT_S  = 5,
	/* Note the two missing values here. This enables trackdir -> track
	 * conversion by doing (trackdir & 7) */
  TRACKDIR_DIAG1_SW = 8,
  TRACKDIR_DIAG2_NW = 9,
  TRACKDIR_UPPER_W  = 10,
  TRACKDIR_LOWER_W  = 11,
  TRACKDIR_LEFT_N   = 12,
  TRACKDIR_RIGHT_N  = 13,
  TRACKDIR_END,
  INVALID_TRACKDIR  = 0xFF,
} Trackdir;

/** These are a combination of tracks and directions. Values are 0-5 in one
direction (corresponding to the Track enum) and 8-13 in the other direction. */
typedef enum TrackdirBits {
  TRACKDIR_BIT_DIAG1_NE = 0x1,
  TRACKDIR_BIT_DIAG2_SE = 0x2,
  TRACKDIR_BIT_UPPER_E  = 0x4,
  TRACKDIR_BIT_LOWER_E  = 0x8,
  TRACKDIR_BIT_LEFT_S   = 0x10,
  TRACKDIR_BIT_RIGHT_S  = 0x20,
	/* Again, note the two missing values here. This enables trackdir -> track conversion by doing (trackdir & 0xFF) */
  TRACKDIR_BIT_DIAG1_SW = 0x0100,
  TRACKDIR_BIT_DIAG2_NW = 0x0200,
  TRACKDIR_BIT_UPPER_W  = 0x0400,
  TRACKDIR_BIT_LOWER_W  = 0x0800,
  TRACKDIR_BIT_LEFT_N   = 0x1000,
  TRACKDIR_BIT_RIGHT_N  = 0x2000,
	TRACKDIR_BIT_MASK			= 0x3F3F,
  INVALID_TRACKDIR_BIT  = 0xFFFF,
} TrackdirBits;

/** These are states in which a signal can be. Currently these are only two, so
 * simple boolean logic will do. But do try to compare to this enum instead of
 * normal boolean evaluation, since that will make future additions easier.
 */
typedef enum SignalStates {
	SIGNAL_STATE_RED = 0,
	SIGNAL_STATE_GREEN = 1,
} SignalState;

/** This struct contains all the info that is needed to draw and construct tracks.
 */
typedef struct RailtypeInfo {
	/** Struct containing the main sprites. @note not all sprites are listed, but only
	 *  the ones used directly in the code */
	struct {
		SpriteID track_y;      ///< single piece of rail in Y direction, with ground
		SpriteID track_ns;     ///< two pieces of rail in North and South corner (East-West direction)
		SpriteID ground;       ///< ground sprite for a 3-way switch
		SpriteID single_y;     ///< single piece of rail in Y direction, without ground
		SpriteID single_x;     ///< single piece of rail in X direction
		SpriteID single_n;     ///< single piece of rail in the northern corner
		SpriteID single_s;     ///< single piece of rail in the southern corner
		SpriteID single_e;     ///< single piece of rail in the eastern corner
		SpriteID single_w;     ///< single piece of rail in the western corner
	} base_sprites;

	/** struct containing the sprites for the rail GUI. @note only sprites referred to
	 * directly in the code are listed */
	struct {
		SpriteID build_ns_rail;      ///< button for building single rail in N-S direction
		SpriteID build_x_rail;       ///< button for building single rail in X direction
		SpriteID build_ew_rail;      ///< button for building single rail in E-W direction
		SpriteID build_y_rail;       ///< button for building single rail in Y direction
		SpriteID auto_rail;          ///< button for the autorail construction
		SpriteID build_depot;        ///< button for building depots
		SpriteID build_tunnel;       ///< button for building a tunnel
		SpriteID convert_rail;       ///< button for converting rail
	} gui_sprites;

	struct {
		StringID toolbar_caption;
	} strings;

	/** sprite number difference between a piece of track on a snowy ground and the corresponding one on normal ground */
	SpriteID snow_offset;

	/** bitmask to the OTHER railtypes that can be used by an engine of THIS railtype */
	byte compatible_railtypes;

	/**
	 * Offset between the current railtype and normal rail. This means that:<p>
	 * 1) All the sprites in a railset MUST be in the same order. This order
	 *    is determined by normal rail. Check sprites 1005 and following for this order<p>
	 * 2) The position where the railtype is loaded must always be the same, otherwise
	 *    the offset will fail.<p>
	 * @note: Something more flexible might be desirable in the future.
	 */
	SpriteID total_offset;
} RailtypeInfo;

extern const RailtypeInfo _railtypes[RAILTYPE_END];

// these are the maximums used for updating signal blocks, and checking if a depot is in a pbs block
enum {
	NUM_SSD_ENTRY = 256, // max amount of blocks
	NUM_SSD_STACK = 32 ,// max amount of blocks to check recursively
};

/**
 * Maps a Trackdir to the corresponding TrackdirBits value
 */
static inline TrackdirBits TrackdirToTrackdirBits(Trackdir trackdir) { return (TrackdirBits)(1 << trackdir); }

/**
 * These functions check the validity of Tracks and Trackdirs. assert against
 * them when convenient.
 */
static inline bool IsValidTrack(Track track) { return track < TRACK_END; }
static inline bool IsValidTrackdir(Trackdir trackdir) { return (TrackdirToTrackdirBits(trackdir) & TRACKDIR_BIT_MASK) != 0; }

/**
 * Functions to map tracks to the corresponding bits in the signal
 * presence/status bytes in the map. You should not use these directly, but
 * wrapper functions below instead. XXX: Which are these?
 */

/**
 * Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction along with the trackdir.
 */
extern const byte _signal_along_trackdir[TRACKDIR_END];
static inline byte SignalAlongTrackdir(Trackdir trackdir) {return _signal_along_trackdir[trackdir];}

/**
 * Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction against the trackdir.
 */
static inline byte SignalAgainstTrackdir(Trackdir trackdir) {
	extern const byte _signal_against_trackdir[TRACKDIR_END];
	return _signal_against_trackdir[trackdir];
}

/**
 * Maps a Track to the bits that store the status of the two signals that can
 * be present on the given track.
 */
static inline byte SignalOnTrack(Track track) {
	extern const byte _signal_on_track[TRACK_END];
	return _signal_on_track[track];
}

/*
 * Some functions to query rail tiles
 */

/**
 * Returns the RailTileType of a given rail tile. (ie normal, with signals,
 * depot, etc.)
 */
static inline RailTileType GetRailTileType(TileIndex tile)
{
	assert(IsTileType(tile, MP_RAILWAY));
	return (_m[tile].m5 & RAIL_TILE_TYPE_MASK);
}

/**
 * Returns the rail type of the given rail tile (ie rail, mono, maglev).
 */
static inline RailType GetRailType(TileIndex tile) { return (RailType)(_m[tile].m3 & RAILTYPE_MASK); }

/**
 * Checks if a rail tile has signals.
 */
static inline bool HasSignals(TileIndex tile)
{
	return GetRailTileType(tile) == RAIL_TYPE_SIGNALS;
}

/**
 * Returns the RailTileSubtype of a given rail tile with type
 * RAIL_TYPE_DEPOT_WAYPOINT
 */
static inline RailTileSubtype GetRailTileSubtype(TileIndex tile)
{
	assert(GetRailTileType(tile) == RAIL_TYPE_DEPOT_WAYPOINT);
	return (RailTileSubtype)(_m[tile].m5 & RAIL_SUBTYPE_MASK);
}

/**
 * Returns whether this is plain rails, with or without signals. Iow, if this
 * tiles RailTileType is RAIL_TYPE_NORMAL or RAIL_TYPE_SIGNALS.
 */
static inline bool IsPlainRailTile(TileIndex tile)
{
	RailTileType rtt = GetRailTileType(tile);
	return rtt == RAIL_TYPE_NORMAL || rtt == RAIL_TYPE_SIGNALS;
}

/**
 * Returns the tracks present on the given plain rail tile (IsPlainRailTile())
 */
static inline TrackBits GetTrackBits(TileIndex tile)
{
	assert(GetRailTileType(tile) == RAIL_TYPE_NORMAL || GetRailTileType(tile) == RAIL_TYPE_SIGNALS);
	return (TrackBits)(_m[tile].m5 & TRACK_BIT_MASK);
}

/**
 * Returns whether the given track is present on the given tile. Tile must be
 * a plain rail tile (IsPlainRailTile()).
 */
static inline bool HasTrack(TileIndex tile, Track track)
{
	assert(IsValidTrack(track));
	return HASBIT(GetTrackBits(tile), track);
}

/*
 * Functions describing logical relations between Tracks, TrackBits, Trackdirs
 * TrackdirBits, Direction and DiagDirections.
 *
 * TODO: Add #unndefs or something similar to remove the arrays used below
 * from the global scope and expose direct uses of them.
 */

/**
 * Maps a trackdir to the reverse trackdir.
 */
static inline Trackdir ReverseTrackdir(Trackdir trackdir) {
	extern const Trackdir _reverse_trackdir[TRACKDIR_END];
	return _reverse_trackdir[trackdir];
}

/*
 * Maps a Track to the corresponding TrackBits value
 */
static inline TrackBits TrackToTrackBits(Track track) { return (TrackBits)(1 << track); }

/* Returns the Track that a given Trackdir represents */
static inline Track TrackdirToTrack(Trackdir trackdir) { return (Track)(trackdir & 0x7); }

/* Returns a Trackdir for the given Track. Since every Track corresponds to
 * two Trackdirs, we choose the one which points between NE and S.
 * Note that the actual implementation is quite futile, but this might change
 * in the future.
 */
static inline Trackdir TrackToTrackdir(Track track) { return (Trackdir)track; }

/* Returns a TrackdirBit mask that contains the two TrackdirBits that
 * correspond with the given Track (one for each direction).
 */
static inline TrackdirBits TrackToTrackdirBits(Track track) { Trackdir td = TrackToTrackdir(track); return TrackdirToTrackdirBits(td) | TrackdirToTrackdirBits(ReverseTrackdir(td));}

/**
 * Maps a trackdir to the trackdir that you will end up on if you go straight
 * ahead. This will be the same trackdir for diagonal trackdirs, but a
 * different (alternating) one for straight trackdirs
 */
static inline Trackdir NextTrackdir(Trackdir trackdir) {
	extern const Trackdir _next_trackdir[TRACKDIR_END];
	return _next_trackdir[trackdir];
}

/**
 * Maps a track to all tracks that make 90 deg turns with it.
 */
static inline TrackBits TrackCrossesTracks(Track track) {
	extern const TrackBits _track_crosses_tracks[TRACK_END];
	return _track_crosses_tracks[track];
}

/**
 * Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir.
 */
static inline DiagDirection TrackdirToExitdir(Trackdir trackdir) {
	extern const DiagDirection _trackdir_to_exitdir[TRACKDIR_END];
	return _trackdir_to_exitdir[trackdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the exit in the given direction.
 */
static inline Trackdir TrackExitdirToTrackdir(Track track, DiagDirection diagdir) {
	extern const Trackdir _track_exitdir_to_trackdir[TRACK_END][DIAGDIR_END];
	return _track_exitdir_to_trackdir[track][diagdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the exit in the given direction.
 */
static inline Trackdir TrackEnterdirToTrackdir(Track track, DiagDirection diagdir) {
	extern const Trackdir _track_enterdir_to_trackdir[TRACK_END][DIAGDIR_END];
	return _track_enterdir_to_trackdir[track][diagdir];
}

/**
 * Maps a track and a full (8-way) direction to the trackdir that represents
 * the track running in the given direction.
 */
static inline Trackdir TrackDirectionToTrackdir(Track track, Direction dir) {
	extern const Trackdir _track_direction_to_trackdir[TRACK_END][DIR_END];
	return _track_direction_to_trackdir[track][dir];
}

/**
 * Maps a (4-way) direction to the diagonal trackdir that runs in that
 * direction.
 */
static inline Trackdir DiagdirToDiagTrackdir(DiagDirection diagdir) {
	extern const Trackdir _dir_to_diag_trackdir[DIAGDIR_END];
	return _dir_to_diag_trackdir[diagdir];
}

/**
 * Maps a trackdir to the trackdirs that can be reached from it (ie, when
 * entering the next tile. This
 */
extern const TrackdirBits _exitdir_reaches_trackdirs[DIAGDIR_END];
/* Note that there is no direct table for this function (there used to be),
 * but it uses two simpeler tables to achieve the result */
static inline TrackdirBits TrackdirReachesTrackdirs(Trackdir trackdir) { return _exitdir_reaches_trackdirs[TrackdirToExitdir(trackdir)]; }

/**
 * Maps a trackdir to all trackdirs that make 90 deg turns with it.
 */
static inline TrackdirBits TrackdirCrossesTrackdirs(Trackdir trackdir) {
	extern const TrackdirBits _track_crosses_trackdirs[TRACKDIR_END];
	return _track_crosses_trackdirs[TrackdirToTrack(trackdir)];
}

/**
 * Maps a (4-way) direction to the reverse.
 */
static inline DiagDirection ReverseDiagdir(DiagDirection diagdir) {
	extern const DiagDirection _reverse_diagdir[DIAGDIR_END];
	return _reverse_diagdir[diagdir];
}

/**
 * Maps a (8-way) direction to a (4-way) DiagDirection
 */
static inline DiagDirection DirToDiagdir(Direction dir) {
	assert(dir < DIR_END);
	return (DiagDirection)(dir >> 1);
}

/* Checks if a given Track is diagonal */
static inline bool IsDiagonalTrack(Track track) { return (track == TRACK_DIAG1) || (track == TRACK_DIAG2); }

/* Checks if a given Trackdir is diagonal. */
static inline bool IsDiagonalTrackdir(Trackdir trackdir) { return IsDiagonalTrack(TrackdirToTrack(trackdir)); }

/*
 * Functions quering signals on tiles.
 */

/**
 * Checks for the presence of signals (either way) on the given track on the
 * given rail tile.
 */
static inline bool HasSignalOnTrack(TileIndex tile, Track track)
{
	assert(IsValidTrack(track));
	return ((GetRailTileType(tile) == RAIL_TYPE_SIGNALS) && ((_m[tile].m3 & SignalOnTrack(track)) != 0));
}

/**
 * Checks for the presence of signals along the given trackdir on the given
 * rail tile.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
static inline bool HasSignalOnTrackdir(TileIndex tile, Trackdir trackdir)
{
	assert (IsValidTrackdir(trackdir));
	return (GetRailTileType(tile) == RAIL_TYPE_SIGNALS) && (_m[tile].m3 & SignalAlongTrackdir(trackdir));
}

/**
 * Gets the state of the signal along the given trackdir.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
static inline SignalState GetSignalState(TileIndex tile, Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	assert(HasSignalOnTrack(tile, TrackdirToTrack(trackdir)));
	return ((_m[tile].m2 & SignalAlongTrackdir(trackdir))?SIGNAL_STATE_GREEN:SIGNAL_STATE_RED);
}

/**
 * Gets the type of signal on a given track on a given rail tile with signals.
 *
 * Note that currently, the track argument is not used, since
 * signal types cannot be mixed. This function is trying to be
 * future-compatible, though.
 */
static inline SignalType GetSignalType(TileIndex tile, Track track)
{
	assert(IsValidTrack(track));
	assert(GetRailTileType(tile) == RAIL_TYPE_SIGNALS);
	return (SignalType)(_m[tile].m4 & SIGTYPE_MASK);
}

/**
 * Checks if this tile contains semaphores (returns true) or normal signals
 * (returns false) on the given track. Does not check if there are actually
 * signals on the track, you should use HasSignalsOnTrack() for that.
 *
 * Note that currently, the track argument is not used, since
 * semaphores/electric signals cannot be mixed. This function is trying to be
 * future-compatible, though.
 */
static inline bool HasSemaphores(TileIndex tile, Track track)
{
	assert(IsValidTrack(track));
	return (_m[tile].m4 & SIG_SEMAPHORE_MASK);
}

/**
 * Return the rail type of tile, or INVALID_RAILTYPE if this is no rail tile.
 * Note that there is no check if the given trackdir is actually present on
 * the tile!
 * The given trackdir is used when there are (could be) multiple rail types on
 * one tile.
 */
RailType GetTileRailType(TileIndex tile, Trackdir trackdir);

/**
 * Returns whether the given tile is a level crossing.
 */
static inline bool IsLevelCrossing(TileIndex tile)
{
	return (_m[tile].m5 & 0xF0) == 0x10;
}

/**
 * Gets the transport type of the given track on the given crossing tile.
 * @return  The transport type of the given track, either TRANSPORT_ROAD,
 * TRANSPORT_RAIL.
 */
static inline TransportType GetCrossingTransportType(TileIndex tile, Track track)
{
	/* XXX: Nicer way to write this? */
	switch(track)
	{
		/* When map5 bit 3 is set, the road runs in the y direction (DIAG2) */
		case TRACK_DIAG1:
			return (HASBIT(_m[tile].m5, 3) ? TRANSPORT_RAIL : TRANSPORT_ROAD);
		case TRACK_DIAG2:
			return (HASBIT(_m[tile].m5, 3) ? TRANSPORT_ROAD : TRANSPORT_RAIL);
		default:
			assert(0);
	}
	return INVALID_TRANSPORT;
}

/**
 * Returns a pointer to the Railtype information for a given railtype
 * @param railtype the rail type which the information is requested for
 * @return The pointer to the RailtypeInfo
 */
static inline const RailtypeInfo *GetRailTypeInfo(RailType railtype)
{
	assert(railtype < RAILTYPE_END);
	return &_railtypes[railtype];
}

/**
 * Checks if an engine of the given RailType can drive on a tile with a given
 * RailType. This would normally just be an equality check, but for electric
 * rails (which also support non-electric engines).
 * @return Whether the engine can drive on this tile.
 * @param  enginetype The RailType of the engine we are considering.
 * @param  tiletype   The RailType of the tile we are considering.
 */
static inline bool IsCompatibleRail(RailType enginetype, RailType tiletype)
{
	return HASBIT(GetRailTypeInfo(enginetype)->compatible_railtypes, tiletype);
}

void DrawTrackBits(TileInfo *ti, TrackBits track, bool earth, bool snow, bool flat);

#endif /* RAIL_H */
