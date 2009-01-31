/* $Id$ */

/** @file ai_sign.cpp Implementation of AISign. */

#include "ai_sign.hpp"
#include "table/strings.h"
#include "../ai_instance.hpp"
#include "../../command_func.h"
#include "../../core/alloc_func.hpp"
#include "../../signs_base.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../tile_map.h"
#include "../../company_func.h"

/* static */ SignID AISign::GetMaxSignID()
{
	return ::GetMaxSignIndex();
}

/* static */ bool AISign::IsValidSign(SignID sign_id)
{
	return ::IsValidSignID(sign_id) && ::GetSign(sign_id)->owner == _current_company;
}

/* static */ bool AISign::SetName(SignID sign_id, const char *name)
{
	EnforcePrecondition(false, IsValidSign(sign_id));
	EnforcePrecondition(false, !::StrEmpty(name));
	EnforcePreconditionCustomError(false, ::strlen(name) < MAX_LENGTH_SIGN_NAME_BYTES, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, sign_id, 0, CMD_RENAME_SIGN, name);
}

/* static */ char *AISign::GetName(SignID sign_id)
{
	if (!IsValidSign(sign_id)) return NULL;

	static const int len = 64;
	char *sign_name = MallocT<char>(len);

	::SetDParam(0, sign_id);
	::GetString(sign_name, STR_SIGN_NAME, &sign_name[len - 1]);

	return sign_name;
}

/* static */ TileIndex AISign::GetLocation(SignID sign_id)
{
	if (!IsValidSign(sign_id)) return INVALID_TILE;

	const Sign *sign = ::GetSign(sign_id);
	return ::TileVirtXY(sign->x, sign->y);
}

/* static */ bool AISign::RemoveSign(SignID sign_id)
{
	EnforcePrecondition(false, IsValidSign(sign_id));
	return AIObject::DoCommand(0, sign_id, 0, CMD_RENAME_SIGN, "");
}

/* static */ SignID AISign::BuildSign(TileIndex location, const char *text)
{
	EnforcePrecondition(INVALID_SIGN, ::IsValidTile(location));
	EnforcePrecondition(INVALID_SIGN, !::StrEmpty(text));
	EnforcePreconditionCustomError(false, ::strlen(text) < MAX_LENGTH_SIGN_NAME_BYTES, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	if (!AIObject::DoCommand(location, 0, 0, CMD_PLACE_SIGN, text, &AIInstance::DoCommandReturnSignID)) return INVALID_SIGN;

	/* In case of test-mode, we return SignID 0 */
	return 0;
}
