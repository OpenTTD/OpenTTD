/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file texteff.cpp Handling of text effects. */

#include "stdafx.h"
#include "texteff.hpp"
#include "transparency.h"
#include "strings_func.h"
#include "core/smallvec_type.hpp"
#include "viewport_func.h"
#include "settings_type.h"

/** Container for all information about a text effect */
struct TextEffect : public ViewportSign {
	uint64 params_1;     ///< DParam parameter
	uint64 params_2;     ///< second DParam parameter
	StringID string_id;  ///< String to draw for the text effect, if INVALID_STRING_ID then it's not valid
	uint8 duration;      ///< How long the text effect should stay, in ticks (applies only when mode == TE_RISING)
	TextEffectMode mode; ///< Type of text effect

	/** Reset the text effect */
	void Reset()
	{
		this->MarkDirty();
		this->width_normal = 0;
		this->string_id = INVALID_STRING_ID;
	}
};

static SmallVector<struct TextEffect, 32> _text_effects; ///< Text effects are stored there

/* Text Effects */
TextEffectID AddTextEffect(StringID msg, int center, int y, uint8 duration, TextEffectMode mode)
{
	if (_game_mode == GM_MENU) return INVALID_TE_ID;

	TextEffectID i;
	for (i = 0; i < _text_effects.Length(); i++) {
		if (_text_effects[i].string_id == INVALID_STRING_ID) break;
	}
	if (i == _text_effects.Length()) _text_effects.Append();

	TextEffect *te = _text_effects.Get(i);

	/* Start defining this object */
	te->string_id = msg;
	te->duration = duration;
	te->params_1 = GetDParam(0);
	te->params_2 = GetDParam(1);
	te->mode = mode;

	/* Make sure we only dirty the new area */
	te->width_normal = 0;
	te->UpdatePosition(center, y, msg);

	return i;
}

void UpdateTextEffect(TextEffectID te_id, StringID msg)
{
	/* Update details */
	TextEffect *te = _text_effects.Get(te_id);
	if (msg == te->string_id && GetDParam(0) == te->params_1) return;
	te->string_id = msg;
	te->params_1 = GetDParam(0);
	te->params_2 = GetDParam(1);

	te->UpdatePosition(te->center, te->top, msg);
}

void RemoveTextEffect(TextEffectID te_id)
{
	_text_effects[te_id].Reset();
}

void MoveAllTextEffects()
{
	const TextEffect *end = _text_effects.End();
	for (TextEffect *te = _text_effects.Begin(); te != end; te++) {
		if (te->string_id == INVALID_STRING_ID) continue;
		if (te->mode != TE_RISING) continue;

		if (te->duration-- == 0) {
			te->Reset();
			continue;
		}

		te->MarkDirty(ZOOM_LVL_OUT_8X);
		te->top -= ZOOM_LVL_BASE;
		te->MarkDirty(ZOOM_LVL_OUT_8X);
	}
}

void InitTextEffects()
{
	_text_effects.Reset();
}

void DrawTextEffects(DrawPixelInfo *dpi)
{
	/* Don't draw the text effects when zoomed out a lot */
	if (dpi->zoom > ZOOM_LVL_OUT_8X) return;

	const TextEffect *end = _text_effects.End();
	for (TextEffect *te = _text_effects.Begin(); te != end; te++) {
		if (te->string_id == INVALID_STRING_ID) continue;
		if (te->mode == TE_RISING || (_settings_client.gui.loading_indicators && !IsTransparencySet(TO_LOADING))) {
			ViewportAddString(dpi, ZOOM_LVL_OUT_8X, te, te->string_id, te->string_id - 1, 0, te->params_1, te->params_2);
		}
	}
}
