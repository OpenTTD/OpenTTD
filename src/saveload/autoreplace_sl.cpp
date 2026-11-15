/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autoreplace_sl.cpp Code handling saving and loading of autoreplace rules. */

#include "../stdafx.h"

#include "saveload.h"
#include "saveload_internal.h"
#include "compat/autoreplace_sl_compat.h"

#include "../autoreplace_type.h"

#include "../safeguards.h"

/** Temporary storage for conversion from old engine renew pool. */
OldEngineRenewPool _old_engine_renew_saveload_pool;

struct ERNWChunkHandler : ChunkHandler {
	ERNWChunkHandler() : ChunkHandler("ERNW", ChunkType::ReadOnly) {}

	/** Saveload description for handler. */
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::U16>("from", SLE_OBJECT_ADDRESS(OldEngineRenewPool::Item, value.from)),
		SaveLoad::Variable<VarFileType::U16>("to", SLE_OBJECT_ADDRESS(OldEngineRenewPool::Item, value.to)),
		SaveLoad::Variable<VarFileType::U16>("next", SLE_OBJECT_ADDRESS(OldEngineRenewPool::Item, next), SaveLoadVersion::MinVersion, SaveLoadVersion::MoreCargoPackets),
		SaveLoad::Variable<VarFileType::U32>("next", SLE_OBJECT_ADDRESS(OldEngineRenewPool::Item, next), SaveLoadVersion::MoreCargoPackets),
		SaveLoad::Variable<VarFileType::U16>("group_id", SLE_OBJECT_ADDRESS(OldEngineRenewPool::Item, value.group_id), SaveLoadVersion::VehicleGroups),
		SaveLoad::Variable<VarFileType::Bool>("replace_when_old", SLE_OBJECT_ADDRESS(OldEngineRenewPool::Item, value.replace_when_old), SaveLoadVersion::AutoreplaceWhenOldTreeLimit),
	};

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(description, _engine_renew_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			auto &item = _old_engine_renew_saveload_pool.Allocate(index);
			SlObject(&item, slt);

			/* Advanced vehicle lists, ungrouped vehicles got added */
			if (IsSavegameVersionBefore(SaveLoadVersion::VehicleGroups)) {
				item.value.group_id = ALL_GROUP;
			} else if (IsSavegameVersionBefore(SaveLoadVersion::UngroupedVehicles)) {
				if (item.value.group_id == DEFAULT_GROUP) item.value.group_id = ALL_GROUP;
			}
			/* Since SaveLoadVersion::EngineRenewsOwnership like-for-like replacements are always explicitly replace when old. */
			if (item.value.from == item.value.to) item.value.replace_when_old = true;
		}
	}
};

static const ERNWChunkHandler ERNW;
static const ChunkHandlerRef autoreplace_chunk_handlers[] = {
	ERNW,
};

extern const ChunkHandlerTable _autoreplace_chunk_handlers(autoreplace_chunk_handlers);
