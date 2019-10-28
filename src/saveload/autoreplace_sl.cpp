/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_sl.cpp Code handling saving and loading of autoreplace rules */

#include "../stdafx.h"
#include "../autoreplace_base.h"

#include "saveload.h"

#include "../safeguards.h"

static const SaveLoad _engine_renew_desc[] = {
	    SLE_VAR(EngineRenew, from,     SLE_UINT16),
	    SLE_VAR(EngineRenew, to,       SLE_UINT16),

	    SLE_REF(EngineRenew, next,     REF_ENGINE_RENEWS),
	SLE_CONDVAR(EngineRenew, group_id, SLE_UINT16, SLV_60, SL_MAX_VERSION),
	SLE_CONDVAR(EngineRenew, replace_when_old, SLE_BOOL, SLV_175, SL_MAX_VERSION),
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
		if (IsSavegameVersionBefore(SLV_60)) {
			er->group_id = ALL_GROUP;
		} else if (IsSavegameVersionBefore(SLV_71)) {
			if (er->group_id == DEFAULT_GROUP) er->group_id = ALL_GROUP;
		}
	}
}

static void Ptrs_ERNW()
{
	EngineRenew *er;

	FOR_ALL_ENGINE_RENEWS(er) {
		SlObject(er, _engine_renew_desc);
	}
}

extern const ChunkHandler _autoreplace_chunk_handlers[] = {
	{ 'ERNW', Save_ERNW, Load_ERNW, Ptrs_ERNW, nullptr, CH_ARRAY | CH_LAST},
};
