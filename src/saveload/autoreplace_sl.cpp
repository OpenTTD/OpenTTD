/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_sl.cpp Code handling saving and loading of autoreplace rules */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/autoreplace_sl_compat.h"

#include "../autoreplace_base.h"

#include "../safeguards.h"

static const SaveLoad _engine_renew_desc[] = {
	    SLE_VAR(EngineRenew, from,     SLE_UINT16),
	    SLE_VAR(EngineRenew, to,       SLE_UINT16),

	    SLE_REF(EngineRenew, next,     REF_ENGINE_RENEWS),
	SLE_CONDVAR(EngineRenew, group_id, SLE_UINT16, SLV_60, SL_MAX_VERSION),
	SLE_CONDVAR(EngineRenew, replace_when_old, SLE_BOOL, SLV_175, SL_MAX_VERSION),
};

struct ERNWChunkHandler : ChunkHandler {
	ERNWChunkHandler() : ChunkHandler('ERNW', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_engine_renew_desc);

		for (EngineRenew *er : EngineRenew::Iterate()) {
			SlSetArrayIndex(er->index);
			SlObject(er, _engine_renew_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_engine_renew_desc, _engine_renew_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			EngineRenew *er = new (index) EngineRenew();
			SlObject(er, slt);

			/* Advanced vehicle lists, ungrouped vehicles got added */
			if (IsSavegameVersionBefore(SLV_60)) {
				er->group_id = ALL_GROUP;
			} else if (IsSavegameVersionBefore(SLV_71)) {
				if (er->group_id == DEFAULT_GROUP) er->group_id = ALL_GROUP;
			}
		}
	}

	void FixPointers() const override
	{
		for (EngineRenew *er : EngineRenew::Iterate()) {
			SlObject(er, _engine_renew_desc);
		}
	}
};

static const ERNWChunkHandler ERNW;
static const ChunkHandlerRef autoreplace_chunk_handlers[] = {
	ERNW,
};

extern const ChunkHandlerTable _autoreplace_chunk_handlers(autoreplace_chunk_handlers);
