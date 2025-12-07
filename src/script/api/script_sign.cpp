/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_sign.cpp Implementation of ScriptSign. */

#include "../../stdafx.h"
#include "script_sign.hpp"
#include "../script_instance.hpp"
#include "../../signs_base.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../tile_map.h"
#include "../../signs_cmd.h"

#include "table/strings.h"

#include "../../safeguards.h"

/* static */ bool ScriptSign::IsValidSign(SignID sign_id)
{
	EnforceDeityOrCompanyModeValid(false);
	const Sign *si = ::Sign::GetIfValid(sign_id);
	return si != nullptr && (si->owner == ScriptObject::GetCompany() || si->owner == OWNER_DEITY);
}

/* static */ ScriptCompany::CompanyID ScriptSign::GetOwner(SignID sign_id)
{
	if (!IsValidSign(sign_id)) return ScriptCompany::COMPANY_INVALID;

	return ScriptCompany::ToScriptCompanyID(::Sign::Get(sign_id)->owner);
}

/* static */ bool ScriptSign::SetName(SignID sign_id, Text *name)
{
	ScriptObjectRef counter(name);

	EnforceDeityOrCompanyModeValid(false);
	EnforcePrecondition(false, IsValidSign(sign_id));
	EnforcePrecondition(false, name != nullptr);
	const std::string &text = name->GetDecodedText();
	EnforcePreconditionEncodedText(false, text);
	EnforcePreconditionCustomError(false, ::Utf8StringLength(text) < MAX_LENGTH_SIGN_NAME_CHARS, ScriptError::ERR_PRECONDITION_STRING_TOO_LONG);

	return ScriptObject::Command<CMD_RENAME_SIGN>::Do(sign_id, text);
}

/* static */ std::optional<std::string> ScriptSign::GetName(SignID sign_id)
{
	if (!IsValidSign(sign_id)) return std::nullopt;

	return ::StrMakeValid(::GetString(STR_SIGN_NAME, sign_id), {});
}

/* static */ TileIndex ScriptSign::GetLocation(SignID sign_id)
{
	if (!IsValidSign(sign_id)) return INVALID_TILE;

	const Sign *sign = ::Sign::Get(sign_id);
	return ::TileVirtXY(sign->x, sign->y);
}

/* static */ bool ScriptSign::RemoveSign(SignID sign_id)
{
	EnforceDeityOrCompanyModeValid(false);
	EnforcePrecondition(false, IsValidSign(sign_id));
	return ScriptObject::Command<CMD_RENAME_SIGN>::Do(sign_id, "");
}

/* static */ SignID ScriptSign::BuildSign(TileIndex location, Text *name)
{
	ScriptObjectRef counter(name);

	EnforceDeityOrCompanyModeValid(SignID::Invalid());
	EnforcePrecondition(SignID::Invalid(), ::IsValidTile(location));
	EnforcePrecondition(SignID::Invalid(), name != nullptr);
	const std::string &text = name->GetDecodedText();
	EnforcePreconditionEncodedText(SignID::Invalid(), text);
	EnforcePreconditionCustomError(SignID::Invalid(), ::Utf8StringLength(text) < MAX_LENGTH_SIGN_NAME_CHARS, ScriptError::ERR_PRECONDITION_STRING_TOO_LONG);

	if (!ScriptObject::Command<CMD_PLACE_SIGN>::Do(&ScriptInstance::DoCommandReturnSignID, location, text)) return SignID::Invalid();

	/* In case of test-mode, we return SignID 0 */
	return SignID::Begin();
}
