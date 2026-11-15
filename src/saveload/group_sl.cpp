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
#include "saveload_internal.h"
#include "compat/group_sl_compat.h"

#include "../safeguards.h"

/** Saveload handler for group engine renew entries. */
class SlGroupEngineRenews : public VectorSaveLoadHandler<SlGroupEngineRenews, Group, EngineRenew> {
public:
	/** Saveload description for handler. Definition common with \c SlCompanyEngineRenews */
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::U16>("old_engine", SLE_OBJECT_ADDRESS(EngineRenew, from)),
		SaveLoad::Variable<VarFileType::U16>("new_engine", SLE_OBJECT_ADDRESS(EngineRenew, to)),
		SaveLoad::Variable<VarFileType::Bool>("replace_when_old", SLE_OBJECT_ADDRESS(EngineRenew, replace_when_old)),
	};
	/** Compatibility saveload description for handler. */
	static inline const SaveLoadCompatTable compat_description = {};

	std::vector<EngineRenew> &GetVector(Group *g) const override { return g->engine_renew_list; }
};

static const SaveLoad _group_desc[] = {
	SaveLoad::Variable<VarFileType::StringID>("name", SLE_OBJECT_ADDRESS(Group, name), SaveLoadVersion::MinVersion, SaveLoadVersion::ReplaceCustomNameArray),
	SaveLoad::String("name", SLE_OBJECT_ADDRESS(Group, name), StringValidationSetting::AllowControlCode, SaveLoadVersion::ReplaceCustomNameArray),
	SaveLoad::Variable<VarFileType::U8>("owner", SLE_OBJECT_ADDRESS(Group, owner)),
	SaveLoad::Variable<VarFileType::U8>("vehicle_type", SLE_OBJECT_ADDRESS(Group, vehicle_type)),
	SaveLoad::Variable<VarFileType::U8>("flags", SLE_OBJECT_ADDRESS(Group, flags)),
	SaveLoad::Variable<VarFileType::U8>("livery.in_use", SLE_OBJECT_ADDRESS(Group, livery.in_use), SaveLoadVersion::GroupLiveries),
	SaveLoad::Variable<VarFileType::U8>("livery.colour1", SLE_OBJECT_ADDRESS(Group, livery.colour1), SaveLoadVersion::GroupLiveries),
	SaveLoad::Variable<VarFileType::U8>("livery.colour2", SLE_OBJECT_ADDRESS(Group, livery.colour2), SaveLoadVersion::GroupLiveries),
	SaveLoad::Variable<VarFileType::U16>("parent", SLE_OBJECT_ADDRESS(Group, parent), SaveLoadVersion::GroupHierarchy),
	SaveLoad::Variable<VarFileType::U16>("number", SLE_OBJECT_ADDRESS(Group, number), SaveLoadVersion::GroupNumbers),
	SaveLoad::StructList<SlGroupEngineRenews>("engine_renew_list", SaveLoadVersion::EngineRenewsOwnership),
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

	void FixPointers() const override
	{
		for (Group *g : Group::Iterate()) {
			SlObject(g, _group_desc);

			if (IsSavegameVersionBefore(SaveLoadVersion::EngineRenewsOwnership)) {
				Company *c = Company::Get(g->owner);
				/* Migrate engine renews for this group. */
				_old_engine_renew_saveload_pool.TransformItems(c->old_engine_renew_index, [g](const auto &item) {
					if (item.group_id == g->index) g->engine_renew_list.emplace_back(item);
				});
				/* Sort by from-engine as lookups use binary-search. */
				std::ranges::sort(g->engine_renew_list, std::less{}, &EngineRenew::from);
			}
		}
	}
};

static const GRPSChunkHandler GRPS;
static const ChunkHandlerRef group_chunk_handlers[] = {
	GRPS,
};

extern const ChunkHandlerTable _group_chunk_handlers(group_chunk_handlers);
