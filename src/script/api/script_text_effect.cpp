/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_text_effect.cpp Implementation of ScriptTextEffect. */

#include "../../stdafx.h"
#include "script_text_effect.hpp"
#include "script_error.hpp"
#include "script_map.hpp"
#include "../../map_func.h"
#include "../../texteff_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptTextEffect::Create(TileIndex tile, Text *text, ScriptCompany::Colours colour)
{
	ScriptObjectRef counter(text);

	EnforcePrecondition(false, ScriptMap::IsValidTile(tile));

	SQInteger x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
	SQInteger y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
	return CreateAtPosition(x, y, text, colour);
}

/* static */ bool ScriptTextEffect::CreateAtPosition(SQInteger x, SQInteger y, Text *text, ScriptCompany::Colours colour)
{
	ScriptObjectRef counter(text);

	EnforceDeityMode(false);
	EnforcePrecondition(false, x >= 0 && x < static_cast<SQInteger>(Map::SizeX() * TILE_SIZE));
	EnforcePrecondition(false, y >= 0 && y < static_cast<SQInteger>(Map::SizeY() * TILE_SIZE));
	EnforcePrecondition(false, text != nullptr);
	EncodedString encoded_text = text->GetEncodedText();
	EnforcePreconditionEncodedText(false, encoded_text);
	EnforcePrecondition(false, colour >= ScriptCompany::COLOUR_DARK_BLUE && colour <= ScriptCompany::COLOUR_WHITE);

	return ScriptObject::Command<Commands::CreateTextEffect>::Do(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<::Colours>(colour), encoded_text);
}
