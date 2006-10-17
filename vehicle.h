/* $Id$ */

#ifndef VEHICLE_H
#define VEHICLE_H

#include "pool.h"
#include "order.h"
#include "rail.h"

enum {
	VEH_Invalid  = 0x00,
	VEH_Train    = 0x10,
	VEH_Road     = 0x11,
	VEH_Ship     = 0x12,
	VEH_Aircraft = 0x13,
	VEH_Special  = 0x14,
	VEH_Disaster = 0x15,
} ;

enum VehStatus {
	VS_HIDDEN          = 0x01,
	VS_STOPPED         = 0x02,
	VS_UNCLICKABLE     = 0x04,
	VS_DEFPAL          = 0x08,
	VS_TRAIN_SLOWING   = 0x10,
	VS_SHADOW          = 0x20,
	VS_AIRCRAFT_BROKEN = 0x40,
	VS_CRASHED         = 0x80,
};

/* Effect vehicle types */
typedef enum EffectVehicle {
	EV_CHIMNEY_SMOKE   = 0,
	EV_STEAM_SMOKE     = 1,
	EV_DIESEL_SMOKE    = 2,
	EV_ELECTRIC_SPARK  = 3,
	EV_SMOKE           = 4,
	EV_EXPLOSION_LARGE = 5,
	EV_BREAKDOWN_SMOKE = 6,
	EV_EXPLOSION_SMALL = 7,
	EV_BULLDOZER       = 8,
	EV_BUBBLE          = 9
} EffectVehicle;

typedef struct VehicleRail {
	uint16 last_speed; // NOSAVE: only used in UI
	uint16 crash_anim_pos;
	uint16 days_since_order_progr;

	// cached values, recalculated on load and each time a vehicle is added to/removed from the consist.
	uint16 cached_max_speed;  // max speed of the consist. (minimum of the max speed of all vehicles in the consist)
	uint32 cached_power;      // total power of the consist.
	uint8 cached_veh_length;  // length of this vehicle in units of 1/8 of normal length, cached because this can be set by a callback
	uint16 cached_total_length; ///< Length of the whole train, valid only for first engine.

	// cached values, recalculated when the cargo on a train changes (in addition to the conditions above)
	uint16 cached_weight;     // total weight of the consist.
	uint16 cached_veh_weight; // weight of the vehicle.
	/**
	 * Position/type of visual effect.
	 * bit 0 - 3 = position of effect relative to vehicle. (0 = front, 8 = centre, 15 = rear)
	 * bit 4 - 5 = type of effect. (0 = default for engine class, 1 = steam, 2 = diesel, 3 = electric)
	 * bit     6 = disable visual effect.
	 * bit     7 = disable powered wagons.
	 */
	byte cached_vis_effect;

	// NOSAVE: for wagon override - id of the first engine in train
	// 0xffff == not in train
	EngineID first_engine;

	byte track;
	byte force_proceed;
	byte railtype;
	RailTypeMask compatible_railtypes;

	byte flags;

	// Link between the two ends of a multiheaded engine
	Vehicle *other_multiheaded_part;
} VehicleRail;

enum {
	VRF_REVERSING         = 0,

	// used to calculate if train is going up or down
	VRF_GOINGUP           = 1,
	VRF_GOINGDOWN         = 2,

	// used to store if a wagon is powered or not
	VRF_POWEREDWAGON      = 3,

	// used to reverse the visible direction of the vehicle
	VRF_REVERSE_DIRECTION = 4,

	// used to mark train as lost because PF can't find the route
	VRF_NO_PATH_TO_DESTINATION = 5,
};

typedef struct VehicleAir {
	uint16 crashed_counter;
	byte pos;
	byte previous_pos;
	StationID targetairport;
	byte state;
} VehicleAir;

typedef struct VehicleRoad {
	byte state;
	byte frame;
	uint16 blocked_ctr;
	byte overtaking;
	byte overtaking_ctr;
	uint16 crashed_ctr;
	byte reverse_ctr;
	struct RoadStop *slot;
	byte slot_age;
} VehicleRoad;

typedef struct VehicleSpecial {
	uint16 unk0;
	byte unk2;
} VehicleSpecial;

typedef struct VehicleDisaster {
	uint16 image_override;
	uint16 unk2;
} VehicleDisaster;

typedef struct VehicleShip {
	byte state;
} VehicleShip;


struct Vehicle {
	byte type;               // type, ie roadven,train,ship,aircraft,special
	byte subtype;            // subtype (Filled with values from EffectVehicles or TrainSubTypes)

	VehicleID index;         // NOSAVE: Index in vehicle array

	Vehicle *next;           // next
	Vehicle *first;          // NOSAVE: pointer to the first vehicle in the chain
	Vehicle *depot_list;     //NOSAVE: linked list to tell what vehicles entered a depot during the last tick. Used by autoreplace

	StringID string_id;      // Displayed string

	UnitID unitnumber;       // unit number, for display purposes only
	PlayerID owner;          // which player owns the vehicle?

	TileIndex tile;          // Current tile index
	TileIndex dest_tile;     // Heading for this tile

	int32 x_pos;             // coordinates
	int32 y_pos;
	byte z_pos;
	byte direction;          // facing

	byte spritenum;          // currently displayed sprite index
	                         // 0xfd == custom sprite, 0xfe == custom second head sprite
	                         // 0xff == reserved for another custom sprite
	uint16 cur_image;        // sprite number for this vehicle
	byte sprite_width;       // width of vehicle sprite
	byte sprite_height;      // height of vehicle sprite
	byte z_height;           // z-height of vehicle sprite
	int8 x_offs;             // x offset for vehicle sprite
	int8 y_offs;             // y offset for vehicle sprite
	EngineID engine_type;

	// for randomized variational spritegroups
	// bitmask used to resolve them; parts of it get reseeded when triggers
	// of corresponding spritegroups get matched
	byte random_bits;
	byte waiting_triggers;   // triggers to be yet matched

	uint16 max_speed;        // maximum speed
	uint16 cur_speed;        // current speed
	byte subspeed;           // fractional speed
	byte acceleration;       // used by train & aircraft
	byte progress;
	uint32 motion_counter;

	byte vehstatus;          // Status
	StationID last_station_visited;

	CargoID cargo_type;      // type of cargo this vehicle is carrying
	byte cargo_days;         // how many days have the pieces been in transit
	StationID cargo_source;  // source of cargo
	uint16 cargo_cap;        // total capacity
	uint16 cargo_count;      // how many pieces are used
	byte cargo_subtype;      ///< Used for livery refits (NewGRF variations)

	byte day_counter;        // increased by one for each day
	byte tick_counter;       // increased by one for each tick

	/* Begin Order-stuff */
	Order current_order;     ///< The current order (+ status, like: loading)
	VehicleOrderID cur_order_index; ///< The index to the current order

	Order *orders;           ///< Pointer to the first order for this vehicle
	VehicleOrderID num_orders;      ///< How many orders there are in the list

	Vehicle *next_shared;    ///< If not NULL, this points to the next vehicle that shared the order
	Vehicle *prev_shared;    ///< If not NULL, this points to the prev vehicle that shared the order
	/* End Order-stuff */

	// Boundaries for the current position in the world and a next hash link.
	// NOSAVE: All of those can be updated with VehiclePositionChanged()
	int32 left_coord;
	int32 top_coord;
	int32 right_coord;
	int32 bottom_coord;
	VehicleID next_hash;

	// Related to age and service time
	Date age;     // Age in days
	Date max_age; // Maximum age
	Date date_of_last_service;
	Date service_interval;
	uint16 reliability;
	uint16 reliability_spd_dec;
	byte breakdown_ctr;
	byte breakdown_delay;
	byte breakdowns_since_last_service;
	byte breakdown_chance;
	Year build_year;

	bool leave_depot_instantly; // NOSAVE: stores if the vehicle needs to leave the depot it just entered. Used by autoreplace

	uint16 load_unload_time_rem;

	int32 profit_this_year;
	int32 profit_last_year;
	uint32 value;

	union {
		VehicleRail rail;
		VehicleAir air;
		VehicleRoad road;
		VehicleSpecial special;
		VehicleDisaster disaster;
		VehicleShip ship;
	} u;
};

#define is_custom_sprite(x) (x >= 0xFD)
#define IS_CUSTOM_FIRSTHEAD_SPRITE(x) (x == 0xFD)
#define IS_CUSTOM_SECONDHEAD_SPRITE(x) (x == 0xFE)

typedef void *VehicleFromPosProc(Vehicle *v, void *data);

void VehicleServiceInDepot(Vehicle *v);
Vehicle *AllocateVehicle(void);
bool AllocateVehicles(Vehicle **vl, int num);
Vehicle *ForceAllocateVehicle(void);
Vehicle *ForceAllocateSpecialVehicle(void);
void VehiclePositionChanged(Vehicle *v);
void AfterLoadVehicles(void);
Vehicle *GetLastVehicleInChain(Vehicle *v);
Vehicle *GetPrevVehicleInChain(const Vehicle *v);
Vehicle *GetFirstVehicleInChain(const Vehicle *v);
uint CountVehiclesInChain(const Vehicle* v);
bool IsEngineCountable(const Vehicle *v);
void DeleteVehicleChain(Vehicle *v);
void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc);
void CallVehicleTicks(void);
Vehicle *FindVehicleOnTileZ(TileIndex tile, byte z);

void InitializeTrains(void);
byte VehicleRandomBits(void);

bool CanFillVehicle(Vehicle *v);
bool CanRefitTo(EngineID engine_type, CargoID cid_to);
CargoID FindFirstRefittableCargo(EngineID engine_type);
int32 GetRefitCost(EngineID engine_type);

void ViewportAddVehicles(DrawPixelInfo *dpi);

/* train_cmd.h */
int GetTrainImage(const Vehicle* v, Direction direction);
int GetAircraftImage(const Vehicle* v, Direction direction);
int GetRoadVehImage(const Vehicle* v, Direction direction);
int GetShipImage(const Vehicle* v, Direction direction);

Vehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicle type);
Vehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicle type);
Vehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicle type);

uint32 VehicleEnterTile(Vehicle *v, TileIndex tile, int x, int y);

StringID VehicleInTheWayErrMsg(const Vehicle* v);
Vehicle *FindVehicleBetween(TileIndex from, TileIndex to, byte z);
TileIndex GetVehicleOutOfTunnelTile(const Vehicle *v);

bool UpdateSignalsOnSegment(TileIndex tile, DiagDirection direction);
void SetSignalsOnBothDir(TileIndex tile, byte track);

Vehicle *CheckClickOnVehicle(const ViewPort *vp, int x, int y);

void DecreaseVehicleValue(Vehicle *v);
void CheckVehicleBreakdown(Vehicle *v);
void AgeVehicle(Vehicle *v);
void VehicleEnteredDepotThisTick(Vehicle *v);

void BeginVehicleMove(Vehicle *v);
void EndVehicleMove(Vehicle *v);

void ShowAircraftViewWindow(const Vehicle* v);

UnitID GetFreeUnitNumber(byte type);

int LoadUnloadVehicle(Vehicle *v);

void TrainConsistChanged(Vehicle *v);
void TrainPowerChanged(Vehicle *v);
int32 GetTrainRunningCost(const Vehicle *v);

int CheckTrainStoppedInDepot(const Vehicle *v);

bool VehicleNeedsService(const Vehicle *v);

uint GenerateVehicleSortList(const Vehicle*** sort_list, uint16 *length_of_array, byte type, PlayerID owner, StationID station, OrderID order, uint16 depot_airport_index, uint16 window_type);
void BuildDepotVehicleList(byte type, TileIndex tile, Vehicle ***engine_list, uint16 *engine_list_length, uint16 *engine_count, Vehicle ***wagon_list, uint16 *wagon_list_length, uint16 *wagon_count);
int32 SendAllVehiclesToDepot(byte type, uint32 flags, bool service, PlayerID owner, uint16 vlw_flag, uint32 id);
bool IsVehicleInDepot(const Vehicle *v);
void VehicleEnterDepot(Vehicle *v);

/* Flags to add to p2 for goto depot commands */
/* Note: bits 8-10 are used for VLW flags */
enum {
	DEPOT_SERVICE       = (1 << 0),	// The vehicle will leave the depot right after arrival (serivce only)
	DEPOT_MASS_SEND     = (1 << 1), // Tells that it's a mass send to depot command (type in VLW flag)
	DEPOT_DONT_CANCEL   = (1 << 2), // Don't cancel current goto depot command if any
	DEPOT_LOCATE_HANGAR = (1 << 3), // Find another airport if the target one lacks a hangar
};

typedef struct GetNewVehiclePosResult {
	int x,y;
	TileIndex old_tile;
	TileIndex new_tile;
} GetNewVehiclePosResult;

/**
 * Returns the Trackdir on which the vehicle is currently located.
 * Works for trains and ships.
 * Currently works only sortof for road vehicles, since they have a fuzzy
 * concept of being "on" a trackdir. Dunno really what it returns for a road
 * vehicle that is halfway a tile, never really understood that part. For road
 * vehicles that are at the beginning or end of the tile, should just return
 * the diagonal trackdir on which they are driving. I _think_.
 * For other vehicles types, or vehicles with no clear trackdir (such as those
 * in depots), returns 0xFF.
 */
Trackdir GetVehicleTrackdir(const Vehicle* v);

/* returns true if staying in the same tile */
bool GetNewVehiclePos(const Vehicle *v, GetNewVehiclePosResult *gp);
Direction GetDirectionTowards(const Vehicle* v, int x, int y);

#define BEGIN_ENUM_WAGONS(v) do {
#define END_ENUM_WAGONS(v) } while ( (v=v->next) != NULL);

extern MemoryPool _vehicle_pool;

/**
 * Get the pointer to the vehicle with index 'index'
 */
static inline Vehicle *GetVehicle(VehicleID index)
{
	return (Vehicle*)GetItemFromPool(&_vehicle_pool, index);
}

/**
 * Get the current size of the VehiclePool
 */
static inline uint16 GetVehiclePoolSize(void)
{
	return _vehicle_pool.total_items;
}

static inline VehicleID GetVehicleArraySize(void)
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index + 1. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetVehiclePoolSize();
}

/**
 * Check if a Vehicle really exists.
 */
static inline bool IsValidVehicle(const Vehicle *v)
{
	return v->type != 0;
}

void DestroyVehicle(Vehicle *v);

static inline void DeleteVehicle(Vehicle *v)
{
	DestroyVehicle(v);
	v->type = 0;
}

#define FOR_ALL_VEHICLES_FROM(v, start) for (v = GetVehicle(start); v != NULL; v = (v->index + 1 < GetVehiclePoolSize()) ? GetVehicle(v->index + 1) : NULL) if (IsValidVehicle(v))
#define FOR_ALL_VEHICLES(v) FOR_ALL_VEHICLES_FROM(v, 0)

/**
 * Check if an index is a vehicle-index (so between 0 and max-vehicles)
 *
 * @return Returns true if the vehicle-id is in range
 */
static inline bool IsValidVehicleID(uint index)
{
	return index < GetVehiclePoolSize() && IsValidVehicle(GetVehicle(index));
}

/* Returns order 'index' of a vehicle or NULL when it doesn't exists */
static inline Order *GetVehicleOrder(const Vehicle *v, int index)
{
	Order *order = v->orders;

	if (index < 0) return NULL;

	while (order != NULL && index-- > 0)
		order = order->next;

	return order;
}

/* Returns the last order of a vehicle, or NULL if it doesn't exists */
static inline Order *GetLastVehicleOrder(const Vehicle *v)
{
	Order *order = v->orders;

	if (order == NULL) return NULL;

	while (order->next != NULL)
		order = order->next;

	return order;
}

/* Get the first vehicle of a shared-list, so we only have to walk forwards */
static inline Vehicle *GetFirstVehicleFromSharedList(const Vehicle *v)
{
	Vehicle *u = (Vehicle *)v;
	while (u->prev_shared != NULL) u = u->prev_shared;

	return u;
}

// NOSAVE: Return values from various commands.
VARDEF VehicleID _new_vehicle_id;
VARDEF uint16 _returned_refit_capacity;

enum {
	INVALID_VEHICLE = 0xFFFF,
};

/**
 * Get the colour map for an engine. This used for unbuilt engines in the user interface.
 * @param engine_type ID of engine
 * @param player ID of player
 * @return A ready-to-use palette modifier
 */
PalSpriteID GetEnginePalette(EngineID engine_type, PlayerID player);

/**
 * Get the colour map for a vehicle.
 * @param v Vehicle to get colour map for
 * @return A ready-to-use palette modifier
 */
PalSpriteID GetVehiclePalette(const Vehicle *v);

/* A lot of code calls for the invalidation of the status bar, which is widget 5.
 * Best is to have a virtual value for it when it needs to change again */
#define STATUS_BAR 5

extern const uint32 _send_to_depot_proc_table[];
#define CMD_SEND_TO_DEPOT(x) _send_to_depot_proc_table[ x - VEH_Train]

#endif /* VEHICLE_H */
