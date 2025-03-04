/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_news.cpp Implementation of ScriptNews. */

#include "../../stdafx.h"
#include "script_news.hpp"
#include "script_industry.hpp"
#include "script_station.hpp"
#include "script_map.hpp"
#include "script_town.hpp"
#include "script_error.hpp"
#include "../../command_type.h"
#include "../../string_func.h"
#include "../../news_cmd.h"

#include "../../safeguards.h"

static NewsReference CreateReference(ScriptNews::NewsReferenceType ref_type, SQInteger reference)
{
	switch (ref_type) {
		case ScriptNews::NR_NONE: return {};
		case ScriptNews::NR_TILE: return ::TileIndex(reference);
		case ScriptNews::NR_STATION: return static_cast<StationID>(reference);
		case ScriptNews::NR_INDUSTRY: return static_cast<IndustryID>(reference);
		case ScriptNews::NR_TOWN: return static_cast<TownID>(reference);
		default: NOT_REACHED();
	}
}

/* static */ bool ScriptNews::Create(NewsType type, Text *text, ScriptCompany::CompanyID company, NewsReferenceType ref_type, SQInteger reference)
{
	ScriptObjectRef counter(text);

	EnforceDeityMode(false);
	EnforcePrecondition(false, text != nullptr);
	EncodedString encoded = text->GetEncodedText();
	EnforcePreconditionEncodedText(false, encoded);
	EnforcePrecondition(false, type == NT_ECONOMY || type == NT_SUBSIDIES || type == NT_GENERAL);
	EnforcePrecondition(false, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);
	EnforcePrecondition(false, (ref_type == NR_NONE) ||
	                           (ref_type == NR_TILE     && ScriptMap::IsValidTile(::TileIndex(reference))) ||
	                           (ref_type == NR_STATION  && ScriptStation::IsValidStation(static_cast<StationID>(reference))) ||
	                           (ref_type == NR_INDUSTRY && ScriptIndustry::IsValidIndustry(static_cast<IndustryID>(reference))) ||
	                           (ref_type == NR_TOWN     && ScriptTown::IsValidTown(static_cast<TownID>(reference))));

	::CompanyID c = ScriptCompany::FromScriptCompanyID(company);

	return ScriptObject::Command<CMD_CUSTOM_NEWS_ITEM>::Do((::NewsType)type, c, CreateReference(ref_type, reference), encoded);
}
