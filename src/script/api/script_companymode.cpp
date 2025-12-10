/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_companymode.cpp Implementation of ScriptCompanyMode. */

#include "../../stdafx.h"
#include "../../company_base.h"
#include "script_companymode.hpp"

#include "../../safeguards.h"

ScriptCompanyMode::ScriptCompanyMode(ScriptCompany::CompanyID company)
{
	company = ScriptCompany::ResolveCompanyID(company);

	this->last_company = ScriptObject::GetCompany();
	ScriptObject::SetCompany(ScriptCompany::FromScriptCompanyID(company));
}

ScriptCompanyMode::~ScriptCompanyMode()
{
	ScriptObject::SetCompany(this->last_company);
}

/* static */ bool ScriptCompanyMode::IsValid()
{
	return ::Company::IsValidID(ScriptObject::GetCompany());
}

/* static */ bool ScriptCompanyMode::IsDeity()
{
	return ScriptObject::GetCompany() == OWNER_DEITY;
}
