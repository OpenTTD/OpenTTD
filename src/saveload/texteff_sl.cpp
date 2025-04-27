/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texteff_sl.cpp Code handling saving and loading of GameScript text effects. */

#include "../stdafx.h"

#include "saveload.h"

#include "../texteff.hpp"

#include "../safeguards.h"

/** Description of the saved fields of a GameScript text effect. */
static const SaveLoad _script_text_effect_desc[] = {
	    SLE_VAR(ScriptTextEffectData, x,        VarTypes::I32),
	    SLE_VAR(ScriptTextEffectData, y,        VarTypes::I32),
	    SLE_VAR(ScriptTextEffectData, mode,     VarTypes::U8),
	    SLE_VAR(ScriptTextEffectData, duration, VarTypes::U8),
	    SLE_VAR(ScriptTextEffectData, colour,   VarTypes::U8),
	   SLE_SSTR(ScriptTextEffectData, msg,      VarTypes::STR | StringValidationSetting::AllowControlCode),
};

/** Chunk handler for GameScript text effects. */
struct TEFFChunkHandler : ChunkHandler {
	/** Create the handler for the "TEFF" chunk. */
	TEFFChunkHandler() : ChunkHandler("TEFF", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_script_text_effect_desc);

		for (ScriptTextEffectData *effect : ScriptTextEffectData::Iterate()) {
			SlSetArrayIndex(effect->index);
			SlObject(effect, _script_text_effect_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_script_text_effect_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			ScriptTextEffectData *effect = ScriptTextEffectData::CreateAtIndex(ScriptTextEffectID(index));
			SlObject(effect, slt);
		}
	}
};

static const TEFFChunkHandler TEFF; ///< Handler for the "TEFF" chunk.

/** All chunk handlers related to text effects. */
static const ChunkHandlerRef text_effect_chunk_handlers[] = {
	TEFF,
};

/** Table of all chunk handlers related to text effects. */
extern const ChunkHandlerTable _text_effect_chunk_handlers{text_effect_chunk_handlers};
