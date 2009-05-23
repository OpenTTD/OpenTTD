/* $Id$ */

/** @file src/roadveh.h Road vehicle states */

#ifndef ROADVEH_H
#define ROADVEH_H

#include "vehicle_base.h"
#include "engine_func.h"
#include "engine_base.h"
#include "economy_func.h"

struct RoadVehicle;

/** Road vehicle states */
enum RoadVehicleStates {
	/*
	 * Lower 4 bits are used for vehicle track direction. (Trackdirs)
	 * When in a road stop (bit 5 or bit 6 set) these bits give the
	 * track direction of the entry to the road stop.
	 * As the entry direction will always be a diagonal
	 * direction (X_NE, Y_SE, X_SW or Y_NW) only bits 0 and 3
	 * are needed to hold this direction. Bit 1 is then used to show
	 * that the vehicle is using the second road stop bay.
	 * Bit 2 is then used for drive-through stops to show the vehicle
	 * is stopping at this road stop.
	 */

	/* Numeric values */
	RVSB_IN_DEPOT                = 0xFE,                      ///< The vehicle is in a depot
	RVSB_WORMHOLE                = 0xFF,                      ///< The vehicle is in a tunnel and/or bridge

	/* Bit numbers */
	RVS_USING_SECOND_BAY         =    1,                      ///< Only used while in a road stop
	RVS_IS_STOPPING              =    2,                      ///< Only used for drive-through stops. Vehicle will stop here
	RVS_DRIVE_SIDE               =    4,                      ///< Only used when retrieving move data
	RVS_IN_ROAD_STOP             =    5,                      ///< The vehicle is in a road stop
	RVS_IN_DT_ROAD_STOP          =    6,                      ///< The vehicle is in a drive-through road stop

	/* Bit sets of the above specified bits */
	RVSB_IN_ROAD_STOP            = 1 << RVS_IN_ROAD_STOP,     ///< The vehicle is in a road stop
	RVSB_IN_ROAD_STOP_END        = RVSB_IN_ROAD_STOP + TRACKDIR_END,
	RVSB_IN_DT_ROAD_STOP         = 1 << RVS_IN_DT_ROAD_STOP,  ///< The vehicle is in a drive-through road stop
	RVSB_IN_DT_ROAD_STOP_END     = RVSB_IN_DT_ROAD_STOP + TRACKDIR_END,

	RVSB_TRACKDIR_MASK           = 0x0F,                      ///< The mask used to extract track dirs
	RVSB_ROAD_STOP_TRACKDIR_MASK = 0x09                       ///< Only bits 0 and 3 are used to encode the trackdir for road stops
};

/** State information about the Road Vehicle controller */
enum {
	RDE_NEXT_TILE = 0x80, ///< We should enter the next tile
	RDE_TURNED    = 0x40, ///< We just finished turning

	/* Start frames for when a vehicle enters a tile/changes its state.
	 * The start frame is different for vehicles that turned around or
	 * are leaving the depot as the do not start at the edge of the tile.
	 * For trams there are a few different start frames as there are two
	 * places where trams can turn. */
	RVC_DEFAULT_START_FRAME                =  0,
	RVC_TURN_AROUND_START_FRAME            =  1,
	RVC_DEPOT_START_FRAME                  =  6,
	RVC_START_FRAME_AFTER_LONG_TRAM        = 21,
	RVC_TURN_AROUND_START_FRAME_SHORT_TRAM = 16,
	/* Stop frame for a vehicle in a drive-through stop */
	RVC_DRIVE_THROUGH_STOP_FRAME           =  7,
	RVC_DEPOT_STOP_FRAME                   = 11,
};

enum RoadVehicleSubType {
	RVST_FRONT,
	RVST_ARTIC_PART,
};

static inline bool IsRoadVehFront(const Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	return v->subtype == RVST_FRONT;
}

static inline void SetRoadVehFront(Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	v->subtype = RVST_FRONT;
}

static inline bool IsRoadVehArticPart(const Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	return v->subtype == RVST_ARTIC_PART;
}

static inline void SetRoadVehArticPart(Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	v->subtype = RVST_ARTIC_PART;
}

static inline bool RoadVehHasArticPart(const Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	return v->Next() != NULL && IsRoadVehArticPart(v->Next());
}


void CcBuildRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2);

byte GetRoadVehLength(const RoadVehicle *v);

void RoadVehUpdateCache(RoadVehicle *v);

/** Cached oftenly queried (NewGRF) values */
struct RoadVehicleCache {
	byte cached_veh_length;
	EngineID first_engine;
};

/**
 * This class 'wraps' Vehicle; you do not actually instantiate this class.
 * You create a Vehicle using AllocateVehicle, so it is added to the pool
 * and you reinitialize that to a Train using:
 *   v = new (v) RoadVehicle();
 *
 * As side-effect the vehicle type is set correctly.
 */
struct RoadVehicle : public Vehicle {
	RoadVehicleCache rcache; ///< Cache of often used calculated values
	byte state;             ///< @see RoadVehicleStates
	byte frame;
	uint16 blocked_ctr;
	byte overtaking;
	byte overtaking_ctr;
	uint16 crashed_ctr;
	byte reverse_ctr;
	struct RoadStop *slot;
	byte slot_age;

	RoadType roadtype;
	RoadTypes compatible_roadtypes;

	/** Initializes the Vehicle to a road vehicle */
	RoadVehicle() { this->type = VEH_ROAD; }

	/** We want to 'destruct' the right class. */
	virtual ~RoadVehicle() { this->PreDestructor(); }

	const char *GetTypeString() const { return "road vehicle"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) const { return income ? EXPENSES_ROADVEH_INC : EXPENSES_ROADVEH_RUN; }
	bool IsPrimaryVehicle() const { return IsRoadVehFront(this); }
	SpriteID GetImage(Direction direction) const;
	int GetDisplaySpeed() const { return this->cur_speed / 2; }
	int GetDisplayMaxSpeed() const { return this->max_speed / 2; }
	Money GetRunningCost() const { return RoadVehInfo(this->engine_type)->running_cost * GetPriceByIndex(RoadVehInfo(this->engine_type)->running_cost_class); }
	bool IsInDepot() const { return this->state == RVSB_IN_DEPOT; }
	bool IsStoppedInDepot() const;
	bool Tick();
	void OnNewDay();
	Trackdir GetVehicleTrackdir() const;
	TileIndex GetOrderStationLocation(StationID station);
	bool FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse);
	RoadVehicle *First() { return (RoadVehicle *)this->Vehicle::First(); }
	RoadVehicle *Next() { return (RoadVehicle *)this->Vehicle::Next(); }
	const RoadVehicle *Next() const { return (const RoadVehicle *)this->Vehicle::Next(); }
};

#endif /* ROADVEH_H */
