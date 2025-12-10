/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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

/* static */ bool Goal::IsValidGoalDestination(CompanyID company, GoalType type, GoalTypeID dest)
{
	switch (type) {
		case GT_NONE:
			if (dest != 0) return false;
			break;

		case GT_TILE:
			if (!IsValidTile(dest)) return false;
			break;

		case GT_INDUSTRY:
			if (!Industry::IsValidID(dest)) return false;
			break;

		case GT_TOWN:
			if (!Town::IsValidID(dest)) return false;
			break;

		case GT_COMPANY:
			if (!Company::IsValidID(dest)) return false;
			break;

		case GT_STORY_PAGE: {
			if (!StoryPage::IsValidID(dest)) return false;
			CompanyID story_company = StoryPage::Get(dest)->company;
			if (company == CompanyID::Invalid() ? story_company != CompanyID::Invalid() : story_company != CompanyID::Invalid() && story_company != company) return false;
			break;
		}

		default: return false;
	}
	return true;
}

/**
 * Create a new goal.
 * @param flags type of operation
 * @param company Company for which this goal is.
 * @param type GoalType of destination.
 * @param dest GoalTypeID of destination.
 * @param text Text of the goal.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, GoalID> CmdCreateGoal(DoCommandFlags flags, CompanyID company, GoalType type, GoalTypeID dest, const EncodedString &text)
{
	if (!Goal::CanAllocateItem()) return { CMD_ERROR, GoalID::Invalid() };

	if (_current_company != OWNER_DEITY) return { CMD_ERROR, GoalID::Invalid() };
	if (text.empty()) return { CMD_ERROR, GoalID::Invalid() };
	if (company != CompanyID::Invalid() && !Company::IsValidID(company)) return { CMD_ERROR, GoalID::Invalid() };
	if (!Goal::IsValidGoalDestination(company, type, dest)) return { CMD_ERROR, GoalID::Invalid() };

	if (flags.Test(DoCommandFlag::Execute)) {
		Goal *g = new Goal(type, dest, company, text);

		if (g->company == CompanyID::Invalid()) {
			InvalidateWindowClassesData(WC_GOALS_LIST);
		} else {
			InvalidateWindowData(WC_GOALS_LIST, g->company);
		}
		if (Goal::GetNumItems() == 1) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);

		return { CommandCost(), g->index };
	}

	return { CommandCost(), GoalID::Invalid() };
}

/**
 * Remove a goal.
 * @param flags type of operation
 * @param goal GoalID to remove.
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveGoal(DoCommandFlags flags, GoalID goal)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		Goal *g = Goal::Get(goal);
		CompanyID c = g->company;
		delete g;

		if (c == CompanyID::Invalid()) {
			InvalidateWindowClassesData(WC_GOALS_LIST);
		} else {
			InvalidateWindowData(WC_GOALS_LIST, c);
		}
		if (Goal::GetNumItems() == 0) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);
	}

	return CommandCost();
}

/**
 * Update goal destination of a goal.
 * @param flags type of operation
 * @param goal GoalID to update.
 * @param type GoalType of destination.
 * @param dest GoalTypeID of destination.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetGoalDestination(DoCommandFlags flags, GoalID goal, GoalType type, GoalTypeID dest)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;
	Goal *g = Goal::Get(goal);
	if (!Goal::IsValidGoalDestination(g->company, type, dest)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		g->type = type;
		g->dst = dest;
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
CommandCost CmdSetGoalText(DoCommandFlags flags, GoalID goal, const EncodedString &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;
	if (text.empty()) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		Goal *g = Goal::Get(goal);
		g->text = text;

		if (g->company == CompanyID::Invalid()) {
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
CommandCost CmdSetGoalProgress(DoCommandFlags flags, GoalID goal, const EncodedString &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		Goal *g = Goal::Get(goal);
		g->progress = text;

		if (g->company == CompanyID::Invalid()) {
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
CommandCost CmdSetGoalCompleted(DoCommandFlags flags, GoalID goal, bool completed)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!Goal::IsValidID(goal)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		Goal *g = Goal::Get(goal);
		g->completed = completed;

		if (g->company == CompanyID::Invalid()) {
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
CommandCost CmdGoalQuestion(DoCommandFlags flags, uint16_t uniqueid, uint32_t target, bool is_client, uint32_t button_mask, GoalQuestionType type, const EncodedString &text)
{
	static_assert(sizeof(uint32_t) >= sizeof(CompanyID));
	CompanyID company = (CompanyID)target;
	static_assert(sizeof(uint32_t) >= sizeof(ClientID));
	ClientID client = (ClientID)target;

	static_assert(GOAL_QUESTION_BUTTON_COUNT < 29);
	button_mask &= (1U << GOAL_QUESTION_BUTTON_COUNT) - 1;

	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (text.empty()) return CMD_ERROR;
	if (is_client) {
		/* Only check during pre-flight; the client might have left between
		 * testing and executing. In that case it is fine to just ignore the
		 * fact the client is no longer here. */
		if (!flags.Test(DoCommandFlag::Execute) && _network_server && NetworkClientInfo::GetByClientID(client) == nullptr) return CMD_ERROR;
	} else {
		if (company != CompanyID::Invalid() && !Company::IsValidID(company)) return CMD_ERROR;
	}
	uint min_buttons = (type == GQT_QUESTION ? 1 : 0);
	if (CountBits(button_mask) < min_buttons || CountBits(button_mask) > 3) return CMD_ERROR;
	if (type >= GQT_END) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		if (is_client) {
			if (client != _network_own_client_id) return CommandCost();
		} else {
			if (company == CompanyID::Invalid() && !Company::IsValidID(_local_company)) return CommandCost();
			if (company != CompanyID::Invalid() && company != _local_company) return CommandCost();
		}
		ShowGoalQuestion(uniqueid, type, button_mask, text);
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
CommandCost CmdGoalQuestionAnswer(DoCommandFlags flags, uint16_t uniqueid, uint8_t button)
{
	if (button >= GOAL_QUESTION_BUTTON_COUNT) return CMD_ERROR;

	if (_current_company == OWNER_DEITY) {
		/* It has been requested to close this specific question on all clients */
		if (flags.Test(DoCommandFlag::Execute)) CloseWindowById(WC_GOAL_QUESTION, uniqueid);
		return CommandCost();
	}

	if (_networking && _local_company == _current_company) {
		/* Somebody in the same company answered the question. Close the window */
		if (flags.Test(DoCommandFlag::Execute)) CloseWindowById(WC_GOAL_QUESTION, uniqueid);
		if (!_network_server) return CommandCost();
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		Game::NewEvent(new ScriptEventGoalQuestionAnswer(uniqueid, _current_company, (ScriptGoal::QuestionButton)(1 << button)));
	}

	return CommandCost();
}
