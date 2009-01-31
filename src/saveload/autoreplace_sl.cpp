/* $Id$ */

/** @file autoreplace_sl.cpp Code handling saving and loading of autoreplace rules */

#include "../stdafx.h"
#include "../engine_type.h"
#include "../group_type.h"
#include "../autoreplace_base.h"

#include "saveload.h"

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
