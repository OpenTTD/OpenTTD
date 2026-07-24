/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_text_effect.cpp Implementation of ScriptTextEffect with multiplayer support. */

#include "../../stdafx.h"
#include "script_text_effect.hpp"
#include "script_error.hpp"
#include "script_map.hpp"
#include "../script_instance.hpp"
#include "../../landscape.h"
#include "../../tile_map.h"
#include "../../texteff_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptTextEffect::IsValidTextEffect(ScriptTextEffectID effect_id)
{
	EnforceDeityMode(false);
	return ::ScriptTextEffectData::IsValidID(effect_id);
}

/* static */ ScriptTextEffectID ScriptTextEffect::CreateAtPosition(SQInteger x, SQInteger y, Text *text, TextEffectMode mode, ScriptCompany::Colours colour)
{
	ScriptObjectRef counter(text);

	EnforceDeityMode(TEXT_EFFECT_INVALID);
	EnforcePrecondition(TEXT_EFFECT_INVALID, x >= 0 && x < static_cast<SQInteger>(Map::SizeX() * TILE_SIZE));
	EnforcePrecondition(TEXT_EFFECT_INVALID, y >= 0 && y < static_cast<SQInteger>(Map::SizeY() * TILE_SIZE));
	EnforcePrecondition(TEXT_EFFECT_INVALID, text != nullptr);
	EncodedString encoded_text = text->GetEncodedText();
	EnforcePreconditionEncodedText(TEXT_EFFECT_INVALID, encoded_text);
	EnforcePrecondition(TEXT_EFFECT_INVALID, mode == TE_RISING || mode == TE_STATIC);
	EnforcePrecondition(TEXT_EFFECT_INVALID, colour >= ScriptCompany::COLOUR_DARK_BLUE && colour <= ScriptCompany::COLOUR_WHITE);

	if (!ScriptObject::Command<Commands::CreateTextEffect>::Do(&ScriptInstance::DoCommandReturnTextEffectID, static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<::TextEffectMode>(mode), static_cast<::Colours>(colour), encoded_text)) {
		return TEXT_EFFECT_INVALID;
	}

	/* In test mode, return the first valid ID. */
	return ScriptTextEffectID::Begin();
}

/* static */ ScriptTextEffectID ScriptTextEffect::Create(TileIndex tile, Text *text, TextEffectMode mode, ScriptCompany::Colours colour)
{
	EnforcePrecondition(TEXT_EFFECT_INVALID, ScriptMap::IsValidTile(tile));

	SQInteger x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
	SQInteger y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
	return CreateAtPosition(x, y, text, mode, colour);
}

/* static */ bool ScriptTextEffect::Update(ScriptTextEffectID effect_id, Text *text, ScriptCompany::Colours colour)
{
	ScriptObjectRef counter(text);

	EnforceDeityMode(false);
	EnforcePrecondition(false, IsValidTextEffect(effect_id));
	EnforcePrecondition(false, text != nullptr);
	EncodedString encoded_text = text->GetEncodedText();
	EnforcePreconditionEncodedText(false, encoded_text);
	EnforcePrecondition(false, colour >= ScriptCompany::COLOUR_DARK_BLUE && colour <= ScriptCompany::COLOUR_WHITE);

	return ScriptObject::Command<Commands::UpdateTextEffect>::Do(effect_id, static_cast<::Colours>(colour), encoded_text);
}

/* static */ bool ScriptTextEffect::Remove(ScriptTextEffectID effect_id)
{
	EnforceDeityMode(false);
	EnforcePrecondition(false, IsValidTextEffect(effect_id));

	return ScriptObject::Command<Commands::RemoveTextEffect>::Do(effect_id);
}
