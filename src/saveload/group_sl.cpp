/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file group_sl.cpp Code handling saving and loading of groups. */

#include "../stdafx.h"
#include "../group.h"
#include "../company_base.h"

#include "saveload.h"
#include "compat/group_sl_compat.h"

#include "../safeguards.h"

static const SaveLoad _group_desc[] = {
	SaveLoad::Variable<VarFileType::StringID>(SLE_NAME_AND_OBJECT_ADDRESS(Group, name), SaveLoadVersion::MinVersion, SaveLoadVersion::ReplaceCustomNameArray),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(Group, name), StringValidationSetting::AllowControlCode, SaveLoadVersion::ReplaceCustomNameArray),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Group, owner)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Group, vehicle_type)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Group, flags)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Group, livery.in_use), SaveLoadVersion::GroupLiveries),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Group, livery.colour1), SaveLoadVersion::GroupLiveries),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Group, livery.colour2), SaveLoadVersion::GroupLiveries),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Group, parent), SaveLoadVersion::GroupHierarchy),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Group, number), SaveLoadVersion::GroupNumbers),
};

struct GRPSChunkHandler : ChunkHandler {
	GRPSChunkHandler() : ChunkHandler("GRPS", ChunkType::Table) {}

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
			Group *g = Group::CreateAtIndex(GroupID(index));
			SlObject(g, slt);

			if (IsSavegameVersionBefore(SaveLoadVersion::GroupHierarchy)) g->parent = GroupID::Invalid();
		}
	}
};

static const GRPSChunkHandler GRPS;
static const ChunkHandlerRef group_chunk_handlers[] = {
	GRPS,
};

extern const ChunkHandlerTable _group_chunk_handlers(group_chunk_handlers);
