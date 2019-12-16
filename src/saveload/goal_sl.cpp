/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal_sl.cpp Code handling saving and loading of goals */

#include "../stdafx.h"
#include "../goal_base.h"

#include "saveload.h"

#include "../safeguards.h"

static const SaveLoad _goals_desc[] = {
	    SLE_VAR(Goal, company,   SLE_FILE_U16 | SLE_VAR_U8),
	    SLE_VAR(Goal, type,      SLE_FILE_U16 | SLE_VAR_U8),
	    SLE_VAR(Goal, dst,       SLE_UINT32),
	    SLE_STR(Goal, text,      SLE_STR | SLF_ALLOW_CONTROL, 0),
	SLE_CONDSTR(Goal, progress,  SLE_STR | SLF_ALLOW_CONTROL, 0, SLV_182, SL_MAX_VERSION),
	SLE_CONDVAR(Goal, completed, SLE_BOOL, SLV_182, SL_MAX_VERSION),
	    SLE_END()
};

static void Save_GOAL()
{
	for (Goal *s : Goal::Iterate()) {
		SlSetArrayIndex(s->index);
		SlObject(s, _goals_desc);
	}
}

static void Load_GOAL()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Goal *s = new (index) Goal();
		SlObject(s, _goals_desc);
	}
}

extern const ChunkHandler _goal_chunk_handlers[] = {
	{ 'GOAL', Save_GOAL, Load_GOAL, nullptr, nullptr, CH_ARRAY | CH_LAST},
};
