/* $Id$ */

/** @file group_cmd.cpp Handling of the engine groups */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "player.h"
#include "table/strings.h"
#include "command_func.h"
#include "vehicle.h"
#include "saveload.h"
#include "debug.h"
#include "group.h"
#include "train.h"
#include "aircraft.h"
#include "string.h"
#include "vehicle_gui.h"
#include "misc/autoptr.hpp"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"

/**
 * Update the num engines of a groupID. Decrease the old one and increase the new one
 * @note called in SetTrainGroupID and UpdateTrainGroupID
 * @param i     EngineID we have to update
 * @param old_g index of the old group
 * @param new_g index of the new group
 */
static inline void UpdateNumEngineGroup(EngineID i, GroupID old_g, GroupID new_g)
{
	if (old_g != new_g) {
		/* Decrease the num engines of EngineID i of the old group if it's not the default one */
		if (!IsDefaultGroupID(old_g) && IsValidGroupID(old_g)) GetGroup(old_g)->num_engines[i]--;

		/* Increase the num engines of EngineID i of the new group if it's not the new one */
		if (!IsDefaultGroupID(new_g) && IsValidGroupID(new_g)) GetGroup(new_g)->num_engines[i]++;
	}
}


DEFINE_OLD_POOL_GENERIC(Group, Group)


Group::Group(StringID str)
{
	this->string_id = str;
}

Group::~Group()
{
	DeleteName(this->string_id);
	this->string_id = STR_NULL;
}

bool Group::IsValid() const
{
	return this->string_id != STR_NULL;
}

void InitializeGroup(void)
{
	_Group_pool.CleanPool();
	_Group_pool.AddBlockToPool();
}


static WindowClass GetWCForVT(VehicleType vt)
{
	switch (vt) {
		default:
		case VEH_TRAIN:    return WC_TRAINS_LIST;
		case VEH_ROAD:     return WC_ROADVEH_LIST;
		case VEH_SHIP:     return WC_SHIPS_LIST;
		case VEH_AIRCRAFT: return WC_AIRCRAFT_LIST;
	}
}


/**
 * Create a new vehicle group.
 * @param tile unused
 * @param p1   vehicle type
 * @param p2   unused
 */
CommandCost CmdCreateGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleType vt = (VehicleType)p1;
	if (!IsPlayerBuildableVehicleType(vt)) return CMD_ERROR;

	AutoPtrT<Group> g_auto_delete;

	Group *g = new Group(STR_EMPTY);
	if (g == NULL) return CMD_ERROR;

	g_auto_delete = g;

	if (flags & DC_EXEC) {
		g->owner = _current_player;
		g->replace_protection = false;
		g->vehicle_type = vt;

		InvalidateWindowData(GetWCForVT(vt), (vt << 11) | VLW_GROUP_LIST | _current_player);

		g_auto_delete.Detach();
	}

	return CommandCost();
}


/**
 * Add all vehicles in the given group to the default group and then deletes the group.
 * @param tile unused
 * @param p1   index of array group
 *      - p1 bit 0-15 : GroupID
 * @param p2   unused
 */
CommandCost CmdDeleteGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidGroupID(p1)) return CMD_ERROR;

	Group *g = GetGroup(p1);
	if (g->owner != _current_player) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Vehicle *v;

		/* Add all vehicles belong to the group to the default group */
		FOR_ALL_VEHICLES(v) {
			if (v->group_id == g->index && v->type == g->vehicle_type) v->group_id = DEFAULT_GROUP;
		}

		/* If we set an autoreplace for the group we delete, remove it. */
		if (_current_player < MAX_PLAYERS) {
			Player *p;
			EngineRenew *er;

			p = GetPlayer(_current_player);
			FOR_ALL_ENGINE_RENEWS(er) {
				if (er->group_id == g->index) RemoveEngineReplacementForPlayer(p, er->from, g->index, flags);
			}
		}

		VehicleType vt = g->vehicle_type;

		/* Delete the Replace Vehicle Windows */
		DeleteWindowById(WC_REPLACE_VEHICLE, g->vehicle_type);
		delete g;

		InvalidateWindowData(GetWCForVT(vt), (vt << 11) | VLW_GROUP_LIST | _current_player);
	}

	return CommandCost();
}

static bool IsUniqueGroupName(const char *name)
{
	const Group *g;
	char buf[512];

	FOR_ALL_GROUPS(g) {
		SetDParam(0, g->index);
		GetString(buf, STR_GROUP_NAME, lastof(buf));
		if (strcmp(buf, name) == 0) return false;
	}

	return true;
}

/**
 * Rename a group
 * @param tile unused
 * @param p1   index of array group
 *   - p1 bit 0-15 : GroupID
 * @param p2   unused
 */
CommandCost CmdRenameGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidGroupID(p1) || StrEmpty(_cmd_text)) return CMD_ERROR;

	Group *g = GetGroup(p1);
	if (g->owner != _current_player) return CMD_ERROR;

	if (!IsUniqueGroupName(_cmd_text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);

	/* Create the name */
	StringID str = AllocateName(_cmd_text, 0);
	if (str == STR_NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Delete the old name */
		DeleteName(g->string_id);
		/* Assign the new one */
		g->string_id = str;

		InvalidateWindowData(GetWCForVT(g->vehicle_type), (g->vehicle_type << 11) | VLW_GROUP_LIST | _current_player);
	}

	return CommandCost();
}


/**
 * Add a vehicle to a group
 * @param tile unused
 * @param p1   index of array group
 *   - p1 bit 0-15 : GroupID
 * @param p2   vehicle to add to a group
 *   - p2 bit 0-15 : VehicleID
 */
CommandCost CmdAddVehicleGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	GroupID new_g = p1;

	if (!IsValidVehicleID(p2) || (!IsValidGroupID(new_g) && !IsDefaultGroupID(new_g))) return CMD_ERROR;

	Vehicle *v = GetVehicle(p2);

	if (IsValidGroupID(new_g)) {
		Group *g = GetGroup(new_g);
		if (g->owner != _current_player || g->vehicle_type != v->type) return CMD_ERROR;
	}

	if (v->owner != _current_player || !v->IsPrimaryVehicle()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DecreaseGroupNumVehicle(v->group_id);
		IncreaseGroupNumVehicle(new_g);

		switch (v->type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
				SetTrainGroupID(v, new_g);
				break;
			case VEH_ROAD:
			case VEH_SHIP:
			case VEH_AIRCRAFT:
				if (IsEngineCountable(v)) UpdateNumEngineGroup(v->engine_type, v->group_id, new_g);
				v->group_id = new_g;
				break;
		}

		/* Update the Replace Vehicle Windows */
		InvalidateWindow(WC_REPLACE_VEHICLE, v->type);
		InvalidateWindowData(GetWCForVT(v->type), (v->type << 11) | VLW_GROUP_LIST | _current_player);
	}

	return CommandCost();
}

/**
 * Add all shared vehicles of all vehicles from a group
 * @param tile unused
 * @param p1   index of group array
 *  - p1 bit 0-15 : GroupID
 * @param p2   type of vehicles
 */
CommandCost CmdAddSharedVehicleGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleType type = (VehicleType)p2;
	if (!IsValidGroupID(p1) || !IsPlayerBuildableVehicleType(type)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Vehicle *v;
		VehicleType type = (VehicleType)p2;
		GroupID id_g = p1;

		/* Find the first front engine which belong to the group id_g
		 * then add all shared vehicles of this front engine to the group id_g */
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->IsPrimaryVehicle()) {
				if (v->group_id != id_g) continue;

				/* For each shared vehicles add it to the group */
				for (Vehicle *v2 = GetFirstVehicleFromSharedList(v); v2 != NULL; v2 = v2->next_shared) {
					if (v2->group_id != id_g) CmdAddVehicleGroup(tile, flags, id_g, v2->index);
				}
			}
		}

		InvalidateWindowData(GetWCForVT(type), (type << 11) | VLW_GROUP_LIST | _current_player);
	}

	return CommandCost();
}


/**
 * Remove all vehicles from a group
 * @param tile unused
 * @param p1   index of group array
 * - p1 bit 0-15 : GroupID
 * @param p2   type of vehicles
 */
CommandCost CmdRemoveAllVehiclesGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleType type = (VehicleType)p2;
	if (!IsValidGroupID(p1) || !IsPlayerBuildableVehicleType(type)) return CMD_ERROR;

	Group *g = GetGroup(p1);
	if (g->owner != _current_player) return CMD_ERROR;

	if (flags & DC_EXEC) {
		GroupID old_g = p1;
		Vehicle *v;

		/* Find each Vehicle that belongs to the group old_g and add it to the default group */
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->IsPrimaryVehicle()) {
				if (v->group_id != old_g) continue;

				/* Add The Vehicle to the default group */
				CmdAddVehicleGroup(tile, flags, DEFAULT_GROUP, v->index);
			}
		}

		InvalidateWindowData(GetWCForVT(type), (type << 11) | VLW_GROUP_LIST | _current_player);
	}

	return CommandCost();
}


/**
 * (Un)set global replace protection from a group
 * @param tile unused
 * @param p1   index of group array
 * - p1 bit 0-15 : GroupID
 * @param p2
 * - p2 bit 0    : 1 to set or 0 to clear protection.
 */
CommandCost CmdSetGroupReplaceProtection(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidGroupID(p1)) return CMD_ERROR;

	Group *g = GetGroup(p1);
	if (g->owner != _current_player) return CMD_ERROR;

	if (flags & DC_EXEC) {
		g->replace_protection = HasBit(p2, 0);

		InvalidateWindowData(GetWCForVT(g->vehicle_type), (g->vehicle_type << 11) | VLW_GROUP_LIST | _current_player);
	}

	return CommandCost();
}

/**
 * Decrease the num_vehicle variable before delete an front engine from a group
 * @note Called in CmdSellRailWagon and DeleteLasWagon,
 * @param v     FrontEngine of the train we want to remove.
 */
void RemoveVehicleFromGroup(const Vehicle *v)
{
	if (!v->IsValid() || !v->IsPrimaryVehicle()) return;

	if (!IsDefaultGroupID(v->group_id)) DecreaseGroupNumVehicle(v->group_id);
}


/**
 * Affect the groupID of a train to new_g.
 * @note called in CmdAddVehicleGroup and CmdMoveRailVehicle
 * @param v     First vehicle of the chain.
 * @param new_g index of array group
 */
void SetTrainGroupID(Vehicle *v, GroupID new_g)
{
	if (!IsValidGroupID(new_g) && !IsDefaultGroupID(new_g)) return;

	assert(v->IsValid() && v->type == VEH_TRAIN && IsFrontEngine(v));

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		if (IsEngineCountable(u)) UpdateNumEngineGroup(u->engine_type, u->group_id, new_g);

		u->group_id = new_g;
	}

	/* Update the Replace Vehicle Windows */
	InvalidateWindow(WC_REPLACE_VEHICLE, VEH_TRAIN);
}


/**
 * Recalculates the groupID of a train. Should be called each time a vehicle is added
 * to/removed from the chain,.
 * @note this needs to be called too for 'wagon chains' (in the depot, without an engine)
 * @note Called in CmdBuildRailVehicle, CmdBuildRailWagon, CmdMoveRailVehicle, CmdSellRailWagon
 * @param v First vehicle of the chain.
 */
void UpdateTrainGroupID(Vehicle *v)
{
	assert(v->IsValid() && v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v)));

	GroupID new_g = IsFrontEngine(v) ? v->group_id : (GroupID)DEFAULT_GROUP;
	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		if (IsEngineCountable(u)) UpdateNumEngineGroup(u->engine_type, u->group_id, new_g);

		u->group_id = new_g;
	}

	/* Update the Replace Vehicle Windows */
	InvalidateWindow(WC_REPLACE_VEHICLE, VEH_TRAIN);
}


void RemoveAllGroupsForPlayer(const Player *p)
{
	Group *g;

	FOR_ALL_GROUPS(g) {
		if (p->index == g->owner) delete g;
	}
}


static const SaveLoad _group_desc[] = {
  SLE_VAR(Group, string_id,          SLE_UINT16),
  SLE_VAR(Group, num_vehicle,        SLE_UINT16),
  SLE_VAR(Group, owner,              SLE_UINT8),
  SLE_VAR(Group, vehicle_type,       SLE_UINT8),
  SLE_VAR(Group, replace_protection, SLE_BOOL),
  SLE_END()
};


static void Save_GROUP(void)
{
	Group *g;

	FOR_ALL_GROUPS(g) {
		SlSetArrayIndex(g->index);
		SlObject(g, _group_desc);
	}
}


static void Load_GROUP(void)
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Group *g = new (index) Group();
		SlObject(g, _group_desc);
	}
}

extern const ChunkHandler _group_chunk_handlers[] = {
	{ 'GRPS', Save_GROUP, Load_GROUP, CH_ARRAY | CH_LAST},
};
