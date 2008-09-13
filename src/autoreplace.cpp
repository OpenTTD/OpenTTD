/* $Id$ */

/** @file autoreplace.cpp Management of replacement lists. */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "command_func.h"
#include "saveload.h"
#include "group.h"
#include "autoreplace_base.h"
#include "oldpool_func.h"

DEFINE_OLD_POOL_GENERIC(EngineRenew, EngineRenew)

/**
 * Retrieves the EngineRenew that specifies the replacement of the given
 * engine type from the given renewlist */
static EngineRenew *GetEngineReplacement(EngineRenewList erl, EngineID engine, GroupID group)
{
	EngineRenew *er = (EngineRenew *)erl;

	while (er) {
		if (er->from == engine && er->group_id == group) return er;
		er = er->next;
	}
	return NULL;
}

void RemoveAllEngineReplacement(EngineRenewList *erl)
{
	EngineRenew *er = (EngineRenew *)(*erl);
	EngineRenew *next;

	while (er != NULL) {
		next = er->next;
		delete er;
		er = next;
	}
	*erl = NULL; // Empty list
}

EngineID EngineReplacement(EngineRenewList erl, EngineID engine, GroupID group)
{
	const EngineRenew *er = GetEngineReplacement(erl, engine, group);
	if (er == NULL && (group == DEFAULT_GROUP || (IsValidGroupID(group) && !GetGroup(group)->replace_protection))) {
		/* We didn't find anything useful in the vehicle's own group so we will try ALL_GROUP */
		er = GetEngineReplacement(erl, engine, ALL_GROUP);
	}
	return er == NULL ? INVALID_ENGINE : er->to;
}

CommandCost AddEngineReplacement(EngineRenewList *erl, EngineID old_engine, EngineID new_engine, GroupID group, uint32 flags)
{
	EngineRenew *er;

	/* Check if the old vehicle is already in the list */
	er = GetEngineReplacement(*erl, old_engine, group);
	if (er != NULL) {
		if (flags & DC_EXEC) er->to = new_engine;
		return CommandCost();
	}

	if (!EngineRenew::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		er = new EngineRenew(old_engine, new_engine);
		er->group_id = group;

		/* Insert before the first element */
		er->next = (EngineRenew *)(*erl);
		*erl = (EngineRenewList)er;
	}

	return CommandCost();
}

CommandCost RemoveEngineReplacement(EngineRenewList *erl, EngineID engine, GroupID group, uint32 flags)
{
	EngineRenew *er = (EngineRenew *)(*erl);
	EngineRenew *prev = NULL;

	while (er)
	{
		if (er->from == engine && er->group_id == group) {
			if (flags & DC_EXEC) {
				if (prev == NULL) { // First element
					/* The second becomes the new first element */
					*erl = (EngineRenewList)er->next;
				} else {
					/* Cut this element out */
					prev->next = er->next;
				}
				delete er;
			}
			return CommandCost();
		}
		prev = er;
		er = er->next;
	}

	return CMD_ERROR;
}

static const SaveLoad _engine_renew_desc[] = {
	    SLE_VAR(EngineRenew, from,     SLE_UINT16),
	    SLE_VAR(EngineRenew, to,       SLE_UINT16),

	    SLE_REF(EngineRenew, next,     REF_ENGINE_RENEWS),
	SLE_CONDVAR(EngineRenew, group_id, SLE_UINT16, 60, SL_MAX_VERSION),
	SLE_END()
};

static void Save_ERNW()
{
	EngineRenew *er;

	FOR_ALL_ENGINE_RENEWS(er) {
		SlSetArrayIndex(er->index);
		SlObject(er, _engine_renew_desc);
	}
}

static void Load_ERNW()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		EngineRenew *er = new (index) EngineRenew();
		SlObject(er, _engine_renew_desc);

		/* Advanced vehicle lists, ungrouped vehicles got added */
		if (CheckSavegameVersion(60)) {
			er->group_id = ALL_GROUP;
		} else if (CheckSavegameVersion(71)) {
			if (er->group_id == DEFAULT_GROUP) er->group_id = ALL_GROUP;
		}
	}
}

extern const ChunkHandler _autoreplace_chunk_handlers[] = {
	{ 'ERNW', Save_ERNW,     Load_ERNW,     CH_ARRAY | CH_LAST},
};

void InitializeEngineRenews()
{
	/* Clean the engine renew pool and create 1 block in it */
	_EngineRenew_pool.CleanPool();
	_EngineRenew_pool.AddBlockToPool();
}
