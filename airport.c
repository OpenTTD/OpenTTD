#include "stdafx.h"
#include "ttd.h"
#include "map.h"
#include "airport.h"

AirportFTAClass *CountryAirport;
AirportFTAClass *CityAirport;
AirportFTAClass *Heliport, *Oilrig;
AirportFTAClass *MetropolitanAirport;
AirportFTAClass *InternationalAirport;

static void AirportFTAClass_Constructor(AirportFTAClass *Airport,
																				const byte nofterminals, const byte nofterminalgroups,
																				const byte nofhelipads,  const byte nofhelipadgroups,
																				const byte entry_point,  const byte acc_planes,
																				const AirportFTAbuildup *FA,
																				const TileIndexDiffC *depots, const byte nof_depots);
static void AirportFTAClass_Destructor(AirportFTAClass *Airport);

static uint16 AirportGetNofElements(const AirportFTAbuildup *FA);
static void AirportBuildAutomata(AirportFTAClass *Airport, const AirportFTAbuildup *FA);
static byte AirportTestFTA(const AirportFTAClass *Airport);
/*static void AirportPrintOut(const AirportFTAClass *Airport, const bool full_report);
static byte AirportBlockToString(uint32 block);*/

void InitializeAirports()
{
	// country airport
	CountryAirport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));
	AirportFTAClass_Constructor(CountryAirport, 2, 1, 0, 0, 16, ALL, _airport_fta_country, _airport_depots_country, lengthof(_airport_depots_country));

	// city airport
	CityAirport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));
	AirportFTAClass_Constructor(CityAirport, 3, 1, 0, 0, 19, ALL, _airport_fta_city, _airport_depots_city, lengthof(_airport_depots_city));

	// metropolitan airport
	MetropolitanAirport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));
	AirportFTAClass_Constructor(MetropolitanAirport, 3, 1, 0, 0, 20, ALL, _airport_fta_metropolitan, _airport_depots_metropolitan, lengthof(_airport_depots_metropolitan));

	// international airport
	InternationalAirport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));
	AirportFTAClass_Constructor(InternationalAirport, 6, 2, 2, 1, 37, ALL, _airport_fta_international, _airport_depots_international, lengthof(_airport_depots_international));

	// heliport, oilrig
	Heliport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));
	AirportFTAClass_Constructor(Heliport, 0, 0, 1, 1, 7, HELICOPTERS_ONLY, _airport_fta_heliport_oilrig, NULL, 0);
	Oilrig = Heliport;  // exactly the same structure for heliport/oilrig, so share state machine
}

void UnInitializeAirports()
{
	AirportFTAClass_Destructor(CountryAirport);
	AirportFTAClass_Destructor(CityAirport);
	AirportFTAClass_Destructor(Heliport);
	AirportFTAClass_Destructor(MetropolitanAirport);
	AirportFTAClass_Destructor(InternationalAirport);
}

static void AirportFTAClass_Constructor(AirportFTAClass *Airport,
																				const byte nofterminals, const byte nofterminalgroups,
																				const byte nofhelipads, const byte nofhelipadgroups,
																				const byte entry_point, const byte acc_planes,
																				const AirportFTAbuildup *FA,
																				const TileIndexDiffC *depots, const byte nof_depots)
{
	// if there are more terminals than 6, internal variables have to be changed, so don't allow that
	// same goes for helipads
	if (nofterminals > MAX_TERMINALS) { printf("Currently only maximum of %2d terminals are supported (you wanted %2d)\n", MAX_TERMINALS, nofterminals);}
	if (nofhelipads > MAX_HELIPADS) { printf("Currently only maximum of %2d helipads are supported (you wanted %2d)\n", MAX_HELIPADS, nofhelipads);}
	// terminals/helipads are divided into groups. Groups are computed by dividing the number
	// of terminals by the number of groups. Half in half. If #terminals is uneven, first group
	// will get the less # of terminals
	if (nofterminalgroups > nofterminals) { printf("# of terminalgroups (%2d) must be less or equal to terminals (%2d)", nofterminals, nofterminalgroups);}
	if (nofhelipadgroups > nofhelipads) { printf("# of helipadgroups (%2d) must be less or equal to helipads (%2d)", nofhelipads, nofhelipadgroups);}

	assert(nofterminals <= MAX_TERMINALS);
	assert(nofhelipads <= MAX_HELIPADS);
	assert(nofterminalgroups <= nofterminals);
	assert(nofhelipadgroups <= nofhelipads);

	Airport->nofelements = AirportGetNofElements(FA);
	// check
	if (entry_point >= Airport->nofelements) {printf("Entry point (%2d) must be within the airport positions (which is max %2d)\n", entry_point, Airport->nofelements);}
	assert(entry_point < Airport->nofelements);

	Airport->nofterminals = nofterminals;
	Airport->nofterminalgroups = nofterminalgroups;
	Airport->nofhelipads = nofhelipads;
	Airport->nofhelipadgroups = nofhelipadgroups;
	Airport->acc_planes = acc_planes;
	Airport->entry_point = entry_point;
	Airport->airport_depots = depots;
	Airport->nof_depots = nof_depots;


	// build the state machine
	AirportBuildAutomata(Airport, FA);
		DEBUG(misc, 1) ("#Elements %2d; #Terminals %2d in %d group(s); #Helipads %2d in %d group(s)", Airport->nofelements,
				  Airport->nofterminals, Airport->nofterminalgroups, Airport->nofhelipads, Airport->nofhelipadgroups);


	{
		byte _retval = AirportTestFTA(Airport);
		if (_retval != MAX_ELEMENTS) {printf("ERROR with element: %d\n", _retval-1);}
		assert(_retval == MAX_ELEMENTS);
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
		if (FA[i].position == MAX_ELEMENTS) {break;}
	}
	return nofelements;
}

static void AirportBuildAutomata(AirportFTAClass *Airport, const AirportFTAbuildup *FA)
{
	AirportFTA *FAutomata;
	AirportFTA *current;
	uint16 internalcounter, i;
	FAutomata = (AirportFTA *)malloc(sizeof(AirportFTA) * Airport->nofelements);
	Airport->layout = FAutomata;
	internalcounter = 0;

	for (i = 0; i < Airport->nofelements; i++) {
		current = &Airport->layout[i];
		current->position = FA[internalcounter].position;
		current->heading  = FA[internalcounter].heading;
		current->block    = FA[internalcounter].block;
		current->next_position = FA[internalcounter].next_in_chain;

		// outgoing nodes from the same position, create linked list
		while (current->position == FA[internalcounter+1].position) {
			AirportFTA *newNode = (AirportFTA *)malloc(sizeof(AirportFTA));
			newNode->position = FA[internalcounter+1].position;
			newNode->heading  = FA[internalcounter+1].heading;
			newNode->block    = FA[internalcounter+1].block;
			newNode->next_position = FA[internalcounter+1].next_in_chain;
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
		if (position != next_element) {return i;}
		temp = &Airport->layout[i];

		do {
			if (temp->heading > MAX_HEADINGS && temp->heading != 255) {return i;}
			if (temp->heading == 0 && temp->next_in_chain != 0) {return i;}
			if (position != temp->position) {return i;}
			if (temp->next_position >= Airport->nofelements) {return i;}
			temp = temp->next_in_chain;
		} while (temp != NULL);
		next_element++;
	}
	return MAX_ELEMENTS;
}

static const char* const _airport_heading_strings[MAX_HEADINGS+2] = {
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

/*
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
		}
		else { printf("P:%2d NP:%2d", temp->position, temp->next_position);}
		while (temp->next_in_chain != NULL) {
			temp = temp->next_in_chain;
			if (full_report) {
				heading = (temp->heading == 255) ? MAX_HEADINGS+1 : temp->heading;
				printf("Pos:%2d NPos:%2d Heading:%15s Block:%2d\n", temp->position, temp->next_position,
							_airport_heading_strings[heading], AirportBlockToString(temp->block));
			}
			else { printf("P:%2d NP:%2d", temp->position, temp->next_position);}
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
}*/

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
