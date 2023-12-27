/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_cmd.cpp Handling of the engine groups */

#include "stdafx.h"
#include "command_func.h"
#include "train.h"
#include "vehiclelist.h"
#include "vehicle_func.h"
#include "autoreplace_base.h"
#include "autoreplace_func.h"
#include "string_func.h"
#include "company_func.h"
#include "core/pool_func.hpp"
#include "order_backup.h"
#include "group_cmd.h"

#include "table/strings.h"

#include "safeguards.h"

GroupPool _group_pool("Group");
INSTANTIATE_POOL_METHODS(Group)

GroupStatistics::GroupStatistics()
{
	this->num_engines = CallocT<uint16_t>(Engine::GetPoolSize());
}

GroupStatistics::~GroupStatistics()
{
	free(this->num_engines);
}

/**
 * Clear all caches.
 */
void GroupStatistics::Clear()
{
	this->num_vehicle = 0;
	this->profit_last_year = 0;
	this->num_vehicle_min_age = 0;
	this->profit_last_year_min_age = 0;

	/* This is also called when NewGRF change. So the number of engines might have changed. Reallocate. */
	free(this->num_engines);
	this->num_engines = CallocT<uint16_t>(Engine::GetPoolSize());
}

/**
 * Returns the GroupStatistics for a specific group.
 * @param company Owner of the group.
 * @param id_g    GroupID of the group.
 * @param type    VehicleType of the vehicles in the group.
 * @return Statistics for the group.
 */
/* static */ GroupStatistics &GroupStatistics::Get(CompanyID company, GroupID id_g, VehicleType type)
{
	if (Group::IsValidID(id_g)) {
		Group *g = Group::Get(id_g);
		assert(g->owner == company);
		assert(g->vehicle_type == type);
		return g->statistics;
	}

	if (IsDefaultGroupID(id_g)) return Company::Get(company)->group_default[type];
	if (IsAllGroupID(id_g)) return Company::Get(company)->group_all[type];

	NOT_REACHED();
}

/**
 * Returns the GroupStatistic for the group of a vehicle.
 * @param v Vehicle.
 * @return GroupStatistics for the group of the vehicle.
 */
/* static */ GroupStatistics &GroupStatistics::Get(const Vehicle *v)
{
	return GroupStatistics::Get(v->owner, v->group_id, v->type);
}

/**
 * Returns the GroupStatistic for the ALL_GROUPO of a vehicle type.
 * @param v Vehicle.
 * @return GroupStatistics for the ALL_GROUP of the vehicle type.
 */
/* static */ GroupStatistics &GroupStatistics::GetAllGroup(const Vehicle *v)
{
	return GroupStatistics::Get(v->owner, ALL_GROUP, v->type);
}

/**
 * Update all caches after loading a game, changing NewGRF, etc.
 */
/* static */ void GroupStatistics::UpdateAfterLoad()
{
	/* Set up the engine count for all companies */
	for (Company *c : Company::Iterate()) {
		for (VehicleType type = VEH_BEGIN; type < VEH_COMPANY_END; type++) {
			c->group_all[type].Clear();
			c->group_default[type].Clear();
		}
	}

	/* Recalculate */
	for (Group *g : Group::Iterate()) {
		g->statistics.Clear();
	}

	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!v->IsEngineCountable()) continue;

		GroupStatistics::CountEngine(v, 1);
		if (v->IsPrimaryVehicle()) GroupStatistics::CountVehicle(v, 1);
	}

	for (const Company *c : Company::Iterate()) {
		GroupStatistics::UpdateAutoreplace(c->index);
	}
}

/**
 * Update num_vehicle when adding or removing a vehicle.
 * @param v Vehicle to count.
 * @param delta +1 to add, -1 to remove.
 */
/* static */ void GroupStatistics::CountVehicle(const Vehicle *v, int delta)
{
	assert(delta == 1 || delta == -1);

	GroupStatistics &stats_all = GroupStatistics::GetAllGroup(v);
	GroupStatistics &stats = GroupStatistics::Get(v);

	stats_all.num_vehicle += delta;
	stats_all.profit_last_year += v->GetDisplayProfitLastYear() * delta;
	stats.num_vehicle += delta;
	stats.profit_last_year += v->GetDisplayProfitLastYear() * delta;

	if (v->age > VEHICLE_PROFIT_MIN_AGE) {
		stats_all.num_vehicle_min_age += delta;
		stats_all.profit_last_year_min_age += v->GetDisplayProfitLastYear() * delta;
		stats.num_vehicle_min_age += delta;
		stats.profit_last_year_min_age += v->GetDisplayProfitLastYear() * delta;
	}
}

/**
 * Update num_engines when adding/removing an engine.
 * @param v Engine to count.
 * @param delta +1 to add, -1 to remove.
 */
/* static */ void GroupStatistics::CountEngine(const Vehicle *v, int delta)
{
	assert(delta == 1 || delta == -1);
	GroupStatistics::GetAllGroup(v).num_engines[v->engine_type] += delta;
	GroupStatistics::Get(v).num_engines[v->engine_type] += delta;
}

/**
 * Add a vehicle's last year profit to the profit sum of its group.
 */
/* static */ void GroupStatistics::AddProfitLastYear(const Vehicle *v)
{
	GroupStatistics &stats_all = GroupStatistics::GetAllGroup(v);
	GroupStatistics &stats = GroupStatistics::Get(v);

	stats_all.profit_last_year += v->GetDisplayProfitLastYear();
	stats.profit_last_year += v->GetDisplayProfitLastYear();
}

/**
 * Add a vehicle to the profit sum of its group.
 */
/* static */ void GroupStatistics::VehicleReachedMinAge(const Vehicle *v)
{
	GroupStatistics &stats_all = GroupStatistics::GetAllGroup(v);
	GroupStatistics &stats = GroupStatistics::Get(v);

	stats_all.num_vehicle_min_age++;
	stats_all.profit_last_year_min_age += v->GetDisplayProfitLastYear();
	stats.num_vehicle_min_age++;
	stats.profit_last_year_min_age += v->GetDisplayProfitLastYear();
}

/**
 * Recompute the profits for all groups.
 */
/* static */ void GroupStatistics::UpdateProfits()
{
	/* Set up the engine count for all companies */
	for (Company *c : Company::Iterate()) {
		for (VehicleType type = VEH_BEGIN; type < VEH_COMPANY_END; type++) {
			c->group_all[type].ClearProfits();
			c->group_default[type].ClearProfits();
		}
	}

	/* Recalculate */
	for (Group *g : Group::Iterate()) {
		g->statistics.ClearProfits();
	}

	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->IsPrimaryVehicle()) {
			GroupStatistics::AddProfitLastYear(v);
			if (v->age > VEHICLE_PROFIT_MIN_AGE) GroupStatistics::VehicleReachedMinAge(v);
		}
	}
}

/**
 * Update autoreplace_defined and autoreplace_finished of all statistics of a company.
 * @param company Company to update statistics for.
 */
/* static */ void GroupStatistics::UpdateAutoreplace(CompanyID company)
{
	/* Set up the engine count for all companies */
	Company *c = Company::Get(company);
	for (VehicleType type = VEH_BEGIN; type < VEH_COMPANY_END; type++) {
		c->group_all[type].ClearAutoreplace();
		c->group_default[type].ClearAutoreplace();
	}

	/* Recalculate */
	for (Group *g : Group::Iterate()) {
		if (g->owner != company) continue;
		g->statistics.ClearAutoreplace();
	}

	for (EngineRenewList erl = c->engine_renew_list; erl != nullptr; erl = erl->next) {
		const Engine *e = Engine::Get(erl->from);
		GroupStatistics &stats = GroupStatistics::Get(company, erl->group_id, e->type);
		if (!stats.autoreplace_defined) {
			stats.autoreplace_defined = true;
			stats.autoreplace_finished = true;
		}
		if (GetGroupNumEngines(company, erl->group_id, erl->from) > 0) stats.autoreplace_finished = false;
	}
}

/**
 * Update the num engines of a groupID. Decrease the old one and increase the new one
 * @note called in SetTrainGroupID and UpdateTrainGroupID
 * @param v     Vehicle we have to update
 * @param old_g index of the old group
 * @param new_g index of the new group
 */
static inline void UpdateNumEngineGroup(const Vehicle *v, GroupID old_g, GroupID new_g)
{
	if (old_g != new_g) {
		/* Decrease the num engines in the old group */
		GroupStatistics::Get(v->owner, old_g, v->type).num_engines[v->engine_type]--;

		/* Increase the num engines in the new group */
		GroupStatistics::Get(v->owner, new_g, v->type).num_engines[v->engine_type]++;
	}
}


const Livery *GetParentLivery(const Group *g)
{
	if (g->parent == INVALID_GROUP) {
		const Company *c = Company::Get(g->owner);
		return &c->livery[LS_DEFAULT];
	}

	const Group *pg = Group::Get(g->parent);
	return &pg->livery;
}


/**
 * Propagate a livery change to a group's children, and optionally update cached vehicle colourmaps.
 * @param g Group to propagate colours to children.
 * @param reset_cache Reset colourmap of vehicles in this group.
 */
static void PropagateChildLivery(const Group *g, bool reset_cache)
{
	if (reset_cache) {
		/* Company colour data is indirectly cached. */
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->group_id == g->index && (!v->IsGroundVehicle() || v->IsFrontEngine())) {
				for (Vehicle *u = v; u != nullptr; u = u->Next()) {
					u->colourmap = PAL_NONE;
					u->InvalidateNewGRFCache();
				}
			}
		}
	}

	for (Group *cg : Group::Iterate()) {
		if (cg->parent == g->index) {
			if (!HasBit(cg->livery.in_use, 0)) cg->livery.colour1 = g->livery.colour1;
			if (!HasBit(cg->livery.in_use, 1)) cg->livery.colour2 = g->livery.colour2;
			PropagateChildLivery(cg, reset_cache);
		}
	}
}

/**
 * Update group liveries for a company. This is called when the LS_DEFAULT scheme is changed, to update groups with
 * colours set to default.
 * @param c Company to update.
 */
void UpdateCompanyGroupLiveries(const Company *c)
{
	for (Group *g : Group::Iterate()) {
		if (g->owner == c->index && g->parent == INVALID_GROUP) {
			if (!HasBit(g->livery.in_use, 0)) g->livery.colour1 = c->livery[LS_DEFAULT].colour1;
			if (!HasBit(g->livery.in_use, 1)) g->livery.colour2 = c->livery[LS_DEFAULT].colour2;
			PropagateChildLivery(g, false);
		}
	}
}

Group::Group(Owner owner)
{
	this->owner = owner;
	this->folded = false;
}


/**
 * Create a new vehicle group.
 * @param flags type of operation
 * @param vt vehicle type
 * @param parent_group parent groupid
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, GroupID> CmdCreateGroup(DoCommandFlag flags, VehicleType vt, GroupID parent_group)
{
	if (!IsCompanyBuildableVehicleType(vt)) return { CMD_ERROR, INVALID_GROUP };

	if (!Group::CanAllocateItem()) return { CMD_ERROR, INVALID_GROUP };

	const Group *pg = Group::GetIfValid(parent_group);
	if (pg != nullptr) {
		if (pg->owner != _current_company) return { CMD_ERROR, INVALID_GROUP };
		if (pg->vehicle_type != vt) return { CMD_ERROR, INVALID_GROUP };
	}

	if (flags & DC_EXEC) {
		Group *g = new Group(_current_company);
		g->vehicle_type = vt;
		g->parent = INVALID_GROUP;

		if (pg == nullptr) {
			const Company *c = Company::Get(_current_company);
			g->livery.colour1 = c->livery[LS_DEFAULT].colour1;
			g->livery.colour2 = c->livery[LS_DEFAULT].colour2;
			if (c->settings.renew_keep_length) SetBit(g->flags, GroupFlags::GF_REPLACE_WAGON_REMOVAL);
		} else {
			g->parent = pg->index;
			g->livery.colour1 = pg->livery.colour1;
			g->livery.colour2 = pg->livery.colour2;
			g->flags = pg->flags;
		}

		InvalidateWindowData(GetWindowClassForVehicleType(vt), VehicleListIdentifier(VL_GROUP_LIST, vt, _current_company).Pack());
		InvalidateWindowData(WC_COMPANY_COLOUR, g->owner, g->vehicle_type);

		return { CommandCost(), g->index };
	}

	return { CommandCost(), INVALID_GROUP};
}


/**
 * Add all vehicles in the given group to the default group and then deletes the group.
 * @param flags type of operation
 * @param group_id index of group
 * @return the cost of this operation or an error
 */
CommandCost CmdDeleteGroup(DoCommandFlag flags, GroupID group_id)
{
	Group *g = Group::GetIfValid(group_id);
	if (g == nullptr || g->owner != _current_company) return CMD_ERROR;

	/* Remove all vehicles from the group */
	Command<CMD_REMOVE_ALL_VEHICLES_GROUP>::Do(flags, group_id);

	/* Delete sub-groups */
	for (const Group *gp : Group::Iterate()) {
		if (gp->parent == g->index) {
			Command<CMD_DELETE_GROUP>::Do(flags, gp->index);
		}
	}

	if (flags & DC_EXEC) {
		/* Update backupped orders if needed */
		OrderBackup::ClearGroup(g->index);

		/* If we set an autoreplace for the group we delete, remove it. */
		if (_current_company < MAX_COMPANIES) {
			Company *c;

			c = Company::Get(_current_company);
			for (EngineRenew *er : EngineRenew::Iterate()) {
				if (er->group_id == g->index) RemoveEngineReplacementForCompany(c, er->from, g->index, flags);
			}
		}

		VehicleType vt = g->vehicle_type;

		/* Delete the Replace Vehicle Windows */
		CloseWindowById(WC_REPLACE_VEHICLE, g->vehicle_type);
		delete g;

		InvalidateWindowData(GetWindowClassForVehicleType(vt), VehicleListIdentifier(VL_GROUP_LIST, vt, _current_company).Pack());
		InvalidateWindowData(WC_COMPANY_COLOUR, _current_company, vt);
	}

	return CommandCost();
}

/**
 * Alter a group
 * @param flags type of operation
 * @param mode Operation to perform.
 * @param group_id GroupID
 * @param parent_id parent group index
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdAlterGroup(DoCommandFlag flags, AlterGroupMode mode, GroupID group_id, GroupID parent_id, const std::string &text)
{
	Group *g = Group::GetIfValid(group_id);
	if (g == nullptr || g->owner != _current_company) return CMD_ERROR;

	if (mode == AlterGroupMode::Rename) {
		/* Rename group */
		bool reset = text.empty();

		if (!reset) {
			if (Utf8StringLength(text) >= MAX_LENGTH_GROUP_NAME_CHARS) return CMD_ERROR;
		}

		if (flags & DC_EXEC) {
			/* Assign the new one */
			if (reset) {
				g->name.clear();
			} else {
				g->name = text;
			}
		}
	} else if (mode == AlterGroupMode::SetParent) {
		/* Set group parent */
		const Group *pg = Group::GetIfValid(parent_id);

		if (pg != nullptr) {
			if (pg->owner != _current_company) return CMD_ERROR;
			if (pg->vehicle_type != g->vehicle_type) return CMD_ERROR;

			/* Ensure request parent isn't child of group.
			 * This is the only place that infinite loops are prevented. */
			if (GroupIsInGroup(pg->index, g->index)) return_cmd_error(STR_ERROR_GROUP_CAN_T_SET_PARENT_RECURSION);
		}

		if (flags & DC_EXEC) {
			g->parent = (pg == nullptr) ? INVALID_GROUP : pg->index;
			GroupStatistics::UpdateAutoreplace(g->owner);

			if (!HasBit(g->livery.in_use, 0) || !HasBit(g->livery.in_use, 1)) {
				/* Update livery with new parent's colours if either colour is default. */
				const Livery *livery = GetParentLivery(g);
				if (!HasBit(g->livery.in_use, 0)) g->livery.colour1 = livery->colour1;
				if (!HasBit(g->livery.in_use, 1)) g->livery.colour2 = livery->colour2;

				PropagateChildLivery(g, true);
				MarkWholeScreenDirty();
			}
		}
	} else {
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		InvalidateWindowData(WC_REPLACE_VEHICLE, g->vehicle_type, 1);
		InvalidateWindowData(GetWindowClassForVehicleType(g->vehicle_type), VehicleListIdentifier(VL_GROUP_LIST, g->vehicle_type, _current_company).Pack());
		InvalidateWindowData(WC_COMPANY_COLOUR, g->owner, g->vehicle_type);
		InvalidateWindowClassesData(WC_VEHICLE_VIEW);
		InvalidateWindowClassesData(WC_VEHICLE_DETAILS);
	}

	return CommandCost();
}


/**
 * Do add a vehicle to a group.
 * @param v Vehicle to add.
 * @param new_g Group to add to.
 */
static void AddVehicleToGroup(Vehicle *v, GroupID new_g)
{
	GroupStatistics::CountVehicle(v, -1);

	switch (v->type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			SetTrainGroupID(Train::From(v), new_g);
			break;

		case VEH_ROAD:
		case VEH_SHIP:
		case VEH_AIRCRAFT:
			if (v->IsEngineCountable()) UpdateNumEngineGroup(v, v->group_id, new_g);
			v->group_id = new_g;
			for (Vehicle *u = v; u != nullptr; u = u->Next()) {
				u->colourmap = PAL_NONE;
				u->InvalidateNewGRFCache();
				u->UpdateViewport(true);
			}
			break;
	}

	GroupStatistics::CountVehicle(v, 1);
}

/**
 * Add a vehicle to a group
 * @param flags type of operation
 * @param group_id index of group
 * @param veh_id vehicle to add to a group
 * @param add_shared Add shared vehicles as well.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, GroupID> CmdAddVehicleGroup(DoCommandFlag flags, GroupID group_id, VehicleID veh_id, bool add_shared, const VehicleListIdentifier &vli)
{
	GroupID new_g = group_id;
	if (!Group::IsValidID(new_g) && !IsDefaultGroupID(new_g) && new_g != NEW_GROUP) return { CMD_ERROR, INVALID_GROUP };

	VehicleList list;
	if (veh_id == INVALID_VEHICLE && vli.Valid()) {
		if (!GenerateVehicleSortList(&list, vli) || list.empty()) return { CMD_ERROR, INVALID_GROUP };
	} else {
		Vehicle *v = Vehicle::GetIfValid(veh_id);
		if (v == nullptr) return { CMD_ERROR, INVALID_GROUP };
		list.push_back(v);
	}

	VehicleType vtype = list.front()->type;
	for (const Vehicle *v : list) {
		if (v->owner != _current_company || !v->IsPrimaryVehicle()) return { CMD_ERROR, INVALID_GROUP };
	}

	if (Group::IsValidID(new_g)) {
		Group *g = Group::Get(new_g);
		if (g->owner != _current_company || g->vehicle_type != vtype) return { CMD_ERROR, INVALID_GROUP };
	}

	if (new_g == NEW_GROUP) {
		/* Create new group. */
		auto [ret, new_group_id] = CmdCreateGroup(flags, vtype, INVALID_GROUP);
		if (ret.Failed()) return { ret, new_group_id };

		new_g = new_group_id;
	}

	if (flags & DC_EXEC) {
		for (const Vehicle *vc : list) {
			/* VehicleList is const but we need to modify the vehicle. */
			Vehicle *v = Vehicle::Get(vc->index);
			AddVehicleToGroup(v, new_g);

			if (add_shared) {
				/* Add vehicles in the shared order list as well. */
				for (Vehicle *v2 = v->FirstShared(); v2 != nullptr; v2 = v2->NextShared()) {
					if (v2->group_id != new_g) AddVehicleToGroup(v2, new_g);
				}
			}

			SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
			SetWindowDirty(WC_VEHICLE_VIEW, v->index);
			SetWindowDirty(WC_VEHICLE_DETAILS, v->index);
			InvalidateWindowData(WC_VEHICLE_VIEW, v->index);
			InvalidateWindowData(WC_VEHICLE_DETAILS, v->index);
		}

		GroupStatistics::UpdateAutoreplace(_current_company);

		/* Update the Replace Vehicle Windows */
		SetWindowDirty(WC_REPLACE_VEHICLE, vtype);
		InvalidateWindowData(GetWindowClassForVehicleType(vtype), VehicleListIdentifier(VL_GROUP_LIST, vtype, _current_company).Pack());
	}

	return { CommandCost(), new_g };
}

/**
 * Add all shared vehicles of all vehicles from a group
 * @param flags type of operation
 * @param id_g index of group
 * @param type type of vehicles
 * @return the cost of this operation or an error
 */
CommandCost CmdAddSharedVehicleGroup(DoCommandFlag flags, GroupID id_g, VehicleType type)
{
	if (!Group::IsValidID(id_g) || !IsCompanyBuildableVehicleType(type)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Find the first front engine which belong to the group id_g
		 * then add all shared vehicles of this front engine to the group id_g */
		for (const Vehicle *v : Vehicle::Iterate()) {
			if (v->type == type && v->IsPrimaryVehicle()) {
				if (v->group_id != id_g) continue;

				/* For each shared vehicles add it to the group */
				for (Vehicle *v2 = v->FirstShared(); v2 != nullptr; v2 = v2->NextShared()) {
					if (v2->group_id != id_g) Command<CMD_ADD_VEHICLE_GROUP>::Do(flags, id_g, v2->index, false, VehicleListIdentifier{});
				}
			}
		}

		InvalidateWindowData(GetWindowClassForVehicleType(type), VehicleListIdentifier(VL_GROUP_LIST, type, _current_company).Pack());
	}

	return CommandCost();
}


/**
 * Remove all vehicles from a group
 * @param flags type of operation
 * @param group_id index of group
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveAllVehiclesGroup(DoCommandFlag flags, GroupID group_id)
{
	Group *g = Group::GetIfValid(group_id);

	if (g == nullptr || g->owner != _current_company) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Find each Vehicle that belongs to the group old_g and add it to the default group */
		for (const Vehicle *v : Vehicle::Iterate()) {
			if (v->IsPrimaryVehicle()) {
				if (v->group_id != group_id) continue;

				/* Add The Vehicle to the default group */
				Command<CMD_ADD_VEHICLE_GROUP>::Do(flags, DEFAULT_GROUP, v->index, false, VehicleListIdentifier{});
			}
		}

		InvalidateWindowData(GetWindowClassForVehicleType(g->vehicle_type), VehicleListIdentifier(VL_GROUP_LIST, g->vehicle_type, _current_company).Pack());
	}

	return CommandCost();
}

/**
 * Set the livery for a vehicle group.
 * @param flags     Command flags.
 * @param group_id Group ID.
 * @param primary Set primary instead of secondary colour
 * @param colour Colour.
 */
CommandCost CmdSetGroupLivery(DoCommandFlag flags, GroupID group_id, bool primary, Colours colour)
{
	Group *g = Group::GetIfValid(group_id);

	if (g == nullptr || g->owner != _current_company) return CMD_ERROR;

	if (colour >= COLOUR_END && colour != INVALID_COLOUR) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (primary) {
			SB(g->livery.in_use, 0, 1, colour != INVALID_COLOUR);
			if (colour == INVALID_COLOUR) colour = (Colours)GetParentLivery(g)->colour1;
			g->livery.colour1 = colour;
		} else {
			SB(g->livery.in_use, 1, 1, colour != INVALID_COLOUR);
			if (colour == INVALID_COLOUR) colour = (Colours)GetParentLivery(g)->colour2;
			g->livery.colour2 = colour;
		}

		PropagateChildLivery(g, true);
		MarkWholeScreenDirty();
	}

	return CommandCost();
}

/**
 * Set group flag for a group and its sub-groups.
 * @param g initial group.
 * @param set 1 to set or 0 to clear protection.
 */
static void SetGroupFlag(Group *g, GroupFlags flag, bool set, bool children)
{
	if (set) {
		SetBit(g->flags, flag);
	} else {
		ClrBit(g->flags, flag);
	}

	if (!children) return;

	for (Group *pg : Group::Iterate()) {
		if (pg->parent == g->index) SetGroupFlag(pg, flag, set, true);
	}
}

/**
 * (Un)set group flag from a group
 * @param flags type of operation
 * @param group_id index of group array
 * @param flag flag to set, by value not bit.
 * @param value value to set the flag to.
 * @param recursive to apply to sub-groups.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetGroupFlag(DoCommandFlag flags, GroupID group_id, GroupFlags flag, bool value, bool recursive)
{
	Group *g = Group::GetIfValid(group_id);
	if (g == nullptr || g->owner != _current_company) return CMD_ERROR;

	if (flag >= GroupFlags::GF_END) return CMD_ERROR;

	if (flags & DC_EXEC) {
		SetGroupFlag(g, flag, value, recursive);

		SetWindowDirty(GetWindowClassForVehicleType(g->vehicle_type), VehicleListIdentifier(VL_GROUP_LIST, g->vehicle_type, _current_company).Pack());
		InvalidateWindowData(WC_REPLACE_VEHICLE, g->vehicle_type);
	}

	return CommandCost();
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

	for (Vehicle *u = v; u != nullptr; u = u->Next()) {
		if (u->IsEngineCountable()) UpdateNumEngineGroup(u, u->group_id, new_g);

		u->group_id = new_g;
		u->colourmap = PAL_NONE;
		u->InvalidateNewGRFCache();
		u->UpdateViewport(true);
	}

	/* Update the Replace Vehicle Windows */
	GroupStatistics::UpdateAutoreplace(v->owner);
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
	for (Vehicle *u = v; u != nullptr; u = u->Next()) {
		if (u->IsEngineCountable()) UpdateNumEngineGroup(u, u->group_id, new_g);

		u->group_id = new_g;
		u->colourmap = PAL_NONE;
		u->InvalidateNewGRFCache();
	}

	/* Update the Replace Vehicle Windows */
	GroupStatistics::UpdateAutoreplace(v->owner);
	SetWindowDirty(WC_REPLACE_VEHICLE, VEH_TRAIN);
}

/**
 * Get the number of engines with EngineID id_e in the group with GroupID
 * id_g and its sub-groups.
 * @param company The company the group belongs to
 * @param id_g The GroupID of the group used
 * @param id_e The EngineID of the engine to count
 * @return The number of engines with EngineID id_e in the group
 */
uint GetGroupNumEngines(CompanyID company, GroupID id_g, EngineID id_e)
{
	uint count = 0;
	const Engine *e = Engine::Get(id_e);
	for (const Group *g : Group::Iterate()) {
		if (g->parent == id_g) count += GetGroupNumEngines(company, g->index, id_e);
	}
	return count + GroupStatistics::Get(company, id_g, e->type).num_engines[id_e];
}

/**
 * Get the number of vehicles in the group with GroupID
 * id_g and its sub-groups.
 * @param company The company the group belongs to
 * @param id_g The GroupID of the group used
 * @param type The vehicle type of the group
 * @return The number of vehicles in the group
 */
uint GetGroupNumVehicle(CompanyID company, GroupID id_g, VehicleType type)
{
	uint count = 0;
	for (const Group *g : Group::Iterate()) {
		if (g->parent == id_g) count += GetGroupNumVehicle(company, g->index, type);
	}
	return count + GroupStatistics::Get(company, id_g, type).num_vehicle;
}

/**
 * Get the number of vehicles above profit minimum age in the group with GroupID
 * id_g and its sub-groups.
 * @param company The company the group belongs to
 * @param id_g The GroupID of the group used
 * @param type The vehicle type of the group
 * @return The number of vehicles above profit minimum age in the group
 */
uint GetGroupNumVehicleMinAge(CompanyID company, GroupID id_g, VehicleType type)
{
	uint count = 0;
	for (const Group *g : Group::Iterate()) {
		if (g->parent == id_g) count += GetGroupNumVehicleMinAge(company, g->index, type);
	}
	return count + GroupStatistics::Get(company, id_g, type).num_vehicle_min_age;
}

/**
 * Get last year's profit of vehicles above minimum age
 * for the group with GroupID id_g and its sub-groups.
 * @param company The company the group belongs to
 * @param id_g The GroupID of the group used
 * @param type The vehicle type of the group
 * @return Last year's profit of vehicles above minimum age for the group
 */
Money GetGroupProfitLastYearMinAge(CompanyID company, GroupID id_g, VehicleType type)
{
	Money sum = 0;
	for (const Group *g : Group::Iterate()) {
		if (g->parent == id_g) sum += GetGroupProfitLastYearMinAge(company, g->index, type);
	}
	return sum + GroupStatistics::Get(company, id_g, type).profit_last_year_min_age;
}

void RemoveAllGroupsForCompany(const CompanyID company)
{
	for (Group *g : Group::Iterate()) {
		if (company == g->owner) delete g;
	}
}


/**
 * Test if GroupID group is a descendant of (or is) GroupID search
 * @param search The GroupID to search in
 * @param group The GroupID to search for
 * @return True iff group is search or a descendant of search
 */
bool GroupIsInGroup(GroupID search, GroupID group)
{
	if (!Group::IsValidID(search)) return search == group;

	do {
		if (search == group) return true;
		search = Group::Get(search)->parent;
	} while (search != INVALID_GROUP);

	return false;
}
