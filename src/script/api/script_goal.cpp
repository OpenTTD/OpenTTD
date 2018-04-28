/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_goal.cpp Implementation of ScriptGoal. */

#include "../../stdafx.h"
#include "script_game.hpp"
#include "script_goal.hpp"
#include "script_error.hpp"
#include "script_industry.hpp"
#include "script_map.hpp"
#include "script_town.hpp"
#include "script_story_page.hpp"
#include "../script_instance.hpp"
#include "../../goal_base.h"
#include "../../string_func.h"
#include "../../network/network_base.h"

#include "../../safeguards.h"

/* static */ bool ScriptGoal::IsValidGoal(GoalID goal_id)
{
	return ::Goal::IsValidID(goal_id);
}

/* static */ ScriptGoal::GoalID ScriptGoal::New(ScriptCompany::CompanyID company, Text *goal, GoalType type, uint32 destination)
{
	CCountedPtr<Text> counter(goal);

	EnforcePrecondition(GOAL_INVALID, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(GOAL_INVALID, goal != NULL);
	const char *text = goal->GetEncodedText();
	EnforcePreconditionEncodedText(GOAL_INVALID, text);
	EnforcePrecondition(GOAL_INVALID, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);

	uint8 c = company;
	if (company == ScriptCompany::COMPANY_INVALID) c = INVALID_COMPANY;
	StoryPage *story_page = NULL;
	if (type == GT_STORY_PAGE && ScriptStoryPage::IsValidStoryPage((ScriptStoryPage::StoryPageID)destination)) story_page = ::StoryPage::Get((ScriptStoryPage::StoryPageID)destination);

	EnforcePrecondition(GOAL_INVALID, (type == GT_NONE && destination == 0) ||
			(type == GT_TILE && ScriptMap::IsValidTile(destination)) ||
			(type == GT_INDUSTRY && ScriptIndustry::IsValidIndustry(destination)) ||
			(type == GT_TOWN && ScriptTown::IsValidTown(destination)) ||
			(type == GT_COMPANY && ScriptCompany::ResolveCompanyID((ScriptCompany::CompanyID)destination) != ScriptCompany::COMPANY_INVALID) ||
			(type == GT_STORY_PAGE && story_page != NULL && (c == INVALID_COMPANY ? story_page->company == INVALID_COMPANY : story_page->company == INVALID_COMPANY || story_page->company == c)));

	if (!ScriptObject::DoCommand(0, type | (c << 8), destination, CMD_CREATE_GOAL, text, &ScriptInstance::DoCommandReturnGoalID)) return GOAL_INVALID;

	/* In case of test-mode, we return GoalID 0 */
	return (ScriptGoal::GoalID)0;
}

/* static */ bool ScriptGoal::Remove(GoalID goal_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, IsValidGoal(goal_id));

	return ScriptObject::DoCommand(0, goal_id, 0, CMD_REMOVE_GOAL);
}

/* static */ bool ScriptGoal::SetText(GoalID goal_id, Text *goal)
{
	CCountedPtr<Text> counter(goal);

	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, goal != NULL);
	EnforcePrecondition(false, !StrEmpty(goal->GetEncodedText()));

	return ScriptObject::DoCommand(0, goal_id, 0, CMD_SET_GOAL_TEXT, goal->GetEncodedText());
}

/* static */ bool ScriptGoal::SetProgress(GoalID goal_id, Text *progress)
{
	CCountedPtr<Text> counter(progress);

	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	/* Ensure null as used for emtpy string. */
	if (progress != NULL && StrEmpty(progress->GetEncodedText())) {
		progress = NULL;
	}

	return ScriptObject::DoCommand(0, goal_id, 0, CMD_SET_GOAL_PROGRESS, progress != NULL ? progress->GetEncodedText() : NULL);
}

/* static */ bool ScriptGoal::SetCompleted(GoalID goal_id, bool completed)
{
	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	return ScriptObject::DoCommand(0, goal_id, completed ? 1 : 0, CMD_SET_GOAL_COMPLETED);
}

/* static */ bool ScriptGoal::IsCompleted(GoalID goal_id)
{
	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	Goal *g = Goal::Get(goal_id);
	return g != NULL && g->completed;
}

/* static */ bool ScriptGoal::DoQuestion(uint16 uniqueid, uint8 target, bool is_client, Text *question, QuestionType type, int buttons)
{
	CCountedPtr<Text> counter(question);

	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, question != NULL);
	const char *text = question->GetEncodedText();
	EnforcePreconditionEncodedText(false, text);
	EnforcePrecondition(false, CountBits(buttons) >= 1 && CountBits(buttons) <= 3);
	EnforcePrecondition(false, buttons < (1 << ::GOAL_QUESTION_BUTTON_COUNT));
	EnforcePrecondition(false, (int)type < ::GOAL_QUESTION_TYPE_COUNT);

	return ScriptObject::DoCommand(0, uniqueid | (target << 16) | (type << 24) | (is_client ? (1 << 31) : 0), buttons, CMD_GOAL_QUESTION, text);
}

/* static */ bool ScriptGoal::Question(uint16 uniqueid, ScriptCompany::CompanyID company, Text *question, QuestionType type, int buttons)
{
	EnforcePrecondition(false, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);
	uint8 c = company;
	if (company == ScriptCompany::COMPANY_INVALID) c = INVALID_COMPANY;

	return DoQuestion(uniqueid, c, false, question, type, buttons);
}

/* static */ bool ScriptGoal::QuestionClient(uint16 uniqueid, ScriptClient::ClientID client, Text *question, QuestionType type, int buttons)
{
	EnforcePrecondition(false, ScriptGame::IsMultiplayer());
	EnforcePrecondition(false, ScriptClient::ResolveClientID(client) != ScriptClient::CLIENT_INVALID);
#ifdef ENABLE_NETWORK
	ClientIndex c = NetworkClientInfo::GetByClientID((::ClientID)client)->index;
	return DoQuestion(uniqueid, c, true, question, type, buttons);
#else
	return false;
#endif
}

/* static */ bool ScriptGoal::CloseQuestion(uint16 uniqueid)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	return ScriptObject::DoCommand(0, uniqueid, 0, CMD_GOAL_QUESTION_ANSWER);
}
