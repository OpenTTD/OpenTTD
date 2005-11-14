/* $Id$ */

#ifndef STATION_H
#define STATION_H

#include "player.h"
#include "pool.h"
#include "sprite.h"
#include "tile.h"
#include "vehicle.h"
#include "station_newgrf.h"

typedef struct GoodsEntry {
	uint16 waiting_acceptance;
	byte days_since_pickup;
	byte rating;
	uint16 enroute_from;
	byte enroute_time;
	byte last_speed;
	byte last_age;
	int32 feeder_profit;
} GoodsEntry;

typedef enum RoadStopType {
	RS_BUS,
	RS_TRUCK
} RoadStopType;

enum {
	INVALID_STATION = 0xFFFF,
	INVALID_SLOT = 0xFFFF,
	NUM_SLOTS = 2,
	ROAD_STOP_LIMIT = 8,
};

typedef uint16 StationID;

typedef struct RoadStop {
	TileIndex xy;
	bool used;
	byte status;
	uint32 index;
	uint16 slot[NUM_SLOTS];
	StationID station;
	uint8 type;
	struct RoadStop *next;
	struct RoadStop *prev;
} RoadStop;

struct Station {
	TileIndex xy;
	RoadStop *bus_stops;
	RoadStop *truck_stops;
	TileIndex train_tile;
	TileIndex airport_tile;
	TileIndex dock_tile;
	Town *town;
	uint16 string_id;

	ViewportSign sign;

	uint16 had_vehicle_of_type;

	byte time_since_load;
	byte time_since_unload;
	byte delete_ctr;
	PlayerID owner;
	byte facilities;
	byte airport_type;

	// trainstation width/height
	byte trainst_w, trainst_h;

	byte class_id; // custom graphics station class
	byte stat_id; // custom graphics station id in the @class_id class
	uint16 build_date;

	//uint16 airport_flags;
	uint32 airport_flags;
	StationID index;

	VehicleID last_vehicle;
	GoodsEntry goods[NUM_CARGO];

	/* Stuff that is no longer used, but needed for conversion */
	TileIndex bus_tile_obsolete;
	TileIndex lorry_tile_obsolete;

	byte truck_stop_status_obsolete;
	byte bus_stop_status_obsolete;
	byte blocked_months_obsolete;
};

enum {
	FACIL_TRAIN = 1,
	FACIL_TRUCK_STOP = 2,
	FACIL_BUS_STOP = 4,
	FACIL_AIRPORT = 8,
	FACIL_DOCK = 0x10,
};

enum {
//	HVOT_PENDING_DELETE = 1<<0, // not needed anymore
	HVOT_TRAIN = 1<<1,
	HVOT_BUS = 1 << 2,
	HVOT_TRUCK = 1 << 3,
	HVOT_AIRCRAFT = 1 << 4,
	HVOT_SHIP = 1 << 5,
	/* This bit is used to mark stations. No, it does not belong here, but what
	 * can we do? ;-) */
	HVOT_BUOY = 1 << 6
};

enum {
	CA_BUS = 3,
	CA_TRUCK = 3,
	CA_AIR_OILPAD = 3,
	CA_TRAIN = 4,
	CA_AIR_HELIPORT = 4,
	CA_AIR_SMALL = 4,
	CA_AIR_LARGE = 5,
	CA_DOCK = 5,
	CA_AIR_METRO = 6,
	CA_AIR_INTER = 8,
};

void ModifyStationRatingAround(TileIndex tile, PlayerID owner, int amount, uint radius);

TileIndex GetStationTileForVehicle(const Vehicle *v, const Station *st);

void ShowStationViewWindow(StationID station);
void UpdateAllStationVirtCoord(void);

VARDEF SortStruct *_station_sort;

extern MemoryPool _station_pool;

/**
 * Get the pointer to the station with index 'index'
 */
static inline Station *GetStation(StationID index)
{
	return (Station*)GetItemFromPool(&_station_pool, index);
}

/**
 * Get the current size of the StationPool
 */
static inline uint16 GetStationPoolSize(void)
{
	return _station_pool.total_items;
}

static inline bool IsStationIndex(uint index)
{
	return index < GetStationPoolSize();
}

#define FOR_ALL_STATIONS_FROM(st, start) for (st = GetStation(start); st != NULL; st = (st->index + 1 < GetStationPoolSize()) ? GetStation(st->index + 1) : NULL)
#define FOR_ALL_STATIONS(st) FOR_ALL_STATIONS_FROM(st, 0)


/* Stuff for ROADSTOPS */

extern MemoryPool _roadstop_pool;

/**
 * Get the pointer to the roadstop with index 'index'
 */
static inline RoadStop *GetRoadStop(uint index)
{
	return (RoadStop*)GetItemFromPool(&_roadstop_pool, index);
}

/**
 * Get the current size of the RoadStoptPool
 */
static inline uint16 GetRoadStopPoolSize(void)
{
	return _roadstop_pool.total_items;
}

#define FOR_ALL_ROADSTOPS_FROM(rs, start) for (rs = GetRoadStop(start); rs != NULL; rs = (rs->index + 1 < GetRoadStopPoolSize()) ? GetRoadStop(rs->index + 1) : NULL)
#define FOR_ALL_ROADSTOPS(rs) FOR_ALL_ROADSTOPS_FROM(rs, 0)

/* End of stuff for ROADSTOPS */


VARDEF bool _station_sort_dirty[MAX_PLAYERS];
VARDEF bool _global_station_sort_dirty;

void GetProductionAroundTiles(AcceptedCargo produced, TileIndex tile, int w, int h, int rad);
void GetAcceptanceAroundTiles(AcceptedCargo accepts, TileIndex tile, int w, int h, int rad);
uint GetStationPlatforms(const Station *st, TileIndex tile);


void StationPickerDrawSprite(int x, int y, RailType railtype, int image);

/* Get sprite offset for a given custom station and station structure (may be
 * NULL if ctype is set - that means we are in a build dialog). The station
 * structure is used for variational sprite groups. */
uint32 GetCustomStationRelocation(const StationSpec *spec, const Station *st, byte ctype);

RoadStop * GetRoadStopByTile(TileIndex tile, RoadStopType type);
static inline int GetRoadStopType(TileIndex tile)
{
	return (_m[tile].m5 < 0x47) ? RS_TRUCK : RS_BUS;
}

uint GetNumRoadStops(const Station *st, RoadStopType type);
RoadStop * GetPrimaryRoadStop(const Station *st, RoadStopType type);
RoadStop * AllocateRoadStop( void );
void ClearSlot(Vehicle *v, RoadStop *rs);

static inline bool IsTrainStationTile(TileIndex tile)
{
	return IsTileType(tile, MP_STATION) && IS_BYTE_INSIDE(_m[tile].m5, 0, 8);
}

static inline bool IsCompatibleTrainStationTile(TileIndex tile, TileIndex ref)
{
	assert(IsTrainStationTile(ref));
	return
		IsTrainStationTile(tile) &&
		GB(_m[tile].m3, 0, 4) == GB(_m[ref].m3, 0, 4) && // same rail type?
		GB(_m[tile].m5, 0, 1) == GB(_m[ref].m5, 0, 1);   // same direction?
}

static inline bool IsRoadStationTile(TileIndex tile) {
	return IsTileType(tile, MP_STATION) && IS_BYTE_INSIDE(_m[tile].m5, 0x43, 0x4B);
}

/**
 * Check if a station really exists.
 */
static inline bool IsValidStation(const Station *st)
{
	return st->xy != 0; /* XXX: Replace by INVALID_TILE someday */
}

static inline bool IsBuoy(const Station* st)
{
	return st->had_vehicle_of_type & HVOT_BUOY; /* XXX: We should really ditch this ugly coding and switch to something sane... */
}

static inline bool IsBuoyTile(TileIndex tile)
{
	return IsTileType(tile, MP_STATION) && _m[tile].m5 == 0x52;
}

static inline bool TileBelongsToRailStation(const Station *st, TileIndex tile)
{
	return IsTileType(tile, MP_STATION) && _m[tile].m2 == st->index && _m[tile].m5 < 8;
}

/* Get's the direction the station exit points towards. Ie, returns 0 for a
 * station with the exit NE. */
static inline byte GetRoadStationDir(TileIndex tile)
{
	assert(IsRoadStationTile(tile));
	return (_m[tile].m5 - 0x43) & 3;
}

#endif /* STATION_H */
