/* $Id$ */

/** @file rail.h */

#ifndef RAIL_H
#define RAIL_H

#include "gfx.h"
#include "rail_type.h"
#include "track_type.h"
#include "tile.h"
#include "variables.h"

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
	return HasBit(GetRailTypeInfo(enginetype)->compatible_railtypes, tiletype);
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
	return HasBit(GetRailTypeInfo(enginetype)->powered_railtypes, tiletype);
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

/**
 * Calculates the cost of rail conversion
 * @param from The railtype we are converting from
 * @param to   The railtype we are converting to
 * @return Cost per TrackBit
 */
static inline Money RailConvertCost(RailType from, RailType to)
{
	/* rail -> el. rail
	 * calculate the price as 5 / 4 of (cost build el. rail) - (cost build rail)
	 * (the price of workers to get to place is that 1/4)
	 */
	if (HasPowerOnRail(from, to)) {
		return ((RailBuildCost(to) - RailBuildCost(from)) * 5) >> 2;
	}

	/* el. rail -> rail
	 * calculate the price as 1 / 4 of (cost build el. rail) - (cost build rail)
	 * (the price of workers is 1 / 4 + price of copper sold to a recycle center)
	 */
	if (HasPowerOnRail(to, from)) {
		return (RailBuildCost(from) - RailBuildCost(to)) >> 2;
	}

	/* make the price the same as remove + build new type */
	return RailBuildCost(to) + _price.remove_rail;
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
