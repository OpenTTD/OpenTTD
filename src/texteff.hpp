/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file texteff.hpp Functions related to text effects. */

#ifndef TEXTEFF_HPP
#define TEXTEFF_HPP

#include "economy_type.h"
#include "gfx_type.h"
#include "strings_type.h"

/**
 * Text effect modes.
 */
enum TextEffectMode : uint8_t {
	TE_RISING, ///< Make the text effect slowly go upwards
	TE_STATIC, ///< Keep the text effect static
};

using TextEffectID = uint16_t;

static const TextEffectID INVALID_TE_ID = UINT16_MAX;

TextEffectID AddTextEffect(StringID msg, int x, int y, uint8_t duration, TextEffectMode mode);
void InitTextEffects();
void DrawTextEffects(DrawPixelInfo *dpi);
void UpdateTextEffect(TextEffectID effect_id, StringID msg);
void RemoveTextEffect(TextEffectID effect_id);
void UpdateAllTextEffectVirtCoords();

/* misc_gui.cpp */
TextEffectID ShowFillingPercent(int x, int y, int z, uint8_t percent, StringID colour);
void UpdateFillingPercent(TextEffectID te_id, uint8_t percent, StringID colour);
void HideFillingPercent(TextEffectID *te_id);

void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost);
void ShowFeederIncomeAnimation(int x, int y, int z, Money transfer, Money income);

#endif /* TEXTEFF_HPP */
