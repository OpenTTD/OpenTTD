/* $Id$ */

/** @file station_newgrf.h Header file for NewGRF stations */

#ifndef STATION_NEWGRF_H
#define STATION_NEWGRF_H

#include "engine.h"

typedef enum {
	STAT_CLASS_DFLT,     ///< Default station class.
	STAT_CLASS_WAYP,     ///< Waypoint class.
	STAT_CLASS_MAX = 16, ///< Maximum number of classes.
} StationClassID;

/* Station layout for given dimensions - it is a two-dimensional array
 * where index is computed as (x * platforms) + platform. */
typedef byte *StationLayout;

typedef struct stationspec {
	uint32 grfid; ///< ID of GRF file station belongs to.
	int localidx; ///< Index within GRF file of station.

	StationClassID sclass; ///< The class to which this spec belongs.

	/**
	 * Bitmask of number of platforms available for the station.
	 * 0..6 correpsond to 1..7, while bit 7 corresponds to >7 platforms.
	 */
	byte allowed_platforms;
	/**
	 * Bitmask of platform lengths available for the station.
	 * 0..6 correpsond to 1..7, while bit 7 corresponds to >7 tiles long.
	 */
	byte allowed_lengths;

	/** Number of tile layouts.
	 * A minimum of 8 is required is required for stations.
	 * 0-1 = plain platform
	 * 2-3 = platform with building
	 * 4-5 = platform with roof, left side
	 * 6-7 = platform with roof, right side
	 */
	int tiles;
	DrawTileSprites *renderdata; ///< Array of tile layouts.

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
typedef struct stationclass {
	uint32 id;          ///< ID of this class, e.g. 'DFLT', 'WAYP', etc.
	char *name;         ///< Name of this class.
	uint stations;      ///< Number of stations in this class.
	StationSpec **spec; ///< Array of station specifications.
} StationClass;

void ResetStationClasses(void);
StationClassID AllocateStationClass(uint32 class);
void SetStationClassName(StationClassID sclass, const char *name);
uint GetNumCustomStations(StationClassID sclass);

void SetCustomStation(StationSpec *spec);
const StationSpec *GetCustomStation(StationClassID sclass, uint station);

#endif /* STATION_NEWGRF_H */
