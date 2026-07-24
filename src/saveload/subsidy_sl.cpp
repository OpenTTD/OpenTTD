/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file subsidy_sl.cpp Code handling saving and loading of subsidies. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/subsidy_sl_compat.h"

#include "../subsidy_base.h"

#include "../safeguards.h"

static const SaveLoad _subsidies_desc[] = {
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Subsidy, cargo_type)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Subsidy, remaining), SaveLoadVersion::MinVersion, SaveLoadVersion::CustomSubsidyDuration),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Subsidy, remaining), SaveLoadVersion::CustomSubsidyDuration),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Subsidy, awarded), SaveLoadVersion::RemoveSubsidyStationBinding),
	SaveLoad::Variable<VarFileType::U8>("src_type", SLE_OBJECT_ADDRESS(Subsidy, src.type), SaveLoadVersion::RemoveSubsidyStationBinding),
	SaveLoad::Variable<VarFileType::U8>("dst_type", SLE_OBJECT_ADDRESS(Subsidy, dst.type), SaveLoadVersion::RemoveSubsidyStationBinding),
	SaveLoad::Variable<VarFileType::U8>("src", SLE_OBJECT_ADDRESS(Subsidy, src.id), SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
	SaveLoad::Variable<VarFileType::U16>("src", SLE_OBJECT_ADDRESS(Subsidy, src.id), SaveLoadVersion::BigMap),
	SaveLoad::Variable<VarFileType::U8>("dst", SLE_OBJECT_ADDRESS(Subsidy, dst.id), SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
	SaveLoad::Variable<VarFileType::U16>("dst", SLE_OBJECT_ADDRESS(Subsidy, dst.id), SaveLoadVersion::BigMap),
};

struct SUBSChunkHandler : ChunkHandler {
	SUBSChunkHandler() : ChunkHandler("SUBS", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_subsidies_desc);

		for (Subsidy *s : Subsidy::Iterate()) {
			SlSetArrayIndex(s->index);
			SlObject(s, _subsidies_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_subsidies_desc, _subsidies_sl_compat);

		int index;
		while ((index = SlIterateArray()) != -1) {
			Subsidy *s = Subsidy::CreateAtIndex(SubsidyID(index));
			SlObject(s, slt);
		}
	}
};

static const SUBSChunkHandler SUBS;
static const ChunkHandlerRef subsidy_chunk_handlers[] = {
	SUBS,
};

extern const ChunkHandlerTable _subsidy_chunk_handlers(subsidy_chunk_handlers);
