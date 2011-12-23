/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal.cpp Handling of goals. */

#include "stdafx.h"
#include "company_func.h"
#include "industry.h"
#include "town.h"
#include "news_func.h"
#include "ai/ai.hpp"
#include "station_base.h"
#include "cargotype.h"
#include "strings_func.h"
#include "window_func.h"
#include "goal_base.h"
#include "core/pool_func.hpp"
#include "core/random_func.hpp"
#include "game/game.hpp"
#include "command_func.h"
#include "company_base.h"
#include "string_func.h"

#include "table/strings.h"

GoalID _new_goal_id;

GoalPool _goal_pool("Goal");
INSTANTIATE_POOL_METHODS(Goal)

/**
 * Create a new goal.
 * @param tile unused.
 * @param flags type of operation
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 -  7) - GoalType of destination.
 * - p1 = (bit  8 - 15) - Company for which this goal is.
 * @param p2 GoalTypeID of destination.
 * @param text Text of the goal.
 * @return the cost of this operation or an error
 */
CommandCost CmdCreateGoal(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!Goal::CanAllocateItem()) return CMD_ERROR;

	GoalType type = (GoalType)GB(p1, 0, 8);
	CompanyID company = (CompanyID)GB(p1, 8, 8);

	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (StrEmpty(text)) return CMD_ERROR;
	if (company != INVALID_COMPANY && !Company::IsValidID(company)) return CMD_ERROR;

	switch (type) {
		case GT_NONE:
			if (p2 != 0) return CMD_ERROR;
			break;

		case GT_TILE:
			if (!IsValidTile(p2)) return CMD_ERROR;
			break;

		case GT_INDUSTRY:
			if (!Industry::IsValidID(p2)) return CMD_ERROR;
			break;

		case GT_TOWN:
			if (!Town::IsValidID(p2)) return CMD_ERROR;
			break;

		case GT_COMPANY:
			if (!Company::IsValidID(p2)) return CMD_ERROR;
			break;

		default: return CMD_ERROR;
	}

	if (company != INVALID_OWNER && company != _local_company) return CommandCost();

	if (flags & DC_EXEC) {
		Goal *g = new Goal();
		g->type = type;
		g->dst = p2;
		g->company = company;
		g->text = strdup(text);

		InvalidateWindowData(WC_GOALS_LIST, 0);

		_new_goal_id = g->index;
	}

	return CommandCost();
}

/**
 * Remove a goal.
 * @param tile unused.
 * @param flags type of operation
 * @param p1 GoalID to remove.
 * @param p2 unused.
 * @param text unused.
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveGoal(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(p1)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Goal *g = Goal::Get(p1);
		delete g;

		InvalidateWindowData(WC_GOALS_LIST, 0);
	}

	return CommandCost();
}
