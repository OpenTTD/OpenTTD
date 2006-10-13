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
	const byte entry_point,  const byte acc_planes,
	const AirportFTAbuildup *apFA,
	const TileIndexDiffC *depots, const byte nof_depots,
	uint size_x, uint size_y
);
static void AirportFTAClass_Destructor(AirportFTAClass *apc);

static uint16 AirportGetNofElements(const AirportFTAbuildup *apFA);
static void AirportBuildAutomata(AirportFTAClass *apc, const AirportFTAbuildup *apFA);
static byte AirportTestFTA(const AirportFTAClass *apc);
#if 0
static void AirportPrintOut(const AirportFTAClass *apc, const bool full_report);
#endif

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
		lengthof(_airport_depots_country),
		4, 3
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
		lengthof(_airport_depots_city),
		6, 6
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
		lengthof(_airport_depots_metropolitan),
		6, 6
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
		lengthof(_airport_depots_international),
		7, 7
	);

	// intercontintental airport
	IntercontinentalAirport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));

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
	Heliport = (AirportFTAClass *)malloc(sizeof(AirportFTAClass));

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
	CommuterAirport = malloc(sizeof(AirportFTAClass));

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
	HeliDepot = malloc(sizeof(AirportFTAClass));

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
	HeliStation = malloc(sizeof(AirportFTAClass));

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
	const byte entry_point, const byte acc_planes,
	const AirportFTAbuildup *apFA,
	const TileIndexDiffC *depots, const byte nof_depots,
	uint size_x, uint size_y
)
{
	byte nofterminals, nofhelipads;
	byte nofterminalgroups = 0;
	byte nofhelipadgroups = 0;
	const byte *curr;
	int i;
	nofterminals = nofhelipads = 0;

	apc->size_x = size_x;
	apc->size_y = size_y;

	//now we read the number of terminals we have
	if (terminals != NULL) {
		i = terminals[0];
		nofterminalgroups = i;
		curr = terminals;
		while (i-- > 0) {
			curr++;
			assert(*curr != 0); //we don't want to have an empty group
			nofterminals += *curr;
		}

	}
	apc->terminals = terminals;

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
	apc->helipads = helipads;

	// if there are more terminals than 6, internal variables have to be changed, so don't allow that
	// same goes for helipads
	if (nofterminals > MAX_TERMINALS) { printf("Currently only maximum of %2d terminals are supported (you wanted %2d)\n", MAX_TERMINALS, nofterminals);}
	if (nofhelipads > MAX_HELIPADS) { printf("Currently only maximum of %2d helipads are supported (you wanted %2d)\n", MAX_HELIPADS, nofhelipads);}
	// terminals/helipads are divided into groups. Groups are computed by dividing the number
	// of terminals by the number of groups. Half in half. If #terminals is uneven, first group
	// will get the less # of terminals

	assert(nofterminals <= MAX_TERMINALS);
	assert(nofhelipads <= MAX_HELIPADS);

	apc->nofelements = AirportGetNofElements(apFA);
	// check
	if (entry_point >= apc->nofelements) {printf("Entry point (%2d) must be within the airport positions (which is max %2d)\n", entry_point, apc->nofelements);}
	assert(entry_point < apc->nofelements);

	apc->acc_planes = acc_planes;
	apc->entry_point = entry_point;
	apc->airport_depots = depots;
	apc->nof_depots = nof_depots;


	// build the state machine
	AirportBuildAutomata(apc, apFA);
	DEBUG(misc, 1) ("#Elements %2d; #Terminals %2d in %d group(s); #Helipads %2d in %d group(s); Entry Point %d",
		apc->nofelements, nofterminals, nofterminalgroups, nofhelipads, nofhelipadgroups, apc->entry_point
	);


	{
		byte ret = AirportTestFTA(apc);
		if (ret != MAX_ELEMENTS) printf("ERROR with element: %d\n", ret - 1);
		assert(ret == MAX_ELEMENTS);
	}
	// print out full information
	// true  -- full info including heading, block, etc
	// false -- short info, only position and next position
#if 0
	AirportPrintOut(apc, false);
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

static void AirportBuildAutomata(AirportFTAClass *apc, const AirportFTAbuildup *apFA)
{
	AirportFTA *FAutomata;
	AirportFTA *current;
	uint16 internalcounter, i;
	FAutomata = malloc(sizeof(AirportFTA) * apc->nofelements);
	apc->layout = FAutomata;
	internalcounter = 0;

	for (i = 0; i < apc->nofelements; i++) {
		current = &apc->layout[i];
		current->position = apFA[internalcounter].position;
		current->heading  = apFA[internalcounter].heading;
		current->block    = apFA[internalcounter].block;
		current->next_position = apFA[internalcounter].next;

		// outgoing nodes from the same position, create linked list
		while (current->position == apFA[internalcounter + 1].position) {
			AirportFTA *newNode = malloc(sizeof(AirportFTA));

			newNode->position = apFA[internalcounter + 1].position;
			newNode->heading  = apFA[internalcounter + 1].heading;
			newNode->block    = apFA[internalcounter + 1].block;
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
	byte position, i, next_element;
	AirportFTA *temp;
	next_element = 0;

	for (i = 0; i < apc->nofelements; i++) {
		position = apc->layout[i].position;
		if (position != next_element) return i;
		temp = &apc->layout[i];

		do {
			if (temp->heading > MAX_HEADINGS && temp->heading != 255) return i;
			if (temp->heading == 0 && temp->next != 0) return i;
			if (position != temp->position) return i;
			if (temp->next_position >= apc->nofelements) return i;
			temp = temp->next;
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


static void AirportPrintOut(const AirportFTAClass *apc, const bool full_report)
{
	byte heading;
	uint i;

	printf("(P = Current Position; NP = Next Position)\n");
	for (i = 0; i < apc->nofelements; i++) {
		const AirportFTA* temp = &apc->layout[i];

		if (full_report) {
			heading = (temp->heading == 255) ? MAX_HEADINGS + 1 : temp->heading;
			printf("Pos:%2d NPos:%2d Heading:%15s Block:%2d\n",
				temp->position, temp->next_position,
				_airport_heading_strings[heading], AirportBlockToString(temp->block)
			);
		} else {
			printf("P:%2d NP:%2d", temp->position, temp->next_position);
		}
		while (temp->next != NULL) {
			temp = temp->next;
			if (full_report) {
				heading = (temp->heading == 255) ? MAX_HEADINGS + 1 : temp->heading;
				printf("Pos:%2d NPos:%2d Heading:%15s Block:%2d\n",
					temp->position, temp->next_position,
					_airport_heading_strings[heading], AirportBlockToString(temp->block)
				);
			} else {
				printf("P:%2d NP:%2d", temp->position, temp->next_position);
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

	if (_cur_year >= 1980) SETBIT(bytemask, 3); // metropilitan airport
	if (_cur_year >= 1990) SETBIT(bytemask, 4); // international airport
	if (_cur_year >= 1983) SETBIT(bytemask, 5); // commuter airport
	if (_cur_year >= 1976) SETBIT(bytemask, 6); // helidepot
	if (_cur_year >= 2002) SETBIT(bytemask, 7); // intercontinental airport
	if (_cur_year >= 1980) SETBIT(bytemask, 8); // helistation
	return bytemask;
}
