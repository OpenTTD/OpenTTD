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
	    SLE_VAR(Subsidy, cargo_type, VarTypes::U8),
	SLE_CONDVAR(Subsidy, remaining, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::CustomSubsidyDuration),
	SLE_CONDVAR(Subsidy, remaining, VarTypes::U16, SaveLoadVersion::CustomSubsidyDuration, SaveLoadVersion::MaxVersion),
	SLE_CONDVAR(Subsidy, awarded, VarTypes::U8, SaveLoadVersion::RemoveSubsidyStationBinding, SaveLoadVersion::MaxVersion),
	SLE_CONDVARNAME(Subsidy, src.type, "src_type", VarTypes::U8, SaveLoadVersion::RemoveSubsidyStationBinding, SaveLoadVersion::MaxVersion),
	SLE_CONDVARNAME(Subsidy, dst.type, "dst_type", VarTypes::U8, SaveLoadVersion::RemoveSubsidyStationBinding, SaveLoadVersion::MaxVersion),
	SLE_CONDVARNAME(Subsidy, src.id, "src", VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
	SLE_CONDVARNAME(Subsidy, src.id, "src", VarTypes::U16, SaveLoadVersion::BigMap, SaveLoadVersion::MaxVersion),
	SLE_CONDVARNAME(Subsidy, dst.id, "dst", VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
	SLE_CONDVARNAME(Subsidy, dst.id, "dst", VarTypes::U16, SaveLoadVersion::BigMap, SaveLoadVersion::MaxVersion),
};

struct SUBSChunkHandler : ChunkHandler {
	SUBSChunkHandler() : ChunkHandler('SUBS', ChunkType::Table) {}

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
