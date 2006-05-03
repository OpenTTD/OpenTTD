/* $Id$ */

/** @file newgrf_station.h Header file for NewGRF stations */

#ifndef NEWGRF_STATION_H
#define NEWGRF_STATION_H

#include "engine.h"

typedef enum {
	STAT_CLASS_DFLT,     ///< Default station class.
	STAT_CLASS_WAYP,     ///< Waypoint class.
	STAT_CLASS_MAX = 16, ///< Maximum number of classes.
} StationClassID;

/* Station layout for given dimensions - it is a two-dimensional array
 * where index is computed as (x * platforms) + platform. */
typedef byte *StationLayout;

typedef struct StationSpec {
	uint32 grfid; ///< ID of GRF file station belongs to.
	int localidx; ///< Index within GRF file of station.

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

	/**
	 * NUM_GLOBAL_CID sprite groups.
	 * Used for obtaining the sprite offset of custom sprites, and for
	 * evaluating callbacks.
	 */
	SpriteGroup *spritegroup[NUM_GLOBAL_CID];
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

void ResetStationClasses(void);
StationClassID AllocateStationClass(uint32 class);
void SetStationClassName(StationClassID sclass, StringID name);
StringID GetStationClassName(StationClassID sclass);
StringID *BuildStationClassDropdown(void);

uint GetNumStationClasses(void);
uint GetNumCustomStations(StationClassID sclass);

void SetCustomStationSpec(StationSpec *statspec);
const StationSpec *GetCustomStationSpec(StationClassID sclass, uint station);

/* Get sprite offset for a given custom station and station structure (may be
 * NULL if ctype is set - that means we are in a build dialog). The station
 * structure is used for variational sprite groups. */
SpriteID GetCustomStationRelocation(const StationSpec *statspec, const Station *st, TileIndex tile, byte ctype);

/* Allocate a StationSpec to a Station. This is called once per build operation. */
int AllocateSpecToStation(const StationSpec *statspec, Station *st, bool exec);

/* Deallocate a StationSpec from a Station. Called when removing a single station tile. */
bool DeallocateSpecFromStation(Station *st, byte specindex);

#endif /* NEWGRF_STATION_H */
