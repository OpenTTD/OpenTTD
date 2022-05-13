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
#include "../../goal_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptGoal::IsValidGoal(GoalID goal_id)
{
	return ::Goal::IsValidID(goal_id);
}

/* static */ ScriptGoal::GoalID ScriptGoal::New(ScriptCompany::CompanyID company, Text *goal, GoalType type, uint32 destination)
{
	CCountedPtr<Text> counter(goal);

	EnforcePrecondition(GOAL_INVALID, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(GOAL_INVALID, goal != nullptr);
	const char *text = goal->GetEncodedText();
	EnforcePreconditionEncodedText(GOAL_INVALID, text);
	EnforcePrecondition(GOAL_INVALID, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);

	CompanyID c = (::CompanyID)company;
	if (company == ScriptCompany::COMPANY_INVALID) c = INVALID_COMPANY;
	StoryPage *story_page = nullptr;
	if (type == GT_STORY_PAGE && ScriptStoryPage::IsValidStoryPage((ScriptStoryPage::StoryPageID)destination)) story_page = ::StoryPage::Get((ScriptStoryPage::StoryPageID)destination);

	EnforcePrecondition(GOAL_INVALID, (type == GT_NONE && destination == 0) ||
			(type == GT_TILE && ScriptMap::IsValidTile(destination)) ||
			(type == GT_INDUSTRY && ScriptIndustry::IsValidIndustry(destination)) ||
			(type == GT_TOWN && ScriptTown::IsValidTown(destination)) ||
			(type == GT_COMPANY && ScriptCompany::ResolveCompanyID((ScriptCompany::CompanyID)destination) != ScriptCompany::COMPANY_INVALID) ||
			(type == GT_STORY_PAGE && story_page != nullptr && (c == INVALID_COMPANY ? story_page->company == INVALID_COMPANY : story_page->company == INVALID_COMPANY || story_page->company == c)));

	if (!ScriptObject::Command<CMD_CREATE_GOAL>::Do(&ScriptInstance::DoCommandReturnGoalID, c, (::GoalType)type, destination, text)) return GOAL_INVALID;

	/* In case of test-mode, we return GoalID 0 */
	return (ScriptGoal::GoalID)0;
}

/* static */ bool ScriptGoal::Remove(GoalID goal_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, IsValidGoal(goal_id));

	return ScriptObject::Command<CMD_REMOVE_GOAL>::Do(goal_id);
}

/* static */ bool ScriptGoal::SetText(GoalID goal_id, Text *goal)
{
	CCountedPtr<Text> counter(goal);

	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, goal != nullptr);
	EnforcePrecondition(false, !StrEmpty(goal->GetEncodedText()));

	return ScriptObject::Command<CMD_SET_GOAL_TEXT>::Do(goal_id, goal->GetEncodedText());
}

/* static */ bool ScriptGoal::SetProgress(GoalID goal_id, Text *progress)
{
	CCountedPtr<Text> counter(progress);

	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	/* Ensure null as used for empty string. */
	if (progress != nullptr && StrEmpty(progress->GetEncodedText())) {
		progress = nullptr;
	}

	return ScriptObject::Command<CMD_SET_GOAL_PROGRESS>::Do(goal_id, progress != nullptr ? std::string{ progress->GetEncodedText() } : std::string{});
}

/* static */ bool ScriptGoal::SetCompleted(GoalID goal_id, bool completed)
{
	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	return ScriptObject::Command<CMD_SET_GOAL_COMPLETED>::Do(goal_id, completed);
}

/* static */ bool ScriptGoal::IsCompleted(GoalID goal_id)
{
	EnforcePrecondition(false, IsValidGoal(goal_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	Goal *g = Goal::Get(goal_id);
	return g != nullptr && g->completed;
}

/* static */ bool ScriptGoal::DoQuestion(uint16 uniqueid, uint32 target, bool is_client, Text *question, QuestionType type, uint32 buttons)
{
	CCountedPtr<Text> counter(question);

	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, question != nullptr);
	const char *text = question->GetEncodedText();
	EnforcePreconditionEncodedText(false, text);
	uint min_buttons = (type == QT_QUESTION ? 1 : 0);
	EnforcePrecondition(false, CountBits(buttons) >= min_buttons && CountBits(buttons) <= 3);
	EnforcePrecondition(false, buttons < (1 << ::GOAL_QUESTION_BUTTON_COUNT));
	EnforcePrecondition(false, (int)type < ::GQT_END);

	return ScriptObject::Command<CMD_GOAL_QUESTION>::Do(uniqueid, target, is_client, buttons, (::GoalQuestionType)type, text);
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
	/* Can only send 16 bits of client_id before proper fix is implemented */
	EnforcePrecondition(false, client < (1 << 16));
	return DoQuestion(uniqueid, client, true, question, type, buttons);
}

/* static */ bool ScriptGoal::CloseQuestion(uint16 uniqueid)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	return ScriptObject::Command<CMD_GOAL_QUESTION_ANSWER>::Do(uniqueid, 0);
}
