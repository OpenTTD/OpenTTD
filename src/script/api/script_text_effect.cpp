/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_text_effect.cpp Implementation of ScriptTextEffect with multiplayer support. */

#include "../../stdafx.h"
#include "script_text_effect.hpp"
#include "script_error.hpp"
#include "script_map.hpp"
#include "../script_instance.hpp"
#include "../../texteff.hpp"
#include "../../strings_func.h"
#include "../../tile_map.h"
#include "../../command_func.h"
#include "../../texteff_cmd.h"

#include "../../safeguards.h"

/* static */ TextEffectID ScriptTextEffect::CreateAtPosition(SQInteger x, SQInteger y, Text *text, ScriptTextEffectMode mode)
{
    ScriptObjectRef counter(text);

    EnforceDeityMode(false);
    EnforcePrecondition(false, text != nullptr);
    EnforcePrecondition(false, !text->GetEncodedText().empty());
    EnforcePrecondition(false, mode == TE_RISING || mode == TE_STATIC);

    return ScriptObject::Command<CMD_CREATE_TEXT_EFFECT>::Do(&ScriptInstance::DoCommandReturnTextEffectID, x, y, (TextEffectMode)mode, text->GetEncodedText());
}

/* static */ TextEffectID ScriptTextEffect::Create(TileIndex tile, Text *text, ScriptTextEffectMode mode)
{
    EnforcePrecondition(false, ScriptMap::IsValidTile(tile));
    
    int x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
    int y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
    
    return CreateAtPosition(x, y, text, mode);
}

/* static */ bool ScriptTextEffect::Update(TextEffectID te_id, Text *text)
{
    ScriptObjectRef counter(text);

    EnforceDeityMode(false);
    EnforcePrecondition(false, te_id != INVALID_TE_ID);
    EnforcePrecondition(false, text != nullptr);
    EnforcePrecondition(false, !text->GetEncodedText().empty());

    return ScriptObject::Command<CMD_UPDATE_TEXT_EFFECT>::Do(te_id, text->GetEncodedText());
}

/* static */ bool ScriptTextEffect::Remove(TextEffectID te_id)
{
    EnforceDeityMode(false);
    EnforcePrecondition(false, te_id != INVALID_TE_ID);

    return ScriptObject::Command<CMD_REMOVE_TEXT_EFFECT>::Do(te_id);    
}