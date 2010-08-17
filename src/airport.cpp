/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport.cpp Functions related to airports. */

#include "stdafx.h"
#include "debug.h"
#include "airport.h"
#include "map_type.h"
#include "core/alloc_func.hpp"
#include "date_func.h"
#include "settings_type.h"
#include "newgrf_airport.h"
#include "station_base.h"
#include "table/strings.h"
#include "table/airport_movement.h"
#include "table/airporttile_ids.h"


static AirportFTAClass _airportfta_dummy(
		_airport_moving_data_dummy,
		NULL,
		0,
		_airport_entries_dummy,
		AirportFTAClass::ALL,
		_airport_fta_dummy,
		0
	);

static AirportFTAClass _airportfta_country(
		_airport_moving_data_country,
		_airport_terminal_country,
		0,
		_airport_entries_country,
		AirportFTAClass::ALL | AirportFTAClass::SHORT_STRIP,
		_airport_fta_country,
		0
	);

static AirportFTAClass _airportfta_city(
		_airport_moving_data_town,
		_airport_terminal_city,
		0,
		_airport_entries_city,
		AirportFTAClass::ALL,
		_airport_fta_city,
		0
	);

static AirportFTAClass _airportfta_oilrig(
		_airport_moving_data_oilrig,
		NULL,
		1,
		_airport_entries_heliport_oilrig,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_heliport_oilrig,
		54
	);

static AirportFTAClass _airportfta_heliport(
		_airport_moving_data_heliport,
		NULL,
		1,
		_airport_entries_heliport_oilrig,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_heliport_oilrig,
		60
	);

static AirportFTAClass _airportfta_metropolitan(
		_airport_moving_data_metropolitan,
		_airport_terminal_metropolitan,
		0,
		_airport_entries_metropolitan,
		AirportFTAClass::ALL,
		_airport_fta_metropolitan,
		0
	);

static AirportFTAClass _airportfta_international(
		_airport_moving_data_international,
		_airport_terminal_international,
		2,
		_airport_entries_international,
		AirportFTAClass::ALL,
		_airport_fta_international,
		0
	);

static AirportFTAClass _airportfta_commuter(
		_airport_moving_data_commuter,
		_airport_terminal_commuter,
		2,
		_airport_entries_commuter,
		AirportFTAClass::ALL | AirportFTAClass::SHORT_STRIP,
		_airport_fta_commuter,
		0
	);

static AirportFTAClass _airportfta_helidepot(
		_airport_moving_data_helidepot,
		NULL,
		1,
		_airport_entries_helidepot,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_helidepot,
		0
	);

static AirportFTAClass _airportfta_intercontinental(
		_airport_moving_data_intercontinental,
		_airport_terminal_intercontinental,
		2,
		_airport_entries_intercontinental,
		AirportFTAClass::ALL,
		_airport_fta_intercontinental,
		0
	);

static AirportFTAClass _airportfta_helistation(
		_airport_moving_data_helistation,
		NULL,
		3,
		_airport_entries_helistation,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_helistation,
		0
	);

#include "table/airport_defaults.h"


static uint16 AirportGetNofElements(const AirportFTAbuildup *apFA);
static AirportFTA *AirportBuildAutomata(uint nofelements, const AirportFTAbuildup *apFA);


/**
 * Rotate the airport moving data to another rotation.
 * @param orig Pointer to the moving data to rotate.
 * @param rotation How to rotate the moving data.
 * @return The rotated moving data.
 */
AirportMovingData RotateAirportMovingData(const AirportMovingData *orig, Direction rotation, uint num_tiles_x, uint num_tiles_y)
{
	AirportMovingData amd;
	amd.flag = orig->flag;
	amd.direction = ChangeDir(orig->direction, (DirDiff)rotation);
	switch (rotation) {
		case DIR_N:
			amd.x = orig->x;
			amd.y = orig->y;
			break;

		case DIR_E:
			amd.x = orig->y;
			amd.y = num_tiles_y * TILE_SIZE - orig->x - 1;
			break;

		case DIR_S:
			amd.x = num_tiles_x * TILE_SIZE - orig->x - 1;
			amd.y = num_tiles_y * TILE_SIZE - orig->y - 1;
			break;

		case DIR_W:
			amd.x = num_tiles_x * TILE_SIZE - orig->y - 1;
			amd.y = orig->x;
			break;

		default: NOT_REACHED();
	}
	return amd;
}

AirportFTAClass::AirportFTAClass(
	const AirportMovingData *moving_data_,
	const byte *terminals_,
	const byte num_helipads_,
	const byte *entry_points_,
	Flags flags_,
	const AirportFTAbuildup *apFA,
	byte delta_z_
) :
	moving_data(moving_data_),
	terminals(terminals_),
	num_helipads(num_helipads_),
	flags(flags_),
	nofelements(AirportGetNofElements(apFA)),
	entry_points(entry_points_),
	delta_z(delta_z_)
{
	/* Build the state machine itself */
	this->layout = AirportBuildAutomata(this->nofelements, apFA);
}

AirportFTAClass::~AirportFTAClass()
{
	for (uint i = 0; i < nofelements; i++) {
		AirportFTA *current = layout[i].next;
		while (current != NULL) {
			AirportFTA *next = current->next;
			free(current);
			current = next;
		};
	}
	free(layout);
}

/**
 * Get the number of elements of a source Airport state automata
 * Since it is actually just a big array of AirportFTA types, we only
 * know one element from the other by differing 'position' identifiers
 */
static uint16 AirportGetNofElements(const AirportFTAbuildup *apFA)
{
	uint16 nofelements = 0;
	int temp = apFA[0].position;

	for (uint i = 0; i < MAX_ELEMENTS; i++) {
		if (temp != apFA[i].position) {
			nofelements++;
			temp = apFA[i].position;
		}
		if (apFA[i].position == MAX_ELEMENTS) break;
	}
	return nofelements;
}


static AirportFTA *AirportBuildAutomata(uint nofelements, const AirportFTAbuildup *apFA)
{
	AirportFTA *FAutomata = MallocT<AirportFTA>(nofelements);
	uint16 internalcounter = 0;

	for (uint i = 0; i < nofelements; i++) {
		AirportFTA *current = &FAutomata[i];
		current->position      = apFA[internalcounter].position;
		current->heading       = apFA[internalcounter].heading;
		current->block         = apFA[internalcounter].block;
		current->next_position = apFA[internalcounter].next;

		/* outgoing nodes from the same position, create linked list */
		while (current->position == apFA[internalcounter + 1].position) {
			AirportFTA *newNode = MallocT<AirportFTA>(1);

			newNode->position      = apFA[internalcounter + 1].position;
			newNode->heading       = apFA[internalcounter + 1].heading;
			newNode->block         = apFA[internalcounter + 1].block;
			newNode->next_position = apFA[internalcounter + 1].next;
			/* create link */
			current->next = newNode;
			current = current->next;
			internalcounter++;
		}
		current->next = NULL;
		internalcounter++;
	}
	return FAutomata;
}

const AirportFTAClass *GetAirport(const byte airport_type)
{
	if (airport_type == AT_DUMMY) return &_airportfta_dummy;
	return AirportSpec::Get(airport_type)->fsm;
}

/**
 * Get the vehicle position when an aircraft is build at the given tile
 * @param hangar_tile The tile on which the vehicle is build
 * @return The position (index in airport node array) where the aircraft ends up
 */
byte GetVehiclePosOnBuild(TileIndex hangar_tile)
{
	const Station *st = Station::GetByTile(hangar_tile);
	const AirportFTAClass *apc = st->airport.GetFTA();
	/* When we click on hangar we know the tile it is on. By that we know
	 * its position in the array of depots the airport has.....we can search
	 * layout for #th position of depot. Since layout must start with a listing
	 * of all depots, it is simple */
	for (uint i = 0;; i++) {
		if (st->airport.GetHangarTile(i) == hangar_tile) {
			assert(apc->layout[i].heading == HANGAR);
			return apc->layout[i].position;
		}
	}
	NOT_REACHED();
}
