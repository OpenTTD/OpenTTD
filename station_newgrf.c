/* $Id$ */

/** @file station_newgrf.c Functions for dealing with station classes and custom stations. */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "sprite.h"
#include "station_newgrf.h"

static StationClass station_classes[STAT_CLASS_MAX];

/**
 * Reset station classes to their default state.
 * This includes initialising the Default and Waypoint classes with an empty
 * entry, for standard stations and waypoints.
 */
void ResetStationClasses(void)
{
	StationClassID i;
	for (i = 0; i < STAT_CLASS_MAX; i++) {
		station_classes[i].id = 0;

		free(station_classes[i].name);
		station_classes[i].name = NULL;

		station_classes[i].stations = 0;

		free(station_classes[i].spec);
		station_classes[i].spec = NULL;
	}

	// Set up initial data
	station_classes[0].id = 'DFLT';
	station_classes[0].name = strdup("Default");
	station_classes[0].stations = 1;
	station_classes[0].spec = malloc(sizeof(*station_classes[0].spec));
	station_classes[0].spec[0] = NULL;

	station_classes[1].id = 'WAYP';
	station_classes[1].name = strdup("Waypoints");
	station_classes[1].stations = 1;
	station_classes[1].spec = malloc(sizeof(*station_classes[1].spec));
	station_classes[1].spec[0] = NULL;
}

/**
 * Allocate a station class for the given class id.
 * @param classid A 32 bit value identifying the class.
 * @return Index into station_classes of allocated class.
 */
StationClassID AllocateStationClass(uint32 class)
{
	StationClassID i;

	for (i = 0; i < STAT_CLASS_MAX; i++) {
		if (station_classes[i].id == class) {
			// ClassID is already allocated, so reuse it.
			return i;
		} else if (station_classes[i].id == 0) {
			// This class is empty, so allocate it to the ClassID.
			station_classes[i].id = class;
			return i;
		}
	}

	DEBUG(grf, 2)("StationClassAllocate: Already allocated %d classes, using default.", STAT_CLASS_MAX);
	return STAT_CLASS_DFLT;
}

/**
 * Return the number of stations for the given station class.
 * @param sclass Index of the station class.
 * @return Number of stations in the class.
 */
uint GetNumCustomStations(StationClassID sclass)
{
	assert(sclass < STAT_CLASS_MAX);
	return station_classes[sclass].stations;
}

/**
 * Tie a station spec to its station class.
 * @param spec The station spec.
 */
void SetCustomStation(StationSpec *spec)
{
	StationClass *station_class;
	int i;

	assert(spec->sclass < STAT_CLASS_MAX);
	station_class = &station_classes[spec->sclass];

	i = station_class->stations++;
	station_class->spec = realloc(station_class->spec, station_class->stations * sizeof(*station_class->spec));

	station_class->spec[i] = spec;
}

/**
 * Retrieve a station spec from a class.
 * @param sclass Index of the station class.
 * @param station The station index with the class.
 * @return The station spec.
 */
const StationSpec *GetCustomStation(StationClassID sclass, uint station)
{
	assert(sclass < STAT_CLASS_MAX);
	if (station < station_classes[sclass].stations)
		return station_classes[sclass].spec[station];

	// If the custom station isn't defined any more, then the GRF file
	// probably was not loaded.
	return NULL;
}
