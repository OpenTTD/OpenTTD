/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_type.h Types related to stations. */

#ifndef STATION_TYPE_H
#define STATION_TYPE_H

#include "core/enum_type.hpp"
#include "core/smallvec_type.hpp"
#include "tile_type.h"

typedef uint16 StationID;
typedef uint16 RoadStopID;

struct BaseStation;
struct Station;
struct RoadStop;
struct StationSpec;
struct Waypoint;

static const StationID NEW_STATION = 0xFFFE;
static const StationID INVALID_STATION = 0xFFFF;

/** Station types */
enum StationType {
	STATION_RAIL,
	STATION_AIRPORT,
	STATION_TRUCK,
	STATION_BUS,
	STATION_OILRIG,
	STATION_DOCK,
	STATION_BUOY,
	STATION_WAYPOINT,
};

/** Types of RoadStops */
enum RoadStopType {
	ROADSTOP_BUS,    ///< A standard stop for buses
	ROADSTOP_TRUCK   ///< A standard stop for trucks
};

/** The facilities a station might be having */
enum StationFacility {
	FACIL_NONE       = 0,      ///< The station has no facilities at all
	FACIL_TRAIN      = 1 << 0, ///< Station with train station
	FACIL_TRUCK_STOP = 1 << 1, ///< Station with truck stops
	FACIL_BUS_STOP   = 1 << 2, ///< Station with bus stops
	FACIL_AIRPORT    = 1 << 3, ///< Station with an airport
	FACIL_DOCK       = 1 << 4, ///< Station with a dock
	FACIL_WAYPOINT   = 1 << 7, ///< Station is a waypoint
};
DECLARE_ENUM_AS_BIT_SET(StationFacility);
typedef SimpleTinyEnumT<StationFacility, byte> StationFacilityByte;

/** The vehicles that may have visited a station */
enum StationHadVehicleOfType {
	HVOT_NONE     = 0,      ///< Station has seen no vehicles
	HVOT_TRAIN    = 1 << 1, ///< Station has seen a train
	HVOT_BUS      = 1 << 2, ///< Station has seen a bus
	HVOT_TRUCK    = 1 << 3, ///< Station has seen a truck
	HVOT_AIRCRAFT = 1 << 4, ///< Station has seen an aircraft
	HVOT_SHIP     = 1 << 5, ///< Station has seen a ship

	HVOT_WAYPOINT = 1 << 6, ///< Station is a waypoint (NewGRF only!)
};
DECLARE_ENUM_AS_BIT_SET(StationHadVehicleOfType);
typedef SimpleTinyEnumT<StationHadVehicleOfType, byte> StationHadVehicleOfTypeByte;

/** The different catchment areas used */
enum CatchmentArea {
	CA_NONE            =  0, ///< Catchment when the station has no facilities
	CA_BUS             =  3, ///< Catchment for bus stops with "modified catchment" enabled
	CA_TRUCK           =  3, ///< Catchment for truck stops with "modified catchment" enabled
	CA_TRAIN           =  4, ///< Catchment for train stations with "modified catchment" enabled
	CA_DOCK            =  5, ///< Catchment for docks with "modified catchment" enabled

	CA_UNMODIFIED      =  4, ///< Catchment for all stations with "modified catchment" disabled

	MAX_CATCHMENT      = 10, ///< Maximum catchment for airports with "modified catchment" enabled
};

enum {
	MAX_LENGTH_STATION_NAME_BYTES  =  31, ///< The maximum length of a station name in bytes including '\0'
	MAX_LENGTH_STATION_NAME_PIXELS = 180, ///< The maximum length of a station name in pixels
};

/** Represents the covered area of e.g. a rail station */
struct TileArea {
	/** Just construct this tile area */
	TileArea() {}

	/**
	 * Construct this tile area with some set values
	 * @param tile the base tile
	 * @param w the width
	 * @param h the height
	 */
	TileArea(TileIndex tile, uint8 w, uint8 h) : tile(tile), w(w), h(h) {}

	/**
	 * Construct this tile area based on two points.
	 * @param start the start of the area
	 * @param end   the end of the area
	 */
	TileArea(TileIndex start, TileIndex end);

	TileIndex tile; ///< The base tile of the area
	uint8 w;        ///< The width of the area
	uint8 h;        ///< The height of the area

	/**
	 * Add a single tile to a tile area; enlarge if needed.
	 * @param to_add The tile to add
	 */
	void Add(TileIndex to_add);

	/**
	 * Clears the 'tile area', i.e. make the tile invalid.
	 */
	void Clear()
	{
		this->tile = INVALID_TILE;
		this->w    = 0;
		this->h    = 0;
	}
};

/** List of stations */
typedef SmallVector<Station *, 2> StationList;

/**
 * Structure contains cached list of stations nearby. The list
 * is created upon first call to GetStations()
 */
class StationFinder {
	StationList stations; ///< List of stations nearby
	TileIndex tile;       ///< Northern tile of producer, INVALID_TILE when # stations is valid
	int x_extent;         ///< Width of producer
	int y_extent;         ///< Height of producer
public:
	/**
	 * Constructs StationFinder
	 * @param t northern tile
	 * @param dx width of producer
	 * @param dy height of producer
	 */
	StationFinder(TileIndex t, int dx, int dy) : tile(t), x_extent(dx), y_extent(dy) {}
	const StationList *GetStations();
};

#endif /* STATION_TYPE_H */
