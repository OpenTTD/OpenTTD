/* $Id$ */

#ifndef STATION_H
#define STATION_H

#include "airport.h"
#include "player.h"
#include "oldpool.h"
#include "sprite.h"
#include "tile.h"
#include "newgrf_station.h"

static const StationID INVALID_STATION = 0xFFFF;

typedef struct GoodsEntry {
	GoodsEntry() :
		waiting_acceptance(0),
		unload_pending(0),
		days_since_pickup(0),
		rating(175),
		enroute_from(INVALID_STATION),
		enroute_from_xy(INVALID_TILE),
		last_speed(0),
		last_age(255),
		feeder_profit(0)
	{}

	uint16 waiting_acceptance;
	uint16 unload_pending;        ///< records how much cargo is awaiting transfer during gradual loading to allow correct fee calc
	byte days_since_pickup;
	byte rating;
	StationID enroute_from;
	TileIndex enroute_from_xy;
	byte enroute_time;
	byte last_speed;
	byte last_age;
	int32 feeder_profit;
} GoodsEntry;

/** A Stop for a Road Vehicle */
struct RoadStop {
	/** Types of RoadStops */
	enum Type {
		BUS,                                ///< A standard stop for buses
		TRUCK                               ///< A standard stop for trucks
	};

	static const int  cDebugCtorLevel =  3;  ///< Debug level on which Contructor / Destructor messages are printed
	static const uint LIMIT           = 16;  ///< The maximum amount of roadstops that are allowed at a single station
	static const uint MAX_BAY_COUNT   =  2;  ///< The maximum number of loading bays

	TileIndex        xy;                    ///< Position on the map
	RoadStopID       index;                 ///< Global (i.e. pool-wide) index
	byte             status;                ///< Current status of the Stop. Like which spot is taken. Access using *Bay and *Busy functions.
	byte             num_vehicles;          ///< Number of vehicles currently slotted to this stop
	struct RoadStop  *next;                 ///< Next stop of the given type at this station

	RoadStop(TileIndex tile);
	~RoadStop();

	void *operator new (size_t size);
	void operator delete(void *rs);

	/* For loading games */
	void *operator new (size_t size, int index);
	void operator delete(void *rs, int index);

	bool IsValid() const;

	/* For accessing status */
	bool HasFreeBay() const;
	bool IsFreeBay(uint nr) const;
	uint AllocateBay();
	void AllocateDriveThroughBay(uint nr);
	void FreeBay(uint nr);
	bool IsEntranceBusy() const;
	void SetEntranceBusy(bool busy);
protected:
	static RoadStop *AllocateRaw(void);
};

typedef struct StationSpecList {
	const StationSpec *spec;
	uint32 grfid;      /// GRF ID of this custom station
	uint8  localidx;   /// Station ID within GRF of station
} StationSpecList;

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

struct Station {
	public:
		RoadStop *GetPrimaryRoadStop(RoadStop::Type type) const
		{
			return type == RoadStop::BUS ? bus_stops : truck_stops;
		}

		const AirportFTAClass *Airport() const
		{
			assert(airport_tile != 0);
			return GetAirport(airport_type);
		}

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
	PlayerByte owner;
	byte facilities;
	byte airport_type;

	// trainstation width/height
	byte trainst_w, trainst_h;

	/** List of custom stations (StationSpecs) allocated to the station */
	uint8 num_specs;
	StationSpecList *speclist;

	Date build_date;

	uint64 airport_flags;   /// stores which blocks on the airport are taken. was 16 bit earlier on, then 32
	StationID index;

	byte last_vehicle_type;
	GoodsEntry goods[NUM_CARGO];

	uint16 random_bits;
	byte waiting_triggers;

	StationRect rect; ///< Station spread out rectangle (not saved) maintained by StationRect_xxx() functions

	static const int cDebugCtorLevel = 3;

	Station(TileIndex tile = 0);
	~Station();

	/* normal new/delete operators. Used when building/removing station */
	void* operator new (size_t size);
	void operator delete(void *p);

	/* new/delete operators accepting station index. Used when loading station from savegame. */
	void* operator new (size_t size, int st_idx);
	void operator delete(void *p, int st_idx);

	void AddFacility(byte new_facility_bit, TileIndex facil_xy);
	void MarkDirty() const;
	void MarkTilesDirty() const;
	bool TileBelongsToRailStation(TileIndex tile) const;
	uint GetPlatformLength(TileIndex tile, DiagDirection dir) const;
	uint GetPlatformLength(TileIndex tile) const;
	bool IsBuoy() const;
	bool IsValid() const;

protected:
	static Station *AllocateRaw(void);
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

typedef enum CatchmentAreas {
	CA_NONE            =  0,
	CA_BUS             =  3,
	CA_TRUCK           =  3,
	CA_TRAIN           =  4,
	CA_DOCK            =  5
} CatchmentArea;

void ModifyStationRatingAround(TileIndex tile, PlayerID owner, int amount, uint radius);

void ShowStationViewWindow(StationID station);
void UpdateAllStationVirtCoord(void);

/* sorter stuff */
void RebuildStationLists(void);
void ResortStationLists(void);

DECLARE_OLD_POOL(Station, Station, 6, 1000)

static inline StationID GetMaxStationIndex(void)
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetStationPoolSize() - 1;
}

static inline uint GetNumStations(void)
{
	return GetStationPoolSize();
}

static inline bool IsValidStationID(StationID index)
{
	return index < GetStationPoolSize() && GetStation(index)->IsValid();
}

#define FOR_ALL_STATIONS_FROM(st, start) for (st = GetStation(start); st != NULL; st = (st->index + 1U < GetStationPoolSize()) ? GetStation(st->index + 1U) : NULL) if (st->IsValid())
#define FOR_ALL_STATIONS(st) FOR_ALL_STATIONS_FROM(st, 0)


/* Stuff for ROADSTOPS */

DECLARE_OLD_POOL(RoadStop, RoadStop, 5, 2000)

#define FOR_ALL_ROADSTOPS_FROM(rs, start) for (rs = GetRoadStop(start); rs != NULL; rs = (rs->index + 1U < GetRoadStopPoolSize()) ? GetRoadStop(rs->index + 1U) : NULL) if (rs->IsValid())
#define FOR_ALL_ROADSTOPS(rs) FOR_ALL_ROADSTOPS_FROM(rs, 0)

/* End of stuff for ROADSTOPS */


void AfterLoadStations(void);
void GetProductionAroundTiles(AcceptedCargo produced, TileIndex tile, int w, int h, int rad);
void GetAcceptanceAroundTiles(AcceptedCargo accepts, TileIndex tile, int w, int h, int rad);


const DrawTileSprites *GetStationTileLayout(byte gfx);
void StationPickerDrawSprite(int x, int y, RailType railtype, int image);

RoadStop * GetRoadStopByTile(TileIndex tile, RoadStop::Type type);
uint GetNumRoadStops(const Station* st, RoadStop::Type type);
RoadStop * AllocateRoadStop( void );
void ClearSlot(Vehicle *v);

void DeleteOilRig(TileIndex t);

#endif /* STATION_H */
