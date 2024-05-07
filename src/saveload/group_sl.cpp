/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_sl.cpp Code handling saving and loading of economy data */

#include "../stdafx.h"
#include "../group.h"
#include "../company_base.h"

#include "saveload.h"
#include "compat/group_sl_compat.h"

#include "../safeguards.h"

static const SaveLoad _group_desc[] = {
	 SLE_CONDVAR(Group, name,               SLE_NAME,                       SL_MIN_VERSION,  SLV_84),
	SLE_CONDSSTR(Group, name,               SLE_STR | SLF_ALLOW_CONTROL,    SLV_84, SL_MAX_VERSION),
	     SLE_VAR(Group, owner,              SLE_UINT8),
	     SLE_VAR(Group, vehicle_type,       SLE_UINT8),
	     SLE_VAR(Group, flags,              SLE_UINT8),
	 SLE_CONDVAR(Group, livery.in_use,      SLE_UINT8,                     SLV_GROUP_LIVERIES, SL_MAX_VERSION),
	 SLE_CONDVAR(Group, livery.colour1,     SLE_UINT8,                     SLV_GROUP_LIVERIES, SL_MAX_VERSION),
	 SLE_CONDVAR(Group, livery.colour2,     SLE_UINT8,                     SLV_GROUP_LIVERIES, SL_MAX_VERSION),
	 SLE_CONDVAR(Group, parent,             SLE_UINT16,                    SLV_189, SL_MAX_VERSION),
	 SLE_CONDVAR(Group, number, SLE_UINT16, SLV_GROUP_NUMBERS, SL_MAX_VERSION),
};

struct GRPSChunkHandler : ChunkHandler {
	GRPSChunkHandler() : ChunkHandler('GRPS', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_group_desc);

		for (Group *g : Group::Iterate()) {
			SlSetArrayIndex(g->index);
			SlObject(g, _group_desc);
		}
	}


	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_group_desc, _group_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			Group *g = new (index) Group();
			SlObject(g, slt);

			if (IsSavegameVersionBefore(SLV_189)) g->parent = INVALID_GROUP;
		}
	}
};

static const GRPSChunkHandler GRPS;
static const ChunkHandlerRef group_chunk_handlers[] = {
	GRPS,
};

extern const ChunkHandlerTable _group_chunk_handlers(group_chunk_handlers);
