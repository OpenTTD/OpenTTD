/* $Id$ */

/** @vehicle.h Functions related to vehicles. */

#ifndef VEHICLE_FUNC_H
#define VEHICLE_FUNC_H

#include "tile_type.h"
#include "strings_type.h"
#include "gfx_type.h"
#include "direction_type.h"
#include "cargo_type.h"
#include "command_type.h"
#include "vehicle_type.h"

#define is_custom_sprite(x) (x >= 0xFD)
#define IS_CUSTOM_FIRSTHEAD_SPRITE(x) (x == 0xFD)
#define IS_CUSTOM_SECONDHEAD_SPRITE(x) (x == 0xFE)

typedef void *VehicleFromPosProc(Vehicle *v, void *data);

void VehicleServiceInDepot(Vehicle *v);
void VehiclePositionChanged(Vehicle *v);
Vehicle *GetLastVehicleInChain(Vehicle *v);
uint CountVehiclesInChain(const Vehicle *v);
bool IsEngineCountable(const Vehicle *v);
void DeleteVehicleChain(Vehicle *v);
void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc);
void *VehicleFromPosXY(int x, int y, void *data, VehicleFromPosProc *proc);
void CallVehicleTicks();
Vehicle *FindVehicleOnTileZ(TileIndex tile, byte z);
uint8 CalcPercentVehicleFilled(Vehicle *v, StringID *color);

void InitializeTrains();
byte VehicleRandomBits();
void ResetVehiclePosHash();
void ResetVehicleColorMap();

bool CanRefitTo(EngineID engine_type, CargoID cid_to);
CargoID FindFirstRefittableCargo(EngineID engine_type);
CommandCost GetRefitCost(EngineID engine_type);

void ViewportAddVehicles(DrawPixelInfo *dpi);

SpriteID GetRotorImage(const Vehicle *v);

uint32 VehicleEnterTile(Vehicle *v, TileIndex tile, int x, int y);

StringID VehicleInTheWayErrMsg(const Vehicle* v);
Vehicle *FindVehicleBetween(TileIndex from, TileIndex to, byte z, bool without_crashed = false);
Vehicle *GetVehicleTunnelBridge(TileIndex tile, TileIndex endtile);

bool UpdateSignalsOnSegment(TileIndex tile, DiagDirection direction);
void SetSignalsOnBothDir(TileIndex tile, byte track);

Vehicle *CheckClickOnVehicle(const ViewPort *vp, int x, int y);

void DecreaseVehicleValue(Vehicle *v);
void CheckVehicleBreakdown(Vehicle *v);
void AgeVehicle(Vehicle *v);
void VehicleEnteredDepotThisTick(Vehicle *v);

void BeginVehicleMove(Vehicle *v);
void EndVehicleMove(Vehicle *v);

UnitID GetFreeUnitNumber(VehicleType type);

void TrainConsistChanged(Vehicle *v);
void TrainPowerChanged(Vehicle *v);
Money GetTrainRunningCost(const Vehicle *v);

bool VehicleNeedsService(const Vehicle *v);

uint GenerateVehicleSortList(const Vehicle*** sort_list, uint16 *length_of_array, VehicleType type, PlayerID owner, uint32 index, uint16 window_type);
void BuildDepotVehicleList(VehicleType type, TileIndex tile, Vehicle ***engine_list, uint16 *engine_list_length, uint16 *engine_count, Vehicle ***wagon_list, uint16 *wagon_list_length, uint16 *wagon_count);
CommandCost SendAllVehiclesToDepot(VehicleType type, uint32 flags, bool service, PlayerID owner, uint16 vlw_flag, uint32 id);
void VehicleEnterDepot(Vehicle *v);

CommandCost MaybeReplaceVehicle(Vehicle *v, bool check, bool display_costs);
bool CanBuildVehicleInfrastructure(VehicleType type);

void CcCloneVehicle(bool success, TileIndex tile, uint32 p1, uint32 p2);

/* Flags to add to p2 for goto depot commands */
/* Note: bits 8-10 are used for VLW flags */
enum {
	DEPOT_SERVICE       = (1 << 0), // The vehicle will leave the depot right after arrival (serivce only)
	DEPOT_MASS_SEND     = (1 << 1), // Tells that it's a mass send to depot command (type in VLW flag)
	DEPOT_DONT_CANCEL   = (1 << 2), // Don't cancel current goto depot command if any
	DEPOT_LOCATE_HANGAR = (1 << 3), // Find another airport if the target one lacks a hangar
};

struct GetNewVehiclePosResult {
	int x, y;
	TileIndex old_tile;
	TileIndex new_tile;
};

/* returns true if staying in the same tile */
GetNewVehiclePosResult GetNewVehiclePos(const Vehicle *v);
Direction GetDirectionTowards(const Vehicle *v, int x, int y);

static inline bool IsPlayerBuildableVehicleType(VehicleType type)
{
	switch (type) {
		case VEH_TRAIN:
		case VEH_ROAD:
		case VEH_SHIP:
		case VEH_AIRCRAFT:
			return true;

		default: return false;
	}
}

static inline bool IsPlayerBuildableVehicleType(const BaseVehicle *v)
{
	return IsPlayerBuildableVehicleType(v->type);
}

const struct Livery *GetEngineLivery(EngineID engine_type, PlayerID player, EngineID parent_engine_type, const Vehicle *v);

/**
 * Get the colour map for an engine. This used for unbuilt engines in the user interface.
 * @param engine_type ID of engine
 * @param player ID of player
 * @return A ready-to-use palette modifier
 */
SpriteID GetEnginePalette(EngineID engine_type, PlayerID player);

/**
 * Get the colour map for a vehicle.
 * @param v Vehicle to get colour map for
 * @return A ready-to-use palette modifier
 */
SpriteID GetVehiclePalette(const Vehicle *v);

/* A lot of code calls for the invalidation of the status bar, which is widget 5.
 * Best is to have a virtual value for it when it needs to change again */
#define STATUS_BAR 5

extern const uint32 _veh_build_proc_table[];
extern const uint32 _veh_sell_proc_table[];
extern const uint32 _veh_refit_proc_table[];
extern const uint32 _send_to_depot_proc_table[];

/* Functions to find the right command for certain vehicle type */
static inline uint32 GetCmdBuildVeh(VehicleType type)
{
	return _veh_build_proc_table[type];
}

static inline uint32 GetCmdBuildVeh(const BaseVehicle *v)
{
	return GetCmdBuildVeh(v->type);
}

static inline uint32 GetCmdSellVeh(VehicleType type)
{
	return _veh_sell_proc_table[type];
}

static inline uint32 GetCmdSellVeh(const BaseVehicle *v)
{
	return GetCmdSellVeh(v->type);
}

static inline uint32 GetCmdRefitVeh(VehicleType type)
{
	return _veh_refit_proc_table[type];
}

static inline uint32 GetCmdRefitVeh(const BaseVehicle *v)
{
	return GetCmdRefitVeh(v->type);
}

static inline uint32 GetCmdSendToDepot(VehicleType type)
{
	return _send_to_depot_proc_table[type];
}

static inline uint32 GetCmdSendToDepot(const BaseVehicle *v)
{
	return GetCmdSendToDepot(v->type);
}

bool EnsureNoVehicleOnGround(TileIndex tile);
void StopAllVehicles();

Vehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicle type);
Vehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicle type);
Vehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicle type);

extern VehicleID _vehicle_id_ctr_day;
extern Vehicle *_place_clicked_vehicle;
extern VehicleID _new_vehicle_id;
extern uint16 _returned_refit_capacity;

#endif /* VEHICLE_H */
