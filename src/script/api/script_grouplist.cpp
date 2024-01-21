/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_grouplist.cpp Implementation of ScriptGroupList and friends. */

#include "../../stdafx.h"
#include "script_grouplist.h"
#include "script_error.h"
#include "../../group.h"

#include "../../safeguards.h"

ScriptGroupList::ScriptGroupList(HSQUIRRELVM vm)
{
	EnforceCompanyModeValid_Void();
	CompanyID owner = ScriptObject::GetCompany();
	ScriptList::FillList<Group>(vm, this,
		[owner](const Group *g) { return g->owner == owner; }
	);
}
