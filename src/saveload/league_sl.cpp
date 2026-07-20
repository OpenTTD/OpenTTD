/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file league_sl.cpp Code handling saving and loading of league tables. */

#include "../stdafx.h"

#include "saveload.h"

#include "../league_base.h"

#include "../safeguards.h"

static const SaveLoad _league_table_elements_desc[] = {
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, table)),
	SaveLoad::Variable<VarFileType::U64>(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, rating), SaveLoadVersion::MinVersion, SaveLoadVersion::LinkgraphEdges),
	SaveLoad::Variable<VarFileType::I64>(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, rating), SaveLoadVersion::LinkgraphEdges),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, company)),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, text), StringValidationSetting::AllowControlCode),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, score), StringValidationSetting::AllowControlCode),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, link.type)),
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTableElement, link.target)),
};

struct LEAEChunkHandler : ChunkHandler {
	LEAEChunkHandler() : ChunkHandler("LEAE", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_league_table_elements_desc);

		for (LeagueTableElement *lte : LeagueTableElement::Iterate()) {
			SlSetArrayIndex(lte->index);
			SlObject(lte, _league_table_elements_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_league_table_elements_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			LeagueTableElement *lte = LeagueTableElement::CreateAtIndex(LeagueTableElementID(index));
			SlObject(lte, slt);
		}
	}
};

static const SaveLoad _league_tables_desc[] = {
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTable, title), StringValidationSetting::AllowControlCode),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTable, header), StringValidationSetting::AllowControlCode),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(LeagueTable, footer), StringValidationSetting::AllowControlCode),
};

struct LEATChunkHandler : ChunkHandler {
	LEATChunkHandler() : ChunkHandler("LEAT", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_league_tables_desc);

		for (LeagueTable *lt : LeagueTable::Iterate()) {
			SlSetArrayIndex(lt->index);
			SlObject(lt, _league_tables_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_league_tables_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			LeagueTable *lt = LeagueTable::CreateAtIndex(LeagueTableID(index));
			SlObject(lt, slt);
		}
	}
};

static const LEAEChunkHandler LEAE;
static const LEATChunkHandler LEAT;
static const ChunkHandlerRef league_chunk_handlers[] = {
	LEAE,
	LEAT,
};

extern const ChunkHandlerTable _league_chunk_handlers(league_chunk_handlers);
