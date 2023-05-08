/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_storypagelist.cpp Implementation of ScriptStoryPageList and friends. */

#include "../../stdafx.h"
#include "script_storypagelist.hpp"
#include "script_story_page.hpp"
#include "../../story_base.h"

#include "../../safeguards.h"

ScriptStoryPageList::ScriptStoryPageList(ScriptCompany::CompanyID company)
{
	uint8_t c = company;
	if (company == ScriptCompany::COMPANY_INVALID) c = INVALID_COMPANY;

	for (StoryPage *p : StoryPage::Iterate()) {
		if (p->company == c || p->company == INVALID_COMPANY) {
			this->AddItem(p->index);
		}
	}
}
