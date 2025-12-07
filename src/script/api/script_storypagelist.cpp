/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_storypagelist.cpp Implementation of ScriptStoryPageList and friends. */

#include "../../stdafx.h"
#include "script_storypagelist.hpp"
#include "script_story_page.hpp"
#include "../../story_base.h"

#include "../../safeguards.h"

ScriptStoryPageList::ScriptStoryPageList(ScriptCompany::CompanyID company)
{
	::CompanyID c = ScriptCompany::FromScriptCompanyID(company);

	ScriptList::FillList<StoryPage>(this,
		[c](const StoryPage *p) {return p->company == c || p->company == CompanyID::Invalid(); }
	);
}
