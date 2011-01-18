/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_cmd.cpp Handling of the engine groups */

#include "stdafx.h"
#include "cmd_helper.h"
#include "command_func.h"
#include "group.h"
#include "train.h"
#include "vehicle_gui.h"
#include "vehiclelist.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "autoreplace_base.h"
#include "autoreplace_func.h"
#include "string_func.h"
#include "company_func.h"
#include "core/pool_func.hpp"
#include "order_backup.h"

#include "table/strings.h"

GroupID _new_group_id;

GroupPool _group_pool("Group");
INSTANTIATE_POOL_METHODS(Group)

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
		if (!IsDefaultGroupID(old_g) && Group::IsValidID(old_g)) Group::Get(old_g)->num_engines[i]--;

		/* Increase the num engines of EngineID i of the new group if it's not the default one */
		if (!IsDefaultGroupID(new_g) && Group::IsValidID(new_g)) Group::Get(new_g)->num_engines[i]++;
	}
}



Group::Group(Owner owner)
{
	this->owner = owner;

	if (!Company::IsValidID(owner)) return;

	this->num_engines = CallocT<uint16>(Engine::GetPoolSize());
}

Group::~Group()
{
	free(this->name);
	free(this->num_engines);
}

void InitializeGroup()
{
	_group_pool.CleanPool();
}


/**
 * Create a new vehicle group.
 * @param tile unused
 * @param flags type of operation
 * @param p1   vehicle type
 * @param p2   unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdCreateGroup(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleType vt = Extract<VehicleType, 0, 3>(p1);
	if (!IsCompanyBuildableVehicleType(vt)) return CMD_ERROR;

	if (!Group::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Group *g = new Group(_current_company);
		g->replace_protection = false;
		g->vehicle_type = vt;

		_new_group_id = g->index;

		InvalidateWindowData(GetWindowClassForVehicleType(vt), VehicleListIdentifier(VL_GROUP_LIST, vt, _current_company).Pack());
	}

	return CommandCost();
}


/**
 * Add all vehicles in the given group to the default group and then deletes the group.
 * @param tile unused
 * @param flags type of operation
 * @param p1   index of array group
 *      - p1 bit 0-15 : GroupID
 * @param p2   unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdDeleteGroup(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Group *g = Group::GetIfValid(p1);
	if (g == NULL || g->owner != _current_company) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Vehicle *v;

		/* Add all vehicles belong to the group to the default group */
		FOR_ALL_VEHICLES(v) {
			if (v->group_id == g->index && v->type == g->vehicle_type) v->group_id = DEFAULT_GROUP;
		}

		/* Update backupped orders if needed */
		OrderBackup::ClearGroup(g->index);

		/* If we set an autoreplace for the group we delete, remove it. */
		if (_current_company < MAX_COMPANIES) {
			Company *c;
			EngineRenew *er;

			c = Company::Get(_current_company);
			FOR_ALL_ENGINE_RENEWS(er) {
				if (er->group_id == g->index) RemoveEngineReplacementForCompany(c, er->from, g->index, flags);
			}
		}

		VehicleType vt = g->vehicle_type;

		/* Delete the Replace Vehicle Windows */
		DeleteWindowById(WC_REPLACE_VEHICLE, g->vehicle_type);
		delete g;

		InvalidateWindowData(GetWindowClassForVehicleType(vt), VehicleListIdentifier(VL_GROUP_LIST, vt, _current_company).Pack());
	}

	return CommandCost();
}

static bool IsUniqueGroupName(const char *name)
{
	const Group *g;

	FOR_ALL_GROUPS(g) {
		if (g->name != NULL && strcmp(g->name, name) == 0) return false;
	}

	return true;
}

/**
 * Rename a group
 * @param tile unused
 * @param flags type of operation
 * @param p1   index of array group
 *   - p1 bit 0-15 : GroupID
 * @param p2   unused
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameGroup(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Group *g = Group::GetIfValid(p1);
	if (g == NULL || g->owner != _current_company) return CMD_ERROR;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_GROUP_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueGroupName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		/* Delete the old name */
		free(g->name);
		/* Assign the new one */
		g->name = reset ? NULL : strdup(text);

		InvalidateWindowData(GetWindowClassForVehicleType(g->vehicle_type), VehicleListIdentifier(VL_GROUP_LIST, g->vehicle_type, _current_company).Pack());
	}

	return CommandCost();
}


/**
 * Add a vehicle to a group
 * @param tile unused
 * @param flags type of operation
 * @param p1   index of array group
 *   - p1 bit 0-15 : GroupID
 * @param p2   vehicle to add to a group
 *   - p2 bit 0-19 : VehicleID
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdAddVehicleGroup(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v = Vehicle::GetIfValid(p2);
	GroupID new_g = p1;

	if (v == NULL || (!Group::IsValidID(new_g) && !IsDefaultGroupID(new_g))) return CMD_ERROR;

	if (Group::IsValidID(new_g)) {
		Group *g = Group::Get(new_g);
		if (g->owner != _current_company || g->vehicle_type != v->type) return CMD_ERROR;
	}

	if (v->owner != _current_company || !v->IsPrimaryVehicle()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DecreaseGroupNumVehicle(v->group_id);
		IncreaseGroupNumVehicle(new_g);

		switch (v->type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
				SetTrainGroupID(Train::From(v), new_g);
				break;
			case VEH_ROAD:
			case VEH_SHIP:
			case VEH_AIRCRAFT:
				if (v->IsEngineCountable()) UpdateNumEngineGroup(v->engine_type, v->group_id, new_g);
				v->group_id = new_g;
				break;
		}

		/* Update the Replace Vehicle Windows */
		SetWindowDirty(WC_REPLACE_VEHICLE, v->type);
		InvalidateWindowData(GetWindowClassForVehicleType(v->type), VehicleListIdentifier(VL_GROUP_LIST, v->type, _current_company).Pack());
	}

	return CommandCost();
}

/**
 * Add all shared vehicles of all vehicles from a group
 * @param tile unused
 * @param flags type of operation
 * @param p1   index of group array
 *  - p1 bit 0-15 : GroupID
 * @param p2   type of vehicles
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdAddSharedVehicleGroup(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleType type = Extract<VehicleType, 0, 3>(p2);
	GroupID id_g = p1;
	if (!Group::IsValidID(id_g) || !IsCompanyBuildableVehicleType(type)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Vehicle *v;

		/* Find the first front engine which belong to the group id_g
		 * then add all shared vehicles of this front engine to the group id_g */
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->IsPrimaryVehicle()) {
				if (v->group_id != id_g) continue;

				/* For each shared vehicles add it to the group */
				for (Vehicle *v2 = v->FirstShared(); v2 != NULL; v2 = v2->NextShared()) {
					if (v2->group_id != id_g) DoCommand(tile, id_g, v2->index, flags, CMD_ADD_VEHICLE_GROUP, text);
				}
			}
		}

		InvalidateWindowData(GetWindowClassForVehicleType(type), VehicleListIdentifier(VL_GROUP_LIST, type, _current_company).Pack());
	}

	return CommandCost();
}


/**
 * Remove all vehicles from a group
 * @param tile unused
 * @param flags type of operation
 * @param p1   index of group array
 * - p1 bit 0-15 : GroupID
 * @param p2   type of vehicles
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveAllVehiclesGroup(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	GroupID old_g = p1;
	Group *g = Group::GetIfValid(old_g);
	VehicleType type = Extract<VehicleType, 0, 3>(p2);

	if (g == NULL || g->owner != _current_company || !IsCompanyBuildableVehicleType(type)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Vehicle *v;

		/* Find each Vehicle that belongs to the group old_g and add it to the default group */
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->IsPrimaryVehicle()) {
				if (v->group_id != old_g) continue;

				/* Add The Vehicle to the default group */
				DoCommand(tile, DEFAULT_GROUP, v->index, flags, CMD_ADD_VEHICLE_GROUP, text);
			}
		}

		InvalidateWindowData(GetWindowClassForVehicleType(type), VehicleListIdentifier(VL_GROUP_LIST, type, _current_company).Pack());
	}

	return CommandCost();
}


/**
 * (Un)set global replace protection from a group
 * @param tile unused
 * @param flags type of operation
 * @param p1   index of group array
 * - p1 bit 0-15 : GroupID
 * @param p2
 * - p2 bit 0    : 1 to set or 0 to clear protection.
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSetGroupReplaceProtection(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Group *g = Group::GetIfValid(p1);
	if (g == NULL || g->owner != _current_company) return CMD_ERROR;

	if (flags & DC_EXEC) {
		g->replace_protection = HasBit(p2, 0);

		InvalidateWindowData(GetWindowClassForVehicleType(g->vehicle_type), VehicleListIdentifier(VL_GROUP_LIST, g->vehicle_type, _current_company).Pack());
		InvalidateWindowData(WC_REPLACE_VEHICLE, g->vehicle_type);
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
	if (!v->IsPrimaryVehicle()) return;

	if (!IsDefaultGroupID(v->group_id)) DecreaseGroupNumVehicle(v->group_id);
}


/**
 * Affect the groupID of a train to new_g.
 * @note called in CmdAddVehicleGroup and CmdMoveRailVehicle
 * @param v     First vehicle of the chain.
 * @param new_g index of array group
 */
void SetTrainGroupID(Train *v, GroupID new_g)
{
	if (!Group::IsValidID(new_g) && !IsDefaultGroupID(new_g)) return;

	assert(v->IsFrontEngine() || IsDefaultGroupID(new_g));

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		if (u->IsEngineCountable()) UpdateNumEngineGroup(u->engine_type, u->group_id, new_g);

		u->group_id = new_g;
	}

	/* Update the Replace Vehicle Windows */
	SetWindowDirty(WC_REPLACE_VEHICLE, VEH_TRAIN);
}


/**
 * Recalculates the groupID of a train. Should be called each time a vehicle is added
 * to/removed from the chain,.
 * @note this needs to be called too for 'wagon chains' (in the depot, without an engine)
 * @note Called in CmdBuildRailVehicle, CmdBuildRailWagon, CmdMoveRailVehicle, CmdSellRailWagon
 * @param v First vehicle of the chain.
 */
void UpdateTrainGroupID(Train *v)
{
	assert(v->IsFrontEngine() || v->IsFreeWagon());

	GroupID new_g = v->IsFrontEngine() ? v->group_id : (GroupID)DEFAULT_GROUP;
	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		if (u->IsEngineCountable()) UpdateNumEngineGroup(u->engine_type, u->group_id, new_g);

		u->group_id = new_g;
	}

	/* Update the Replace Vehicle Windows */
	SetWindowDirty(WC_REPLACE_VEHICLE, VEH_TRAIN);
}

/**
 * Get the number of engines with EngineID id_e in the group with GroupID
 * id_g
 * @param company The company the group belongs to
 * @param id_g The GroupID of the group used
 * @param id_e The EngineID of the engine to count
 * @return The number of engines with EngineID id_e in the group
 */
uint GetGroupNumEngines(CompanyID company, GroupID id_g, EngineID id_e)
{
	if (Group::IsValidID(id_g)) return Group::Get(id_g)->num_engines[id_e];

	uint num = Company::Get(company)->num_engines[id_e];
	if (!IsDefaultGroupID(id_g)) return num;

	const Group *g;
	FOR_ALL_GROUPS(g) {
		if (g->owner == company) num -= g->num_engines[id_e];
	}
	return num;
}

void RemoveAllGroupsForCompany(const CompanyID company)
{
	Group *g;

	FOR_ALL_GROUPS(g) {
		if (company == g->owner) delete g;
	}
}
