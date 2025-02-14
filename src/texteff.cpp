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
	TextEffectMode mode; ///< Type of text effect.
	uint8_t duration; ///< How long the text effect should stay, in ticks (applies only when mode == TE_RISING)
	EncodedString msg; ///< Encoded message for text effect.

	/** Reset the text effect */
	void Reset()
	{
		this->MarkDirty();
		this->width_normal = 0;
		this->mode = TE_INVALID;
	}

	inline bool IsValid() const { return this->mode != TE_INVALID; }
};

static std::vector<TextEffect> _text_effects; ///< Text effects are stored there

/* Text Effects */
TextEffectID AddTextEffect(EncodedString &&msg, int center, int y, uint8_t duration, TextEffectMode mode)
{
	if (_game_mode == GM_MENU) return INVALID_TE_ID;

	auto it = std::ranges::find_if(_text_effects, [](const TextEffect &te) { return !te.IsValid(); });
	if (it == std::end(_text_effects)) {
		/* _text_effects.size() is the maximum ID + 1 that has been allocated. We should not allocate INVALID_TE_ID or beyond. */
		if (_text_effects.size() >= INVALID_TE_ID) return INVALID_TE_ID;
		it = _text_effects.emplace(std::end(_text_effects));
	}

	TextEffect &te = *it;

	/* Start defining this object */
	te.msg = std::move(msg);
	te.duration = duration;
	te.mode = mode;

	/* Make sure we only dirty the new area */
	te.width_normal = 0;
	te.UpdatePosition(center, y, te.msg.GetDecodedString());

	return static_cast<TextEffectID>(it - std::begin(_text_effects));
}

void UpdateTextEffect(TextEffectID te_id, EncodedString &&msg)
{
	/* Update details */
	TextEffect &te = _text_effects[te_id];
	if (msg == te.msg) return;
	te.msg = std::move(msg);

	te.UpdatePosition(te.center, te.top, te.msg.GetDecodedString());
}

void UpdateAllTextEffectVirtCoords()
{
	for (auto &te : _text_effects) {
		if (!te.IsValid()) continue;

		te.UpdatePosition(te.center, te.top, te.msg.GetDecodedString());
	}
}

void RemoveTextEffect(TextEffectID te_id)
{
	_text_effects[te_id].Reset();
}

/** Slowly move text effects upwards. */
IntervalTimer<TimerWindow> move_all_text_effects_interval = {std::chrono::milliseconds(30), [](uint count) {
	if (_pause_mode.Any() && _game_mode != GM_EDITOR && _settings_game.construction.command_pause_level <= CMDPL_NO_CONSTRUCTION) return;

	for (TextEffect &te : _text_effects) {
		if (!te.IsValid()) continue;
		if (te.mode != TE_RISING) continue;

		if (te.duration < count) {
			te.Reset();
			continue;
		}

		te.MarkDirty(ZOOM_LVL_TEXT_EFFECT);
		te.duration -= count;
		te.top -= count * ZOOM_BASE;
		te.MarkDirty(ZOOM_LVL_TEXT_EFFECT);
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
	if (dpi->zoom > ZOOM_LVL_TEXT_EFFECT) return;
	if (IsTransparencySet(TO_TEXT)) return;

	ViewportStringFlags flags{};
	if (dpi->zoom >= ZOOM_LVL_TEXT_EFFECT) flags.Set(ViewportStringFlag::Small);

	for (const TextEffect &te : _text_effects) {
		if (!te.IsValid()) continue;

		if (te.mode == TE_RISING || _settings_client.gui.loading_indicators) {
			std::string *str = ViewportAddString(dpi, &te, flags, INVALID_COLOUR);
			if (str == nullptr) continue;

			*str = te.msg.GetDecodedString();
		}
	}
}
