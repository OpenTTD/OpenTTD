/* $Id$ */

/** @file rail.h */

#ifndef RAIL_H
#define RAIL_H

#include "rail_type.h"
#include "track_type.h"
#include "vehicle_type.h"
#include "gfx_type.h"
#include "core/bitmath_func.hpp"
#include "economy_func.h"
#include "tile_cmd.h"

enum RailTypeFlag {
	RTF_CATENARY = 0,  ///< Set if the rail type should have catenary drawn
};

enum RailTypeFlags {
	RTFB_NONE     = 0,
	RTFB_CATENARY = 1 << RTF_CATENARY,
};
DECLARE_ENUM_AS_BIT_SET(RailTypeFlags);

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
	RailTypes powered_railtypes;

	/** bitmask to the OTHER railtypes on which an engine of THIS railtype can physically travel */
	RailTypes compatible_railtypes;

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

	/**
	 * Multiplier for curve maximum speed advantage
	 */
	byte curve_speed;

	/**
	 * Bit mask of rail type flags
	 */
	RailTypeFlags flags;
};


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
void *EnsureNoTrainOnTrackProc(Vehicle *v, void *data);
int TicksToLeaveDepot(const Vehicle *v);


/**
 * Test if a rail type has catenary
 * @param rt Rail type to test
 */
static inline bool HasCatenary(RailType rt)
{
	return HasBit(GetRailTypeInfo(rt)->flags, RTF_CATENARY);
}


/**
 * Draws overhead wires and pylons for electric railways.
 * @param ti The TileInfo struct of the tile being drawn
 * @see DrawCatenaryRailway
 */
void DrawCatenary(const TileInfo *ti);
void DrawCatenaryOnTunnel(const TileInfo *ti);

Foundation GetRailFoundation(Slope tileh, TrackBits bits);

int32 SettingsDisableElrail(int32 p1); ///< _patches.disable_elrail callback

/**
 * Finds out if a Player has a certain railtype available
 * @param p Player in question
 * @param railtype requested RailType
 * @return true if player has requested RailType available
 */
bool HasRailtypeAvail(const PlayerID p, const RailType railtype);

/**
 * Validate functions for rail building.
 * @param rail the railtype to check.
 * @return true if the current player may build the rail.
 */
bool ValParamRailtype(const RailType rail);

/**
 * Returns the "best" railtype a player can build.
 * As the AI doesn't know what the BEST one is, we have our own priority list
 * here. When adding new railtypes, modify this function
 * @param p the player "in action"
 * @return The "best" railtype a player has available
 */
RailType GetBestRailtype(const PlayerID p);

/**
 * Get the rail types the given player can build.
 * @param p the player to get the rail types for.
 * @return the rail types.
 */
RailTypes GetPlayerRailtypes(const PlayerID p);

#endif /* RAIL_H */
