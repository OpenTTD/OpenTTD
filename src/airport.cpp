/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "map.h"
#include "airport.h"
#include "macros.h"
#include "variables.h"
#include "airport_movement.h"
#include "date.h"
#include "helpers.hpp"

/* Uncomment this to print out a full report of the airport-structure
 * You should either use
 * - true: full-report, print out every state and choice with string-names
 * OR
 * - false: give a summarized report which only shows current and next position */
//#define DEBUG_AIRPORT false

static AirportFTAClass *CountryAirport;
static AirportFTAClass *CityAirport;
static AirportFTAClass *Oilrig;
static AirportFTAClass *Heliport;
static AirportFTAClass *MetropolitanAirport;
static AirportFTAClass *InternationalAirport;
static AirportFTAClass *CommuterAirport;
static AirportFTAClass *HeliDepot;
static AirportFTAClass *IntercontinentalAirport;
static AirportFTAClass *HeliStation;


void InitializeAirports(void)
{
	CountryAirport = new AirportFTAClass(
		_airport_moving_data_country,
		_airport_terminal_country,
		NULL,
		_airport_entries_country,
		AirportFTAClass::ALL | AirportFTAClass::SHORT_STRIP,
		_airport_fta_country,
		_airport_depots_country,
		lengthof(_airport_depots_country),
		4, 3,
		0,
		4
	);

	CityAirport = new AirportFTAClass(
		_airport_moving_data_town,
		_airport_terminal_city,
		NULL,
		_airport_entries_city,
		AirportFTAClass::ALL,
		_airport_fta_city,
		_airport_depots_city,
		lengthof(_airport_depots_city),
		6, 6,
		0,
		5
	);

	MetropolitanAirport = new AirportFTAClass(
		_airport_moving_data_metropolitan,
		_airport_terminal_metropolitan,
		NULL,
		_airport_entries_metropolitan,
		AirportFTAClass::ALL,
		_airport_fta_metropolitan,
		_airport_depots_metropolitan,
		lengthof(_airport_depots_metropolitan),
		6, 6,
		0,
		6
	);

	InternationalAirport = new AirportFTAClass(
		_airport_moving_data_international,
		_airport_terminal_international,
		_airport_helipad_international,
		_airport_entries_international,
		AirportFTAClass::ALL,
		_airport_fta_international,
		_airport_depots_international,
		lengthof(_airport_depots_international),
		7, 7,
		0,
		8
	);

	IntercontinentalAirport = new AirportFTAClass(
		_airport_moving_data_intercontinental,
		_airport_terminal_intercontinental,
		_airport_helipad_intercontinental,
		_airport_entries_intercontinental,
		AirportFTAClass::ALL,
		_airport_fta_intercontinental,
		_airport_depots_intercontinental,
		lengthof(_airport_depots_intercontinental),
		9, 11,
		0,
		10
	);

	Heliport = new AirportFTAClass(
		_airport_moving_data_heliport,
		NULL,
		_airport_helipad_heliport_oilrig,
		_airport_entries_heliport_oilrig,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_heliport_oilrig,
		NULL,
		0,
		1, 1,
		60,
		4
	);

	Oilrig = new AirportFTAClass(
		_airport_moving_data_oilrig,
		NULL,
		_airport_helipad_heliport_oilrig,
		_airport_entries_heliport_oilrig,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_heliport_oilrig,
		NULL,
		0,
		1, 1,
		54,
		3
	);

	CommuterAirport = new AirportFTAClass(
		_airport_moving_data_commuter,
		_airport_terminal_commuter,
		_airport_helipad_commuter,
		_airport_entries_commuter,
		AirportFTAClass::ALL | AirportFTAClass::SHORT_STRIP,
		_airport_fta_commuter,
		_airport_depots_commuter,
		lengthof(_airport_depots_commuter),
		5, 4,
		0,
		4
	);

	HeliDepot = new AirportFTAClass(
		_airport_moving_data_helidepot,
		NULL,
		_airport_helipad_helidepot,
		_airport_entries_helidepot,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_helidepot,
		_airport_depots_helidepot,
		lengthof(_airport_depots_helidepot),
		2, 2,
		0,
		4
	);

	HeliStation = new AirportFTAClass(
		_airport_moving_data_helistation,
		NULL,
		_airport_helipad_helistation,
		_airport_entries_helistation,
		AirportFTAClass::HELICOPTERS,
		_airport_fta_helistation,
		_airport_depots_helistation,
		lengthof(_airport_depots_helistation),
		4, 2,
		0,
		4
	);
}

void UnInitializeAirports(void)
{
	delete CountryAirport;
	delete CityAirport;
	delete Heliport;
	delete MetropolitanAirport;
	delete InternationalAirport;
	delete CommuterAirport;
	delete HeliDepot;
	delete IntercontinentalAirport;
	delete HeliStation;
}


static uint16 AirportGetNofElements(const AirportFTAbuildup *apFA);
static AirportFTA* AirportBuildAutomata(uint nofelements, const AirportFTAbuildup *apFA);
static byte AirportGetTerminalCount(const byte *terminals, byte *groups);
static byte AirportTestFTA(uint nofelements, const AirportFTA *layout, const byte *terminals);

#ifdef DEBUG_AIRPORT
static void AirportPrintOut(uint nofelements, const AirportFTA *layout, bool full_report);
#endif


AirportFTAClass::AirportFTAClass(
	const AirportMovingData *moving_data_,
	const byte *terminals_,
	const byte *helipads_,
	const byte *entry_points_,
	Flags flags_,
	const AirportFTAbuildup *apFA,
	const TileIndexDiffC *depots_,
	const byte nof_depots_,
	uint size_x_,
	uint size_y_,
	byte delta_z_,
	byte catchment_
) :
	moving_data(moving_data_),
	terminals(terminals_),
	helipads(helipads_),
	airport_depots(depots_),
	flags(flags_),
	nof_depots(nof_depots_),
	nofelements(AirportGetNofElements(apFA)),
	entry_points(entry_points_),
	size_x(size_x_),
	size_y(size_y_),
	delta_z(delta_z_),
	catchment(catchment_)
{
	byte nofterminalgroups, nofhelipadgroups;

	/* Set up the terminal and helipad count for an airport.
	 * TODO: If there are more than 10 terminals or 4 helipads, internal variables
	 * need to be changed, so don't allow that for now */
	uint nofterminals = AirportGetTerminalCount(terminals, &nofterminalgroups);
	if (nofterminals > MAX_TERMINALS) {
		DEBUG(misc, 0, "[Ap] only a maximum of %d terminals are supported (requested %d)", MAX_TERMINALS, nofterminals);
		assert(nofterminals <= MAX_TERMINALS);
	}

	uint nofhelipads = AirportGetTerminalCount(helipads, &nofhelipadgroups);
	if (nofhelipads > MAX_HELIPADS) {
		DEBUG(misc, 0, "[Ap] only a maximum of %d helipads are supported (requested %d)", MAX_HELIPADS, nofhelipads);
		assert(nofhelipads <= MAX_HELIPADS);
	}

	/* Get the number of elements from the source table. We also double check this
	 * with the entry point which must be within bounds and use this information
	 * later on to build and validate the state machine */
	for (DiagDirection i = DIAGDIR_BEGIN; i < DIAGDIR_END; i++) {
		if (entry_points[i] >= nofelements) {
			DEBUG(misc, 0, "[Ap] entry (%d) must be within the airport (maximum %d)", entry_points[i], nofelements);
			assert(entry_points[i] < nofelements);
		}
	}

	/* Build the state machine itself */
	layout = AirportBuildAutomata(nofelements, apFA);
	DEBUG(misc, 2, "[Ap] #count %3d; #term %2d (%dgrp); #helipad %2d (%dgrp); entries %3d, %3d, %3d, %3d",
		nofelements, nofterminals, nofterminalgroups, nofhelipads, nofhelipadgroups,
		entry_points[DIAGDIR_NE], entry_points[DIAGDIR_SE], entry_points[DIAGDIR_SW], entry_points[DIAGDIR_NW]);

	/* Test if everything went allright. This is only a rude static test checking
	 * the symantic correctness. By no means does passing the test mean that the
	 * airport is working correctly or will not deadlock for example */
	uint ret = AirportTestFTA(nofelements, layout, terminals);
	if (ret != MAX_ELEMENTS) DEBUG(misc, 0, "[Ap] problem with element: %d", ret - 1);
	assert(ret == MAX_ELEMENTS);

#ifdef DEBUG_AIRPORT
	AirportPrintOut(nofelements, layout, DEBUG_AIRPORT);
#endif
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

/** Get the number of elements of a source Airport state automata
 * Since it is actually just a big array of AirportFTA types, we only
 * know one element from the other by differing 'position' identifiers */
static uint16 AirportGetNofElements(const AirportFTAbuildup *apFA)
{
	int i;
	uint16 nofelements = 0;
	int temp = apFA[0].position;

	for (i = 0; i < MAX_ELEMENTS; i++) {
		if (temp != apFA[i].position) {
			nofelements++;
			temp = apFA[i].position;
		}
		if (apFA[i].position == MAX_ELEMENTS) break;
	}
	return nofelements;
}

/* We calculate the terminal/helipod count based on the data passed to us
 * This data (terminals) contains an index as a first element as to how many
 * groups there are, and then the number of terminals for each group */
static byte AirportGetTerminalCount(const byte *terminals, byte *groups)
{
	byte i;
	byte nof_terminals = 0;
	*groups = 0;

	if (terminals != NULL) {
		i = terminals[0];
		*groups = i;
		while (i-- > 0) {
			terminals++;
			assert(*terminals != 0); // no empty groups please
			nof_terminals += *terminals;
		}
	}
	return nof_terminals;
}


static AirportFTA* AirportBuildAutomata(uint nofelements, const AirportFTAbuildup *apFA)
{
	AirportFTA *current;
	AirportFTA *FAutomata = MallocT<AirportFTA>(nofelements);
	uint16 internalcounter = 0;

	for (uint i = 0; i < nofelements; i++) {
		current = &FAutomata[i];
		current->position      = apFA[internalcounter].position;
		current->heading       = apFA[internalcounter].heading;
		current->block         = apFA[internalcounter].block;
		current->next_position = apFA[internalcounter].next;

		// outgoing nodes from the same position, create linked list
		while (current->position == apFA[internalcounter + 1].position) {
			AirportFTA *newNode = MallocT<AirportFTA>(1);

			newNode->position      = apFA[internalcounter + 1].position;
			newNode->heading       = apFA[internalcounter + 1].heading;
			newNode->block         = apFA[internalcounter + 1].block;
			newNode->next_position = apFA[internalcounter + 1].next;
			// create link
			current->next = newNode;
			current = current->next;
			internalcounter++;
		} // while
		current->next = NULL;
		internalcounter++;
	}
	return FAutomata;
}


static byte AirportTestFTA(uint nofelements, const AirportFTA *layout, const byte *terminals)
{
	uint next_position = 0;

	for (uint i = 0; i < nofelements; i++) {
		uint position = layout[i].position;
		if (position != next_position) return i;
		const AirportFTA *first = &layout[i];
		const AirportFTA *current = first;

		for (; current != NULL; current = current->next) {
			/* A heading must always be valid. The only exceptions are
			 * - multiple choices as start, identified by a special value of 255
			 * - terminal group which is identified by a special value of 255 */
			if (current->heading > MAX_HEADINGS) {
				if (current->heading != 255) return i;
				if (current == first && current->next == NULL) return i;
				if (current != first && current->next_position > terminals[0]) return i;
			}

			/* If there is only one choice, it must be at the end */
			if (current->heading == 0 && current->next != NULL) return i;
			/* Obviously the elements of the linked list must have the same identifier */
			if (position != current->position) return i;
			/* A next position must be within bounds */
			if (current->next_position >= nofelements) return i;
		}
		next_position++;
	}
	return MAX_ELEMENTS;
}

#ifdef DEBUG_AIRPORT
static const char* const _airport_heading_strings[] = {
	"TO_ALL",
	"HANGAR",
	"TERM1",
	"TERM2",
	"TERM3",
	"TERM4",
	"TERM5",
	"TERM6",
	"HELIPAD1",
	"HELIPAD2",
	"TAKEOFF",
	"STARTTAKEOFF",
	"ENDTAKEOFF",
	"HELITAKEOFF",
	"FLYING",
	"LANDING",
	"ENDLANDING",
	"HELILANDING",
	"HELIENDLANDING",
	"TERM7",
	"TERM8",
	"HELIPAD3",
	"HELIPAD4",
	"DUMMY" // extra heading for 255
};

static uint AirportBlockToString(uint32 block)
{
	uint i = 0;
	if (block & 0xffff0000) { block >>= 16; i += 16; }
	if (block & 0x0000ff00) { block >>=  8; i +=  8; }
	if (block & 0x000000f0) { block >>=  4; i +=  4; }
	if (block & 0x0000000c) { block >>=  2; i +=  2; }
	if (block & 0x00000002) { i += 1; }
	return i;
}


static void AirportPrintOut(uint nofelements, const AirportFTA *layout, bool full_report)
{
	if (!full_report) printf("(P = Current Position; NP = Next Position)\n");

	for (uint i = 0; i < nofelements; i++) {
		const AirportFTA *current = &layout[i];

		for (; current != NULL; current = current->next) {
			if (full_report) {
				byte heading = (current->heading == 255) ? MAX_HEADINGS + 1 : current->heading;
				printf("\tPos:%2d NPos:%2d Heading:%15s Block:%2d\n", current->position,
					    current->next_position, _airport_heading_strings[heading],
							AirportBlockToString(current->block));
			} else {
				printf("P:%2d NP:%2d", current->position, current->next_position);
			}
		}
		printf("\n");
	}
}
#endif

const AirportFTAClass *GetAirport(const byte airport_type)
{
	//FIXME -- AircraftNextAirportPos_and_Order -> Needs something nicer, don't like this code
	// needs constant change if more airports are added
	switch (airport_type) {
		default:               NOT_REACHED();
		case AT_SMALL:         return CountryAirport;
		case AT_LARGE:         return CityAirport;
		case AT_METROPOLITAN:  return MetropolitanAirport;
		case AT_HELIPORT:      return Heliport;
		case AT_OILRIG:        return Oilrig;
		case AT_INTERNATIONAL: return InternationalAirport;
		case AT_COMMUTER:      return CommuterAirport;
		case AT_HELIDEPOT:     return HeliDepot;
		case AT_INTERCON:      return IntercontinentalAirport;
		case AT_HELISTATION:   return HeliStation;
	}
}


uint32 GetValidAirports(void)
{
	uint32 mask = 0;

	if (_cur_year <  1960 || _patches.always_small_airport) SETBIT(mask, 0);  // small airport
	if (_cur_year >= 1955) SETBIT(mask, 1); // city airport
	if (_cur_year >= 1963) SETBIT(mask, 2); // heliport
	if (_cur_year >= 1980) SETBIT(mask, 3); // metropolitan airport
	if (_cur_year >= 1990) SETBIT(mask, 4); // international airport
	if (_cur_year >= 1983) SETBIT(mask, 5); // commuter airport
	if (_cur_year >= 1976) SETBIT(mask, 6); // helidepot
	if (_cur_year >= 2002) SETBIT(mask, 7); // intercontinental airport
	if (_cur_year >= 1980) SETBIT(mask, 8); // helistation
	return mask;
}
