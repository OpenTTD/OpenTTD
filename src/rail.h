/* $Id$ */

/** @file rail.h */

#ifndef RAIL_H
#define RAIL_H

#include "gfx.h"
#include "direction.h"
#include "tile.h"
#include "variables.h"

/**
 * Enumeration for all possible railtypes.
 *
 * This enumeration defines all 4 possible railtypes.
 */
enum RailType {
	RAILTYPE_BEGIN    = 0,          ///< Used for iterations
	RAILTYPE_RAIL     = 0,          ///< Standard non-electric rails
	RAILTYPE_ELECTRIC = 1,          ///< Electric rails
	RAILTYPE_MONO     = 2,          ///< Monorail
	RAILTYPE_MAGLEV   = 3,          ///< Maglev
	RAILTYPE_END,                   ///< Used for iterations
	INVALID_RAILTYPE  = 0xFF        ///< Flag for invalid railtype
};

typedef byte RailTypeMask;

/** Allow incrementing of Track variables */
DECLARE_POSTFIX_INCREMENT(RailType);
/** Define basic enum properties */
template <> struct EnumPropsT<RailType> : MakeEnumPropsT<RailType, byte, RAILTYPE_BEGIN, RAILTYPE_END, INVALID_RAILTYPE> {};
typedef TinyEnumT<RailType> RailTypeByte;


/**
 * These are used to specify a single track.
 * Can be translated to a trackbit with TrackToTrackbit
 */
enum Track {
	TRACK_BEGIN = 0,        ///< Used for iterations
	TRACK_X     = 0,        ///< Track along the x-axis (north-east to south-west)
	TRACK_Y     = 1,        ///< Track along the y-axis (north-west to south-east)
	TRACK_UPPER = 2,        ///< Track in the upper corner of the tile (north)
	TRACK_LOWER = 3,        ///< Track in the lower corner of the tile (south)
	TRACK_LEFT  = 4,        ///< Track in the left corner of the tile (west)
	TRACK_RIGHT = 5,        ///< Track in the right corner of the tile (east)
	TRACK_END,              ///< Used for iterations
	INVALID_TRACK = 0xFF    ///< Flag for an invalid track
};

/** Allow incrementing of Track variables */
DECLARE_POSTFIX_INCREMENT(Track);
/** Define basic enum properties */
template <> struct EnumPropsT<Track> : MakeEnumPropsT<Track, byte, TRACK_BEGIN, TRACK_END, INVALID_TRACK> {};
typedef TinyEnumT<Track> TrackByte;


/**
 * Convert an Axis to the corresponding Track
 * AXIS_X -> TRACK_X
 * AXIS_Y -> TRACK_Y
 * Uses the fact that they share the same internal encoding
 *
 * @param a the axis to convert
 * @return the track corresponding to the axis
 */
static inline Track AxisToTrack(Axis a)
{
	return (Track)a;
}


/** Bitfield corresponding to Track */
enum TrackBits {
	TRACK_BIT_NONE    = 0U,                                                 ///< No track
	TRACK_BIT_X       = 1U << TRACK_X,                                      ///< X-axis track
	TRACK_BIT_Y       = 1U << TRACK_Y,                                      ///< Y-axis track
	TRACK_BIT_UPPER   = 1U << TRACK_UPPER,                                  ///< Upper track
	TRACK_BIT_LOWER   = 1U << TRACK_LOWER,                                  ///< Lower track
	TRACK_BIT_LEFT    = 1U << TRACK_LEFT,                                   ///< Left track
	TRACK_BIT_RIGHT   = 1U << TRACK_RIGHT,                                  ///< Right track
	TRACK_BIT_CROSS   = TRACK_BIT_X     | TRACK_BIT_Y,                      ///< X-Y-axis cross
	TRACK_BIT_HORZ    = TRACK_BIT_UPPER | TRACK_BIT_LOWER,                  ///< Upper and lower track
	TRACK_BIT_VERT    = TRACK_BIT_LEFT  | TRACK_BIT_RIGHT,                  ///< Left and right track
	TRACK_BIT_3WAY_NE = TRACK_BIT_X     | TRACK_BIT_UPPER | TRACK_BIT_RIGHT,///< "Arrow" to the north-east
	TRACK_BIT_3WAY_SE = TRACK_BIT_Y     | TRACK_BIT_LOWER | TRACK_BIT_RIGHT,///< "Arrow" to the south-east
	TRACK_BIT_3WAY_SW = TRACK_BIT_X     | TRACK_BIT_LOWER | TRACK_BIT_LEFT, ///< "Arrow" to the south-west
	TRACK_BIT_3WAY_NW = TRACK_BIT_Y     | TRACK_BIT_UPPER | TRACK_BIT_LEFT, ///< "Arrow" to the north-west
	TRACK_BIT_ALL     = TRACK_BIT_CROSS | TRACK_BIT_HORZ  | TRACK_BIT_VERT, ///< All possible tracks
	TRACK_BIT_MASK    = 0x3FU,                                              ///< Bitmask for the first 6 bits
	TRACK_BIT_WORMHOLE = 0x40U,                                             ///< Bitflag for a wormhole (used for tunnels)
	TRACK_BIT_DEPOT   = 0x80U,                                              ///< Bitflag for a depot
	INVALID_TRACK_BIT = 0xFF                                                ///< Flag for an invalid trackbits value
};

/** Define basic enum properties */
template <> struct EnumPropsT<TrackBits> : MakeEnumPropsT<TrackBits, byte, TRACK_BIT_NONE, TRACK_BIT_ALL, INVALID_TRACK_BIT> {};
typedef TinyEnumT<TrackBits> TrackBitsByte;

DECLARE_ENUM_AS_BIT_SET(TrackBits);

/**
 * Maps a Track to the corresponding TrackBits value
 * @param track the track to convert
 * @return the converted TrackBits value of the track
 */
static inline TrackBits TrackToTrackBits(Track track)
{
	return (TrackBits)(1 << track);
}

/**
 * Maps an Axis to the corresponding TrackBits value
 * @param a the axis to convert
 * @return the converted TrackBits value of the axis
 */
static inline TrackBits AxisToTrackBits(Axis a)
{
	return TrackToTrackBits(AxisToTrack(a));
}

/**
 * Returns a single horizontal/vertical trackbit, that is in a specific tile corner.
 *
 * @param corner The corner of a tile.
 * @return The TrackBits of the track in the corner.
 */
static inline TrackBits CornerToTrackBits(Corner corner)
{
	extern const TrackBits _corner_to_trackbits[];
	assert(IsValidCorner(corner));
	return _corner_to_trackbits[corner];
}


/**
 * Enumeration for tracks and directions.
 *
 * These are a combination of tracks and directions. Values are 0-5 in one
 * direction (corresponding to the Track enum) and 8-13 in the other direction.
 * 6, 7, 14 and 15 are used to encode the reversing of road vehicles. Those
 * reversing track dirs are not considered to be 'valid' except in a small
 * corner in the road vehicle controller.
 */
enum Trackdir {
	TRACKDIR_BEGIN    =  0,         ///< Used for iterations
	TRACKDIR_X_NE     =  0,         ///< X-axis and direction to north-east
	TRACKDIR_Y_SE     =  1,         ///< Y-axis and direction to south-east
	TRACKDIR_UPPER_E  =  2,         ///< Upper track and direction to east
	TRACKDIR_LOWER_E  =  3,         ///< Lower track and direction to east
	TRACKDIR_LEFT_S   =  4,         ///< Left track and direction to south
	TRACKDIR_RIGHT_S  =  5,         ///< Right track and direction to south
	TRACKDIR_RVREV_NE =  6,         ///< (Road vehicle) reverse direction north-east
	TRACKDIR_RVREV_SE =  7,         ///< (Road vehicle) reverse direction south-east
	TRACKDIR_X_SW     =  8,         ///< X-axis and direction to south-west
	TRACKDIR_Y_NW     =  9,         ///< Y-axis and direction to north-west
	TRACKDIR_UPPER_W  = 10,         ///< Upper track and direction to west
	TRACKDIR_LOWER_W  = 11,         ///< Lower track and direction to west
	TRACKDIR_LEFT_N   = 12,         ///< Left track and direction to north
	TRACKDIR_RIGHT_N  = 13,         ///< Right track and direction to north
	TRACKDIR_RVREV_SW = 14,         ///< (Road vehicle) reverse direction south-west
	TRACKDIR_RVREV_NW = 15,         ///< (Road vehicle) reverse direction north-west
	TRACKDIR_END,                   ///< Used for iterations
	INVALID_TRACKDIR  = 0xFF,       ///< Flag for an invalid trackdir
};

/** Define basic enum properties */
template <> struct EnumPropsT<Trackdir> : MakeEnumPropsT<Trackdir, byte, TRACKDIR_BEGIN, TRACKDIR_END, INVALID_TRACKDIR> {};
typedef TinyEnumT<Trackdir> TrackdirByte;

/**
 * Enumeration of bitmasks for the TrackDirs
 *
 * These are a combination of tracks and directions. Values are 0-5 in one
 * direction (corresponding to the Track enum) and 8-13 in the other direction.
 */
enum TrackdirBits {
	TRACKDIR_BIT_NONE     = 0x0000, ///< No track build
	TRACKDIR_BIT_X_NE     = 0x0001, ///< Track x-axis, direction north-east
	TRACKDIR_BIT_Y_SE     = 0x0002, ///< Track y-axis, direction south-east
	TRACKDIR_BIT_UPPER_E  = 0x0004, ///< Track upper, direction east
	TRACKDIR_BIT_LOWER_E  = 0x0008, ///< Track lower, direction east
	TRACKDIR_BIT_LEFT_S   = 0x0010, ///< Track left, direction south
	TRACKDIR_BIT_RIGHT_S  = 0x0020, ///< Track right, direction south
	/* Again, note the two missing values here. This enables trackdir -> track conversion by doing (trackdir & 0xFF) */
	TRACKDIR_BIT_X_SW     = 0x0100, ///< Track x-axis, direction south-west
	TRACKDIR_BIT_Y_NW     = 0x0200, ///< Track y-axis, direction north-west
	TRACKDIR_BIT_UPPER_W  = 0x0400, ///< Track upper, direction west
	TRACKDIR_BIT_LOWER_W  = 0x0800, ///< Track lower, direction west
	TRACKDIR_BIT_LEFT_N   = 0x1000, ///< Track left, direction north
	TRACKDIR_BIT_RIGHT_N  = 0x2000, ///< Track right, direction north
	TRACKDIR_BIT_MASK     = 0x3F3F, ///< Bitmask for bit-operations
	INVALID_TRACKDIR_BIT  = 0xFFFF, ///< Flag for an invalid trackdirbit value
};

/** Define basic enum properties */
template <> struct EnumPropsT<TrackdirBits> : MakeEnumPropsT<TrackdirBits, uint16, TRACKDIR_BIT_NONE, TRACKDIR_BIT_MASK, INVALID_TRACKDIR_BIT> {};
typedef TinyEnumT<TrackdirBits> TrackdirBitsShort;
DECLARE_ENUM_AS_BIT_SET(TrackdirBits);

/** This struct contains all the info that is needed to draw and construct tracks.
 */
struct RailtypeInfo {
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
		CursorID rail_ns;    ///< Cursor for building rail in N-S direction
		CursorID rail_swne;  ///< Cursor for building rail in X direction
		CursorID rail_ew;    ///< Cursor for building rail in E-W direction
		CursorID rail_nwse;  ///< Cursor for building rail in Y direction
		CursorID autorail;   ///< Cursor for autorail tool
		CursorID depot;      ///< Cursor for building a depot
		CursorID tunnel;     ///< Cursor for building a tunnel
		CursorID convert;    ///< Cursor for converting track
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
	 *    the offset will fail.
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
};


/** these are the maximums used for updating signal blocks, and checking if a depot is in a pbs block */
enum {
	NUM_SSD_ENTRY = 256, ///< max amount of blocks
	NUM_SSD_STACK =  32, ///< max amount of blocks to check recursively
};

/**
 * Maps a Trackdir to the corresponding TrackdirBits value
 * @param trackdir the track direction to convert
 * @return the converted TrackdirBits value
 */
static inline TrackdirBits TrackdirToTrackdirBits(Trackdir trackdir)
{
	return (TrackdirBits)(1 << trackdir);
}

/**
 * Removes first Track from TrackBits and returns it
 *
 * This function searchs for the first bit in the TrackBits,
 * remove this bit from the parameter and returns the found
 * bit as Track value. It returns INVALID_TRACK if the
 * parameter was TRACK_BIT_NONE or INVALID_TRACK_BIT. This
 * is basically used in while-loops to get up to 6 possible
 * tracks on a tile until the parameter becomes TRACK_BIT_NONE.
 *
 * @param tracks The value with the TrackBits
 * @return The first Track from the TrackBits value
 * @see FindFirstTrack
 */
static inline Track RemoveFirstTrack(TrackBits *tracks)
{
	if (*tracks != TRACK_BIT_NONE && *tracks != INVALID_TRACK_BIT) {
		Track first = (Track)FIND_FIRST_BIT(*tracks);
		ClrBitT(*tracks, first);
		return first;
	}
	return INVALID_TRACK;
}

/**
 * Removes first Trackdir from TrackdirBits and returns it
 *
 * This function searchs for the first bit in the TrackdirBits parameter,
 * remove this bit from the parameter and returns the fnound bit as
 * Trackdir value. It returns INVALID_TRACKDIR if the trackdirs is
 * TRACKDIR_BIT_NONE or INVALID_TRACKDIR_BIT. This is basically used in a
 * while-loop to get all track-directions step by step until the value
 * reaches TRACKDIR_BIT_NONE.
 *
 * @param trackdirs The value with the TrackdirBits
 * @return The first Trackdir from the TrackdirBits value
 * @see FindFirstTrackdir
 */
static inline Trackdir RemoveFirstTrackdir(TrackdirBits *trackdirs)
{
	if (*trackdirs != TRACKDIR_BIT_NONE && *trackdirs != INVALID_TRACKDIR_BIT) {
		Trackdir first = (Trackdir)FindFirstBit2x64(*trackdirs);
		ClrBitT(*trackdirs, first);
		return first;
	}
	return INVALID_TRACKDIR;
}

/**
 * Returns first Track from TrackBits or INVALID_TRACK
 *
 * This function returns the first Track found in the TrackBits value as Track-value.
 * It returns INVALID_TRACK if the parameter is TRACK_BIT_NONE or INVALID_TRACK_BIT.
 *
 * @param tracks The TrackBits value
 * @return The first Track found or INVALID_TRACK
 * @see RemoveFirstTrack
 */
static inline Track FindFirstTrack(TrackBits tracks)
{
	return (tracks != TRACK_BIT_NONE && tracks != INVALID_TRACK_BIT) ? (Track)FIND_FIRST_BIT(tracks) : INVALID_TRACK;
}

/**
 * Converts TrackBits to Track.
 *
 * This function converts a TrackBits value to a Track value. As it
 * is not possible to convert two or more tracks to one track the
 * parameter must contain only one track or be the INVALID_TRACK_BIT value.
 *
 * @param tracks The TrackBits value to convert
 * @return The Track from the value or INVALID_TRACK
 * @pre tracks must contains only one Track or be INVALID_TRACK_BIT
 */
static inline Track TrackBitsToTrack(TrackBits tracks)
{
	assert(tracks == INVALID_TRACK_BIT || (tracks != TRACK_BIT_NONE && KILL_FIRST_BIT(tracks & TRACK_BIT_MASK) == 0));
	return tracks != INVALID_TRACK_BIT ? (Track)FIND_FIRST_BIT(tracks & TRACK_BIT_MASK) : INVALID_TRACK;
}

/**
 * Returns first Trackdir from TrackdirBits or INVALID_TRACKDIR
 *
 * This function returns the first Trackdir in the given TrackdirBits value or
 * INVALID_TRACKDIR if the value is TRACKDIR_BIT_NONE. The TrackdirBits must
 * not be INVALID_TRACKDIR_BIT.
 *
 * @param trackdirs The TrackdirBits value
 * @return The first Trackdir from the TrackdirBits or INVALID_TRACKDIR on TRACKDIR_BIT_NONE.
 * @pre trackdirs must not be INVALID_TRACKDIR_BIT
 * @see RemoveFirstTrackdir
 */
static inline Trackdir FindFirstTrackdir(TrackdirBits trackdirs)
{
	assert((trackdirs & ~TRACKDIR_BIT_MASK) == TRACKDIR_BIT_NONE);
	return (trackdirs != TRACKDIR_BIT_NONE) ? (Trackdir)FindFirstBit2x64(trackdirs) : INVALID_TRACKDIR;
}

/**
 * Checks if a Track is valid.
 *
 * @param track The value to check
 * @return true if the given value is a valid track.
 * @note Use this in an assert()
 */
static inline bool IsValidTrack(Track track)
{
	return track < TRACK_END;
}

/**
 * Checks if a Trackdir is valid.
 *
 * @param trackdir The value to check
 * @return true if the given valie is a valid Trackdir
 * @note Use this in an assert()
 */
static inline bool IsValidTrackdir(Trackdir trackdir)
{
	return (TrackdirToTrackdirBits(trackdir) & TRACKDIR_BIT_MASK) != 0;
}

/*
 * Functions to map tracks to the corresponding bits in the signal
 * presence/status bytes in the map. You should not use these directly, but
 * wrapper functions below instead. XXX: Which are these?
 */

/**
 * Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction along with the trackdir.
 */
static inline byte SignalAlongTrackdir(Trackdir trackdir)
{
	extern const byte _signal_along_trackdir[TRACKDIR_END];
	return _signal_along_trackdir[trackdir];
}

/**
 * Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction against the trackdir.
 */
static inline byte SignalAgainstTrackdir(Trackdir trackdir)
{
	extern const byte _signal_against_trackdir[TRACKDIR_END];
	return _signal_against_trackdir[trackdir];
}

/**
 * Maps a Track to the bits that store the status of the two signals that can
 * be present on the given track.
 */
static inline byte SignalOnTrack(Track track)
{
	extern const byte _signal_on_track[TRACK_END];
	return _signal_on_track[track];
}


/*
 * Functions describing logical relations between Tracks, TrackBits, Trackdirs
 * TrackdirBits, Direction and DiagDirections.
 */

/**
 * Maps a trackdir to the reverse trackdir.
 *
 * Returns the reverse trackdir of a Trackdir value. The reverse trackdir
 * is the same track with the other direction on it.
 *
 * @param trackdir The Trackdir value
 * @return The reverse trackdir
 * @pre trackdir must not be INVALID_TRACKDIR
 */
static inline Trackdir ReverseTrackdir(Trackdir trackdir)
{
	assert(trackdir != INVALID_TRACKDIR);
	return (Trackdir)(trackdir ^ 8);
}

/**
 * Returns the Track that a given Trackdir represents
 *
 * This function filters the Track which is used in the Trackdir value and
 * returns it as a Track value.
 *
 * @param trackdir The trackdir value
 * @return The Track which is used in the value
 */
static inline Track TrackdirToTrack(Trackdir trackdir)
{
	return (Track)(trackdir & 0x7);
}

/**
 * Returns a Trackdir for the given Track
 *
 * Since every Track corresponds to two Trackdirs, we choose the
 * one which points between NE and S. Note that the actual
 * implementation is quite futile, but this might change
 * in the future.
 *
 * @param track The given Track
 * @return The Trackdir from the given Track
 */
static inline Trackdir TrackToTrackdir(Track track)
{
	return (Trackdir)track;
}

/**
 * Returns a TrackdirBit mask from a given Track
 *
 * The TrackdirBit mask contains the two TrackdirBits that
 * correspond with the given Track (one for each direction).
 *
 * @param track The track to get the TrackdirBits from
 * @return The TrackdirBits which the selected tracks
 */
static inline TrackdirBits TrackToTrackdirBits(Track track)
{
	Trackdir td = TrackToTrackdir(track);
	return (TrackdirBits)(TrackdirToTrackdirBits(td) | TrackdirToTrackdirBits(ReverseTrackdir(td)));
}

/**
 * Discards all directional information from a TrackdirBits value
 *
 * Any Track which is present in either direction will be present in the result.
 *
 * @param bits The TrackdirBits to get the TrackBits from
 * @return The TrackBits
 */
static inline TrackBits TrackdirBitsToTrackBits(TrackdirBits bits)
{
	return (TrackBits)((bits | (bits >> 8)) & TRACK_BIT_MASK);
}

/**
 * Maps a trackdir to the trackdir that you will end up on if you go straight
 * ahead.
 *
 * This will be the same trackdir for diagonal trackdirs, but a
 * different (alternating) one for straight trackdirs
 *
 * @param trackdir The given trackdir
 * @return The next Trackdir value of the next tile.
 */
static inline Trackdir NextTrackdir(Trackdir trackdir)
{
	extern const Trackdir _next_trackdir[TRACKDIR_END];
	return _next_trackdir[trackdir];
}

/**
 * Maps a track to all tracks that make 90 deg turns with it.
 *
 * For the diagonal directions these are the complement of the
 * direction, for the straight directions these are the
 * two vertical or horizontal tracks, depend on the given direction
 *
 * @param track The given track
 * @return The TrackBits with the tracks marked which cross the given track by 90 deg.
 */
static inline TrackBits TrackCrossesTracks(Track track)
{
	extern const TrackBits _track_crosses_tracks[TRACK_END];
	return _track_crosses_tracks[track];
}

/**
 * Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir.
 *
 * For the diagonal directions these are the same directions. For
 * the straight directions these are the directions from the imagined
 * base-tile to the bordering tile which will be joined if the given
 * straight direction is leaved from the base-tile.
 *
 * @param trackdir The given track direction
 * @return The direction which points to the resulting tile if following the Trackdir
 */
static inline DiagDirection TrackdirToExitdir(Trackdir trackdir)
{
	extern const DiagDirection _trackdir_to_exitdir[TRACKDIR_END];
	return _trackdir_to_exitdir[trackdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the exit in the given direction.
 *
 * For the diagonal tracks the resulting track direction are clear for a given
 * DiagDirection. It either matches the direction or it returns INVALID_TRACKDIR,
 * as a TRACK_X cannot be applied with DIAG_SE.
 * For the straight tracks the resulting track direction will be the
 * direction which the DiagDirection is pointing. But this will be INVALID_TRACKDIR
 * if the DiagDirection is pointing 'away' the track.
 *
 * @param track The track to applie an direction on
 * @param diagdir The DiagDirection to applie on
 * @return The resulting track direction or INVALID_TRACKDIR if not possible.
 */
static inline Trackdir TrackExitdirToTrackdir(Track track, DiagDirection diagdir)
{
	extern const Trackdir _track_exitdir_to_trackdir[TRACK_END][DIAGDIR_END];
	return _track_exitdir_to_trackdir[track][diagdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the entry in the given direction.
 *
 * For the diagonal tracks the return value is clear, its either the matching
 * track direction or INVALID_TRACKDIR.
 * For the straight tracks this returns the track direction which results if
 * you follow the DiagDirection and then turn by 45 deg left or right on the
 * next tile. The new direction on the new track will be the returning Trackdir
 * value. If the parameters makes no sense like the track TRACK_UPPER and the
 * diraction DIAGDIR_NE (target track cannot be reached) this function returns
 * INVALID_TRACKDIR.
 *
 * @param track The target track
 * @param diagdir The direction to "come from"
 * @return the resulting Trackdir or INVALID_TRACKDIR if not possible.
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
 *
 * @param diagdir The direction
 * @return The resulting Trackdir direction
 */
static inline Trackdir DiagdirToDiagTrackdir(DiagDirection diagdir)
{
	extern const Trackdir _dir_to_diag_trackdir[DIAGDIR_END];
	return _dir_to_diag_trackdir[diagdir];
}

/**
 * Returns all trackdirs that can be reached when entering a tile from a given
 * (diagonal) direction.
 *
 * This will obviously include 90 degree turns, since no information is available
 * about the exact angle of entering
 *
 * @param diagdir The joining direction
 * @return The TrackdirBits which can be used from the given direction
 * @see DiagdirReachesTracks
 */
static inline TrackdirBits DiagdirReachesTrackdirs(DiagDirection diagdir)
{
	extern const TrackdirBits _exitdir_reaches_trackdirs[DIAGDIR_END];
	return _exitdir_reaches_trackdirs[diagdir];
}

/**
 * Returns all tracks that can be reached when entering a tile from a given
 * (diagonal) direction.
 *
 * This will obviously include 90 degree turns, since no
 * information is available about the exact angle of entering
 *
 * @param diagdir The joining irection
 * @return The tracks which can be used
 * @see DiagdirReachesTrackdirs
 */
static inline TrackBits DiagdirReachesTracks(DiagDirection diagdir) { return TrackdirBitsToTrackBits(DiagdirReachesTrackdirs(diagdir)); }

/**
 * Maps a trackdir to the trackdirs that can be reached from it (ie, when
 * entering the next tile.
 *
 * This will include 90 degree turns!
 *
 * @param trackdir The track direction which will be leaved
 * @return The track directions which can be used from this direction (in the next tile)
 */
static inline TrackdirBits TrackdirReachesTrackdirs(Trackdir trackdir)
{
	extern const TrackdirBits _exitdir_reaches_trackdirs[DIAGDIR_END];
	return _exitdir_reaches_trackdirs[TrackdirToExitdir(trackdir)];
}
/* Note that there is no direct table for this function (there used to be),
 * but it uses two simpeler tables to achieve the result */

/**
 * Maps a trackdir to all trackdirs that make 90 deg turns with it.
 *
 * For the diagonal tracks this returns the track direction bits
 * of the other axis in both directions, which cannot be joined by
 * the given track direction.
 * For the straight tracks this returns all possible 90 deg turns
 * either on the current tile (which no train can joined) or on the
 * bordering tiles.
 *
 * @param trackdir The track direction
 * @return The TrackdirBits which are (more or less) 90 deg turns.
 */
static inline TrackdirBits TrackdirCrossesTrackdirs(Trackdir trackdir)
{
	extern const TrackdirBits _track_crosses_trackdirs[TRACKDIR_END];
	return _track_crosses_trackdirs[TrackdirToTrack(trackdir)];
}

/**
 * Checks if a given Track is diagonal
 *
 * @param track The given track to check
 * @return true if diagonal, else false
 */
static inline bool IsDiagonalTrack(Track track)
{
	return (track == TRACK_X) || (track == TRACK_Y);
}

/**
 * Checks if a given Trackdir is diagonal.
 *
 * @param trackdir The given trackdir
 * @return true if the trackdir use a diagonal track
 */
static inline bool IsDiagonalTrackdir(Trackdir trackdir)
{
	return IsDiagonalTrack(TrackdirToTrack(trackdir));
}

/**
 * Returns a pointer to the Railtype information for a given railtype
 * @param railtype the rail type which the information is requested for
 * @return The pointer to the RailtypeInfo
 */
static inline const RailtypeInfo *GetRailTypeInfo(RailType railtype)
{
	extern RailtypeInfo _railtypes[RAILTYPE_END];
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

/**
 * Checks if an engine of the given RailType got power on a tile with a given
 * RailType. This would normally just be an equality check, but for electric
 * rails (which also support non-electric engines).
 * @return Whether the engine got power on this tile.
 * @param  enginetype The RailType of the engine we are considering.
 * @param  tiletype   The RailType of the tile we are considering.
 */
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


extern int _railtype_cost_multiplier[RAILTYPE_END];
extern const int _default_railtype_cost_multiplier[RAILTYPE_END];

/**
 * Returns the cost of building the specified railtype.
 * @param railtype The railtype being built.
 * @return The cost multiplier.
 */
static inline Money RailBuildCost(RailType railtype)
{
	assert(railtype < RAILTYPE_END);
	return (_price.build_rail * _railtype_cost_multiplier[railtype]) >> 3;
}

void *UpdateTrainPowerProc(Vehicle *v, void *data);
void DrawTrainDepotSprite(int x, int y, int image, RailType railtype);
void DrawDefaultWaypointSprite(int x, int y, RailType railtype);

/**
 * Draws overhead wires and pylons for electric railways.
 * @param ti The TileInfo struct of the tile being drawn
 * @see DrawCatenaryRailway
 */
void DrawCatenary(const TileInfo *ti);
void DrawCatenaryOnTunnel(const TileInfo *ti);

Foundation GetRailFoundation(Slope tileh, TrackBits bits);

void FloodHalftile(TileIndex t);

int32 SettingsDisableElrail(int32 p1); ///< _patches.disable_elrail callback

#endif /* RAIL_H */
