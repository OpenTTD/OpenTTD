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
	    SLE_VAR(LeagueTableElement, table,       VarTypes::U8),
	SLE_CONDVAR(LeagueTableElement, rating, VarFileType::U64 | VarMemType::I64, SaveLoadVersion::MinVersion, SaveLoadVersion::LinkgraphEdges),
	SLE_CONDVAR(LeagueTableElement, rating, VarTypes::I64, SaveLoadVersion::LinkgraphEdges, SaveLoadVersion::MaxVersion),
	    SLE_VAR(LeagueTableElement, company,     VarTypes::U8),
	   SLE_SSTR(LeagueTableElement, text,        VarTypes::STR | StringValidationSetting::AllowControlCode),
	   SLE_SSTR(LeagueTableElement, score,       VarTypes::STR | StringValidationSetting::AllowControlCode),
	    SLE_VAR(LeagueTableElement, link.type,   VarTypes::U8),
	    SLE_VAR(LeagueTableElement, link.target, VarTypes::U32),
};

struct LEAEChunkHandler : ChunkHandler {
	LEAEChunkHandler() : ChunkHandler('LEAE', ChunkType::Table) {}

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
	SLE_SSTR(LeagueTable, title, VarTypes::STR | StringValidationSetting::AllowControlCode),
	SLE_SSTR(LeagueTable, header, VarTypes::STR | StringValidationSetting::AllowControlCode),
	SLE_SSTR(LeagueTable, footer, VarTypes::STR | StringValidationSetting::AllowControlCode),
};

struct LEATChunkHandler : ChunkHandler {
	LEATChunkHandler() : ChunkHandler('LEAT', ChunkType::Table) {}

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
