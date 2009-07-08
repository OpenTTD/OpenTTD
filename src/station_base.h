/* $Id$ */

/** @file station_base.h Base classes/functions for stations. */

#ifndef STATION_BASE_H
#define STATION_BASE_H

#include "station_type.h"
#include "airport.h"
#include "core/pool_type.hpp"
#include "cargopacket.h"
#include "cargo_type.h"
#include "town_type.h"
#include "strings_type.h"
#include "date_type.h"
#include "vehicle_type.h"
#include "company_type.h"
#include "industry_type.h"
#include "core/geometry_type.hpp"
#include "viewport_type.h"
#include "station_map.h"
#include <list>

typedef Pool<Station, StationID, 32, 64000> StationPool;
extern StationPool _station_pool;

static const byte INITIAL_STATION_RATING = 175;

struct GoodsEntry {
	enum AcceptancePickup {
		ACCEPTANCE,
		PICKUP
	};

	GoodsEntry() :
		acceptance_pickup(0),
		days_since_pickup(255),
		rating(INITIAL_STATION_RATING),
		last_speed(0),
		last_age(255)
	{}

	byte acceptance_pickup;
	byte days_since_pickup;
	byte rating;
	byte last_speed;
	byte last_age;
	CargoList cargo; ///< The cargo packets of cargo waiting in this station
};

struct StationSpecList {
	const StationSpec *spec;
	uint32 grfid;      ///< GRF ID of this custom station
	uint8  localidx;   ///< Station ID within GRF of station
};

/** StationRect - used to track station spread out rectangle - cheaper than scanning whole map */
struct StationRect : public Rect {
	enum StationRectMode
	{
		ADD_TEST = 0,
		ADD_TRY,
		ADD_FORCE
	};

	StationRect();
	void MakeEmpty();
	bool PtInExtendedRect(int x, int y, int distance = 0) const;
	bool IsEmpty() const;
	bool BeforeAddTile(TileIndex tile, StationRectMode mode);
	bool BeforeAddRect(TileIndex tile, int w, int h, StationRectMode mode);
	bool AfterRemoveTile(Station *st, TileIndex tile);
	bool AfterRemoveRect(Station *st, TileIndex tile, int w, int h);

	static bool ScanForStationTiles(StationID st_id, int left_a, int top_a, int right_a, int bottom_a);

	StationRect& operator = (Rect src);
};

typedef SmallVector<Industry *, 2> IndustryVector;

/** Station data structure */
struct Station : StationPool::PoolItem<&_station_pool> {
public:
	RoadStop *GetPrimaryRoadStop(RoadStopType type) const
	{
		return type == ROADSTOP_BUS ? bus_stops : truck_stops;
	}

	RoadStop *GetPrimaryRoadStop(const struct RoadVehicle *v) const;

	const AirportFTAClass *Airport() const
	{
		if (airport_tile == INVALID_TILE) return GetAirport(AT_DUMMY);
		return GetAirport(airport_type);
	}

	TileIndex xy;
	RoadStop *bus_stops;
	RoadStop *truck_stops;
	TileIndex train_tile;
	TileIndex airport_tile;
	TileIndex dock_tile;
	Town *town;

	/* Place to get a name from, in order of importance: */
	char *name;             ///< Custom name
	IndustryType indtype;   ///< Industry type to get the name from
	StringID string_id;     ///< Default name (town area) of station

	ViewportSign sign;

	StationHadVehicleOfTypeByte had_vehicle_of_type;

	byte time_since_load;
	byte time_since_unload;
	byte delete_ctr;
	OwnerByte owner;
	StationFacilityByte facilities;
	byte airport_type;

	/* trainstation width/height */
	byte trainst_w, trainst_h;

	/** List of custom stations (StationSpecs) allocated to the station */
	uint8 num_specs;
	StationSpecList *speclist;

	Date build_date;  ///< Date of construction

	uint64 airport_flags;   ///< stores which blocks on the airport are taken. was 16 bit earlier on, then 32

	byte last_vehicle_type;
	std::list<Vehicle *> loading_vehicles;
	GoodsEntry goods[NUM_CARGO];  ///< Goods at this station

	IndustryVector industries_near; ///< Cached list of industries near the station that can accept cargo, @see DeliverGoodsToIndustry()

	uint16 random_bits;
	byte waiting_triggers;
	uint8 cached_anim_triggers; ///< Combined animation trigger bitmask, used to determine if trigger processing should happen.

	StationRect rect; ///< Station spread out rectangle (not saved) maintained by StationRect_xxx() functions

	Station(TileIndex tile = INVALID_TILE);
	~Station();

	void AddFacility(StationFacility new_facility_bit, TileIndex facil_xy);

	/**
	 * Marks the tiles of the station as dirty.
	 *
	 * @ingroup dirty
	 */
	void MarkTilesDirty(bool cargo_change) const;

	void UpdateVirtCoord();

	uint GetPlatformLength(TileIndex tile, DiagDirection dir) const;
	uint GetPlatformLength(TileIndex tile) const;
	void RecomputeIndustriesNear();
	static void RecomputeIndustriesNearForAll();

	uint GetCatchmentRadius() const;

	FORCEINLINE bool TileBelongsToRailStation(TileIndex tile) const
	{
		return IsRailwayStationTile(tile) && GetStationIndex(tile) == this->index;
	}

	/**
	 * Determines whether a station is a buoy only.
	 * @todo Ditch this encoding of buoys
	 */
	FORCEINLINE bool IsBuoy() const
	{
		return (this->had_vehicle_of_type & HVOT_BUOY) != 0;
	}

	static FORCEINLINE Station *GetByTile(TileIndex tile)
	{
		return Station::Get(GetStationIndex(tile));
	}

	static void PostDestructor(size_t index);
};

#define FOR_ALL_STATIONS_FROM(var, start) FOR_ALL_ITEMS_FROM(Station, station_index, var, start)
#define FOR_ALL_STATIONS(var) FOR_ALL_STATIONS_FROM(var, 0)

#endif /* STATION_BASE_H */
