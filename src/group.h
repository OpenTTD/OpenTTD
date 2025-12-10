/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file group.h Base class for groups and group functions. */

#ifndef GROUP_H
#define GROUP_H

#include "group_type.h"
#include "core/flatset_type.hpp"
#include "core/pool_type.hpp"
#include "company_type.h"
#include "vehicle_type.h"
#include "engine_type.h"
#include "livery.h"

using GroupPool = Pool<Group, GroupID, 16>;
extern GroupPool _group_pool; ///< Pool of groups.

/** Statistics and caches on the vehicles in a group. */
struct GroupStatistics {
	Money profit_last_year = 0; ///< Sum of profits for all vehicles.
	Money profit_last_year_min_age = 0; ///< Sum of profits for vehicles considered for profit statistics.
	std::map<EngineID, uint16_t> num_engines{}; ///< Caches the number of engines of each type the company owns.
	uint16_t num_vehicle = 0; ///< Number of vehicles.
	uint16_t num_vehicle_min_age = 0; ///< Number of vehicles considered for profit statistics;
	bool autoreplace_defined = false; ///< Are any autoreplace rules set?
	bool autoreplace_finished = false; ///< Have all autoreplacement finished?

	void Clear();

	void ClearProfits()
	{
		this->profit_last_year = 0;

		this->num_vehicle_min_age = 0;
		this->profit_last_year_min_age = 0;
	}

	void ClearAutoreplace()
	{
		this->autoreplace_defined = false;
		this->autoreplace_finished = false;
	}

	uint16_t GetNumEngines(EngineID engine) const;

	static GroupStatistics &Get(CompanyID company, GroupID id_g, VehicleType type);
	static GroupStatistics &Get(const Vehicle *v);
	static GroupStatistics &GetAllGroup(const Vehicle *v);

	static void CountVehicle(const Vehicle *v, int delta);
	static void CountEngine(const Vehicle *v, int delta);
	static void AddProfitLastYear(const Vehicle *v);
	static void VehicleReachedMinAge(const Vehicle *v);

	static void UpdateProfits();
	static void UpdateAfterLoad();
	static void UpdateAutoreplace(CompanyID company);
};

enum class GroupFlag : uint8_t {
	ReplaceProtection = 0, ///< If set, the global autoreplace has no effect on the group
	ReplaceWagonRemoval = 1, ///< If set, autoreplace will perform wagon removal on vehicles in this group.
};
using GroupFlags = EnumBitSet<GroupFlag, uint8_t>;

/** Group data. */
struct Group : GroupPool::PoolItem<&_group_pool> {
	std::string name{}; ///< Group Name
	Owner owner = INVALID_OWNER; ///< Group Owner
	VehicleType vehicle_type = VEH_INVALID; ///< Vehicle type of the group

	GroupFlags flags{}; ///< Group flags
	Livery livery{}; ///< Custom colour scheme for vehicles in this group
	GroupStatistics statistics{}; ///< NOSAVE: Statistics and caches on the vehicles in the group.

	FlatSet<GroupID> children; ///< NOSAVE: child groups belonging to this group.
	bool folded = false; ///< NOSAVE: Is this group folded in the group view?

	GroupID parent = GroupID::Invalid(); ///< Parent group
	uint16_t number = 0; ///< Per-company group number.

	Group() {}
	Group(CompanyID owner, VehicleType vehicle_type) : owner(owner), vehicle_type(vehicle_type) {}
};


inline bool IsDefaultGroupID(GroupID index)
{
	return index == DEFAULT_GROUP;
}

/**
 * Checks if a GroupID stands for all vehicles of a company
 * @param id_g The GroupID to check
 * @return true is id_g is identical to ALL_GROUP
 */
inline bool IsAllGroupID(GroupID id_g)
{
	return id_g == ALL_GROUP;
}


void UpdateGroupChildren();
uint GetGroupNumEngines(CompanyID company, GroupID id_g, EngineID id_e);
uint GetGroupNumVehicle(CompanyID company, GroupID id_g, VehicleType type);
uint GetGroupNumVehicleMinAge(CompanyID company, GroupID id_g, VehicleType type);
Money GetGroupProfitLastYearMinAge(CompanyID company, GroupID id_g, VehicleType type);

void SetTrainGroupID(Train *v, GroupID grp);
void UpdateTrainGroupID(Train *v);
void RemoveAllGroupsForCompany(const CompanyID company);
bool GroupIsInGroup(GroupID search, GroupID group);
void UpdateCompanyGroupLiveries(const Company *c);

#endif /* GROUP_H */
