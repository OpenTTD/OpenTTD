#ifndef VEHICLE_H
#define VEHICLE_H

#include "vehicle_gui.h"

typedef struct VehicleRail {
	uint16 last_speed;		// NOSAVE: only used in UI
	uint16 crash_anim_pos;
	uint16 days_since_order_progr;

	uint16 cached_weight; // cached power and weight for the vehicle.
	uint32 cached_power;  // no need to save those, they are recomputed on load.

	// NOSAVE: for wagon override - id of the first engine in train
	// 0xffff == not in train
	uint16 first_engine;

	byte track;
	byte force_proceed;
	byte railtype;

	byte flags;
} VehicleRail;

enum {
	VRF_REVERSING = 1,

	// used to calculate if train is going up or down
	VRF_GOINGUP = 2,
	VRF_GOINGDOWN = 4,
};

typedef struct VehicleAir {
	uint16 crashed_counter;
	byte pos;
  byte previous_pos;
	byte targetairport;
	byte state;
} VehicleAir;

typedef struct VehicleRoad {
	byte state;
	byte frame;
	uint16 unk2;
	byte overtaking;
	byte overtaking_ctr;
	uint16 crashed_ctr;
	byte reverse_ctr;
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

// not used ATM
struct WorldSprite {
	struct WorldSprite *next;			// next sprite in hash chain
	uint16 image;			// sprite number for this vehicle

	// screen coordinates
	int16 left, top, right, bottom;

	// world coordinates
	int16 x;
	int16 y;
	byte z;

	int8 x_offs;			// x offset for vehicle sprite
	int8 y_offs;			// y offset for vehicle sprite

	byte width;				// width of vehicle sprite
	byte height;			// height of vehicle sprite
	byte depth;				// depth of vehicle sprite

	byte flags;				// draw flags
};

struct Vehicle {
	byte type;				// type, ie roadven,train,ship,aircraft,special
	byte subtype;			// subtype (for trains, 0 == loco, 4 wagon ??)

	uint16 index;			// NOSAVE: Index in vehicle array
	uint16 next_in_chain_old; // Next vehicle index for chained vehicles

	Vehicle *next;		// next

	StringID string_id; // Displayed string

	byte unitnumber;	// unit number, for display purposes only
	byte owner;				// which player owns the vehicle?

	TileIndex tile;		// Current tile index
	TileIndex dest_tile; // Heading for this tile

	int16 x_pos;			// coordinates
	int16 y_pos;
	byte z_pos;
	byte direction;		// facing

	byte spritenum; // currently displayed sprite index
	                // 0xfd == custom sprite, 0xfe == custom second head sprite
	                // 0xff == reserved for another custom sprite
	uint16 cur_image; // sprite number for this vehicle
	byte sprite_width;// width of vehicle sprite
	byte sprite_height;// height of vehicle sprite
	byte z_height;		// z-height of vehicle sprite
	int8 x_offs;			// x offset for vehicle sprite
	int8 y_offs;			// y offset for vehicle sprite
	uint16 engine_type;

	// for randomized variational spritegroups
	// bitmask used to resolve them; parts of it get reseeded when triggers
	// of corresponding spritegroups get matched
	byte random_bits;
	byte waiting_triggers; // triggers to be yet matched

	uint16 max_speed;	// maximum speed
	uint16 cur_speed;	// current speed
	byte subspeed;		// fractional speed
	byte acceleration; // used by train & aircraft
	byte progress;

	byte vehstatus;		// Status
	byte last_station_visited;

	byte cargo_type;	// type of cargo this vehicle is carrying
	byte cargo_days; // how many days have the pieces been in transit
	byte cargo_source;// source of cargo
	uint16 cargo_cap;	// total capacity
	uint16 cargo_count;// how many pieces are used

	byte day_counter; // increased by one for each day
	byte tick_counter;// increased by one for each tick

	// related to the current order
	byte cur_order_index;
	byte num_orders;
	byte next_order;
	byte next_order_param;
	uint16 *schedule_ptr;

	// Boundaries for the current position in the world and a next hash link.
	// NOSAVE: All of those can be updated with VehiclePositionChanged()
	int16 left_coord, top_coord, right_coord, bottom_coord;
	uint16 next_hash;

	// Related to age and service time
	uint16 age;				// Age in days
	uint16 max_age;		// Maximum age
	uint16 date_of_last_service;
	uint16 service_interval;
	uint16 reliability;
	uint16 reliability_spd_dec;
	byte breakdown_ctr;
	byte breakdown_delay;
	byte breakdowns_since_last_service;
	byte breakdown_chance;
	byte build_year;

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

#define is_custom_sprite(x) (x >= 0xfd)
#define is_custom_firsthead_sprite(x) (x == 0xfd)
#define is_custom_secondhead_sprite(x) (x == 0xfe)

struct Depot {
	TileIndex xy;
	uint16 town_index;
};

// train waypoint
struct Waypoint {
	TileIndex xy;
	uint16 town_or_string; // if this is 0xC000, it's a string id, otherwise a town.
	ViewportSign sign;
	uint16 build_date;
	byte stat_id;
	byte deleted;					 // this is a delete counter. when it reaches 0, the waypoint struct is deleted.
};

enum {
	VEH_Train = 0x10,
	VEH_Road = 0x11,
	VEH_Ship = 0x12,
	VEH_Aircraft = 0x13,
	VEH_Special = 0x14,
	VEH_Disaster = 0x15,
};

/* Order types */
enum {
	OT_NOTHING = 0,
	OT_GOTO_STATION = 1,
	OT_GOTO_DEPOT = 2,
	OT_LOADING = 3,
	OT_LEAVESTATION = 4,
	OT_DUMMY = 5,
	OT_GOTO_WAYPOINT = 6,

	OT_MASK = 0x1F,
};

/* Order flags */
enum {
	OF_UNLOAD = 0x20,
	OF_FULL_LOAD = 0x40, // Also used when to force an aircraft into a depot.
	OF_NON_STOP = 0x80,
	OF_MASK = 0xE0,
};


enum VehStatus {
	VS_HIDDEN = 1,
	VS_STOPPED = 2,
	VS_UNCLICKABLE = 4,
	VS_DEFPAL = 0x8,
	VS_TRAIN_SLOWING = 0x10,
	VS_DISASTER = 0x20,
	VS_AIRCRAFT_BROKEN = 0x40,
	VS_CRASHED = 0x80,
};



/* Effect vehicle types */
enum {
	EV_INDUSTRYSMOKE = 0,
	EV_STEAM_SMOKE = 1,

	EV_SMOKE_1 = 2,
	EV_SMOKE_2 = 3,
	EV_SMOKE_3 = 4,

	EV_CRASHED_SMOKE = 5,
	EV_BREAKDOWN_SMOKE = 6,

	EV_DEMOLISH = 7,
	EV_ROADWORK = 8,

	EV_INDUSTRY_SMOKE = 9,

};

typedef void VehicleTickProc(Vehicle *v);
typedef void *VehicleFromPosProc(Vehicle *v, void *data);

typedef struct {
	byte orderindex;
	uint16 order[41];
	uint16 service_interval;
	char name[32];
} BackuppedOrders;

void BackupVehicleOrders(Vehicle *v, BackuppedOrders *order);
void RestoreVehicleOrders(Vehicle *v, BackuppedOrders *order);
Vehicle *AllocateVehicle();
Vehicle *ForceAllocateVehicle();
Vehicle *ForceAllocateSpecialVehicle();
void UpdateVehiclePosHash(Vehicle *v, int x, int y);
void InitializeVehicles();
void VehiclePositionChanged(Vehicle *v);
void AfterLoadVehicles();
Vehicle *GetLastVehicleInChain(Vehicle *v);
Vehicle *GetPrevVehicleInChain(Vehicle *v);
Vehicle *GetFirstVehicleInChain(Vehicle *v);
int CountVehiclesInChain(Vehicle *v);
void DeleteVehicle(Vehicle *v);
void DeleteVehicleChain(Vehicle *v);
void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc);
void CallVehicleTicks();
void DeleteVehicleSchedule(Vehicle *v);
Vehicle *IsScheduleShared(Vehicle *v);

Depot *AllocateDepot();
Waypoint *AllocateWaypoint();
void UpdateWaypointSign(Waypoint *cp);
void RedrawWaypointSign(Waypoint *cp);

void InitializeTrains();
bool IsTrainDepotTile(TileIndex tile);
bool IsRoadDepotTile(TileIndex tile);

bool CanFillVehicle(Vehicle *v);

void ViewportAddVehicles(DrawPixelInfo *dpi);

void TrainEnterDepot(Vehicle *v, uint tile);

/* train_cmd.h */
int GetTrainImage(Vehicle *v, byte direction);
int GetAircraftImage(Vehicle *v, byte direction);
int GetRoadVehImage(Vehicle *v, byte direction);
int GetShipImage(Vehicle *v, byte direction);

Vehicle *CreateEffectVehicle(int x, int y, int z, int type);
Vehicle *CreateEffectVehicleAbove(int x, int y, int z, int type);
Vehicle *CreateEffectVehicleRel(Vehicle *v, int x, int y, int z, int type);

uint32 VehicleEnterTile(Vehicle *v, uint tile, int x, int y);

void VehicleInTheWayErrMsg(Vehicle *v);
Vehicle *FindVehicleBetween(TileIndex from, TileIndex to, byte z);
uint GetVehicleOutOfTunnelTile(Vehicle *v);

bool UpdateSignalsOnSegment(uint tile, byte direction);
void SetSignalsOnBothDir(uint tile, byte track);

Vehicle *CheckClickOnVehicle(ViewPort *vp, int x, int y);
//uint GetVehicleWeight(Vehicle *v);

void DecreaseVehicleValue(Vehicle *v);
void CheckVehicleBreakdown(Vehicle *v);
void AgeVehicle(Vehicle *v);
void MaybeRenewVehicle(Vehicle *v, int32 build_cost);

void DeleteCommandFromVehicleSchedule(uint cmd);

void BeginVehicleMove(Vehicle *v);
void EndVehicleMove(Vehicle *v);

bool IsAircraftHangarTile(TileIndex tile);
void ShowAircraftViewWindow(Vehicle *v);

void InvalidateVehicleOrderWidget(Vehicle *v);

bool IsShipDepotTile(TileIndex tile);
uint GetFreeUnitNumber(byte type);

int LoadUnloadVehicle(Vehicle *v);
int GetDepotByTile(uint tile);
uint GetWaypointByTile(uint tile);

void DoDeleteDepot(uint tile);

void UpdateTrainAcceleration(Vehicle *v);
int32 GetTrainRunningCost(Vehicle *v);

int CheckStoppedInDepot(Vehicle *v);

int ScheduleHasDepotOrders(uint16 *schedule);
int CheckOrders(Vehicle *v);

typedef struct GetNewVehiclePosResult {
	int x,y;
	uint old_tile;
	uint new_tile;
} GetNewVehiclePosResult;

/* returns true if staying in the same tile */
bool GetNewVehiclePos(Vehicle *v, GetNewVehiclePosResult *gp);
byte GetDirectionTowards(Vehicle *v, int x, int y);

#define BEGIN_ENUM_WAGONS(v) do {
#define END_ENUM_WAGONS(v) } while ( (v=v->next) != NULL);

#define DEREF_VEHICLE(i) (&_vehicles[i])
#define FOR_ALL_VEHICLES(v) for(v=_vehicles; v != endof(_vehicles); v++)

/* vehicle.c */
enum {
	NUM_NORMAL_VEHICLES = 2048,
	NUM_SPECIAL_VEHICLES = 512,
	NUM_VEHICLES = NUM_NORMAL_VEHICLES + NUM_SPECIAL_VEHICLES
};

VARDEF Vehicle _vehicles[NUM_VEHICLES];

VARDEF uint16 _order_array[5000];
VARDEF uint16 *_ptr_to_next_order;

VARDEF Depot _depots[255];

// 128 waypoints
VARDEF Waypoint _waypoints[128];

// NOSAVE: Can be regenerated by inspecting the vehicles.
VARDEF VehicleID _vehicle_position_hash[0x1000];

// NOSAVE: Return values from various commands.
VARDEF VehicleID _new_train_id;
VARDEF VehicleID _new_wagon_id;
VARDEF VehicleID _new_aircraft_id;
VARDEF VehicleID _new_ship_id;
VARDEF VehicleID _new_roadveh_id;
VARDEF uint16 _aircraft_refit_capacity;
VARDEF byte _cmd_build_rail_veh_score;
VARDEF byte _cmd_build_rail_veh_var1;

// NOSAVE: Player specific info
VARDEF TileIndex _last_built_train_depot_tile;
VARDEF TileIndex _last_built_road_depot_tile;
VARDEF TileIndex _last_built_aircraft_depot_tile;
VARDEF TileIndex _last_built_ship_depot_tile;
VARDEF TileIndex _backup_orders_tile;
VARDEF BackuppedOrders _backup_orders_data[1];

// for each player, for each vehicle type, keep a list of the vehicles.
//VARDEF Vehicle *_vehicle_arr[8][4];

#define INVALID_VEHICLE 0xffff

#define SERVICE_INTERVAL (_patches.servint_ispercent ? (v->reliability > _engines[v->engine_type].reliability * (100 - v->service_interval) / 100) : (v->date_of_last_service + v->service_interval > _date))
#define MIN_SERVINT_PERCENT  5
#define MAX_SERVINT_PERCENT 90
#define MIN_SERVINT_DAYS    30
#define MAX_SERVINT_DAYS   800

#endif /* VEHICLE_H */
