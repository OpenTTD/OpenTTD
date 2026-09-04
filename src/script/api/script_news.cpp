/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_news.cpp Implementation of ScriptNews. */

#include "../../stdafx.h"
#include "script_news.hpp"
#include "script_industry.hpp"
#include "script_station.hpp"
#include "script_map.hpp"
#include "script_town.hpp"
#include "script_engine.hpp"
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
		case ScriptNews::NR_ENGINE: return static_cast<EngineID>(reference);
		default: NOT_REACHED();
	}
}

/**
 * Internal function performing operations from ScriptNews::Create and ScriptNews::CreateWithInfo.
 * @copydetails ScriptNews::CreateWithInfo
 */
/* static */ bool ScriptNews::CreateHelper(NewsType type, Text *text, ScriptCompany::CompanyID company, NewsReferenceType ref_type, SQInteger reference, const EncodedString &title, const EncodedString &additional_message1, const EncodedString &additional_message2)
{
	ScriptObjectRef counter(text);

	EnforceDeityMode(false);
	EnforcePrecondition(false, text != nullptr);
	EncodedString encoded = text->GetEncodedText();
	EnforcePreconditionEncodedText(false, encoded);
	EnforcePrecondition(false, type == NT_ECONOMY || type == NT_SUBSIDIES || type == NT_GENERAL || type == NT_PREVIEW_RESULT);
	EnforcePrecondition(false, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);
	EnforcePrecondition(false, (ref_type == NR_NONE) ||
	                           (ref_type == NR_TILE     && ScriptMap::IsValidTile(::TileIndex(reference))) ||
	                           (ref_type == NR_STATION  && ScriptStation::IsValidStation(static_cast<StationID>(reference))) ||
	                           (ref_type == NR_INDUSTRY && ScriptIndustry::IsValidIndustry(static_cast<IndustryID>(reference))) ||
	                           (ref_type == NR_ENGINE   && ScriptEngine::IsValidEngine(static_cast<EngineID>(reference))) ||
	                           (ref_type == NR_TOWN     && ScriptTown::IsValidTown(static_cast<TownID>(reference))));

	::CompanyID c = ScriptCompany::FromScriptCompanyID(company);

	return ScriptObject::Command<Commands::CreateCustomNewsItem>::Do(static_cast<::NewsType>(type), c, CreateReference(ref_type, reference), encoded, title, additional_message1, additional_message2);
}

/* static */ bool ScriptNews::Create(NewsType type, Text *text, ScriptCompany::CompanyID company, NewsReferenceType ref_type, SQInteger reference)
{
	return ScriptNews::CreateHelper(type, text, company, ref_type, reference, text->GetEncodedText(), {}, {});
}

/* static */ bool ScriptNews::CreateWithInfo(NewsType type, Text *text, ScriptCompany::CompanyID company, NewsReferenceType ref_type, SQInteger reference, Text *title, Text *additional_message1, Text *additional_message2)
{
	EncodedString msg1 = additional_message1->GetEncodedText();
	EncodedString msg2 = additional_message2->GetEncodedText();
	EncodedString encoded_title = title->GetEncodedText();

	return ScriptNews::CreateHelper(type, text, company, ref_type, reference, encoded_title, msg1, msg2);
}
