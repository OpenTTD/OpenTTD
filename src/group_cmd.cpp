/* $Id$ */

/** @file group_cmd.cpp Handling of the engine groups */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "player.h"
#include "table/strings.h"
#include "command.h"
#include "vehicle.h"
#include "saveload.h"
#include "debug.h"
#include "group.h"
#include "train.h"
#include "aircraft.h"
#include "string.h"

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


/**
 * Called if a new block is added to the group-pool
 */
static void GroupPoolNewBlock(uint start_item)
{
	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (Group *g = GetGroup(start_item); g != NULL; g = (g->index + 1U < GetGroupPoolSize()) ? GetGroup(g->index + 1) : NULL) g->index = start_item++;
}

DEFINE_OLD_POOL(Group, Group, GroupPoolNewBlock, NULL)

static Group *AllocateGroup(void)
{
	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (Group *g = GetGroup(0); g != NULL; g = (g->index + 1U < GetGroupPoolSize()) ? GetGroup(g->index + 1) : NULL) {
		if (!IsValidGroup(g)) {
			const GroupID index = g->index;

			memset(g, 0, sizeof(*g));
			g->index = index;

			return g;
		}
	}

	/* Check if we can add a block to the pool */
	return (AddBlockToPool(&_Group_pool)) ? AllocateGroup() : NULL;
}

void InitializeGroup(void)
{
	CleanPool(&_Group_pool);
	AddBlockToPool(&_Group_pool);
}


/**
 * Add a vehicle to a group
 * @param tile unused
 * @param p1   vehicle type
 * @param p2   unused
 */
int32 CmdCreateGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleType vt = (VehicleType)p1;
	if (!IsPlayerBuildableVehicleType(vt)) return CMD_ERROR;

	Group *g = AllocateGroup();
	if (g == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		g->owner = _current_player;
		g->string_id = STR_SV_GROUP_NAME;
		g->replace_protection = false;
		g->vehicle_type = vt;
	}

	return 0;
}


/**
 * Add a vehicle to a group
 * @param tile unused
 * @param p1   index of array group
 *      - p1 bit 0-15 : GroupID
 * @param p2   unused
 */
int32 CmdDeleteGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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

		/* Delete the Replace Vehicle Windows */
		DeleteWindowById(WC_REPLACE_VEHICLE, g->vehicle_type);
		DeleteGroup(g);
	}

	return 0;
}


/**
 * Rename a group
 * @param tile unused
 * @param p1   index of array group
 *   - p1 bit 0-15 : GroupID
 * @param p2   unused
 */
int32 CmdRenameGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidGroupID(p1) || StrEmpty(_cmd_text)) return CMD_ERROR;

	/* Create the name */
	StringID str = AllocateName(_cmd_text, 0);
	if (str == STR_NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Group *g = GetGroup(p1);

		/* Delete the old name */
		DeleteName(g->string_id);
		/* Assign the new one */
		g->string_id = str;
		g->owner = _current_player;
	}

	return 0;
}


/**
 * Add a vehicle to a group
 * @param tile unused
 * @param p1   index of array group
 *   - p1 bit 0-15 : GroupID
 * @param p2   vehicle to add to a group
 *   - p2 bit 0-15 : VehicleID
 */
int32 CmdAddVehicleGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	GroupID new_g = p1;

	if (!IsValidVehicleID(p2) || !IsValidGroupID(new_g)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p2);
	if (v->owner != _current_player || (v->type == VEH_TRAIN && !IsFrontEngine(v))) return CMD_ERROR;

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
	}

	return 0;
}

/**
 * Add all shared vehicles of all vehicles from a group
 * @param tile unused
 * @param p1   index of group array
 *  - p1 bit 0-15 : GroupID
 * @param p2   type of vehicles
 */
int32 CmdAddSharedVehicleGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleType type = (VehicleType)p2;
	if (!IsValidGroupID(p1) || !IsPlayerBuildableVehicleType(type)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Vehicle *v;
		VehicleType type = (VehicleType)p2;
		GroupID id_g = p1;
		uint subtype = (type == VEH_AIRCRAFT) ? AIR_AIRCRAFT : 0;

		/* Find the first front engine which belong to the group id_g
		 * then add all shared vehicles of this front engine to the group id_g */
		FOR_ALL_VEHICLES(v) {
			if ((v->type == type) && (
					(type == VEH_TRAIN && IsFrontEngine(v)) ||
					(type != VEH_TRAIN && v->subtype <= subtype))) {
				if (v->group_id != id_g) continue;

				/* For each shared vehicles add it to the group */
				for (Vehicle *v2 = GetFirstVehicleFromSharedList(v); v2 != NULL; v2 = v2->next_shared) {
					if (v2->group_id != id_g) CmdAddVehicleGroup(tile, flags, id_g, v2->index);
				}
			}
		}
	}

	return 0;
}


/**
 * Remove all vehicles from a group
 * @param tile unused
 * @param p1   index of group array
 * - p1 bit 0-15 : GroupID
 * @param p2   type of vehicles
 */
int32 CmdRemoveAllVehiclesGroup(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleType type = (VehicleType)p2;
	if (!IsValidGroupID(p1) || !IsPlayerBuildableVehicleType(type)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		GroupID old_g = p1;
		uint subtype = (type == VEH_AIRCRAFT) ? AIR_AIRCRAFT : 0;
		Vehicle *v;

		/* Find each Vehicle that belongs to the group old_g and add it to the default group */
		FOR_ALL_VEHICLES(v) {
			if ((v->type == type) && (
					(type == VEH_TRAIN && IsFrontEngine(v)) ||
					(type != VEH_TRAIN && v->subtype <= subtype))) {
				if (v->group_id != old_g) continue;

				/* Add The Vehicle to the default group */
				CmdAddVehicleGroup(tile, flags, DEFAULT_GROUP, v->index);
			}
		}
	}

	return 0;
}

/**
 * Decrease the num_vehicle variable before delete an front engine from a group
 * @note Called in CmdSellRailWagon and DeleteLasWagon,
 * @param v     FrontEngine of the train we want to remove.
 */
void RemoveVehicleFromGroup(const Vehicle *v)
{
	if (!IsValidVehicle(v) || v->type != VEH_TRAIN || !IsFrontEngine(v)) return;

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

	assert(IsValidVehicle(v) && v->type == VEH_TRAIN && IsFrontEngine(v));

	for (Vehicle *u = v; u != NULL; u = u->next) {
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
	assert(IsValidVehicle(v) && v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v)));

	GroupID new_g = IsFrontEngine(v) ? v->group_id : (GroupID)DEFAULT_GROUP;
	for (Vehicle *u = v; u != NULL; u = u->next) {
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
		if (p->index == g->owner) DeleteGroup(g);
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
		if (!AddBlockIfNeeded(&_Group_pool, index)) {
			error("Groups: failed loading savegame: too many groups");
		}

		Group *g = GetGroup(index);
		SlObject(g, _group_desc);
	}
}

extern const ChunkHandler _group_chunk_handlers[] = {
	{ 'GRPS', Save_GROUP, Load_GROUP, CH_ARRAY | CH_LAST},
};
