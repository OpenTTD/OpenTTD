/* $Id$ */

/** @file rail.h */

#ifndef RAIL_H
#define RAIL_H

#include "direction.h"
#include "tile.h"

typedef enum RailTypes {
	RAILTYPE_BEGIN    = 0,
	RAILTYPE_RAIL     = 0,
	RAILTYPE_ELECTRIC = 1,
	RAILTYPE_MONO     = 2,
	RAILTYPE_MAGLEV   = 3,
	RAILTYPE_END,
	INVALID_RAILTYPE  = 0xFF
} RailType;

typedef byte RailTypeMask;

/** Allow incrementing of Track variables */
DECLARE_POSTFIX_INCREMENT(RailType);
/** Define basic enum properties */
template <> struct EnumPropsT<RailType> : MakeEnumPropsT<RailType, byte, RAILTYPE_BEGIN, RAILTYPE_END, INVALID_RAILTYPE> {};
typedef TinyEnumT<RailType> RailTypeByte;


/** These are used to specify a single track.
 * Can be translated to a trackbit with TrackToTrackbit */
typedef enum Track {
	TRACK_BEGIN = 0,
	TRACK_X     = 0,
	TRACK_Y     = 1,
	TRACK_UPPER = 2,
	TRACK_LOWER = 3,
	TRACK_LEFT  = 4,
	TRACK_RIGHT = 5,
	TRACK_END,
	INVALID_TRACK = 0xFF
} Track;

/** Allow incrementing of Track variables */
DECLARE_POSTFIX_INCREMENT(Track);
/** Define basic enum properties */
template <> struct EnumPropsT<Track> : MakeEnumPropsT<Track, byte, TRACK_BEGIN, TRACK_END, INVALID_TRACK> {};
typedef TinyEnumT<Track> TrackByte;


/** Convert an Axis to the corresponding Track
 * AXIS_X -> TRACK_X
 * AXIS_Y -> TRACK_Y
 * Uses the fact that they share the same internal encoding
 */
static inline Track AxisToTrack(Axis a)
{
	return (Track)a;
}


/** Bitfield corresponding to Track */
typedef enum TrackBits {
	TRACK_BIT_NONE    = 0U,
	TRACK_BIT_X       = 1U << TRACK_X,
	TRACK_BIT_Y       = 1U << TRACK_Y,
	TRACK_BIT_UPPER   = 1U << TRACK_UPPER,
	TRACK_BIT_LOWER   = 1U << TRACK_LOWER,
	TRACK_BIT_LEFT    = 1U << TRACK_LEFT,
	TRACK_BIT_RIGHT   = 1U << TRACK_RIGHT,
	TRACK_BIT_CROSS   = TRACK_BIT_X     | TRACK_BIT_Y,
	TRACK_BIT_HORZ    = TRACK_BIT_UPPER | TRACK_BIT_LOWER,
	TRACK_BIT_VERT    = TRACK_BIT_LEFT  | TRACK_BIT_RIGHT,
	TRACK_BIT_3WAY_NE = TRACK_BIT_X     | TRACK_BIT_UPPER | TRACK_BIT_RIGHT,
	TRACK_BIT_3WAY_SE = TRACK_BIT_Y     | TRACK_BIT_LOWER | TRACK_BIT_RIGHT,
	TRACK_BIT_3WAY_SW = TRACK_BIT_X     | TRACK_BIT_LOWER | TRACK_BIT_LEFT,
	TRACK_BIT_3WAY_NW = TRACK_BIT_Y     | TRACK_BIT_UPPER | TRACK_BIT_LEFT,
	TRACK_BIT_ALL     = TRACK_BIT_CROSS | TRACK_BIT_HORZ  | TRACK_BIT_VERT,
	TRACK_BIT_MASK    = 0x3FU,
	TRACK_BIT_WORMHOLE = 0x40U,
	TRACK_BIT_SPECIAL = 0x80U,
	INVALID_TRACK_BIT = 0xFF
} TrackBits;

/** Define basic enum properties */
template <> struct EnumPropsT<TrackBits> : MakeEnumPropsT<TrackBits, byte, TRACK_BIT_NONE, TRACK_BIT_ALL, INVALID_TRACK_BIT> {};
typedef TinyEnumT<TrackBits> TrackBitsByte;

DECLARE_ENUM_AS_BIT_SET(TrackBits);
DECLARE_ENUM_AS_BIT_INDEX(Track, TrackBits);

/**
 * Maps a Track to the corresponding TrackBits value
 */
static inline TrackBits TrackToTrackBits(Track track)
{
	return (TrackBits)(1 << track);
}


static inline TrackBits AxisToTrackBits(Axis a)
{
	return TrackToTrackBits(AxisToTrack(a));
}


/** These are a combination of tracks and directions. Values are 0-5 in one
 * direction (corresponding to the Track enum) and 8-13 in the other direction. */
typedef enum Trackdirs {
	TRACKDIR_BEGIN    =  0,
	TRACKDIR_X_NE     =  0,
	TRACKDIR_Y_SE     =  1,
	TRACKDIR_UPPER_E  =  2,
	TRACKDIR_LOWER_E  =  3,
	TRACKDIR_LEFT_S   =  4,
	TRACKDIR_RIGHT_S  =  5,
	/* Note the two missing values here. This enables trackdir -> track
	 * conversion by doing (trackdir & 7) */
	TRACKDIR_X_SW     =  8,
	TRACKDIR_Y_NW     =  9,
	TRACKDIR_UPPER_W  = 10,
	TRACKDIR_LOWER_W  = 11,
	TRACKDIR_LEFT_N   = 12,
	TRACKDIR_RIGHT_N  = 13,
	TRACKDIR_END,
	INVALID_TRACKDIR  = 0xFF,
} Trackdir;

/** Define basic enum properties */
template <> struct EnumPropsT<Trackdir> : MakeEnumPropsT<Trackdir, byte, TRACKDIR_BEGIN, TRACKDIR_END, INVALID_TRACKDIR> {};
typedef TinyEnumT<Trackdir> TrackdirByte;

/** These are a combination of tracks and directions. Values are 0-5 in one
 * direction (corresponding to the Track enum) and 8-13 in the other direction. */
typedef enum TrackdirBits {
	TRACKDIR_BIT_NONE     = 0x0000,
	TRACKDIR_BIT_X_NE     = 0x0001,
	TRACKDIR_BIT_Y_SE     = 0x0002,
	TRACKDIR_BIT_UPPER_E  = 0x0004,
	TRACKDIR_BIT_LOWER_E  = 0x0008,
	TRACKDIR_BIT_LEFT_S   = 0x0010,
	TRACKDIR_BIT_RIGHT_S  = 0x0020,
	/* Again, note the two missing values here. This enables trackdir -> track conversion by doing (trackdir & 0xFF) */
	TRACKDIR_BIT_X_SW     = 0x0100,
	TRACKDIR_BIT_Y_NW     = 0x0200,
	TRACKDIR_BIT_UPPER_W  = 0x0400,
	TRACKDIR_BIT_LOWER_W  = 0x0800,
	TRACKDIR_BIT_LEFT_N   = 0x1000,
	TRACKDIR_BIT_RIGHT_N  = 0x2000,
	TRACKDIR_BIT_MASK     = 0x3F3F,
	INVALID_TRACKDIR_BIT  = 0xFFFF,
} TrackdirBits;

/** Define basic enum properties */
template <> struct EnumPropsT<TrackdirBits> : MakeEnumPropsT<TrackdirBits, uint16, TRACKDIR_BIT_NONE, TRACKDIR_BIT_MASK, INVALID_TRACKDIR_BIT> {};
typedef TinyEnumT<TrackdirBits> TrackdirBitsShort;
DECLARE_ENUM_AS_BIT_SET(TrackdirBits);
DECLARE_ENUM_AS_BIT_INDEX(Trackdir, TrackdirBits);

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
		SpriteID crossing;     ///< level crossing, rail in X direction
		SpriteID tunnel;       ///< tunnel sprites base
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
		CursorID rail_ns;
		CursorID rail_swne;
		CursorID rail_ew;
		CursorID rail_nwse;
		CursorID autorail;
		CursorID depot;
		CursorID tunnel;
		CursorID convert;
	} cursor;

	struct {
		StringID toolbar_caption;
	} strings;

	/** sprite number difference between a piece of track on a snowy ground and the corresponding one on normal ground */
	SpriteID snow_offset;

	/** bitmask to the OTHER railtypes on which an engine of THIS railtype generates power */
	RailTypeMask powered_railtypes;

	/** bitmask to the OTHER railtypes on which an engine of THIS railtype can physically travel */
	RailTypeMask compatible_railtypes;

	/**
	 * Offset between the current railtype and normal rail. This means that:<p>
	 * 1) All the sprites in a railset MUST be in the same order. This order
	 *    is determined by normal rail. Check sprites 1005 and following for this order<p>
	 * 2) The position where the railtype is loaded must always be the same, otherwise
	 *    the offset will fail.<p>
	 * @note: Something more flexible might be desirable in the future.
	 */
	SpriteID total_offset;

	/**
	 * Bridge offset
	 */
	SpriteID bridge_offset;

	/**
	 * Offset to add to ground sprite when drawing custom waypoints / stations
	 */
	byte custom_ground_offset;
} RailtypeInfo;

extern RailtypeInfo _railtypes[RAILTYPE_END];

// these are the maximums used for updating signal blocks, and checking if a depot is in a pbs block
enum {
	NUM_SSD_ENTRY = 256, // max amount of blocks
	NUM_SSD_STACK =  32, // max amount of blocks to check recursively
};

/**
 * Maps a Trackdir to the corresponding TrackdirBits value
 */
static inline TrackdirBits TrackdirToTrackdirBits(Trackdir trackdir) { return (TrackdirBits)(1 << trackdir); }

/**
* Removes first Track from TrackBits and returns it
*/
static inline Track RemoveFirstTrack(TrackBits *tracks)
{
	if (*tracks != TRACK_BIT_NONE && *tracks != INVALID_TRACK_BIT) {
		Track first = (Track)FIND_FIRST_BIT(*tracks);
		*tracks = ClrBitT(*tracks, first);
		return first;
	}
	return INVALID_TRACK;
}

/**
* Removes first Trackdir from TrackdirBits and returns it
*/
static inline Trackdir RemoveFirstTrackdir(TrackdirBits *trackdirs)
{
	if (*trackdirs != TRACKDIR_BIT_NONE && *trackdirs != INVALID_TRACKDIR_BIT) {
		Trackdir first = (Trackdir)FindFirstBit2x64(*trackdirs);
		*trackdirs = ClrBitT(*trackdirs, first);
		return first;
	}
	return INVALID_TRACKDIR;
}

/**
* Returns first Track from TrackBits or INVALID_TRACK
*/
static inline Track FindFirstTrack(TrackBits tracks)
{
	return (tracks != TRACK_BIT_NONE && tracks != INVALID_TRACK_BIT) ? (Track)FIND_FIRST_BIT(tracks) : INVALID_TRACK;
}

/**
* Converts TrackBits to Track. TrackBits must contain just one Track or INVALID_TRACK_BIT!
*/
static inline Track TrackBitsToTrack(TrackBits tracks)
{
	assert (tracks == INVALID_TRACK_BIT || (tracks != TRACK_BIT_NONE && KILL_FIRST_BIT(tracks) == 0));
	return tracks != INVALID_TRACK_BIT ? (Track)FIND_FIRST_BIT(tracks) : INVALID_TRACK;
}

/**
* Returns first Trackdir from TrackdirBits or INVALID_TRACKDIR
*/
static inline Trackdir FindFirstTrackdir(TrackdirBits trackdirs)
{
	assert((trackdirs & ~TRACKDIR_BIT_MASK) == TRACKDIR_BIT_NONE);
	return (trackdirs != TRACKDIR_BIT_NONE) ? (Trackdir)FindFirstBit2x64(trackdirs) : INVALID_TRACKDIR;
}

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
 * Functions describing logical relations between Tracks, TrackBits, Trackdirs
 * TrackdirBits, Direction and DiagDirections.
 *
 * TODO: Add #unndefs or something similar to remove the arrays used below
 * from the global scope and expose direct uses of them.
 */

/**
 * Maps a trackdir to the reverse trackdir.
 */
static inline Trackdir ReverseTrackdir(Trackdir trackdir)
{
	assert(trackdir != INVALID_TRACKDIR);
	return (Trackdir)(trackdir ^ 8);
}

/**
 * Returns the Track that a given Trackdir represents
 */
static inline Track TrackdirToTrack(Trackdir trackdir) { return (Track)(trackdir & 0x7); }

/**
 * Returns a Trackdir for the given Track. Since every Track corresponds to
 * two Trackdirs, we choose the one which points between NE and S.
 * Note that the actual implementation is quite futile, but this might change
 * in the future.
 */
static inline Trackdir TrackToTrackdir(Track track) { return (Trackdir)track; }

/**
 * Returns a TrackdirBit mask that contains the two TrackdirBits that
 * correspond with the given Track (one for each direction).
 */
static inline TrackdirBits TrackToTrackdirBits(Track track)
{
	Trackdir td = TrackToTrackdir(track);
	return (TrackdirBits)(TrackdirToTrackdirBits(td) | TrackdirToTrackdirBits(ReverseTrackdir(td)));
}

/**
 * Discards all directional information from the given TrackdirBits. Any
 * Track which is present in either direction will be present in the result.
 */
static inline TrackBits TrackdirBitsToTrackBits(TrackdirBits bits)
{
	return (TrackBits)((bits | (bits >> 8)) & TRACK_BIT_MASK);
}

/**
 * Maps a trackdir to the trackdir that you will end up on if you go straight
 * ahead. This will be the same trackdir for diagonal trackdirs, but a
 * different (alternating) one for straight trackdirs
 */
static inline Trackdir NextTrackdir(Trackdir trackdir)
{
	extern const Trackdir _next_trackdir[TRACKDIR_END];
	return _next_trackdir[trackdir];
}

/**
 * Maps a track to all tracks that make 90 deg turns with it.
 */
static inline TrackBits TrackCrossesTracks(Track track)
{
	extern const TrackBits _track_crosses_tracks[TRACK_END];
	return _track_crosses_tracks[track];
}

/**
 * Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir.
 */
static inline DiagDirection TrackdirToExitdir(Trackdir trackdir)
{
	extern const DiagDirection _trackdir_to_exitdir[TRACKDIR_END];
	return _trackdir_to_exitdir[trackdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the exit in the given direction.
 */
static inline Trackdir TrackExitdirToTrackdir(Track track, DiagDirection diagdir)
{
	extern const Trackdir _track_exitdir_to_trackdir[TRACK_END][DIAGDIR_END];
	return _track_exitdir_to_trackdir[track][diagdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the exit in the given direction.
 */
static inline Trackdir TrackEnterdirToTrackdir(Track track, DiagDirection diagdir)
{
	extern const Trackdir _track_enterdir_to_trackdir[TRACK_END][DIAGDIR_END];
	return _track_enterdir_to_trackdir[track][diagdir];
}

/**
 * Maps a track and a full (8-way) direction to the trackdir that represents
 * the track running in the given direction.
 */
static inline Trackdir TrackDirectionToTrackdir(Track track, Direction dir)
{
	extern const Trackdir _track_direction_to_trackdir[TRACK_END][DIR_END];
	return _track_direction_to_trackdir[track][dir];
}

/**
 * Maps a (4-way) direction to the diagonal trackdir that runs in that
 * direction.
 */
static inline Trackdir DiagdirToDiagTrackdir(DiagDirection diagdir)
{
	extern const Trackdir _dir_to_diag_trackdir[DIAGDIR_END];
	return _dir_to_diag_trackdir[diagdir];
}

extern const TrackdirBits _exitdir_reaches_trackdirs[DIAGDIR_END];

/**
 * Returns all trackdirs that can be reached when entering a tile from a given
 * (diagonal) direction. This will obviously include 90 degree turns, since no
 * information is available about the exact angle of entering */
static inline TrackdirBits DiagdirReachesTrackdirs(DiagDirection diagdir) { return _exitdir_reaches_trackdirs[diagdir]; }

/**
 * Returns all tracks that can be reached when entering a tile from a given
 * (diagonal) direction. This will obviously include 90 degree turns, since no
 * information is available about the exact angle of entering */
static inline TrackBits DiagdirReachesTracks(DiagDirection diagdir) { return TrackdirBitsToTrackBits(DiagdirReachesTrackdirs(diagdir)); }

/**
 * Maps a trackdir to the trackdirs that can be reached from it (ie, when
 * entering the next tile. This will include 90 degree turns!
 */
static inline TrackdirBits TrackdirReachesTrackdirs(Trackdir trackdir) { return _exitdir_reaches_trackdirs[TrackdirToExitdir(trackdir)]; }
/* Note that there is no direct table for this function (there used to be),
 * but it uses two simpeler tables to achieve the result */


/**
 * Maps a trackdir to all trackdirs that make 90 deg turns with it.
 */
static inline TrackdirBits TrackdirCrossesTrackdirs(Trackdir trackdir) {
	extern const TrackdirBits _track_crosses_trackdirs[TRACKDIR_END];
	return _track_crosses_trackdirs[TrackdirToTrack(trackdir)];
}


/* Checks if a given Track is diagonal */
static inline bool IsDiagonalTrack(Track track) { return (track == TRACK_X) || (track == TRACK_Y); }

/* Checks if a given Trackdir is diagonal. */
static inline bool IsDiagonalTrackdir(Trackdir trackdir) { return IsDiagonalTrack(TrackdirToTrack(trackdir)); }


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

static inline bool HasPowerOnRail(RailType enginetype, RailType tiletype)
{
	return HASBIT(GetRailTypeInfo(enginetype)->powered_railtypes, tiletype);
}

/**
 * Checks if the given tracks overlap, ie form a crossing. Basically this
 * means when there is more than one track on the tile, exept when there are
 * two parallel tracks.
 * @param  bits The tracks present.
 * @return Whether the tracks present overlap in any way.
 */
static inline bool TracksOverlap(TrackBits bits)
{
	/* With no, or only one track, there is no overlap */
	if (bits == 0 || KILL_FIRST_BIT(bits) == 0) return false;
	/* We know that there are at least two tracks present. When there are more
	 * than 2 tracks, they will surely overlap. When there are two, they will
	 * always overlap unless they are lower & upper or right & left. */
	return bits != TRACK_BIT_HORZ && bits != TRACK_BIT_VERT;
}

void DrawTrainDepotSprite(int x, int y, int image, RailType railtype);
void DrawDefaultWaypointSprite(int x, int y, RailType railtype);

/**
 * Draws overhead wires and pylons for electric railways.
 * @param ti The TileInfo struct of the tile being drawn
 * @see DrawCatenaryRailway
 */
void DrawCatenary(const TileInfo *ti);

uint GetRailFoundation(Slope tileh, TrackBits bits);

int32 SettingsDisableElrail(int32 p1); ///< _patches.disable_elrail callback

#endif /* RAIL_H */
