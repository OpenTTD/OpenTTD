/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "map.h"
#include "airport.h"
#include "macros.h"
#include "variables.h"

AirportFTAClass *CountryAirport;
AirportFTAClass *CityAirport;
AirportFTAClass *Heliport, *Oilrig;
AirportFTAClass *MetropolitanAirport;
AirportFTAClass *InternationalAirport;

static void AirportFTAClass_Constructor(AirportFTAClass *Airport,
																				const byte *terminals, const byte *helipads,
																				const byte entry_point,  const byte acc_planes,
																				const AirportFTAbuildup *FA,
																				const TileIndexDiffC *depots, const byte nof_depots);
static void AirportFTAClass_Destructor(AirportFTAClass *Airport);

static uint16 AirportGetNofElements(const AirportFTAbuildup *FA);
static void AirportBuildAutomata(AirportFTAClass *Airport, const AirportFTAbuildup *FA);
static byte AirportTestFTA(const AirportFTAClass *Airport);
/*static void AirportPrintOut(const AirportFTAClass *Airport, const bool full_report);
static byte AirportBlockToString(uint32 block);*/

void InitializeAirports(void)
{
	// country airport
	CountryAirport = malloc(sizeof(AirportFTAClass));

	AirportFTAClass_Constructor(
		CountryAirport,
		_airport_terminal_country,
		NULL,
		16,
		ALL,
		_airport_fta_country,
		_airport_depots_country,
		lengthof(_airport_depots_country)
	);

	// city airport
	CityAirport = malloc(sizeof(AirportFTAClass));

	AirportFTAClass_Constructor(
		CityAirport,
		_airport_terminal_city,
		NULL,
		19,
		ALL,
		_airport_fta_city,
		_airport_depots_city,
		lengthof(_airport_depots_city)
	);

	// metropolitan airport
	MetropolitanAirport = malloc(sizeof(AirportFTAClass));

	AirportFTAClass_Constructor(
		MetropolitanAirport,
		_airport_terminal_metropolitan,
		NULL,
		20,
		ALL,
		_airport_fta_metropolitan,
		_airport_depots_metropolitan,
		lengthof(_airport_depots_metropolitan)
	);

	// international airport
	InternationalAirport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));

	AirportFTAClass_Constructor(
		InternationalAirport,
		_airport_terminal_international,
		_airport_helipad_international,
		37,
		ALL,
		_airport_fta_international,
		_airport_depots_international,
		lengthof(_airport_depots_international)
	);

	// heliport, oilrig
	Heliport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));

	AirportFTAClass_Constructor(
		Heliport,
		NULL,
		_airport_helipad_heliport_oilrig,
		7,
		HELICOPTERS_ONLY,
		_airport_fta_heliport_oilrig,
		NULL,
		0
	);

	Oilrig = Heliport;  // exactly the same structure for heliport/oilrig, so share state machine
}

void UnInitializeAirports(void)
{
	AirportFTAClass_Destructor(CountryAirport);
	AirportFTAClass_Destructor(CityAirport);
	AirportFTAClass_Destructor(Heliport);
	AirportFTAClass_Destructor(MetropolitanAirport);
	AirportFTAClass_Destructor(InternationalAirport);
}

static void AirportFTAClass_Constructor(AirportFTAClass *Airport,
																				const byte *terminals, const byte *helipads,
																				const byte entry_point, const byte acc_planes,
																				const AirportFTAbuildup *FA,
																				const TileIndexDiffC *depots, const byte nof_depots)
{
	byte nofterminals, nofhelipads;
	byte nofterminalgroups = 0;
	byte nofhelipadgroups = 0;
	const byte * curr;
	int i;
	nofterminals = nofhelipads = 0;

	//now we read the number of terminals we have
	if (terminals != NULL) {
		i = terminals[0];
		nofterminalgroups = i;
		curr = terminals;
		while (i-- > 0) {
			curr++;
			assert(*curr != 0);	//we don't want to have an empty group
			nofterminals += *curr;
		}

	}
	Airport->terminals = terminals;

	//read helipads
	if (helipads != NULL) {
		i = helipads[0];
		nofhelipadgroups = i;
		curr = helipads;
		while (i-- > 0) {
			curr++;
			assert(*curr != 0); //no empty groups please
			nofhelipads += *curr;
		}

	}
	Airport->helipads = helipads;

	// if there are more terminals than 6, internal variables have to be changed, so don't allow that
	// same goes for helipads
	if (nofterminals > MAX_TERMINALS) { printf("Currently only maximum of %2d terminals are supported (you wanted %2d)\n", MAX_TERMINALS, nofterminals);}
	if (nofhelipads > MAX_HELIPADS) { printf("Currently only maximum of %2d helipads are supported (you wanted %2d)\n", MAX_HELIPADS, nofhelipads);}
	// terminals/helipads are divided into groups. Groups are computed by dividing the number
	// of terminals by the number of groups. Half in half. If #terminals is uneven, first group
	// will get the less # of terminals

	assert(nofterminals <= MAX_TERMINALS);
	assert(nofhelipads <= MAX_HELIPADS);

	Airport->nofelements = AirportGetNofElements(FA);
	// check
	if (entry_point >= Airport->nofelements) {printf("Entry point (%2d) must be within the airport positions (which is max %2d)\n", entry_point, Airport->nofelements);}
	assert(entry_point < Airport->nofelements);

	Airport->acc_planes = acc_planes;
	Airport->entry_point = entry_point;
	Airport->airport_depots = depots;
	Airport->nof_depots = nof_depots;


	// build the state machine
	AirportBuildAutomata(Airport, FA);
	DEBUG(misc, 1) ("#Elements %2d; #Terminals %2d in %d group(s); #Helipads %2d in %d group(s); Entry Point %d",
		Airport->nofelements, nofterminals, nofterminalgroups, nofhelipads, nofhelipadgroups, Airport->entry_point
	);


	{
		byte ret = AirportTestFTA(Airport);
		if (ret != MAX_ELEMENTS) printf("ERROR with element: %d\n", ret - 1);
		assert(ret == MAX_ELEMENTS);
	}
	// print out full information
	// true  -- full info including heading, block, etc
	// false -- short info, only position and next position
	//AirportPrintOut(Airport, false);
}

static void AirportFTAClass_Destructor(AirportFTAClass *Airport)
{
	int i;
	AirportFTA *current, *next;

	for (i = 0; i < Airport->nofelements; i++) {
		current = Airport->layout[i].next_in_chain;
		while (current != NULL) {
			next = current->next_in_chain;
			free(current);
			current = next;
		};
	}
	free(Airport->layout);
	free(Airport);
}

static uint16 AirportGetNofElements(const AirportFTAbuildup *FA)
{
	int i;
	uint16 nofelements = 0;
	int temp = FA[0].position;

	for (i = 0; i < MAX_ELEMENTS; i++) {
		if (temp != FA[i].position) {
			nofelements++;
			temp = FA[i].position;
		}
		if (FA[i].position == MAX_ELEMENTS) break;
	}
	return nofelements;
}

static void AirportBuildAutomata(AirportFTAClass *Airport, const AirportFTAbuildup *FA)
{
	AirportFTA *FAutomata;
	AirportFTA *current;
	uint16 internalcounter, i;
	FAutomata = malloc(sizeof(AirportFTA) * Airport->nofelements);
	Airport->layout = FAutomata;
	internalcounter = 0;

	for (i = 0; i < Airport->nofelements; i++) {
		current = &Airport->layout[i];
		current->position = FA[internalcounter].position;
		current->heading  = FA[internalcounter].heading;
		current->block    = FA[internalcounter].block;
		current->next_position = FA[internalcounter].next_in_chain;

		// outgoing nodes from the same position, create linked list
		while (current->position == FA[internalcounter + 1].position) {
			AirportFTA* newNode = malloc(sizeof(AirportFTA));

			newNode->position = FA[internalcounter + 1].position;
			newNode->heading  = FA[internalcounter + 1].heading;
			newNode->block    = FA[internalcounter + 1].block;
			newNode->next_position = FA[internalcounter + 1].next_in_chain;
			// create link
			current->next_in_chain = newNode;
			current = current->next_in_chain;
			internalcounter++;
		} // while
		current->next_in_chain = NULL;
		internalcounter++;
	}
}

static byte AirportTestFTA(const AirportFTAClass *Airport)
{
	byte position, i, next_element;
	AirportFTA *temp;
	next_element = 0;

	for (i = 0; i < Airport->nofelements; i++) {
		position = Airport->layout[i].position;
		if (position != next_element) return i;
		temp = &Airport->layout[i];

		do {
			if (temp->heading > MAX_HEADINGS && temp->heading != 255) return i;
			if (temp->heading == 0 && temp->next_in_chain != 0) return i;
			if (position != temp->position) return i;
			if (temp->next_position >= Airport->nofelements) return i;
			temp = temp->next_in_chain;
		} while (temp != NULL);
		next_element++;
	}
	return MAX_ELEMENTS;
}

#if 0
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
	"DUMMY"	// extra heading for 255
};

static void AirportPrintOut(const AirportFTAClass *Airport, const bool full_report)
{
	AirportFTA *temp;
	uint16 i;
	byte heading;

	printf("(P = Current Position; NP = Next Position)\n");
	for (i = 0; i < Airport->nofelements; i++) {
		temp = &Airport->layout[i];
		if (full_report) {
			heading = (temp->heading == 255) ? MAX_HEADINGS+1 : temp->heading;
			printf("Pos:%2d NPos:%2d Heading:%15s Block:%2d\n", temp->position, temp->next_position,
						 _airport_heading_strings[heading], AirportBlockToString(temp->block));
		} else {
			printf("P:%2d NP:%2d", temp->position, temp->next_position);
		}
		while (temp->next_in_chain != NULL) {
			temp = temp->next_in_chain;
			if (full_report) {
				heading = (temp->heading == 255) ? MAX_HEADINGS+1 : temp->heading;
				printf("Pos:%2d NPos:%2d Heading:%15s Block:%2d\n", temp->position, temp->next_position,
							_airport_heading_strings[heading], AirportBlockToString(temp->block));
			} else {
				printf("P:%2d NP:%2d", temp->position, temp->next_position);
			}
		}
		printf("\n");
	}
}


static byte AirportBlockToString(uint32 block)
{
	byte i = 0;
	if (block & 0xffff0000) { block >>= 16; i += 16; }
	if (block & 0x0000ff00) { block >>= 8;  i += 8; }
	if (block & 0x000000f0) { block >>= 4;  i += 4; }
	if (block & 0x0000000c) { block >>= 2;  i += 2; }
	if (block & 0x00000002) { i += 1; }
	return i;
}
#endif

const AirportFTAClass* GetAirport(const byte airport_type)
{
	AirportFTAClass *Airport = NULL;
	//FIXME -- AircraftNextAirportPos_and_Order -> Needs something nicer, don't like this code
	// needs constant change if more airports are added
	switch (airport_type) {
		case AT_SMALL: Airport = CountryAirport; break;
		case AT_LARGE: Airport = CityAirport; break;
		case AT_METROPOLITAN: Airport = MetropolitanAirport; break;
		case AT_HELIPORT: Airport = Heliport; break;
		case AT_OILRIG: Airport = Oilrig; break;
		case AT_INTERNATIONAL: Airport = InternationalAirport; break;
		default:
			#ifdef DEBUG__
				printf("Airport AircraftNextAirportPos_and_Order not yet implemented\n");
			#endif
			assert(airport_type <= AT_INTERNATIONAL);
	}
	return Airport;
}

uint32 GetValidAirports(void)
{
	uint32 bytemask = _avail_aircraft; /// sets the first 3 bytes, 0 - 2, @see AdjustAvailAircraft()

	// 1980-1-1 is --> 21915
	// 1990-1-1 is --> 25568
	if (_date >= 21915) SETBIT(bytemask, 3); // metropilitan airport 1980
	if (_date >= 25568) SETBIT(bytemask, 4); // international airport 1990
	return bytemask;
}
