/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road.h Road specific functions. */

#ifndef ROAD_H
#define ROAD_H

#include "road_type.h"
#include "gfx_type.h"
#include "core/bitmath_func.hpp"
#include "strings_type.h"
#include "date_type.h"
#include "core/enum_type.hpp"
#include "core/smallvec_type.hpp"
#include "newgrf.h"
#include "economy_func.h"

/** Roadtype flags. Starts with RO instead of R because R is used for rails */
enum RoadTypeFlags {
	ROTF_CATENARY = 0,                                     ///< Bit number for adding catenary
	ROTF_NO_LEVEL_CROSSING,                                ///< Bit number for disabling level crossing
	ROTF_NO_HOUSES,                                        ///< Bit number for setting this roadtype as not house friendly

	ROTFB_NONE = 0,                                        ///< All flags cleared.
	ROTFB_CATENARY = 1 << ROTF_CATENARY,                   ///< Value for drawing a catenary.
	ROTFB_NO_LEVEL_CROSSING = 1 << ROTF_NO_LEVEL_CROSSING, ///< Value for disabling a level crossing.
	ROTFB_NO_HOUSES = 1 << ROTF_NO_HOUSES,                 ///< Value for for setting this roadtype as not house friendly.
};
DECLARE_ENUM_AS_BIT_SET(RoadTypeFlags)

struct SpriteGroup;

/** Sprite groups for a roadtype. */
enum RoadTypeSpriteGroup {
	ROTSG_CURSORS,        ///< Optional: Cursor and toolbar icon images
	ROTSG_OVERLAY,        ///< Optional: Images for overlaying track
	ROTSG_GROUND,         ///< Required: Main group of ground images
	ROTSG_reserved1,      ///<           Placeholder, if we need specific tunnel sprites.
	ROTSG_CATENARY_FRONT, ///< Optional: Catenary front
	ROTSG_CATENARY_BACK,  ///< Optional: Catenary back
	ROTSG_BRIDGE,         ///< Required: Bridge surface images
	ROTSG_reserved2,      ///<           Placeholder, if we need specific level crossing sprites.
	ROTSG_DEPOT,          ///< Optional: Depot images
	ROTSG_reserved3,      ///<           Placeholder, if we add road fences (for highways).
	ROTSG_ROADSTOP,       ///< Required: Drive-in stop surface
	ROTSG_END,
};

/** List of road type labels. */
typedef SmallVector<RoadTypeLabel, 4> RoadTypeLabelList;

class RoadtypeInfo {
public:
	/**
	 * struct containing the sprites for the road GUI. @note only sprites referred to
	 * directly in the code are listed
	 */
	struct {
		SpriteID build_x_road;        ///< button for building single rail in X direction
		SpriteID build_y_road;        ///< button for building single rail in Y direction
		SpriteID auto_road;           ///< button for the autoroad construction
		SpriteID build_depot;         ///< button for building depots
		SpriteID build_tunnel;        ///< button for building a tunnel
		SpriteID convert_road;        ///< button for converting road types
	} gui_sprites;

	struct {
		CursorID road_swne;     ///< Cursor for building rail in X direction
		CursorID road_nwse;     ///< Cursor for building rail in Y direction
		CursorID autoroad;      ///< Cursor for autorail tool
		CursorID depot;         ///< Cursor for building a depot
		CursorID tunnel;        ///< Cursor for building a tunnel
		SpriteID convert_road;  ///< Cursor for converting road types
	} cursor;                       ///< Cursors associated with the road type.

	struct {
		StringID name;            ///< Name of this rail type.
		StringID toolbar_caption; ///< Caption in the construction toolbar GUI for this rail type.
		StringID menu_text;       ///< Name of this rail type in the main toolbar dropdown.
		StringID build_caption;   ///< Caption of the build vehicle GUI for this rail type.
		StringID replace_text;    ///< Text used in the autoreplace GUI.
		StringID new_engine;      ///< Name of an engine for this type of road in the engine preview GUI.

		StringID err_build_road;        ///< Building a normal piece of road
		StringID err_remove_road;       ///< Removing a normal piece of road
		StringID err_depot;             ///< Building a depot
		StringID err_build_station[2];  ///< Building a bus or truck station
		StringID err_remove_station[2]; ///< Removing of a bus or truck station
		StringID err_convert_road;      ///< Converting a road type

		StringID picker_title[2];       ///< Title for the station picker for bus or truck stations
		StringID picker_tooltip[2];     ///< Tooltip for the station picker for bus or truck stations
	} strings;                        ///< Strings associated with the rail type.

	/** bitmask to the OTHER roadtypes on which a vehicle of THIS roadtype generates power */
	RoadSubTypes powered_roadtypes;

	/**
	 * Bit mask of road type flags
	 */
	RoadTypeFlags flags;

	/**
	 * Cost multiplier for building this road type
	 */
	uint16 cost_multiplier;

	/**
	 * Cost multiplier for maintenance of this road type
	 */
	uint16 maintenance_multiplier;

	/**
	 * Maximum speed for vehicles travelling on this road type
	 */
	uint16 max_speed;

	/**
	 * Unique 32 bit road type identifier
	 */
	RoadTypeLabel label;

	/**
	 * Road type labels this type provides in addition to the main label.
	 */
	RoadTypeLabelList alternate_labels;

	/**
	 * Colour on mini-map
	 */
	byte map_colour;

	/**
	 * Introduction date.
	 * When #INVALID_DATE or a vehicle using this roadtype gets introduced earlier,
	 * the vehicle's introduction date will be used instead for this roadtype.
	 * The introduction at this date is furthermore limited by the
	 * #introduction_required_types.
	 */
	Date introduction_date;

	/**
	 * Bitmask of roadtypes that are required for this roadtype to be introduced
	 * at a given #introduction_date.
	 */
	RoadSubTypes introduction_required_roadtypes;

	/**
	 * Bitmask of which other roadtypes are introduced when this roadtype is introduced.
	 */
	RoadSubTypes introduces_roadtypes;

	/**
	 * The sorting order of this roadtype for the toolbar dropdown.
	 */
	byte sorting_order;

	/**
	 * NewGRF providing the Action3 for the roadtype. NULL if not available.
	 */
	const GRFFile *grffile[ROTSG_END];

	/**
	 * Sprite groups for resolving sprites
	 */
	const SpriteGroup *group[ROTSG_END];

	inline bool UsesOverlay() const
	{
		return this->group[ROTSG_GROUND] != NULL;
	}
};

/**
 * Returns a pointer to the Roadtype information for a given roadtype
 * @param roadtype the road type which the information is requested for
 * @return The pointer to the RoadtypeInfo
 */
static inline const RoadtypeInfo *GetRoadTypeInfo(RoadTypeIdentifier rtid)
{
	extern RoadtypeInfo _roadtypes[ROADTYPE_END][ROADSUBTYPE_END];
	assert(rtid.IsValid());
	return &_roadtypes[rtid.basetype][rtid.subtype];
}

/**
 * Checks if an engine of the given RoadType got power on a tile with a given
 * RoadType. This would normally just be an equality check, but for electrified
 * roads (which also support non-electric vehicles).
 * @return Whether the engine got power on this tile.
 * @param  engine_rtid The RoadType of the engine we are considering.
 * @param  tile_rtid   The RoadType of the tile we are considering.
 */
static inline bool HasPowerOnRoad(RoadTypeIdentifier engine_rtid, RoadTypeIdentifier tile_rtid)
{
	return engine_rtid.basetype == tile_rtid.basetype && HasBit(GetRoadTypeInfo(engine_rtid)->powered_roadtypes, tile_rtid.subtype);
}

/**
 * Returns the cost of building the specified roadtype.
 * @param rti The roadtype being built.
 * @return The cost multiplier.
 */
static inline Money RoadBuildCost(RoadTypeIdentifier rtid)
{
	assert(rtid.IsValid());
	return (_price[PR_BUILD_ROAD] * GetRoadTypeInfo(rtid)->cost_multiplier) >> 3;
}

/**
 * Calculates the cost of road conversion
 * @param from The roadtype we are converting from
 * @param to   The roadtype we are converting to
 * @return Cost per RoadBit
 */
static inline Money RoadConvertCost(RoadTypeIdentifier from, RoadTypeIdentifier to)
{
	/* Don't apply convert costs when converting to the same roadtype (ex. building a roadstop over existing road) */
	if (from == to) return (Money)0;

	/* Get the costs for removing and building anew
	 * A conversion can never be more costly */
	Money rebuildcost = RoadBuildCost(to) - _price[PR_CLEAR_ROAD];

	/* Conversion between somewhat compatible roadtypes:
	 * Pay 1/8 of the target road cost (labour costs) and additionally any difference in the
	 * build costs, if the target type is more expensive (material upgrade costs).
	 * Upgrade can never be more expensive than re-building. */
	if (HasPowerOnRoad(from, to) || HasPowerOnRoad(to, from)) {
		Money upgradecost = RoadBuildCost(to) / 8 + max((Money)0, RoadBuildCost(to) - RoadBuildCost(from));
		return min(upgradecost, rebuildcost);
	}

	/* make the price the same as remove + build new type for road types
	 * which are not compatible in any way */
	return rebuildcost;
}

/**
 * 
 */
static inline bool RoadNoLevelCrossing(RoadTypeIdentifier rtid)
{
	assert(rtid.IsValid());
	return HasBit(GetRoadTypeInfo(rtid)->flags, ROTF_NO_LEVEL_CROSSING);
}

RoadTypeIdentifier GetRoadTypeByLabel(RoadTypeLabel label, RoadType subtype, bool allow_alternate_labels = true);

void ResetRoadTypes();
void InitRoadTypes();
RoadTypeIdentifier AllocateRoadType(RoadTypeLabel label, RoadType basetype);
RoadSubTypes ExistingRoadSubTypesForRoadType(RoadType rt, CompanyID c);
bool CanBuildRoadTypeInfrastructure(RoadTypeIdentifier rtid, CompanyID company);

extern RoadTypeIdentifier _sorted_roadtypes[ROADTYPE_END][ROADSUBTYPE_END];
extern uint8 _sorted_roadtypes_size[ROADTYPE_END];

/**
 * Loop header for iterating over roadtypes, sorted by sortorder.
 * @param var Roadtype.
 */
#define FOR_ALL_SORTED_ROADTYPES(var, type) for (uint8 index = 0; index < _sorted_roadtypes_size[type] && (var = _sorted_roadtypes[type][index], true) ; index++)

#endif /* ROAD_H */
