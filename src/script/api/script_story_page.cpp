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
#include "../../story_cmd.h"

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

	EnforceDeityMode(STORY_PAGE_INVALID);
	EnforcePrecondition(STORY_PAGE_INVALID, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);

	uint8_t c = company;
	if (company == ScriptCompany::COMPANY_INVALID) c = INVALID_COMPANY;

	if (!ScriptObject::Command<CMD_CREATE_STORY_PAGE>::Do(&ScriptInstance::DoCommandReturnStoryPageID,
		(::CompanyID)c, title != nullptr ? title->GetEncodedText() : std::string{})) return STORY_PAGE_INVALID;

	/* In case of test-mode, we return StoryPageID 0 */
	return (ScriptStoryPage::StoryPageID)0;
}

/* static */ ScriptStoryPage::StoryPageElementID ScriptStoryPage::NewElement(StoryPageID story_page_id, StoryPageElementType type, SQInteger reference, Text *text)
{
	CCountedPtr<Text> counter(text);

	::StoryPageElementType btype = static_cast<::StoryPageElementType>(type);

	EnforceDeityMode(STORY_PAGE_ELEMENT_INVALID);
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, IsValidStoryPage(story_page_id));
	std::string encoded_text;
	if (StoryPageElementTypeRequiresText(btype)) {
		EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, text != nullptr);
		encoded_text = text->GetEncodedText();
		EnforcePreconditionEncodedText(STORY_PAGE_ELEMENT_INVALID, encoded_text);
	}
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, type != SPET_LOCATION || ::IsValidTile((::TileIndex)reference));
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, type != SPET_GOAL || ScriptGoal::IsValidGoal((ScriptGoal::GoalID)reference));
	EnforcePrecondition(STORY_PAGE_ELEMENT_INVALID, type != SPET_GOAL || !(StoryPage::Get(story_page_id)->company == INVALID_COMPANY && Goal::Get(reference)->company != INVALID_COMPANY));

	uint32_t refid = 0;
	TileIndex reftile = 0;
	switch (type) {
		case SPET_LOCATION:
			reftile = reference;
			break;
		case SPET_GOAL:
		case SPET_BUTTON_PUSH:
		case SPET_BUTTON_TILE:
		case SPET_BUTTON_VEHICLE:
			refid = reference;
			break;
		case SPET_TEXT:
			break;
		default:
			NOT_REACHED();
	}

	if (!ScriptObject::Command<CMD_CREATE_STORY_PAGE_ELEMENT>::Do(&ScriptInstance::DoCommandReturnStoryPageElementID,
			reftile,
			(::StoryPageID)story_page_id, (::StoryPageElementType)type,
			refid,
			encoded_text)) return STORY_PAGE_ELEMENT_INVALID;

	/* In case of test-mode, we return StoryPageElementID 0 */
	return (ScriptStoryPage::StoryPageElementID)0;
}

/* static */ bool ScriptStoryPage::UpdateElement(StoryPageElementID story_page_element_id, SQInteger reference, Text *text)
{
	CCountedPtr<Text> counter(text);

	EnforceDeityMode(false);
	EnforcePrecondition(false, IsValidStoryPageElement(story_page_element_id));

	StoryPageElement *pe = StoryPageElement::Get(story_page_element_id);
	StoryPage *p = StoryPage::Get(pe->page);
	::StoryPageElementType type = pe->type;

	std::string encoded_text;
	if (StoryPageElementTypeRequiresText(type)) {
		EnforcePrecondition(false, text != nullptr);
		encoded_text = text->GetEncodedText();
		EnforcePreconditionEncodedText(false, encoded_text);
	}
	EnforcePrecondition(false, type != ::SPET_LOCATION || ::IsValidTile((::TileIndex)reference));
	EnforcePrecondition(false, type != ::SPET_GOAL || ScriptGoal::IsValidGoal((ScriptGoal::GoalID)reference));
	EnforcePrecondition(false, type != ::SPET_GOAL || !(p->company == INVALID_COMPANY && Goal::Get(reference)->company != INVALID_COMPANY));

	uint32_t refid = 0;
	TileIndex reftile = 0;
	switch (type) {
		case ::SPET_LOCATION:
			reftile = reference;
			break;
		case ::SPET_GOAL:
		case ::SPET_BUTTON_PUSH:
		case ::SPET_BUTTON_TILE:
		case ::SPET_BUTTON_VEHICLE:
			refid = reference;
			break;
		case ::SPET_TEXT:
			break;
		default:
			NOT_REACHED();
	}

	return ScriptObject::Command<CMD_UPDATE_STORY_PAGE_ELEMENT>::Do(reftile, story_page_element_id, refid, encoded_text);
}

/* static */ SQInteger ScriptStoryPage::GetPageSortValue(StoryPageID story_page_id)
{
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));

	return StoryPage::Get(story_page_id)->sort_value;
}

/* static */ SQInteger ScriptStoryPage::GetPageElementSortValue(StoryPageElementID story_page_element_id)
{
	EnforcePrecondition(false, IsValidStoryPageElement(story_page_element_id));

	return StoryPageElement::Get(story_page_element_id)->sort_value;
}

/* static */ bool ScriptStoryPage::SetTitle(StoryPageID story_page_id, Text *title)
{
	CCountedPtr<Text> counter(title);

	EnforcePrecondition(false, IsValidStoryPage(story_page_id));
	EnforceDeityMode(false);

	return ScriptObject::Command<CMD_SET_STORY_PAGE_TITLE>::Do(story_page_id, title != nullptr ? title->GetEncodedText() : std::string{});
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
	EnforceDeityMode(ScriptDate::DATE_INVALID);

	return (ScriptDate::Date)StoryPage::Get(story_page_id)->date.base();
}

/* static */ bool ScriptStoryPage::SetDate(StoryPageID story_page_id, ScriptDate::Date date)
{
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));
	EnforceDeityMode(false);

	return ScriptObject::Command<CMD_SET_STORY_PAGE_DATE>::Do(story_page_id, date);
}


/* static */ bool ScriptStoryPage::Show(StoryPageID story_page_id)
{
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));
	EnforceDeityMode(false);

	return ScriptObject::Command<CMD_SHOW_STORY_PAGE>::Do(story_page_id);
}

/* static */ bool ScriptStoryPage::Remove(StoryPageID story_page_id)
{
	EnforceDeityMode(false);
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));

	return ScriptObject::Command<CMD_REMOVE_STORY_PAGE>::Do(story_page_id);
}

/* static */ bool ScriptStoryPage::RemoveElement(StoryPageElementID story_page_element_id)
{
	EnforceDeityMode(false);
	EnforcePrecondition(false, IsValidStoryPageElement(story_page_element_id));

	return ScriptObject::Command<CMD_REMOVE_STORY_PAGE_ELEMENT>::Do(story_page_element_id);
}

/* static */ ScriptStoryPage::StoryPageButtonFormatting ScriptStoryPage::MakePushButtonReference(StoryPageButtonColour colour, StoryPageButtonFlags flags)
{
	StoryPageButtonData data;
	data.SetColour((Colours)colour);
	data.SetFlags((::StoryPageButtonFlags)flags);
	if (!data.ValidateColour()) return UINT32_MAX;
	if (!data.ValidateFlags()) return UINT32_MAX;
	return data.referenced_id;
}

/* static */ ScriptStoryPage::StoryPageButtonFormatting ScriptStoryPage::MakeTileButtonReference(StoryPageButtonColour colour, StoryPageButtonFlags flags, StoryPageButtonCursor cursor)
{
	StoryPageButtonData data;
	data.SetColour((Colours)colour);
	data.SetFlags((::StoryPageButtonFlags)flags);
	data.SetCursor((::StoryPageButtonCursor)cursor);
	if (!data.ValidateColour()) return UINT32_MAX;
	if (!data.ValidateFlags()) return UINT32_MAX;
	if (!data.ValidateCursor()) return UINT32_MAX;
	return data.referenced_id;
}

/* static */ ScriptStoryPage::StoryPageButtonFormatting ScriptStoryPage::MakeVehicleButtonReference(StoryPageButtonColour colour, StoryPageButtonFlags flags, StoryPageButtonCursor cursor, ScriptVehicle::VehicleType vehtype)
{
	StoryPageButtonData data;
	data.SetColour((Colours)colour);
	data.SetFlags((::StoryPageButtonFlags)flags);
	data.SetCursor((::StoryPageButtonCursor)cursor);
	data.SetVehicleType((::VehicleType)vehtype);
	if (!data.ValidateColour()) return UINT32_MAX;
	if (!data.ValidateFlags()) return UINT32_MAX;
	if (!data.ValidateCursor()) return UINT32_MAX;
	if (!data.ValidateVehicleType()) return UINT32_MAX;
	return data.referenced_id;
}

