/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group.h Base class for groups and group functions. */

#ifndef GROUP_H
#define GROUP_H

#include "group_type.h"
#include "core/pool_type.hpp"
#include "company_type.h"
#include "vehicle_type.h"
#include "engine_type.h"
#include "livery.h"
#include <string>

typedef Pool<Group, GroupID, 16, 64000> GroupPool;
extern GroupPool _group_pool; ///< Pool of groups.

/** Statistics and caches on the vehicles in a group. */
struct GroupStatistics {
	uint16 num_vehicle;                     ///< Number of vehicles.
	uint16 *num_engines;                    ///< Caches the number of engines of each type the company owns.

	bool autoreplace_defined;               ///< Are any autoreplace rules set?
	bool autoreplace_finished;              ///< Have all autoreplacement finished?

	uint16 num_profit_vehicle;              ///< Number of vehicles considered for profit statistics;
	Money profit_last_year;                 ///< Sum of profits for all vehicles.

	GroupStatistics();
	~GroupStatistics();

	void Clear();

	void ClearProfits()
	{
		this->num_profit_vehicle = 0;
		this->profit_last_year = 0;
	}

	void ClearAutoreplace()
	{
		this->autoreplace_defined = false;
		this->autoreplace_finished = false;
	}

	static GroupStatistics &Get(CompanyID company, GroupID id_g, VehicleType type);
	static GroupStatistics &Get(const Vehicle *v);
	static GroupStatistics &GetAllGroup(const Vehicle *v);

	static void CountVehicle(const Vehicle *v, int delta);
	static void CountEngine(const Vehicle *v, int delta);
	static void VehicleReachedProfitAge(const Vehicle *v);

	static void UpdateProfits();
	static void UpdateAfterLoad();
	static void UpdateAutoreplace(CompanyID company);
};

enum GroupFlags : uint8 {
	GF_REPLACE_PROTECTION,    ///< If set to true, the global autoreplace has no effect on the group
	GF_REPLACE_WAGON_REMOVAL, ///< If set, autoreplace will perform wagon removal on vehicles in this group.
	GF_END,
};

/** Group data. */
struct Group : GroupPool::PoolItem<&_group_pool> {
	std::string name;           ///< Group Name
	Owner owner;                ///< Group Owner
	VehicleType vehicle_type;   ///< Vehicle type of the group

	uint8 flags;                ///< Group flags
	Livery livery;              ///< Custom colour scheme for vehicles in this group
	GroupStatistics statistics; ///< NOSAVE: Statistics and caches on the vehicles in the group.

	bool folded;                ///< NOSAVE: Is this group folded in the group view?

	GroupID parent;             ///< Parent group

	Group(CompanyID owner = INVALID_COMPANY);
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


uint GetGroupNumEngines(CompanyID company, GroupID id_g, EngineID id_e);
uint GetGroupNumVehicle(CompanyID company, GroupID id_g, VehicleType type);
uint GetGroupNumProfitVehicle(CompanyID company, GroupID id_g, VehicleType type);
Money GetGroupProfitLastYear(CompanyID company, GroupID id_g, VehicleType type);

void SetTrainGroupID(Train *v, GroupID grp);
void UpdateTrainGroupID(Train *v);
void RemoveVehicleFromGroup(const Vehicle *v);
void RemoveAllGroupsForCompany(const CompanyID company);
bool GroupIsInGroup(GroupID search, GroupID group);

extern GroupID _new_group_id;

#endif /* GROUP_H */
