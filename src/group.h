/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group.h Base class from groups. */

#ifndef GROUP_H
#define GROUP_H

#include "group_type.h"
#include "core/pool_type.hpp"
#include "company_type.h"
#include "vehicle_type.h"
#include "engine_type.h"

typedef Pool<Group, GroupID, 16, 64000> GroupPool;
extern GroupPool _group_pool;

struct Group : GroupPool::PoolItem<&_group_pool> {
	char *name;                             ///< Group Name

	uint16 num_vehicle;                     ///< Number of vehicles wich belong to the group
	OwnerByte owner;                        ///< Group Owner
	VehicleTypeByte vehicle_type;           ///< Vehicle type of the group

	bool replace_protection;                ///< If set to true, the global autoreplace have no effect on the group
	uint16 *num_engines;                    ///< Caches the number of engines of each type the company owns (no need to save this)

	Group(CompanyID owner = INVALID_COMPANY);
	~Group();
};


static inline bool IsDefaultGroupID(GroupID index)
{
	return index == DEFAULT_GROUP;
}

/**
 * Checks if a GroupID stands for all vehicles of a company
 * @param id_g The GroupID to check
 * @return true is id_g is identical to ALL_GROUP
 */
static inline bool IsAllGroupID(GroupID id_g)
{
	return id_g == ALL_GROUP;
}

#define FOR_ALL_GROUPS_FROM(var, start) FOR_ALL_ITEMS_FROM(Group, group_index, var, start)
#define FOR_ALL_GROUPS(var) FOR_ALL_GROUPS_FROM(var, 0)

/**
 * Get the current size of the GroupPool
 */
static inline uint GetGroupArraySize()
{
	const Group *g;
	uint num = 0;

	FOR_ALL_GROUPS(g) num++;

	return num;
}

uint GetGroupNumEngines(CompanyID company, GroupID id_g, EngineID id_e);

static inline void IncreaseGroupNumVehicle(GroupID id_g)
{
	Group *g = Group::GetIfValid(id_g);
	if (g != NULL) g->num_vehicle++;
}

static inline void DecreaseGroupNumVehicle(GroupID id_g)
{
	Group *g = Group::GetIfValid(id_g);
	if (g != NULL) g->num_vehicle--;
}


void InitializeGroup();
void SetTrainGroupID(Train *v, GroupID grp);
void UpdateTrainGroupID(Train *v);
void RemoveVehicleFromGroup(const Vehicle *v);
void RemoveAllGroupsForCompany(const CompanyID company);

extern GroupID _new_group_id;

#endif /* GROUP_H */
