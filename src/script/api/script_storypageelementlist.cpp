/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_storypageelementlist.cpp Implementation of ScriptStoryPageElementList and friends. */

#include "../../stdafx.h"
#include "script_storypageelementlist.hpp"
#include "../../story_base.h"

#include "../../safeguards.h"

ScriptStoryPageElementList::ScriptStoryPageElementList(ScriptStoryPage::StoryPageID story_page_id)
{
	if (!ScriptStoryPage::IsValidStoryPage(story_page_id)) return;

	StoryPageElement *pe;
	FOR_ALL_STORY_PAGE_ELEMENTS(pe) {
		if (pe->page == story_page_id) {
			this->AddItem(pe->index);
		}
	}
}
