/* $Id$ */

#ifndef AI_TROLLY_H
#define AI_TROLLY_H

#include "../../aystar.h"
#include "../../player.h"

/*
 * These defines can be altered to change the behavoir of the AI
 *
 * WARNING:
 *   This can also alter the AI in a negative way. I will never claim these settings
 *   are perfect, but don't change them if you don't know what the effect is.
 */

// How many times it the H multiplied. The higher, the more it will go straight to the
//   end point. The lower, how more it will find the route with the lowest cost.
//   also: the lower, the longer it takes before route is calculated..
#define AI_PATHFINDER_H_MULTIPLER 100

// How many loops may AyStar do before it stops
//   0 = infinite
#define AI_PATHFINDER_LOOPS_PER_TICK 5

// How long may the AI search for one route?
//   0 = infinite
// This number is the number of tiles tested.
//  It takes (AI_PATHFINDER_MAX_SEARCH_NODES / AI_PATHFINDER_LOOPS_PER_TICK) ticks
//  to get here.. with 5000 / 10 = 500. 500 / 74 (one day) = 8 days till it aborts
//   (that is: if the AI is on VERY FAST! :p
#define AI_PATHFINDER_MAX_SEARCH_NODES 5000

// If you enable this, the AI is not allowed to make 90degree turns
#define AI_PATHFINDER_NO_90DEGREES_TURN

// Below are defines for the g-calculation

// Standard penalty given to a tile
#define AI_PATHFINDER_PENALTY 150
// The penalty given to a tile that is going up
#define AI_PATHFINDER_TILE_GOES_UP_PENALTY 450
// The penalty given to a tile which would have to use fundation
#define AI_PATHFINDER_FOUNDATION_PENALTY 100
// Changing direction is a penalty, to prevent curved ways (with that: slow ways)
#define AI_PATHFINDER_DIRECTION_CHANGE_PENALTY 200
// Same penalty, only for when road already exists
#define AI_PATHFINDER_DIRECTION_CHANGE_ON_EXISTING_ROAD_PENALTY 50
// A diagonal track cost the same as a straigh, but a diagonal is faster... so give
//  a bonus for using diagonal track
#ifdef AI_PATHFINDER_NO_90DEGREES_TURN
#define AI_PATHFINDER_DIAGONAL_BONUS 95
#else
#define AI_PATHFINDER_DIAGONAL_BONUS 75
#endif
// If a roadblock already exists, it gets a bonus
#define AI_PATHFINDER_ROAD_ALREADY_EXISTS_BONUS 140
// To prevent 3 direction changes in 3 tiles, this penalty is given in such situation
#define AI_PATHFINDER_CURVE_PENALTY 200

// Penalty a bridge gets per length
#define AI_PATHFINDER_BRIDGE_PENALTY 180
// The penalty for a bridge going up
#define AI_PATHFINDER_BRIDGE_GOES_UP_PENALTY 1000

// Tunnels are expensive...
//  Because of that, every tile the cost is increased with 1/8th of his value
//  This is also true if you are building a tunnel yourself
#define AI_PATHFINDER_TUNNEL_PENALTY 350

/*
 * Ai_New defines
 */

// How long may we search cities and industry for a new route?
#define AI_LOCATE_ROUTE_MAX_COUNTER 200

// How many days must there be between building the first station and the second station
//  within one city. This number is in days and should be more than 4 months.
#define AI_CHECKCITY_DATE_BETWEEN 180

// How many cargo is needed for one station in a city?
#define AI_CHECKCITY_CARGO_PER_STATION 60
// How much cargo must there not be used in a city before we can build a new station?
#define AI_CHECKCITY_NEEDED_CARGO 50
// When there is already a station which takes the same good and the rating of that
//  city is higher then this numer, we are not going to attempt to build anything
//  there
#define AI_CHECKCITY_CARGO_RATING 50
// But, there is a chance of 1 out of this number, that we do ;)
#define AI_CHECKCITY_CARGO_RATING_CHANCE 5
// If a city is too small to contain a station, there is a small chance
//  that we still do so.. just to make the city bigger!
#define AI_CHECKCITY_CITY_CHANCE 5

// This number indicates for every unit of cargo, how many tiles two stations maybe be away
//  from eachother. In other words: if we have 120 units of cargo in one station, and 120 units
//  of the cargo in the other station, both stations can be 96 units away from eachother, if the
//  next number is 0.4.
#define AI_LOCATEROUTE_BUS_CARGO_DISTANCE 0.4
#define AI_LOCATEROUTE_TRUCK_CARGO_DISTANCE 0.7
// In whole tiles, the minimum distance for a truck route
#define AI_LOCATEROUTE_TRUCK_MIN_DISTANCE 30

// The amount of tiles in a square from -X to +X that is scanned for a station spot
//  (so if this number is 10, 20x20 = 400 tiles are scanned for _the_ perfect spot
// Safe values are between 15 and 5
#define AI_FINDSTATION_TILE_RANGE 10

// Building on normal speed goes very fast. Idle this amount of ticks between every
//  building part. It is calculated like this: (4 - competitor_speed) * num + 1
//  where competitor_speed is between 0 (very slow) to 4 (very fast)
#define AI_BUILDPATH_PAUSE 10

// Minimum % of reliabilty a vehicle has to have before the AI buys it
#define AI_VEHICLE_MIN_RELIABILTY 60

// The minimum amount of money a player should always have
#define AI_MINIMUM_MONEY 15000

// If the most cheap route is build, how much is it going to cost..
// This is to prevent the AI from trying to build a route which can not be paid for
#define AI_MINIMUM_BUS_ROUTE_MONEY 25000
#define AI_MINIMUM_TRUCK_ROUTE_MONEY 35000

// The minimum amount of money before we are going to repay any money
#define AI_MINIMUM_LOAN_REPAY_MONEY 40000
// How many repays do we do if we have enough money to do so?
//  Every repay is 10000
#define AI_LOAN_REPAY 2
// How much income must we have before paying back a loan? Month-based (and looked at the last month)
#define AI_MINIMUM_INCOME_FOR_LOAN 7000

// If there is <num> time as much cargo in the station then the vehicle can handle
//  reuse the station instead of building a new one!
#define AI_STATION_REUSE_MULTIPLER 2

// No more than this amount of vehicles per station..
#define AI_CHECK_MAX_VEHICLE_PER_STATION 10

// How many thick between building 2 vehicles
#define AI_BUILD_VEHICLE_TIME_BETWEEN DAY_TICKS

// How many days must there between vehicle checks
//  The more often, the less non-money-making lines there will be
//   but the unfair it may seem to a human player
#define AI_DAYS_BETWEEN_VEHICLE_CHECKS 30

// How money profit does a vehicle needs to make to stay in order
//  This is the profit of this year + profit of last year
//  But also for vehicles that are just one year old. In other words:
//   Vehicles of 2 years do easier meet this setting then vehicles
//   of one year. This is a very good thing. New vehicles are filtered,
//   while old vehicles stay longer, because we do get less in return.
#define AI_MINIMUM_ROUTE_PROFIT 1000

// A vehicle is considered lost when he his cargo is more than 180 days old
#define AI_VEHICLE_LOST_DAYS 180

// How many times may the AI try to find a route before it gives up
#define AI_MAX_TRIES_FOR_SAME_ROUTE 8

/*
 * End of defines
 */

// This stops 90degrees curves
static const byte _illegal_curves[6] = {
	255, 255, // Horz and vert, don't have the effect
	5, // upleft and upright are not valid
	4, // downright and downleft are not valid
	2, // downleft and upleft are not valid
	3, // upright and downright are not valid
};

enum {
	AI_STATE_STARTUP = 0,
	AI_STATE_FIRST_TIME,
	AI_STATE_NOTHING,
	AI_STATE_WAKE_UP,
	AI_STATE_LOCATE_ROUTE,
	AI_STATE_FIND_STATION,
	AI_STATE_FIND_PATH,
	AI_STATE_FIND_DEPOT,
	AI_STATE_VERIFY_ROUTE,
	AI_STATE_BUILD_STATION,
	AI_STATE_BUILD_PATH,
	AI_STATE_BUILD_DEPOT,
	AI_STATE_BUILD_VEHICLE,
	AI_STATE_WAIT_FOR_BUILD,
	AI_STATE_GIVE_ORDERS,
	AI_STATE_START_VEHICLE,
	AI_STATE_REPAY_MONEY,
	AI_STATE_CHECK_ALL_VEHICLES,
	AI_STATE_ACTION_DONE,
	AI_STATE_STOP, // Temporary function to stop the AI
};

// Used for tbt (train/bus/truck)
enum {
	AI_TRAIN = 0,
	AI_BUS,
	AI_TRUCK,
};

enum {
	AI_ACTION_NONE = 0,
	AI_ACTION_BUS_ROUTE,
	AI_ACTION_TRUCK_ROUTE,
	AI_ACTION_REPAY_LOAN,
	AI_ACTION_CHECK_ALL_VEHICLES,
};

// Used for from_type/to_type
enum {
	AI_NO_TYPE = 0,
	AI_CITY,
	AI_INDUSTRY,
};

// Flags for in the vehicle
enum {
	AI_VEHICLEFLAG_SELL = 1,
	// Remember, flags must be in power of 2
};

#define AI_NO_CARGO 0xFF // Means that there is no cargo defined yet (used for industry)
#define AI_NEED_CARGO 0xFE // Used when the AI needs to find out a cargo for the route
#define AI_STATION_RANGE TileXY(MapMaxX(), MapMaxY())

#define AI_PATHFINDER_NO_DIRECTION (byte)-1

// Flags used in user_data
#define AI_PATHFINDER_FLAG_BRIDGE 1
#define AI_PATHFINDER_FLAG_TUNNEL 2

typedef void AiNew_StateFunction(Player *p);

// ai_new.c
void AiNewDoGameLoop(Player *p);

struct Ai_PathFinderInfo {
	TileIndex start_tile_tl; ///< tl = top-left
	TileIndex start_tile_br; ///< br = bottom-right
	TileIndex end_tile_tl;   ///< tl = top-left
	TileIndex end_tile_br;   ///< br = bottom-right
	DiagDirection start_direction; ///< 0 to 3 or AI_PATHFINDER_NO_DIRECTION
	DiagDirection end_direction;   ///< 0 to 3 or AI_PATHFINDER_NO_DIRECTION

	TileIndex route[500];
	byte route_extra[500];   ///< Some extra information about the route like bridge/tunnel
	int route_length;
	int position;            ///< Current position in the build-path, needed to build the path

	bool rail_or_road;       ///< true = rail, false = road
};

// ai_pathfinder.c
AyStar *new_AyStar_AiPathFinder(int max_tiles_around, Ai_PathFinderInfo *PathFinderInfo);
void clean_AyStar_AiPathFinder(AyStar *aystar, Ai_PathFinderInfo *PathFinderInfo);

// ai_shared.c
int AiNew_GetRailDirection(TileIndex tile_a, TileIndex tile_b, TileIndex tile_c);
int AiNew_GetRoadDirection(TileIndex tile_a, TileIndex tile_b, TileIndex tile_c);
DiagDirection AiNew_GetDirection(TileIndex tile_a, TileIndex tile_b);
bool AiNew_SetSpecialVehicleFlag(Player *p, Vehicle *v, uint flag);
uint AiNew_GetSpecialVehicleFlag(Player *p, Vehicle *v);

// ai_build.c
bool AiNew_Build_CompanyHQ(Player *p, TileIndex tile);
CommandCost AiNew_Build_Station(Player *p, byte type, TileIndex tile, byte length, byte numtracks, byte direction, byte flag);
CommandCost AiNew_Build_Bridge(Player *p, TileIndex tile_a, TileIndex tile_b, byte flag);
CommandCost AiNew_Build_RoutePart(Player *p, Ai_PathFinderInfo *PathFinderInfo, byte flag);
EngineID AiNew_PickVehicle(Player *p);
CommandCost AiNew_Build_Vehicle(Player *p, TileIndex tile, byte flag);
CommandCost AiNew_Build_Depot(Player* p, TileIndex tile, DiagDirection direction, byte flag);

/* The amount of memory reserved for the AI-special-vehicles */
#define AI_MAX_SPECIAL_VEHICLES 100

struct Ai_SpecialVehicle {
	VehicleID veh_id;
	uint32 flag;
};

struct PlayerAiNew {
	uint8 state;
	uint tick;
	uint idle;

	int temp;    ///< A value used in more than one function, but it just temporary
	             ///< The use is pretty simple: with this we can 'think' about stuff
	             ///<   in more than one tick, and more than one AI. A static will not
	             ///<   do, because they are not saved. This way, the AI is almost human ;)
	int counter; ///< For the same reason as temp, we have counter. It can count how
	             ///<  long we are trying something, and just abort if it takes too long

	/* Pathfinder stuff */
	Ai_PathFinderInfo path_info;
	AyStar *pathfinder;

	/* Route stuff */

	CargoID cargo;
	byte tbt;    ///< train/bus/truck 0/1/2 AI_TRAIN/AI_BUS/AI_TRUCK
	Money new_cost;

	byte action;

	int last_id; ///< here is stored the last id of the searched city/industry
	Date last_vehiclecheck_date; // Used in CheckVehicle
	Ai_SpecialVehicle special_vehicles[AI_MAX_SPECIAL_VEHICLES]; ///< Some vehicles have some special flags

	TileIndex from_tile;
	TileIndex to_tile;

	DiagDirectionByte from_direction;
	DiagDirectionByte to_direction;

	bool from_deliver; ///< True if this is the station that GIVES cargo
	bool to_deliver;

	TileIndex depot_tile;
	DiagDirectionByte depot_direction;

	byte amount_veh;       ///< How many vehicles we are going to build in this route
	byte cur_veh;          ///< How many vehicles did we bought?
	VehicleID veh_id;      ///< Used when bought a vehicle
	VehicleID veh_main_id; ///< The ID of the first vehicle, for shared copy

	int from_ic;           ///< ic = industry/city. This is the ID of them
	byte from_type;        ///< AI_NO_TYPE/AI_CITY/AI_INDUSTRY
	int to_ic;
	byte to_type;
};
extern PlayerAiNew _players_ainew[MAX_PLAYERS];

#endif /* AI_TROLLY_H */
