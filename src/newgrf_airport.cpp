/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airport.h NewGRF handling of airports. */

#include "stdafx.h"
#include "airport.h"
#include "newgrf_airport.h"
#include "date_func.h"
#include "settings_type.h"
#include "core/alloc_type.hpp"
#include "newgrf.h"
#include "table/strings.h"

static AirportClass _airport_classes[APC_MAX];

AirportSpec AirportSpec::dummy = {NULL, NULL, 0, NULL, 0, 0, 0, 0, 0, MIN_YEAR, MIN_YEAR, STR_NULL, ATP_TTDP_LARGE, APC_BEGIN, false};

AirportSpec AirportSpec::specs[NUM_AIRPORTS];

/**
 * Retrieve airport spec for the given airport. If an override is available
 *  it is returned.
 * @param type index of airport
 * @return A pointer to the corresponding AirportSpec
 */
/* static */ const AirportSpec *AirportSpec::Get(byte type)
{
	assert(type < lengthof(AirportSpec::specs));
	return &AirportSpec::specs[type];
}

/**
 * Retrieve airport spec for the given airport. Even if an override is
 *  available the base spec is returned.
 * @param type index of airport
 * @return A pointer to the corresponding AirportSpec
 */
/* static */ AirportSpec *AirportSpec::GetWithoutOverride(byte type)
{
	assert(type < lengthof(AirportSpec::specs));
	return &AirportSpec::specs[type];
}

bool AirportSpec::IsAvailable() const
{
	if (!this->enabled) return false;
	if (_cur_year < this->min_year) return false;
	if (_settings_game.station.never_expire_airports) return true;
	return _cur_year <= this->max_year;
}

/**
 * This function initialize the airportspec array.
 */
void AirportSpec::ResetAirports()
{
	extern const AirportSpec _origin_airport_specs[];
	memset(&AirportSpec::specs, 0, sizeof(AirportSpec::specs));
	memcpy(&AirportSpec::specs, &_origin_airport_specs, sizeof(AirportSpec) * NUM_AIRPORTS);
}

/**
 * Allocate an airport class for the given class id
 * @param cls A 32 bit value identifying the class
 * @return Index into _airport_classes of allocated class
 */
AirportClassID AllocateAirportClass(uint32 cls)
{
	for (AirportClassID i = APC_BEGIN; i < APC_MAX; i++) {
		if (_airport_classes[i].id == cls) {
			/* ClassID is already allocated, so reuse it. */
			return i;
		} else if (_airport_classes[i].id == 0) {
			/* This class is empty, so allocate it to the ClassID. */
			_airport_classes[i].id = cls;
			return i;
		}
	}

	grfmsg(2, "AllocateAirportClass: already allocated %d classes, using small airports class", APC_MAX);
	return APC_SMALL;
}

/**
 * Set the name of an airport class
 * @param id The id of the class to change the name from
 * @param name The new name for the class
 */
void SetAirportClassName(AirportClassID id, StringID name)
{
	assert(id < APC_MAX);
	_airport_classes[id].name = name;
}

/**
 * Retrieve the name of an airport class
 * @param id The id of the airport class to get the name from
 * @return The name of the given class
 */
StringID GetAirportClassName(AirportClassID id)
{
	assert(id < APC_MAX);
	return _airport_classes[id].name;
}

/**
 * Get the number of airport classes in use
 * @return The number of airport classes
 */
uint GetNumAirportClasses()
{
	uint i;
	for (i = APC_BEGIN; i < APC_MAX && _airport_classes[i].id != 0; i++) {}
	return i;
}

/**
 * Return the number of airports for the given airport class.
 * @param id Index of the airport class.
 * @return Number of airports in the class.
 */
uint GetNumAirportsInClass(AirportClassID id)
{
	assert(id < APC_MAX);
	return _airport_classes[id].airports;
}

/**
 * Tie an airport spec to its airport class.
 * @param statspec The airport spec.
 */
static void BindAirportSpecToClass(AirportSpec *as)
{
	assert(as->aclass < APC_MAX);
	AirportClass *airport_class = &_airport_classes[as->aclass];

	int i = airport_class->airports++;
	airport_class->spec = ReallocT(airport_class->spec, airport_class->airports);

	airport_class->spec[i] = as;
}

/**
 * Tie all airportspecs to their class.
 */
void BindAirportSpecs()
{
	for (int i = 0; i < NUM_AIRPORTS; i++) {
		AirportSpec *as = AirportSpec::GetWithoutOverride(i);
		if (as->enabled) BindAirportSpecToClass(as);
	}
}


/**
 * Retrieve an airport spec from a class.
 * @param aclass Index of the airport class.
 * @param airport The airport index with the class.
 * @return The station spec.
 */
const AirportSpec *GetAirportSpecFromClass(AirportClassID aclass, uint airport)
{
	assert(aclass < APC_MAX);
	assert(airport < _airport_classes[aclass].airports);

	return _airport_classes[aclass].spec[airport];
}

/**
 * Reset airport classes to their default state.
 * This includes initialising the defaults classes with an empty
 * entry, for standard airports.
 */
void ResetAirportClasses()
{
	for (AirportClassID i = APC_BEGIN; i < APC_MAX; i++) {
		_airport_classes[i].id = 0;
		_airport_classes[i].name = STR_EMPTY;
		_airport_classes[i].airports = 0;

		free(_airport_classes[i].spec);
		_airport_classes[i].spec = NULL;
	}

	/* Set up initial data */
	AirportClassID id = AllocateAirportClass('SMAL');
	SetAirportClassName(id, STR_AIRPORT_CLASS_SMALL);

	id = AllocateAirportClass('LARG');
	SetAirportClassName(id, STR_AIRPORT_CLASS_LARGE);

	id = AllocateAirportClass('HUB_');
	SetAirportClassName(id, STR_AIRPORT_CLASS_HUB);

	id = AllocateAirportClass('HELI');
	SetAirportClassName(id, STR_AIRPORT_CLASS_HELIPORTS);
}

