/* $Id$ */

/** @file group.h */

#ifndef GROUP_H
#define GROUP_H

#include "oldpool.h"

enum {
	DEFAULT_GROUP = 0xFFFE,
	INVALID_GROUP = 0xFFFF,
};

struct Group {
	StringID string_id;                     ///< Group Name

	uint16 num_vehicle;                     ///< Number of vehicles wich belong to the group
	PlayerID owner;                         ///< Group Owner
	GroupID index;                          ///< Array index
	VehicleTypeByte vehicle_type;           ///< Vehicle type of the group

	bool replace_protection;                ///< If set to true, the global autoreplace have no effect on the group
	uint16 num_engines[TOTAL_NUM_ENGINES];  ///< Caches the number of engines of each type the player owns (no need to save this)
};

DECLARE_OLD_POOL(Group, Group, 5, 2047)


static inline bool IsValidGroup(const Group *g)
{
	return g->string_id != STR_NULL;
}

static inline void DestroyGroup(Group *g)
{
	DeleteName(g->string_id);
}

static inline void DeleteGroup(Group *g)
{
	DestroyGroup(g);
	g->string_id = STR_NULL;
}

static inline bool IsValidGroupID(GroupID index)
{
	return index < GetGroupPoolSize() && IsValidGroup(GetGroup(index));
}

static inline bool IsDefaultGroupID(GroupID index)
{
	return (index == DEFAULT_GROUP);
}

#define FOR_ALL_GROUPS_FROM(g, start) for (g = GetGroup(start); g != NULL; g = (g->index + 1U < GetGroupPoolSize()) ? GetGroup(g->index + 1) : NULL) if (IsValidGroup(g))
#define FOR_ALL_GROUPS(g) FOR_ALL_GROUPS_FROM(g, 0)

/**
 * Get the current size of the GroupPool
 */
static inline uint GetGroupArraySize(void)
{
	const Group *g;
	uint num = 0;

	FOR_ALL_GROUPS(g) num++;

	return num;
}

static inline void IncreaseGroupNumVehicle(GroupID id_g)
{
	if (IsValidGroupID(id_g)) GetGroup(id_g)->num_vehicle++;
}

static inline void DecreaseGroupNumVehicle(GroupID id_g)
{
	if (IsValidGroupID(id_g)) GetGroup(id_g)->num_vehicle--;
}


void InitializeGroup();
void SetTrainGroupID(Vehicle *v, GroupID grp);
void UpdateTrainGroupID(Vehicle *v);
void RemoveVehicleFromGroup(const Vehicle *v);
void RemoveAllGroupsForPlayer(const Player *p);

#endif /* GROUP_H */
