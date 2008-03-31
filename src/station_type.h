/* $Id$ */

/** @file station_type.h Types related to stations. */

#ifndef STATION_TYPE_H
#define STATION_TYPE_H

typedef uint16 StationID;
typedef uint16 RoadStopID;

struct Station;
struct RoadStop;
struct StationSpec;

static const StationID INVALID_STATION = 0xFFFF;

enum StationType {
	STATION_RAIL,
	STATION_AIRPORT,
	STATION_TRUCK,
	STATION_BUS,
	STATION_OILRIG,
	STATION_DOCK,
	STATION_BUOY
};

/** Types of RoadStops */
enum RoadStopType {
	ROADSTOP_BUS,    ///< A standard stop for buses
	ROADSTOP_TRUCK   ///< A standard stop for trucks
};

enum {
	FACIL_TRAIN      = 0x01,
	FACIL_TRUCK_STOP = 0x02,
	FACIL_BUS_STOP   = 0x04,
	FACIL_AIRPORT    = 0x08,
	FACIL_DOCK       = 0x10,
};

enum {
//	HVOT_PENDING_DELETE = 1 << 0, // not needed anymore
	HVOT_TRAIN    = 1 << 1,
	HVOT_BUS      = 1 << 2,
	HVOT_TRUCK    = 1 << 3,
	HVOT_AIRCRAFT = 1 << 4,
	HVOT_SHIP     = 1 << 5,
	/* This bit is used to mark stations. No, it does not belong here, but what
	 * can we do? ;-) */
	HVOT_BUOY     = 1 << 6
};

enum CatchmentArea {
	CA_NONE            =  0,
	CA_BUS             =  3,
	CA_TRUCK           =  3,
	CA_TRAIN           =  4,
	CA_DOCK            =  5,

	CA_UNMODIFIED      =  4, ///< Used when _patches.modified_catchment is false

	MAX_CATCHMENT      = 10, ///< Airports have a catchment up to this number.
};

#endif /* STATION_TYPE_H */
