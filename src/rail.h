/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail.h Rail specific functions. */

#ifndef RAIL_H
#define RAIL_H

#include "rail_type.h"
#include "track_type.h"
#include "gfx_type.h"
#include "core/bitmath_func.hpp"
#include "economy_func.h"
#include "slope_type.h"
#include "strings_type.h"
#include "date_type.h"

/** Railtype flags. */
enum RailTypeFlags {
	RTF_CATENARY          = 0,                           ///< Bit number for drawing a catenary.
	RTF_NO_LEVEL_CROSSING = 1,                           ///< Bit number for disallowing level crossings.

	RTFB_NONE              = 0,                          ///< All flags cleared.
	RTFB_CATENARY          = 1 << RTF_CATENARY,          ///< Value for drawing a catenary.
	RTFB_NO_LEVEL_CROSSING = 1 << RTF_NO_LEVEL_CROSSING, ///< Value for disallowing level crossings.
};
DECLARE_ENUM_AS_BIT_SET(RailTypeFlags)

struct SpriteGroup;

/** Sprite groups for a railtype. */
enum RailTypeSpriteGroup {
	RTSG_CURSORS,     ///< Cursor and toolbar icon images
	RTSG_OVERLAY,     ///< Images for overlaying track
	RTSG_GROUND,      ///< Main group of ground images
	RTSG_TUNNEL,      ///< Main group of ground images for snow or desert
	RTSG_WIRES,       ///< Catenary wires
	RTSG_PYLONS,      ///< Catenary pylons
	RTSG_BRIDGE,      ///< Bridge surface images
	RTSG_CROSSING,    ///< Level crossing overlay images
	RTSG_DEPOT,       ///< Depot images
	RTSG_FENCES,      ///< Fence images
	RTSG_END,
};

/**
 * Offsets for sprites within an overlay/underlay set.
 * These are the same for overlay and underlay sprites.
 */
enum RailTrackOffset {
	RTO_X,            ///< Piece of rail in X direction
	RTO_Y,            ///< Piece of rail in Y direction
	RTO_N,            ///< Piece of rail in northern corner
	RTO_S,            ///< Piece of rail in southern corner
	RTO_E,            ///< Piece of rail in eastern corner
	RTO_W,            ///< Piece of rail in western corner
	RTO_SLOPE_NE,     ///< Piece of rail on slope with north-east raised
	RTO_SLOPE_SE,     ///< Piece of rail on slope with south-east raised
	RTO_SLOPE_SW,     ///< Piece of rail on slope with south-west raised
	RTO_SLOPE_NW,     ///< Piece of rail on slope with north-west raised
	RTO_CROSSING_XY,  ///< Crossing of X and Y rail, with ballast
	RTO_JUNCTION_SW,  ///< Ballast for junction 'pointing' SW
	RTO_JUNCTION_NE,  ///< Ballast for junction 'pointing' NE
	RTO_JUNCTION_SE,  ///< Ballast for junction 'pointing' SE
	RTO_JUNCTION_NW,  ///< Ballast for junction 'pointing' NW
	RTO_JUNCTION_NSEW,///< Ballast for full junction
};

/**
 * Offsets for spries within a bridge surface overlay set.
 */
enum RailTrackBridgeOffset {
	RTBO_X,     ///< Piece of rail in X direction
	RTBO_Y,     ///< Piece of rail in Y direction
	RTBO_SLOPE, ///< Sloped rail pieces, in order NE, SE, SW, NW
};

/**
 * Offsets from base sprite for fence sprites. These are in the order of
 *  the sprites in the original data files.
 */
enum RailFenceOffset {
	RFO_FLAT_X,
	RFO_FLAT_Y,
	RFO_FLAT_VERT,
	RFO_FLAT_HORZ,
	RFO_SLOPE_SW,
	RFO_SLOPE_SE,
	RFO_SLOPE_NE,
	RFO_SLOPE_NW,
};

/**
 * This struct contains all the info that is needed to draw and construct tracks.
 */
struct RailtypeInfo {
	/**
	 * Struct containing the main sprites. @note not all sprites are listed, but only
	 *  the ones used directly in the code
	 */
	struct {
		SpriteID track_y;      ///< single piece of rail in Y direction, with ground
		SpriteID track_ns;     ///< two pieces of rail in North and South corner (East-West direction)
		SpriteID ground;       ///< ground sprite for a 3-way switch
		SpriteID single_x;     ///< single piece of rail in X direction, without ground
		SpriteID single_y;     ///< single piece of rail in Y direction, without ground
		SpriteID single_n;     ///< single piece of rail in the northern corner
		SpriteID single_s;     ///< single piece of rail in the southern corner
		SpriteID single_e;     ///< single piece of rail in the eastern corner
		SpriteID single_w;     ///< single piece of rail in the western corner
		SpriteID single_sloped;///< single piecs of rail for slopes
		SpriteID crossing;     ///< level crossing, rail in X direction
		SpriteID tunnel;       ///< tunnel sprites base
	} base_sprites;

	/**
	 * struct containing the sprites for the rail GUI. @note only sprites referred to
	 * directly in the code are listed
	 */
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
		StringID menu_text;
		StringID build_caption;
		StringID replace_text;
		StringID new_loco;
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

	/**
	 * Cost multiplier for building this rail type
	 */
	uint16 cost_multiplier;

	/**
	 * Acceleration type of this rail type
	 */
	uint8 acceleration_type;

	/**
	 * Maximum speed for vehicles travelling on this rail type
	 */
	uint16 max_speed;

	/**
	 * Unique 32 bit rail type identifier
	 */
	RailTypeLabel label;

	/**
	 * Colour on mini-map
	 */
	byte map_colour;

	/**
	 * Introduction date.
	 * When #INVALID_DATE or a vehicle using this railtype gets introduced earlier,
	 * the vehicle's introduction date will be used instead for this railtype.
	 * The introduction at this date is furthermore limited by the
	 * #introduction_required_types.
	 */
	Date introduction_date;

	/**
	 * Bitmask of railtypes that are required for this railtype to be introduced
	 * at a given #introduction_date.
	 */
	RailTypes introduction_required_railtypes;

	/**
	 * Bitmask of which other railtypes are introduced when this railtype is introduced.
	 */
	RailTypes introduces_railtypes;

	/**
	 * Sprite groups for resolving sprites
	 */
	const SpriteGroup *group[RTSG_END];

	inline bool UsesOverlay() const
	{
		return this->group[RTSG_GROUND] != NULL;
	}
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

/**
 * Test if a RailType disallows build of level crossings.
 * @param rt The RailType to check.
 * @return Whether level crossings are not allowed.
 */
static inline bool RailNoLevelCrossings(RailType rt)
{
	return HasBit(GetRailTypeInfo(rt)->flags, RTF_NO_LEVEL_CROSSING);
}

/**
 * Returns the cost of building the specified railtype.
 * @param railtype The railtype being built.
 * @return The cost multiplier.
 */
static inline Money RailBuildCost(RailType railtype)
{
	assert(railtype < RAILTYPE_END);
	return (_price[PR_BUILD_RAIL] * GetRailTypeInfo(railtype)->cost_multiplier) >> 3;
}

/**
 * Returns the 'cost' of clearing the specified railtype.
 * @param railtype The railtype being removed.
 * @return The cost.
 */
static inline Money RailClearCost(RailType railtype)
{
	/* Clearing rail in fact earns money, but if the build cost is set
	 * very low then a loophole exists where money can be made.
	 * In this case we limit the removal earnings to 3/4s of the build
	 * cost.
	 */
	assert(railtype < RAILTYPE_END);
	return max(_price[PR_CLEAR_RAIL], -RailBuildCost(railtype) * 3 / 4);
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
		Money cost = ((RailBuildCost(to) - RailBuildCost(from)) * 5) >> 2;
		if (cost != 0) return cost;
	}

	/* el. rail -> rail
	 * calculate the price as 1 / 4 of (cost build el. rail) - (cost build rail)
	 * (the price of workers is 1 / 4 + price of copper sold to a recycle center)
	 */
	if (HasPowerOnRail(to, from)) {
		Money cost = (RailBuildCost(from) - RailBuildCost(to)) >> 2;
		if (cost != 0) return cost;
	}

	/* make the price the same as remove + build new type */
	return RailBuildCost(to) + RailClearCost(from);
}

void DrawTrainDepotSprite(int x, int y, int image, RailType railtype);
int TicksToLeaveDepot(const Train *v);

Foundation GetRailFoundation(Slope tileh, TrackBits bits);


bool HasRailtypeAvail(const CompanyID company, const RailType railtype);
bool ValParamRailtype(const RailType rail);

RailTypes AddDateIntroducedRailTypes(RailTypes current, Date date);

RailType GetBestRailtype(const CompanyID company);
RailTypes GetCompanyRailtypes(const CompanyID c);

RailType GetRailTypeByLabel(RailTypeLabel label);

void ResetRailTypes();
void InitRailTypes();
RailType AllocateRailType(RailTypeLabel label);

#endif /* RAIL_H */
