/* $Id$ */

/** @file newgrf_station.h Header file for NewGRF stations */

#ifndef NEWGRF_STATION_H
#define NEWGRF_STATION_H

#include "engine.h"
#include "newgrf_cargo.h"
#include "helpers.hpp"

typedef enum {
	STAT_CLASS_BEGIN = 0,    ///< the lowest valid value
	STAT_CLASS_DFLT = 0,     ///< Default station class.
	STAT_CLASS_WAYP,         ///< Waypoint class.
	STAT_CLASS_MAX = 32,     ///< Maximum number of classes.
} StationClassID;

/** Define basic enum properties */
template <> struct EnumPropsT<StationClassID> : MakeEnumPropsT<StationClassID, byte, STAT_CLASS_BEGIN, STAT_CLASS_MAX, STAT_CLASS_MAX> {};
typedef TinyEnumT<StationClassID> StationClassIDByte;

/** Allow incrementing of StationClassID variables */
DECLARE_POSTFIX_INCREMENT(StationClassID);

/* Station layout for given dimensions - it is a two-dimensional array
 * where index is computed as (x * platforms) + platform. */
typedef byte *StationLayout;

typedef struct StationSpec {
	uint32 grfid; ///< ID of GRF file station belongs to.
	int localidx; ///< Index within GRF file of station.

	bool allocated; ///< Flag whether this station has been added to a station class list

	StationClassID sclass; ///< The class to which this spec belongs.
	StringID name; ///< Name of this station.

	/**
	 * Bitmask of number of platforms available for the station.
	 * 0..6 correpsond to 1..7, while bit 7 corresponds to >7 platforms.
	 */
	byte disallowed_platforms;
	/**
	 * Bitmask of platform lengths available for the station.
	 * 0..6 correpsond to 1..7, while bit 7 corresponds to >7 tiles long.
	 */
	byte disallowed_lengths;

	/** Number of tile layouts.
	 * A minimum of 8 is required is required for stations.
	 * 0-1 = plain platform
	 * 2-3 = platform with building
	 * 4-5 = platform with roof, left side
	 * 6-7 = platform with roof, right side
	 */
	uint tiles;
	DrawTileSprites *renderdata; ///< Array of tile layouts.
	bool copied_renderdata;

	/** Cargo threshold for choosing between little and lots of cargo
	 * @note little/lots are equivalent to the moving/loading states for vehicles
	 */
	uint16 cargo_threshold;

	uint32 cargo_triggers; ///< Bitmask of cargo types which cause trigger re-randomizing

	byte callbackmask; ///< Bitmask of callbacks to use, @see newgrf_callbacks.h

	byte flags; ///< Bitmask of flags, bit 0: use different sprite set; bit 1: divide cargo about by station size

	byte pylons;  ///< Bitmask of base tiles (0 - 7) which should contain elrail pylons
	byte wires;   ///< Bitmask of base tiles (0 - 7) which should contain elrail wires
	byte blocked; ///< Bitmask of base tiles (0 - 7) which are blocked to trains

	byte lengths;
	byte *platforms;
	StationLayout **layouts;
	bool copied_layouts;

	/**
	 * NUM_CARGO real cargo plus three pseudo cargo sprite groups.
	 * Used for obtaining the sprite offset of custom sprites, and for
	 * evaluating callbacks.
	 */
	const struct SpriteGroup *spritegroup[NUM_CARGO + 3];
} StationSpec;

/**
 * Struct containing information relating to station classes.
 */
typedef struct StationClass {
	uint32 id;          ///< ID of this class, e.g. 'DFLT', 'WAYP', etc.
	StringID name;      ///< Name of this class.
	uint stations;      ///< Number of stations in this class.
	StationSpec **spec; ///< Array of station specifications.
} StationClass;

void ResetStationClasses();
StationClassID AllocateStationClass(uint32 cls);
void SetStationClassName(StationClassID sclass, StringID name);
StringID GetStationClassName(StationClassID sclass);
StringID *BuildStationClassDropdown();

uint GetNumStationClasses();
uint GetNumCustomStations(StationClassID sclass);

void SetCustomStationSpec(StationSpec *statspec);
const StationSpec *GetCustomStationSpec(StationClassID sclass, uint station);
const StationSpec *GetCustomStationSpecByGrf(uint32 grfid, byte localidx);

/* Evaluate a tile's position within a station, and return the result a bitstuffed format. */
uint32 GetPlatformInfo(Axis axis, byte tile, int platforms, int length, int x, int y, bool centred);

/* Get sprite offset for a given custom station and station structure (may be
 * NULL - that means we are in a build dialog). The station structure is used
 * for variational sprite groups. */
SpriteID GetCustomStationRelocation(const StationSpec *statspec, const Station *st, TileIndex tile);
SpriteID GetCustomStationGroundRelocation(const StationSpec *statspec, const Station *st, TileIndex tile);
uint16 GetStationCallback(uint16 callback, uint32 param1, uint32 param2, const StationSpec *statspec, const Station *st, TileIndex tile);

/* Check if a rail station tile is traversable. */
bool IsStationTileBlocked(TileIndex tile);

/* Check if a rail station tile is electrifiable. */
bool IsStationTileElectrifiable(TileIndex tile);

/* Allocate a StationSpec to a Station. This is called once per build operation. */
int AllocateSpecToStation(const StationSpec *statspec, Station *st, bool exec);

/* Deallocate a StationSpec from a Station. Called when removing a single station tile. */
void DeallocateSpecFromStation(Station* st, byte specindex);

/* Draw representation of a station tile for GUI purposes. */
bool DrawStationTile(int x, int y, RailType railtype, Axis axis, StationClassID sclass, uint station);

#endif /* NEWGRF_STATION_H */
