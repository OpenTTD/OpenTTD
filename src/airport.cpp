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

static void AirportFTAClass_Constructor(AirportFTAClass *apc,
	const byte *terminals, const byte *helipads,
	const byte entry_point,  const AcceptPlanes acc_planes,
	const AirportFTAbuildup *apFA,
	const TileIndexDiffC *depots, const byte nof_depots,
	uint size_x, uint size_y
);
static void AirportFTAClass_Destructor(AirportFTAClass *apc);

static uint16 AirportGetNofElements(const AirportFTAbuildup *apFA);
static void AirportBuildAutomata(AirportFTAClass *apc, const AirportFTAbuildup *apFA);
static byte AirportGetTerminalCount(const byte *terminals, byte *groups);
static byte AirportTestFTA(const AirportFTAClass *apc);

#ifdef DEBUG_AIRPORT
static void AirportPrintOut(const AirportFTAClass *apc, bool full_report);
#endif /* DEBUG_AIRPORT */

void InitializeAirports(void)
{
	// country airport
	CountryAirport = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		CountryAirport,
		_airport_terminal_country,
		NULL,
		16,
		ALL,
		_airport_fta_country,
		_airport_depots_country,
		lengthof(_airport_depots_country),
		4, 3
	);

	// city airport
	CityAirport = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		CityAirport,
		_airport_terminal_city,
		NULL,
		19,
		ALL,
		_airport_fta_city,
		_airport_depots_city,
		lengthof(_airport_depots_city),
		6, 6
	);

	// metropolitan airport
	MetropolitanAirport = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		MetropolitanAirport,
		_airport_terminal_metropolitan,
		NULL,
		20,
		ALL,
		_airport_fta_metropolitan,
		_airport_depots_metropolitan,
		lengthof(_airport_depots_metropolitan),
		6, 6
	);

	// international airport
	InternationalAirport = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		InternationalAirport,
		_airport_terminal_international,
		_airport_helipad_international,
		37,
		ALL,
		_airport_fta_international,
		_airport_depots_international,
		lengthof(_airport_depots_international),
		7, 7
	);

	// intercontintental airport
	IntercontinentalAirport = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		IntercontinentalAirport,
		_airport_terminal_intercontinental,
		_airport_helipad_intercontinental,
		43,
		ALL,
		_airport_fta_intercontinental,
		_airport_depots_intercontinental,
		lengthof(_airport_depots_intercontinental),
		9,11
	);

	// heliport, oilrig
	Heliport = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		Heliport,
		NULL,
		_airport_helipad_heliport_oilrig,
		7,
		HELICOPTERS_ONLY,
		_airport_fta_heliport_oilrig,
		NULL,
		0,
		1, 1
	);

	Oilrig = Heliport;  // exactly the same structure for heliport/oilrig, so share state machine

	// commuter airport
	CommuterAirport = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		CommuterAirport,
		_airport_terminal_commuter,
		_airport_helipad_commuter,
		22,
		ALL,
		_airport_fta_commuter,
		_airport_depots_commuter,
		lengthof(_airport_depots_commuter),
		5,4
	);

	// helidepot airport
	HeliDepot = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		HeliDepot,
		NULL,
		_airport_helipad_helidepot,
		4,
		HELICOPTERS_ONLY,
		_airport_fta_helidepot,
		_airport_depots_helidepot,
		lengthof(_airport_depots_helidepot),
		2,2
	);

	// helistation airport
	HeliStation = MallocT<AirportFTAClass>(1);

	AirportFTAClass_Constructor(
		HeliStation,
		NULL,
		_airport_helipad_helistation,
		25,
		HELICOPTERS_ONLY,
		_airport_fta_helistation,
		_airport_depots_helistation,
		lengthof(_airport_depots_helistation),
		4,2
	);

}

void UnInitializeAirports(void)
{
	AirportFTAClass_Destructor(CountryAirport);
	AirportFTAClass_Destructor(CityAirport);
	AirportFTAClass_Destructor(Heliport);
	AirportFTAClass_Destructor(MetropolitanAirport);
	AirportFTAClass_Destructor(InternationalAirport);
	AirportFTAClass_Destructor(CommuterAirport);
	AirportFTAClass_Destructor(HeliDepot);
	AirportFTAClass_Destructor(IntercontinentalAirport);
	AirportFTAClass_Destructor(HeliStation);
}

static void AirportFTAClass_Constructor(AirportFTAClass *apc,
	const byte *terminals, const byte *helipads,
	const byte entry_point, const AcceptPlanes acc_planes,
	const AirportFTAbuildup *apFA,
	const TileIndexDiffC *depots, const byte nof_depots,
	uint size_x, uint size_y
)
{
	byte nofterminals, nofhelipads;
	byte nofterminalgroups, nofhelipadgroups;

	apc->size_x = size_x;
	apc->size_y = size_y;

	/* Set up the terminal and helipad count for an airport.
	 * TODO: If there are more than 10 terminals or 4 helipads, internal variables
	 * need to be changed, so don't allow that for now */
	nofterminals = AirportGetTerminalCount(terminals, &nofterminalgroups);
	if (nofterminals > MAX_TERMINALS) {
		DEBUG(misc, 0, "[Ap] only a maximum of %d terminals are supported (requested %d)", MAX_TERMINALS, nofterminals);
		assert(nofterminals <= MAX_TERMINALS);
	}
	apc->terminals = terminals;

	nofhelipads = AirportGetTerminalCount(helipads, &nofhelipadgroups);
	if (nofhelipads > MAX_HELIPADS) {
		DEBUG(misc, 0, "[Ap] only a maximum of %d helipads are supported (requested %d)", MAX_HELIPADS, nofhelipads);
		assert(nofhelipads <= MAX_HELIPADS);
	}
	apc->helipads = helipads;

	/* Get the number of elements from the source table. We also double check this
	 * with the entry point which must be within bounds and use this information
	 * later on to build and validate the state machine */
	apc->nofelements = AirportGetNofElements(apFA);
	if (entry_point >= apc->nofelements) {
		DEBUG(misc, 0, "[Ap] entry (%d) must be within the airport (maximum %d)", entry_point, apc->nofelements);
		assert(entry_point < apc->nofelements);
	}

	apc->acc_planes     = acc_planes;
	apc->entry_point    = entry_point;
	apc->airport_depots = depots;
	apc->nof_depots     = nof_depots;

	/* Build the state machine itself */
	AirportBuildAutomata(apc, apFA);
	DEBUG(misc, 2, "[Ap] #count %3d; #term %2d (%dgrp); #helipad %2d (%dgrp); entry %3d",
		apc->nofelements, nofterminals, nofterminalgroups, nofhelipads, nofhelipadgroups, apc->entry_point);

	/* Test if everything went allright. This is only a rude static test checking
	 * the symantic correctness. By no means does passing the test mean that the
	 * airport is working correctly or will not deadlock for example */
	{ byte ret = AirportTestFTA(apc);
		if (ret != MAX_ELEMENTS) DEBUG(misc, 0, "[Ap] problem with element: %d", ret - 1);
		assert(ret == MAX_ELEMENTS);
	}

#ifdef DEBUG_AIRPORT
	AirportPrintOut(apc, DEBUG_AIRPORT);
#endif
}

static void AirportFTAClass_Destructor(AirportFTAClass *apc)
{
	int i;
	AirportFTA *current, *next;

	for (i = 0; i < apc->nofelements; i++) {
		current = apc->layout[i].next;
		while (current != NULL) {
			next = current->next;
			free(current);
			current = next;
		};
	}
	free(apc->layout);
	free(apc);
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

static void AirportBuildAutomata(AirportFTAClass *apc, const AirportFTAbuildup *apFA)
{
	AirportFTA *current;
	AirportFTA *FAutomata = MallocT<AirportFTA>(apc->nofelements);
	uint16 internalcounter = 0;
	uint16 i;

	apc->layout = FAutomata;
	for (i = 0; i < apc->nofelements; i++) {
		current = &apc->layout[i];
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
}

static byte AirportTestFTA(const AirportFTAClass *apc)
{
	byte position, i, next_position;
	AirportFTA *current, *first;
	next_position = 0;

	for (i = 0; i < apc->nofelements; i++) {
		position = apc->layout[i].position;
		if (position != next_position) return i;
		current = first = &apc->layout[i];

		for (; current != NULL; current = current->next) {
			/* A heading must always be valid. The only exceptions are
			 * - multiple choices as start, identified by a special value of 255
			 * - terminal group which is identified by a special value of 255 */
			if (current->heading > MAX_HEADINGS) {
				if (current->heading != 255) return i;
				if (current == first && current->next == NULL) return i;
				if (current != first && current->next_position > apc->terminals[0]) return i;
			}

			/* If there is only one choice, it must be at the end */
			if (current->heading == 0 && current->next != NULL) return i;
			/* Obviously the elements of the linked list must have the same identifier */
			if (position != current->position) return i;
			/* A next position must be within bounds */
			if (current->next_position >= apc->nofelements) return i;
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

static void AirportPrintOut(const AirportFTAClass *apc, bool full_report)
{
	uint16 i;

	if (!full_report) printf("(P = Current Position; NP = Next Position)\n");

	for (i = 0; i < apc->nofelements; i++) {
		AirportFTA *current = &apc->layout[i];

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

const AirportMovingData *GetAirportMovingData(byte airport_type, byte position)
{
	assert(airport_type < lengthof(_airport_moving_datas));
	assert(position < GetAirport(airport_type)->nofelements);
	return &_airport_moving_datas[airport_type][position];
}

uint32 GetValidAirports(void)
{
	uint32 bytemask = _avail_aircraft; /// sets the first 3 bytes, 0 - 2, @see AdjustAvailAircraft()

	if (_cur_year >= 1980) SETBIT(bytemask, 3); // metropolitan airport
	if (_cur_year >= 1990) SETBIT(bytemask, 4); // international airport
	if (_cur_year >= 1983) SETBIT(bytemask, 5); // commuter airport
	if (_cur_year >= 1976) SETBIT(bytemask, 6); // helidepot
	if (_cur_year >= 2002) SETBIT(bytemask, 7); // intercontinental airport
	if (_cur_year >= 1980) SETBIT(bytemask, 8); // helistation
	return bytemask;
}
