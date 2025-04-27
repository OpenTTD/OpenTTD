/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texteff.hpp Functions related to text effects. */

#ifndef TEXTEFF_HPP
#define TEXTEFF_HPP

#include "economy_type.h"
#include "gfx_type.h"
#include "strings_type.h"
#include "viewport_type.h"
#include "core/pool_type.hpp"

/**
 * Text effect modes.
 */
enum class TextEffectMode : uint8_t {
	Invalid, ///< Text effect is invalid.
	Rising, ///< Make the text effect slowly go upwards
	Static, ///< Keep the text effect static
};

using TextEffectID = uint16_t;

static const TextEffectID INVALID_TE_ID = UINT16_MAX;

/** The type of IDs for GameScript-created text effects. */
using ScriptTextEffectID = PoolID<uint32_t, struct ScriptTextEffectIDTag, 65535, UINT32_MAX>;

/** Sentinel value for an invalid GameScript-created text effect. */
static constexpr ScriptTextEffectID INVALID_SCRIPT_TEXT_EFFECT_ID = ScriptTextEffectID::Invalid();

TextEffectID AddTextEffect(EncodedString &&msg, int x, int y, uint8_t duration, TextEffectMode mode);

struct ScriptTextEffectData;
/** Type of the pool holding all GameScript-created text effects. */
using ScriptTextEffectDataPool = Pool<ScriptTextEffectData, ScriptTextEffectID, 16>;
extern ScriptTextEffectDataPool _script_text_effect_pool; ///< Pool holding all GameScript-created text effects.

/** Authoritative state for a GameScript-created text effect. */
struct ScriptTextEffectData : ScriptTextEffectDataPool::PoolItem<&_script_text_effect_pool> {
	int32_t x = 0; ///< X coordinate in the game world.
	int32_t y = 0; ///< Y coordinate in the game world.
	TextEffectMode mode = TextEffectMode::Invalid; ///< Type of text effect.
	uint8_t duration = 0; ///< Remaining lifetime in game ticks for rising effects.
	Colours colour = Colours::Invalid; ///< Text colour.
	EncodedString msg{}; ///< Encoded message.
	ViewportSign sign{}; ///< Client-side viewport position cache.

	/**
	 * Create an empty text effect, to be filled in later.
	 * @param index Pool index to create the text effect at.
	 */
	ScriptTextEffectData(ScriptTextEffectID index) : ScriptTextEffectDataPool::PoolItem<&_script_text_effect_pool>(index) {}

	/**
	 * Create a fully defined text effect.
	 * @param index Pool index to create the text effect at.
	 * @param x X coordinate in the game world.
	 * @param y Y coordinate in the game world.
	 * @param mode Type of text effect.
	 * @param duration Remaining lifetime in game ticks for rising effects.
	 * @param colour Text colour.
	 * @param msg Encoded message to show.
	 */
	ScriptTextEffectData(ScriptTextEffectID index, int32_t x, int32_t y, TextEffectMode mode, uint8_t duration, Colours colour, const EncodedString &msg) :
		ScriptTextEffectDataPool::PoolItem<&_script_text_effect_pool>(index), x(x), y(y), mode(mode), duration(duration), colour(colour), msg(msg) {}

	~ScriptTextEffectData();

	void UpdateVirtCoord();
};

void InitTextEffects();
void DrawTextEffects(DrawPixelInfo *dpi);
void UpdateTextEffect(TextEffectID effect_id, EncodedString &&msg);
void RemoveTextEffect(TextEffectID effect_id);
void UpdateAllTextEffectVirtCoords();

/* misc_gui.cpp */
TextEffectID ShowFillingPercent(int x, int y, int z, uint8_t percent, StringID colour);
void UpdateFillingPercent(TextEffectID te_id, uint8_t percent, StringID colour);
void HideFillingPercent(TextEffectID *te_id);

void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost);
void ShowFeederIncomeAnimation(int x, int y, int z, Money transfer, Money income);

#endif /* TEXTEFF_HPP */
