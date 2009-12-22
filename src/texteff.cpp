/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file texteff.cpp Handling of text effects. */

#include "stdafx.h"
#include "openttd.h"
#include "strings_type.h"
#include "texteff.hpp"
#include "core/bitmath_func.hpp"
#include "transparency.h"
#include "strings_func.h"
#include "core/alloc_func.hpp"
#include "viewport_func.h"
#include "settings_type.h"

enum {
	INIT_NUM_TEXT_EFFECTS  =  20,
};

/** Container for all information about a text effect */
struct TextEffect : public ViewportSign{
	StringID string_id;  ///< String to draw for the text effect, if INVALID_STRING_ID then it's not valid
	uint16 duration;     ///< How long the text effect should stay
	uint64 params_1;     ///< DParam parameter
	TextEffectMode mode; ///< Type of text effect

	/** Reset the text effect */
	void Reset()
	{
		this->MarkDirty();
		this->width_normal = 0;
		this->string_id = INVALID_STRING_ID;
	}
};

/* used for text effects */
static TextEffect *_text_effect_list = NULL;
static uint16 _num_text_effects = INIT_NUM_TEXT_EFFECTS;

/* Text Effects */
TextEffectID AddTextEffect(StringID msg, int center, int y, uint16 duration, TextEffectMode mode)
{
	TextEffectID i;

	if (_game_mode == GM_MENU) return INVALID_TE_ID;

	/* Look for a free spot in the text effect array */
	for (i = 0; i < _num_text_effects; i++) {
		if (_text_effect_list[i].string_id == INVALID_STRING_ID) break;
	}

	/* If there is none found, we grow the array */
	if (i == _num_text_effects) {
		_num_text_effects += 25;
		_text_effect_list = ReallocT<TextEffect>(_text_effect_list, _num_text_effects);
		for (; i < _num_text_effects; i++) _text_effect_list[i].string_id = INVALID_STRING_ID;
		i = _num_text_effects - 1;
	}

	TextEffect *te = &_text_effect_list[i];

	/* Start defining this object */
	te->string_id = msg;
	te->duration = duration;
	te->params_1 = GetDParam(0);
	te->mode = mode;

	/* Make sure we only dirty the new area */
	te->width_normal = 0;
	te->UpdatePosition(center, y, msg);

	return i;
}

void UpdateTextEffect(TextEffectID te_id, StringID msg)
{
	assert(te_id < _num_text_effects);

	/* Update details */
	TextEffect *te = &_text_effect_list[te_id];
	te->string_id = msg;
	te->params_1 = GetDParam(0);

	te->UpdatePosition(te->center, te->top, msg);
}

void RemoveTextEffect(TextEffectID te_id)
{
	assert(te_id < _num_text_effects);
	_text_effect_list[te_id].Reset();
}

static void MoveTextEffect(TextEffect *te)
{
	/* Never expire for duration of 0xFFFF */
	if (te->duration == 0xFFFF) return;
	if (te->duration < 8) {
		te->Reset();
	} else {
		te->duration -= 8;
		te->MarkDirty();
		te->top--;
		te->MarkDirty();
	}
}

void MoveAllTextEffects()
{
	for (TextEffectID i = 0; i < _num_text_effects; i++) {
		TextEffect *te = &_text_effect_list[i];
		if (te->string_id != INVALID_STRING_ID && te->mode == TE_RISING) MoveTextEffect(te);
	}
}

void InitTextEffects()
{
	if (_text_effect_list == NULL) _text_effect_list = MallocT<TextEffect>(_num_text_effects);

	for (TextEffectID i = 0; i < _num_text_effects; i++) _text_effect_list[i].string_id = INVALID_STRING_ID;
}

void DrawTextEffects(DrawPixelInfo *dpi)
{
	/* Don't draw the text effects when zoomed out a lot */
	if (dpi->zoom > ZOOM_LVL_OUT_2X) return;

	for (TextEffectID i = 0; i < _num_text_effects; i++) {
		const TextEffect *te = &_text_effect_list[i];
		if (te->string_id == INVALID_STRING_ID) continue;

		if (te->mode == TE_RISING || (_settings_client.gui.loading_indicators && !IsTransparencySet(TO_LOADING))) {
			ViewportAddString(dpi, ZOOM_LVL_OUT_2X, te, te->string_id, te->string_id - 1, 0, te->params_1);
		}
	}
}
