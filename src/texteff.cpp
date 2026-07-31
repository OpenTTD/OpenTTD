/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texteff.cpp Handling of text effects. */

#include "stdafx.h"
#include "texteff.hpp"
#include "landscape.h"
#include "transparency.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "settings_type.h"
#include "command_type.h"
#include "core/pool_func.hpp"
#include "timer/timer.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_window.h"

#include "safeguards.h"

/** Container for all information about a text effect */
struct TextEffect : public ViewportSign {
	TextEffectMode mode; ///< Type of text effect.
	uint8_t duration; ///< How long the text effect should stay, in ticks (applies only when mode == TextEffectMode::Rising)
	EncodedString msg; ///< Encoded message for text effect.

	/** Reset the text effect */
	void Reset()
	{
		this->MarkDirty();
		this->width_normal = 0;
		this->mode = TextEffectMode::Invalid;
	}

	inline bool IsValid() const { return this->mode != TextEffectMode::Invalid; }
};

static std::vector<TextEffect> _text_effects; ///< Text effects are stored there

ScriptTextEffectDataPool _script_text_effect_pool("ScriptTextEffect"); ///< Pool holding all GameScript-created text effects.
INSTANTIATE_POOL_METHODS(ScriptTextEffectData)

/** Remove the text effect from the screen. */
ScriptTextEffectData::~ScriptTextEffectData()
{
	if (!CleaningPool()) this->sign.MarkDirty();
}

/** Recompute the viewport position of this text effect. */
void ScriptTextEffectData::UpdateVirtCoord()
{
	Point pt = RemapCoords(this->x, this->y, GetSlopePixelZ(this->x, this->y));
	int offset = this->mode == TextEffectMode::Rising ? (Ticks::DAY_TICKS - this->duration) * ZOOM_BASE : 0;
	this->sign.UpdatePosition(pt.x, pt.y - offset, this->msg.GetDecodedString());
}

/**
 * Add a text effect to the list of text effects.
 * @param msg Encoded message to show.
 * @param center Horizontal center of the text effect, in viewport coordinates.
 * @param y Top of the text effect, in viewport coordinates.
 * @param duration Lifetime in game ticks for rising effects.
 * @param mode Type of text effect.
 * @return The ID of the new text effect, or INVALID_TE_ID when none could be allocated.
 */
static TextEffectID AddTextEffectInternal(EncodedString &&msg, int center, int y, uint8_t duration, TextEffectMode mode)
{
	if (_game_mode == GameMode::Menu) return INVALID_TE_ID;

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

/* Text Effects */
TextEffectID AddTextEffect(EncodedString &&msg, int center, int y, uint8_t duration, TextEffectMode mode)
{
	return AddTextEffectInternal(std::move(msg), center, y, duration, mode);
}

void UpdateTextEffect(TextEffectID te_id, EncodedString &&msg)
{
	if (te_id >= _text_effects.size() || !_text_effects[te_id].IsValid()) return;

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

	for (ScriptTextEffectData *te : ScriptTextEffectData::Iterate()) {
		te->UpdateVirtCoord();
	}
}

void RemoveTextEffect(TextEffectID te_id)
{
	if (te_id >= _text_effects.size() || !_text_effects[te_id].IsValid()) return;
	_text_effects[te_id].Reset();
}

/** Slowly move text effects upwards. */
const IntervalTimer<TimerWindow> move_all_text_effects_interval = {std::chrono::milliseconds(30), [](uint count) {
	if (_pause_mode.Any() && _game_mode != GameMode::Editor && _settings_game.construction.command_pause_level <= CommandPauseLevel::NoConstruction) return;

	for (TextEffect &te : _text_effects) {
		if (!te.IsValid()) continue;
		if (te.mode != TextEffectMode::Rising) continue;

		if (te.duration < count) {
			te.Reset();
			continue;
		}

		te.MarkDirty(ZoomLevel::TextEffect);
		te.duration -= count;
		te.top -= count * ZOOM_BASE;
		te.MarkDirty(ZoomLevel::TextEffect);
	}
}};

/** Move and expire GameScript-created rising effects deterministically. */
const IntervalTimer<TimerGameTick> move_script_text_effects_interval = {{TimerGameTick::Priority::None, 1}, [](uint count) {
	for (ScriptTextEffectData *te : ScriptTextEffectData::Iterate()) {
		if (te->mode != TextEffectMode::Rising) continue;

		if (te->duration < count) {
			delete te;
			continue;
		}

		te->sign.MarkDirty(ZoomLevel::TextEffect);
		te->duration -= count;
		te->sign.top -= count * ZOOM_BASE;
		te->sign.MarkDirty(ZoomLevel::TextEffect);
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
	if (dpi->zoom > ZoomLevel::TextEffect) return;
	if (IsTransparencySet(TransparencyOption::Text)) return;

	ViewportStringFlags flags{};
	if (dpi->zoom >= ZoomLevel::TextEffect) flags.Set(ViewportStringFlag::Small);

	for (const TextEffect &te : _text_effects) {
		if (!te.IsValid()) continue;

		if (te.mode == TextEffectMode::Rising || _settings_client.gui.loading_indicators) {
			std::string *str = ViewportAddString(dpi, &te, flags, Colours::Invalid);
			if (str == nullptr) continue;

			*str = te.msg.GetDecodedString();
		}
	}

	for (const ScriptTextEffectData *te : ScriptTextEffectData::Iterate()) {
		ViewportStringFlags effect_flags = flags;
		Colours colour = te->colour;
		effect_flags.Set(ViewportStringFlag::TextColour);
		if (colour == Colours::White) colour = Colours::Invalid;

		std::string *str = ViewportAddString(dpi, &te->sign, effect_flags, colour);
		if (str == nullptr) continue;

		*str = te->msg.GetDecodedString();
	}
}
