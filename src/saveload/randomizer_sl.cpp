/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file randomizer_sl.cpp Code handling saving and loading of script randomizers */

#include "../stdafx.h"
#include "../script/api/script_object.hpp"
#include "saveload.h"
#include "saveload_internal.h"
#include "../safeguards.h"

static const SaveLoad _randomizer_desc[] = {
	SLE_VAR(Randomizer, state[0], SLE_UINT32),
	SLE_VAR(Randomizer, state[1], SLE_UINT32),
};

struct SRNDChunkHandler : ChunkHandler {
	SRNDChunkHandler() : ChunkHandler('SRND', CH_TABLE)
	{}

	void Save() const override
	{
		SlTableHeader(_randomizer_desc);

		for (Owner owner = OWNER_BEGIN; owner < OWNER_END; owner++) {
			SlSetArrayIndex(owner);
			SlObject(&ScriptObject::GetRandomizer(owner), _randomizer_desc);
		}
	}

	void Load() const override
	{
		SlTableHeader(_randomizer_desc);

		Owner index;
		while ((index = (Owner)SlIterateArray()) != (Owner)-1) {
			SlObject(&ScriptObject::GetRandomizer(index), _randomizer_desc);
		}
	}
};

static const SRNDChunkHandler SRND;
static const ChunkHandlerRef randomizer_chunk_handlers[] = {
	SRND,
};

extern const ChunkHandlerTable _randomizer_chunk_handlers(randomizer_chunk_handlers);
