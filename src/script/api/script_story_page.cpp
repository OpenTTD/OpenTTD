/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_story_page.cpp Implementation of ScriptStoryPage. */

#include "../../stdafx.h"
#include "script_story_page.hpp"
#include "script_error.hpp"
#include "script_industry.hpp"
#include "script_map.hpp"
#include "script_town.hpp"
#include "script_goal.hpp"
#include "../script_instance.hpp"
#include "../../story_base.h"
#include "../../goal_base.h"
#include "../../string_func.h"
#include "../../tile_map.h"

#include "../../safeguards.h"

static inline bool StoryPageElementTypeRequiresText(StoryPageElementType type)
{
	return type == SPET_TEXT || type == SPET_LOCATION || type == SPET_BUTTON_PUSH || type == SPET_BUTTON_TILE || type == SPET_BUTTON_VEHICLE;
}

/* static */ bool ScriptStoryPage::IsValidStoryPage(StoryPageID story_page_id)
{
	return ::StoryPage::IsValidID(story_page_id);
}

/* static */ bool ScriptStoryPage::IsValidStoryPageElement(StoryPageElementID story_page_element_id)
{
	return ::StoryPageElement::IsValidID(story_page_element_id);
}

/* static */ ScriptStoryPage::StoryPageID ScriptStoryPage::New(ScriptCompany::CompanyID company, Text *title)
{
	CCountedPtr<Text> counter(title);

	EnforcePrecondition(STORY_PAGE_INVALID, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(STORY_PAGE_INVALID, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);

	uint8 c = company;
	if (company == ScriptCompany::COMPANY_INVALID) c = INVALID_COMPANY;

	if (!ScriptObject::DoCommand(0,
		c,
		0,
		CMD_CREATE_STORY_PAGE,
		title != nullptr? title->GetEncodedText() : nullptr,
		&ScriptInstance::DoCommandReturnStoryPageID)) return STORY_PAGE_INVALID;

	/* In case of test-mode, we return StoryPageID 0 */
	return (ScriptStoryPage::StoryPageID)0;
}

/* static */ ScriptStoryPage::StoryPageElementID ScriptStoryPage::NewElement(StoryPageID story_page_id, StoryPageElementType type, uint32 reference, Text *text)
{
	CCountedPtr<Text> counter(text);

	::StoryPageElementType btype = static_cast<::StoryPageElementType>(type);

	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, IsValidStoryPage(story_page_id));
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, !StoryPageElementTypeRequiresText(btype) || (text != nullptr && !StrEmpty(text->GetEncodedText())));
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, type != SPET_LOCATION || ::IsValidTile(reference));
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, type != SPET_GOAL || ScriptGoal::IsValidGoal((ScriptGoal::GoalID)reference));
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, type != SPET_GOAL || !(StoryPage::Get(story_page_id)->company == INVALID_COMPANY && Goal::Get(reference)->company != INVALID_COMPANY));

	if (!ScriptObject::DoCommand(type == SPET_LOCATION ? reference : 0,
			story_page_id + (type << 16),
			type == SPET_GOAL ? reference : 0,
			CMD_CREATE_STORY_PAGE_ELEMENT,
			StoryPageElementTypeRequiresText(btype) ? text->GetEncodedText() : nullptr,
			&ScriptInstance::DoCommandReturnStoryPageElementID)) return STORY_PAGE_ELEMENT_INVALID;

	/* In case of test-mode, we return StoryPageElementID 0 */
	return (ScriptStoryPage::StoryPageElementID)0;
}

/* static */ bool ScriptStoryPage::UpdateElement(StoryPageElementID story_page_element_id, uint32 reference, Text *text)
{
	CCountedPtr<Text> counter(text);

	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, IsValidStoryPageElement(story_page_element_id));

	StoryPageElement *pe = StoryPageElement::Get(story_page_element_id);
	StoryPage *p = StoryPage::Get(pe->page);
	::StoryPageElementType type = pe->type;

	EnforcePrecondition(false, !StoryPageElementTypeRequiresText(type) || (text != nullptr && !StrEmpty(text->GetEncodedText())));
	EnforcePrecondition(false, type != ::SPET_LOCATION || ::IsValidTile(reference));
	EnforcePrecondition(false, type != ::SPET_GOAL || ScriptGoal::IsValidGoal((ScriptGoal::GoalID)reference));
	EnforcePrecondition(false, type != ::SPET_GOAL || !(p->company == INVALID_COMPANY && Goal::Get(reference)->company != INVALID_COMPANY));

	return ScriptObject::DoCommand(type == ::SPET_LOCATION ? reference : 0,
			story_page_element_id,
			type == ::SPET_GOAL ? reference : 0,
			CMD_UPDATE_STORY_PAGE_ELEMENT,
			StoryPageElementTypeRequiresText(type) ? text->GetEncodedText() : nullptr);
}

/* static */ uint32 ScriptStoryPage::GetPageSortValue(StoryPageID story_page_id)
{
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));

	return StoryPage::Get(story_page_id)->sort_value;
}

/* static */ uint32 ScriptStoryPage::GetPageElementSortValue(StoryPageElementID story_page_element_id)
{
	EnforcePrecondition(false, IsValidStoryPageElement(story_page_element_id));

	return StoryPageElement::Get(story_page_element_id)->sort_value;
}

/* static */ bool ScriptStoryPage::SetTitle(StoryPageID story_page_id, Text *title)
{
	CCountedPtr<Text> counter(title);

	EnforcePrecondition(false, IsValidStoryPage(story_page_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	return ScriptObject::DoCommand(0, story_page_id, 0, CMD_SET_STORY_PAGE_TITLE, title != nullptr? title->GetEncodedText() : nullptr);
}

/* static */ ScriptCompany::CompanyID ScriptStoryPage::GetCompany(StoryPageID story_page_id)
{
	EnforcePrecondition(ScriptCompany::COMPANY_INVALID, IsValidStoryPage(story_page_id));

	CompanyID c = StoryPage::Get(story_page_id)->company;
	ScriptCompany::CompanyID company = c == INVALID_COMPANY ? ScriptCompany::COMPANY_INVALID : (ScriptCompany::CompanyID)c;

	return company;
}

/* static */ ScriptDate::Date ScriptStoryPage::GetDate(StoryPageID story_page_id)
{
	EnforcePrecondition(ScriptDate::DATE_INVALID, IsValidStoryPage(story_page_id));
	EnforcePrecondition(ScriptDate::DATE_INVALID, ScriptObject::GetCompany() == OWNER_DEITY);

	return (ScriptDate::Date)StoryPage::Get(story_page_id)->date;
}

/* static */ bool ScriptStoryPage::SetDate(StoryPageID story_page_id, ScriptDate::Date date)
{
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	return ScriptObject::DoCommand(0, story_page_id, date, CMD_SET_STORY_PAGE_DATE, nullptr);
}


/* static */ bool ScriptStoryPage::Show(StoryPageID story_page_id)
{
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);

	return ScriptObject::DoCommand(0, story_page_id, 0, CMD_SHOW_STORY_PAGE);
}

/* static */ bool ScriptStoryPage::Remove(StoryPageID story_page_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));

	return ScriptObject::DoCommand(0, story_page_id, 0, CMD_REMOVE_STORY_PAGE);
}

/* static */ bool ScriptStoryPage::RemoveElement(StoryPageElementID story_page_element_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() == OWNER_DEITY);
	EnforcePrecondition(false, IsValidStoryPageElement(story_page_element_id));

	return ScriptObject::DoCommand(0, story_page_element_id, 0, CMD_REMOVE_STORY_PAGE_ELEMENT);
}

