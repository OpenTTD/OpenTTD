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
#include "viewport_func.h"
#include "settings_type.h"
#include "command_type.h"
#include "timer/timer.h"
#include "timer/timer_window.h"

#include "safeguards.h"

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

static std::vector<struct TextEffect> _text_effects; ///< Text effects are stored there

/* Text Effects */
TextEffectID AddTextEffect(StringID msg, int center, int y, uint8 duration, TextEffectMode mode)
{
	if (_game_mode == GM_MENU) return INVALID_TE_ID;

	TextEffectID i;
	for (i = 0; i < _text_effects.size(); i++) {
		if (_text_effects[i].string_id == INVALID_STRING_ID) break;
	}
	if (i == _text_effects.size()) _text_effects.emplace_back();

	TextEffect &te = _text_effects[i];

	/* Start defining this object */
	te.string_id = msg;
	te.duration = duration;
	te.params_1 = GetDParam(0);
	te.params_2 = GetDParam(1);
	te.mode = mode;

	/* Make sure we only dirty the new area */
	te.width_normal = 0;
	te.UpdatePosition(center, y, msg);

	return i;
}

void UpdateTextEffect(TextEffectID te_id, StringID msg)
{
	/* Update details */
	TextEffect *te = _text_effects.data() + te_id;
	if (msg == te->string_id && GetDParam(0) == te->params_1) return;
	te->string_id = msg;
	te->params_1 = GetDParam(0);
	te->params_2 = GetDParam(1);

	te->UpdatePosition(te->center, te->top, te->string_id, te->string_id - 1);
}

void UpdateAllTextEffectVirtCoords()
{
	for (auto &te : _text_effects) {
		if (te.string_id == INVALID_STRING_ID) continue;
		SetDParam(0, te.params_1);
		SetDParam(1, te.params_2);
		te.UpdatePosition(te.center, te.top, te.string_id, te.string_id - 1);
	}
}

void RemoveTextEffect(TextEffectID te_id)
{
	_text_effects[te_id].Reset();
}

/** Slowly move text effects upwards. */
IntervalTimer<TimerWindow> move_all_text_effects_interval = {std::chrono::milliseconds(30), [](uint count) {
	if (_pause_mode && _game_mode != GM_EDITOR && _settings_game.construction.command_pause_level <= CMDPL_NO_CONSTRUCTION) return;

	for (TextEffect &te : _text_effects) {
		if (te.string_id == INVALID_STRING_ID) continue;
		if (te.mode != TE_RISING) continue;

		if (te.duration < count) {
			te.Reset();
			continue;
		}

		te.MarkDirty(ZOOM_LVL_OUT_8X);
		te.duration -= count;
		te.top -= count * ZOOM_LVL_BASE;
		te.MarkDirty(ZOOM_LVL_OUT_8X);
	}
}};

void InitTextEffects()
{
	_text_effects.clear();
	_text_effects.shrink_to_fit();
}

void DrawTextEffects(DrawPixelInfo *dpi)
{
	/* Don't draw the text effects when zoomed out a lot */
	if (dpi->zoom > ZOOM_LVL_OUT_8X) return;

	for (TextEffect &te : _text_effects) {
		if (te.string_id == INVALID_STRING_ID) continue;
		if (te.mode == TE_RISING || (_settings_client.gui.loading_indicators && !IsTransparencySet(TO_LOADING))) {
			SetDParam(0, te.params_1);
			SetDParam(1, te.params_2);
			ViewportAddString(dpi, ZOOM_LVL_OUT_8X, &te, te.string_id, te.string_id - 1, STR_NULL);
		}
	}
}
