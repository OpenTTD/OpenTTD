/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file goal_sl.cpp Code handling saving and loading of goals. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/goal_sl_compat.h"

#include "../goal_base.h"

#include "../safeguards.h"

static const SaveLoad _goals_desc[] = {
	     SLE_VAR(Goal, company,   VarFileType::U16 | VarMemType::U8),
	     SLE_VAR(Goal, type,      VarFileType::U16 | VarMemType::U8),
	     SLE_VAR(Goal, dst,       VarTypes::U32),
	    SLE_SSTR(Goal, text,      VarTypes::STR | StringValidationSetting::AllowControlCode),
	SLE_CONDSSTR(Goal, progress, VarTypes::STR | StringValidationSetting::AllowControlCode, SaveLoadVersion::GoalProgressPlaneAcceleration, SaveLoadVersion::MaxVersion),
	 SLE_CONDVAR(Goal, completed, VarTypes::BOOL, SaveLoadVersion::GoalProgressPlaneAcceleration, SaveLoadVersion::MaxVersion),
};

struct GOALChunkHandler : ChunkHandler {
	GOALChunkHandler() : ChunkHandler("GOAL", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_goals_desc);

		for (Goal *s : Goal::Iterate()) {
			SlSetArrayIndex(s->index);
			SlObject(s, _goals_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_goals_desc, _goals_sl_compat);

		int index;
		while ((index = SlIterateArray()) != -1) {
			Goal *s = Goal::CreateAtIndex(GoalID(index));
			SlObject(s, slt);
		}
	}
};

static const GOALChunkHandler GOAL;
static const ChunkHandlerRef goal_chunk_handlers[] = {
	GOAL,
};

extern const ChunkHandlerTable _goal_chunk_handlers(goal_chunk_handlers);
