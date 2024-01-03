/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cheat_sl.cpp Code handling saving and loading of cheats */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/cheat_sl_compat.h"

#include "../cheat_type.h"

#include "../safeguards.h"

static const SaveLoad _cheats_desc[] = {
	SLE_VAR(Cheats, magic_bulldozer.been_used, SLE_BOOL),
	SLE_VAR(Cheats, magic_bulldozer.value, SLE_BOOL),
	SLE_VAR(Cheats, switch_company.been_used, SLE_BOOL),
	SLE_VAR(Cheats, switch_company.value, SLE_BOOL),
	SLE_VAR(Cheats, money.been_used, SLE_BOOL),
	SLE_VAR(Cheats, money.value, SLE_BOOL),
	SLE_VAR(Cheats, crossing_tunnels.been_used, SLE_BOOL),
	SLE_VAR(Cheats, crossing_tunnels.value, SLE_BOOL),
	SLE_VAR(Cheats, no_jetcrash.been_used, SLE_BOOL),
	SLE_VAR(Cheats, no_jetcrash.value, SLE_BOOL),
	SLE_VAR(Cheats, change_date.been_used, SLE_BOOL),
	SLE_VAR(Cheats, change_date.value, SLE_BOOL),
	SLE_VAR(Cheats, setup_prod.been_used, SLE_BOOL),
	SLE_VAR(Cheats, setup_prod.value, SLE_BOOL),
	SLE_VAR(Cheats, edit_max_hl.been_used, SLE_BOOL),
	SLE_VAR(Cheats, edit_max_hl.value, SLE_BOOL),
	SLE_CONDVAR(Cheats, station_rating.been_used, SLE_BOOL, SLV_STATION_RATING_CHEAT, SL_MAX_VERSION),
	SLE_CONDVAR(Cheats, station_rating.value, SLE_BOOL, SLV_STATION_RATING_CHEAT, SL_MAX_VERSION),
};


struct CHTSChunkHandler : ChunkHandler {
	CHTSChunkHandler() : ChunkHandler('CHTS', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_cheats_desc);

		SlSetArrayIndex(0);
		SlObject(&_cheats, _cheats_desc);
	}

	void Load() const override
	{
		std::vector<SaveLoad> slt = SlCompatTableHeader(_cheats_desc, _cheats_sl_compat);

		if (IsSavegameVersionBefore(SLV_TABLE_CHUNKS)) {
			size_t count = SlGetFieldLength();
			std::vector<SaveLoad> oslt;

			/* Cheats were added over the years without a savegame bump. They are
			 * stored as 2 SLE_BOOLs per entry. "count" indicates how many SLE_BOOLs
			 * are stored for this savegame. So read only "count" SLE_BOOLs (and in
			 * result "count / 2" cheats). */
			for (auto &sld : slt) {
				count--;
				oslt.push_back(sld);

				if (count == 0) break;
			}
			slt = oslt;
		}

		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() == -1) return;
		SlObject(&_cheats, slt);
		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() != -1) SlErrorCorrupt("Too many CHTS entries");
	}
};

static const CHTSChunkHandler CHTS;
static const ChunkHandlerRef cheat_chunk_handlers[] = {
	CHTS,
};

extern const ChunkHandlerTable _cheat_chunk_handlers(cheat_chunk_handlers);
