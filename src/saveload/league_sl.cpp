/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file league_sl.cpp Code handling saving and loading of league tables */

#include "../stdafx.h"

#include "saveload.h"

#include "../league_base.h"

#include "../safeguards.h"

static const SaveLoad _league_table_elements_desc[] = {
	    SLE_VAR(LeagueTableElement, table,       SLE_UINT8),
	SLE_CONDVAR(LeagueTableElement, rating,      SLE_FILE_U64 | SLE_VAR_I64, SL_MIN_VERSION, SLV_LINKGRAPH_EDGES),
	SLE_CONDVAR(LeagueTableElement, rating,      SLE_INT64,                  SLV_LINKGRAPH_EDGES, SL_MAX_VERSION),
	    SLE_VAR(LeagueTableElement, company,     SLE_UINT8),
	   SLE_SSTR(LeagueTableElement, text,        SLE_STR | SLF_ALLOW_CONTROL),
	   SLE_SSTR(LeagueTableElement, score,       SLE_STR | SLF_ALLOW_CONTROL),
	    SLE_VAR(LeagueTableElement, link.type,   SLE_UINT8),
	    SLE_VAR(LeagueTableElement, link.target, SLE_UINT32),
};

struct LEAEChunkHandler : ChunkHandler {
	LEAEChunkHandler() : ChunkHandler('LEAE', CH_TABLE) {}

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
			LeagueTableElement *lte = new (index) LeagueTableElement();
			SlObject(lte, slt);
		}
	}
};

static const SaveLoad _league_tables_desc[] = {
	SLE_SSTR(LeagueTable, title, SLE_STR | SLF_ALLOW_CONTROL),
	SLE_SSTR(LeagueTable, header, SLE_STR | SLF_ALLOW_CONTROL),
	SLE_SSTR(LeagueTable, footer, SLE_STR | SLF_ALLOW_CONTROL),
};

struct LEATChunkHandler : ChunkHandler {
	LEATChunkHandler() : ChunkHandler('LEAT', CH_TABLE) {}

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
			LeagueTable *lt = new (index) LeagueTable();
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
