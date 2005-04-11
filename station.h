#ifndef STATION_H
#define STATION_H

#include "pool.h"
#include "sprite.h"
#include "tile.h"
#include "vehicle.h"

typedef struct GoodsEntry {
	uint16 waiting_acceptance;
	byte days_since_pickup;
	byte rating;
	uint16 enroute_from;
	byte enroute_time;
	byte last_speed;
	byte last_age;
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
	// alpha_order is obsolete since savegame format 4
	byte alpha_order_obsolete;
	uint16 string_id;

	ViewportSign sign;

	uint16 had_vehicle_of_type;

	byte time_since_load;
	byte time_since_unload;
	byte delete_ctr;
	byte owner;
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

void ModifyStationRatingAround(TileIndex tile, byte owner, int amount, uint radius);

TileIndex GetStationTileForVehicle(const Vehicle *v, const Station *st);

void ShowStationViewWindow(int station);
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
uint GetStationPlatforms(Station *st, uint tile);


/* Station layout for given dimensions - it is a two-dimensional array
 * where index is computed as (x * platforms) + platform. */
typedef byte *StationLayout;

typedef enum StationClass {
	STAT_CLASS_NONE, // unused station slot or so
	STAT_CLASS_DFLT, // default station class
	STAT_CLASS_WAYP, // waypoints

	/* TODO: When we actually support custom classes, they are
	 * going to be allocated dynamically (with some classid->sclass
	 * mapping, there's a TTDPatch limit on 16 custom classes in
	 * the whole game at the same time) with base at
	 * STAT_CLASS_CUSTOM. --pasky */
	STAT_CLASS_CUSTOM, // some custom class
} StationClass;

typedef struct StationSpec {
	uint32 grfid;
	int localidx; // per-GRFFile station index + 1; SetCustomStation() takes care of this

	StationClass sclass;

	/* Bitmask of platform numbers/lengths available for the station.  Bits
	 * 0..6 correspond to 1..7, while bit 7 corresponds to >7 platforms or
	 * lenght. */
	byte allowed_platforms;
	byte allowed_lengths;

	/* Custom sprites */
	byte tiles;
	/* 00 = plain platform
	 * 02 = platform with building
	 * 04 = platform with roof, left side
	 * 06 = platform with roof, right side
	 *
	 * These numbers are used for stations in NE-SW direction, or these
	 * numbers plus one for stations in the NW-SE direction.  */
	DrawTileSprites renderdata[8];

	/* Custom layouts */
	/* The layout array is organized like [lenghts][platforms], both being
	 * dynamic arrays, the field itself is length*platforms array containing
	 * indexes to @renderdata (only even numbers allowed) for the given
	 * station tile. */
	/* @lengths is length of the @platforms and @layouts arrays, that is
	 * number of maximal length for which the layout is defined (since
	 * arrays are indexed from 0, the length itself is at [length - 1]). */
	byte lengths;
	/* @platforms is array of number of platforms defined for each length.
	 * Zero means no platforms defined. */
	byte *platforms;
	/* @layout is @layouts-sized array of @platforms-sized arrays,
	 * containing pointers to length*platforms-sized arrays or NULL if
	 * default OTTD station layout should be used for stations of these
	 * dimensions. */
	StationLayout **layouts;

	/* Sprite offsets for renderdata->seq->image. spritegroup[0] is default
	 * whilst spritegroup[1] is "CID_PURCHASE". */
	SpriteGroup spritegroup[2];
} StationSpec;

/* Here, @stid is local per-GRFFile station index. If spec->localidx is not yet
 * set, it gets new dynamically allocated global index and spec->localidx is
 * set to @stid, otherwise we take it as that we are replacing it and try to
 * search for it first (that isn't much fast but we do it only very seldom). */
void SetCustomStation(byte stid, StationSpec *spec);
/* Here, @stid is global station index (in continous range 0..GetCustomStationsCount())
 * (lookup is therefore very fast as we do this very frequently). */
StationSpec *GetCustomStation(StationClass sclass, byte stid);
/* Get sprite offset for a given custom station and station structure (may be
 * NULL if ctype is set - that means we are in a build dialog). The station
 * structure is used for variational sprite groups. */
uint32 GetCustomStationRelocation(StationSpec *spec, Station *stat, byte ctype);
int GetCustomStationsCount(StationClass sclass);

RoadStop * GetRoadStopByTile(TileIndex tile, RoadStopType type);
static inline int GetRoadStopType(TileIndex tile)
{
	return (_map5[tile] < 0x47) ? RS_TRUCK : RS_BUS;
}

uint GetNumRoadStops(const Station *st, RoadStopType type);
RoadStop * GetPrimaryRoadStop(const Station *st, RoadStopType type);
RoadStop * AllocateRoadStop( void );
void ClearSlot(Vehicle *v, RoadStop *rs);

static inline bool IsTrainStationTile(uint tile) {
	return IsTileType(tile, MP_STATION) && IS_BYTE_INSIDE(_map5[tile], 0, 8);
}

static inline bool IsRoadStationTile(uint tile) {
	return IsTileType(tile, MP_STATION) && IS_BYTE_INSIDE(_map5[tile], 0x43, 0x4B);
}

/**
 * Check if a station really exists.
 */
static inline bool IsValidStation(Station* station)
{
	return station->xy != 0; /* XXX: Replace by INVALID_TILE someday */
}

/* Get's the direction the station exit points towards. Ie, returns 0 for a
 * station with the exit NE. */
static inline byte GetRoadStationDir(uint tile) {
	assert(IsRoadStationTile(tile));
	return (_map5[tile] - 0x43) & 3;
}

#endif /* STATION_H */
