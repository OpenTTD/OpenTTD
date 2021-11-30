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
#include "window_func.h"
#include "goal_base.h"
#include "core/pool_func.hpp"
#include "game/game.hpp"
#include "command_func.h"
#include "company_base.h"
#include "story_base.h"
#include "string_func.h"
#include "gui.h"
#include "network/network.h"
#include "network/network_base.h"
#include "network/network_func.h"
#include "goal_cmd.h"

#include "safeguards.h"


GoalPool _goal_pool("Goal");
INSTANTIATE_POOL_METHODS(Goal)

/**
 * Create a new goal.
 * @param flags type of operation
 * @param company Company for which this goal is.
 * @param type GoalType of destination.
 * @param dest GoalTypeID of destination.
 * @param text Text of the goal.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, GoalID> CmdCreateGoal(DoCommandFlag flags, CompanyID company, GoalType type, GoalTypeID dest, const std::string &text)
{
	if (!Goal::CanAllocateItem()) return { CMD_ERROR, INVALID_GOAL };

	if (_current_company != OWNER_DEITY) return { CMD_ERROR, INVALID_GOAL };
	if (text.empty()) return { CMD_ERROR, INVALID_GOAL };
	if (company != INVALID_COMPANY && !Company::IsValidID(company)) return { CMD_ERROR, INVALID_GOAL };

	switch (type) {
		case GT_NONE:
			if (dest != 0) return { CMD_ERROR, INVALID_GOAL };
			break;

		case GT_TILE:
			if (!IsValidTile(dest)) return { CMD_ERROR, INVALID_GOAL };
			break;

		case GT_INDUSTRY:
			if (!Industry::IsValidID(dest)) return { CMD_ERROR, INVALID_GOAL };
			break;

		case GT_TOWN:
			if (!Town::IsValidID(dest)) return { CMD_ERROR, INVALID_GOAL };
			break;

		case GT_COMPANY:
			if (!Company::IsValidID(dest)) return { CMD_ERROR, INVALID_GOAL };
			break;

		case GT_STORY_PAGE: {
			if (!StoryPage::IsValidID(dest)) return { CMD_ERROR, INVALID_GOAL };
			CompanyID story_company = StoryPage::Get(dest)->company;
			if (company == INVALID_COMPANY ? story_company != INVALID_COMPANY : story_company != INVALID_COMPANY && story_company != company) return { CMD_ERROR, INVALID_GOAL };
			break;
		}

		default: return { CMD_ERROR, INVALID_GOAL };
	}

	if (flags & DC_EXEC) {
		Goal *g = new Goal();
		g->type = type;
		g->dst = dest;
		g->company = company;
		g->text = stredup(text.c_str());
		g->progress = nullptr;
		g->completed = false;

		if (g->company == INVALID_COMPANY) {
			InvalidateWindowClassesData(WC_GOALS_LIST);
		} else {
			InvalidateWindowData(WC_GOALS_LIST, g->company);
		}
		if (Goal::GetNumItems() == 1) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);

		return { CommandCost(), g->index };
	}

	return { CommandCost(), INVALID_GOAL };
}

/**
 * Remove a goal.
 * @param flags type of operation
 * @param goal GoalID to remove.
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveGoal(DoCommandFlag flags, GoalID goal)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Goal *g = Goal::Get(goal);
		CompanyID c = g->company;
		delete g;

		if (c == INVALID_COMPANY) {
			InvalidateWindowClassesData(WC_GOALS_LIST);
		} else {
			InvalidateWindowData(WC_GOALS_LIST, c);
		}
		if (Goal::GetNumItems() == 0) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);
	}

	return CommandCost();
}

/**
 * Update goal text of a goal.
 * @param flags type of operation
 * @param goal GoalID to update.
 * @param text Text of the goal.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetGoalText(DoCommandFlag flags, GoalID goal, const std::string &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;
	if (text.empty()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Goal *g = Goal::Get(goal);
		free(g->text);
		g->text = stredup(text.c_str());

		if (g->company == INVALID_COMPANY) {
			InvalidateWindowClassesData(WC_GOALS_LIST);
		} else {
			InvalidateWindowData(WC_GOALS_LIST, g->company);
		}
	}

	return CommandCost();
}

/**
 * Update progress text of a goal.
 * @param flags type of operation
 * @param goal GoalID to update.
 * @param text Progress text of the goal.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetGoalProgress(DoCommandFlag flags, GoalID goal, const std::string &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Goal *g = Goal::Get(goal);
		free(g->progress);
		if (text.empty()) {
			g->progress = nullptr;
		} else {
			g->progress = stredup(text.c_str());
		}

		if (g->company == INVALID_COMPANY) {
			InvalidateWindowClassesData(WC_GOALS_LIST);
		} else {
			InvalidateWindowData(WC_GOALS_LIST, g->company);
		}
	}

	return CommandCost();
}

/**
 * Update completed state of a goal.
 * @param flags type of operation
 * @param goal GoalID to update.
 * @param completed completed state of goal.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetGoalCompleted(DoCommandFlag flags, GoalID goal, bool completed)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Goal *g = Goal::Get(goal);
		g->completed = completed;

		if (g->company == INVALID_COMPANY) {
			InvalidateWindowClassesData(WC_GOALS_LIST);
		} else {
			InvalidateWindowData(WC_GOALS_LIST, g->company);
		}
	}

	return CommandCost();
}

/**
 * Ask a goal related question
 * @param flags type of operation
 * @param uniqueid Unique ID to use for this question.
 * @param target Company or client for which this question is.
 * @param is_client Question target: false - company, true - client.
 * @param button_mask Buttons of the question.
 * @param type Question type.
 * @param text Text of the question.
 * @return the cost of this operation or an error
 */
CommandCost CmdGoalQuestion(DoCommandFlag flags, uint16 uniqueid, uint16 target, bool is_client, uint32 button_mask, GoalQuestionType type, const std::string &text)
{
	CompanyID company = (CompanyID)target;
	ClientID client = (ClientID)target;

	static_assert(GOAL_QUESTION_BUTTON_COUNT < 29);
	button_mask &= (1U << GOAL_QUESTION_BUTTON_COUNT) - 1;

	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (text.empty()) return CMD_ERROR;
	if (is_client) {
		/* Only check during pre-flight; the client might have left between
		 * testing and executing. In that case it is fine to just ignore the
		 * fact the client is no longer here. */
		if (!(flags & DC_EXEC) && _network_server && NetworkClientInfo::GetByClientID(client) == nullptr) return CMD_ERROR;
	} else {
		if (company != INVALID_COMPANY && !Company::IsValidID(company)) return CMD_ERROR;
	}
	uint min_buttons = (type == GQT_QUESTION ? 1 : 0);
	if (CountBits(button_mask) < min_buttons || CountBits(button_mask) > 3) return CMD_ERROR;
	if (type >= GQT_END) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (is_client) {
			if (client != _network_own_client_id) return CommandCost();
		} else {
			if (company == INVALID_COMPANY && !Company::IsValidID(_local_company)) return CommandCost();
			if (company != INVALID_COMPANY && company != _local_company) return CommandCost();
		}
		ShowGoalQuestion(uniqueid, type, button_mask, text.c_str());
	}

	return CommandCost();
}

/**
 * Reply to a goal question.
 * @param flags type of operation
 * @param uniqueid Unique ID to use for this question.
 * @param button Button the company pressed
 * @return the cost of this operation or an error
 */
CommandCost CmdGoalQuestionAnswer(DoCommandFlag flags, uint16 uniqueid, uint8 button)
{
	if (button >= GOAL_QUESTION_BUTTON_COUNT) return CMD_ERROR;

	if (_current_company == OWNER_DEITY) {
		/* It has been requested to close this specific question on all clients */
		if (flags & DC_EXEC) CloseWindowById(WC_GOAL_QUESTION, uniqueid);
		return CommandCost();
	}

	if (_networking && _local_company == _current_company) {
		/* Somebody in the same company answered the question. Close the window */
		if (flags & DC_EXEC) CloseWindowById(WC_GOAL_QUESTION, uniqueid);
		if (!_network_server) return CommandCost();
	}

	if (flags & DC_EXEC) {
		Game::NewEvent(new ScriptEventGoalQuestionAnswer(uniqueid, (ScriptCompany::CompanyID)(byte)_current_company, (ScriptGoal::QuestionButton)(1 << button)));
	}

	return CommandCost();
}
