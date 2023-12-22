/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file palette.cpp Handling of palettes. */

#include "stdafx.h"
#include "blitter/base.hpp"
#include "blitter/factory.hpp"
#include "fileio_func.h"
#include "gfx_type.h"
#include "landscape_type.h"
#include "palette_func.h"
#include "settings_type.h"
#include "thread.h"

#include "table/palettes.h"

#include "safeguards.h"

Palette _cur_palette;

byte _colour_gradient[COLOUR_END][8];

static std::recursive_mutex _palette_mutex; ///< To coordinate access to _cur_palette.

/**
 * PALETTE_BITS reduces the bits-per-channel of 32bpp graphics data to allow faster palette lookups from
 * a smaller lookup table.
 *
 * 6 bpc is chosen as this results in a palette lookup table of 256KiB with adequate fidelty.
 * In constract, a 5 bpc lookup table would be 32KiB, and 7 bpc would be 2MiB.
 *
 * Values in the table are filled as they are first encountered -- larger lookup table means more colour
 * distance calculations, and is therefore slower.
 */
const uint PALETTE_BITS = 6;
const uint PALETTE_SHIFT = 8 - PALETTE_BITS;
const uint PALETTE_BITS_MASK = ((1U << PALETTE_BITS) - 1) << PALETTE_SHIFT;
const uint PALETTE_BITS_OR = (1U << (PALETTE_SHIFT - 1));

/* Palette and reshade lookup table. */
using PaletteLookup = std::array<uint8_t, 1U << (PALETTE_BITS * 3)>;
static PaletteLookup _palette_lookup{};

/**
 * Reduce bits per channel to PALETTE_BITS, and place value in the middle of the reduced range.
 * This is to counteract the information lost between bright and dark pixels, e.g if PALETTE_BITS was 2:
 *    0 -  63 ->  32
 *   64 - 127 ->  96
 *  128 - 191 -> 160
 *  192 - 255 -> 224
 * @param c 8 bit colour component.
 * @returns Colour component reduced to PALETTE_BITS.
 */
inline uint CrunchColour(uint c)
{
	return (c & PALETTE_BITS_MASK) | PALETTE_BITS_OR;
}

/**
 * Calculate distance between two colours.
 * @param col1 First colour.
 * @param r2 Red component of second colour.
 * @param g2 Green component of second colour.
 * @param b2 Blue component of second colour.
 * @returns Euclidean distance between first and second colour.
 */
static uint CalculateColourDistance(const Colour &col1, int r2, int g2, int b2)
{
	/* Euclidean colour distance for sRGB based on https://en.wikipedia.org/wiki/Color_difference#sRGB */
	int r = (int)col1.r - (int)r2;
	int g = (int)col1.g - (int)g2;
	int b = (int)col1.b - (int)b2;

	int avgr = (col1.r + r2) / 2;
	return ((2 + (avgr / 256.0)) * r * r) + (4 * g * g) + ((2 + ((255 - avgr) / 256.0)) * b * b);
}

/* Palette indexes for conversion. See docs/palettes/palette_key.png */
const uint8_t PALETTE_INDEX_CC_START = 198; ///< Palette index of start of company colour remap area.
const uint8_t PALETTE_INDEX_CC_END = PALETTE_INDEX_CC_START + 8; ///< Palette index of end of company colour remap area.
const uint8_t PALETTE_INDEX_START = 1; ///< Palette index of start of defined palette.
const uint8_t PALETTE_INDEX_END = 215; ///< Palette index of end of defined palette.

/**
 * Find nearest colour palette index for a 32bpp pixel.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @returns palette index of nearest colour.
 */
static uint8_t FindNearestColourIndex(uint8_t r, uint8_t g, uint8_t b)
{
	r = CrunchColour(r);
	g = CrunchColour(g);
	b = CrunchColour(b);

	uint best_index = 0;
	uint best_distance = UINT32_MAX;

	for (uint i = PALETTE_INDEX_START; i < PALETTE_INDEX_CC_START; i++) {
		if (uint distance = CalculateColourDistance(_palette.palette[i], r, g, b); distance < best_distance) {
			best_index = i;
			best_distance = distance;
		}
	}
	/* There's a hole in the palette reserved for company colour remaps. */
	for (uint i = PALETTE_INDEX_CC_END; i < PALETTE_INDEX_END; i++) {
		if (uint distance = CalculateColourDistance(_palette.palette[i], r, g, b); distance < best_distance) {
			best_index = i;
			best_distance = distance;
		}
	}
	return best_index;
}

/**
 * Get nearest colour palette index from an RGB colour.
 * A search is performed if this colour is not already in the lookup table.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @returns nearest colour palette index.
 */
uint8_t GetNearestColourIndex(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t key = (r >> PALETTE_SHIFT) | (g >> PALETTE_SHIFT) << PALETTE_BITS | (b >> PALETTE_SHIFT) << (PALETTE_BITS * 2);
	if (_palette_lookup[key] == 0) _palette_lookup[key] = FindNearestColourIndex(r, g, b);
	return _palette_lookup[key];
}

void DoPaletteAnimations();

void GfxInitPalettes()
{
	std::lock_guard<std::recursive_mutex> lock(_palette_mutex);
	memcpy(&_cur_palette, &_palette, sizeof(_cur_palette));
	DoPaletteAnimations();
}

/**
 * Copy the current palette if the palette was updated.
 * Used by video-driver to get a current up-to-date version of the palette,
 * to avoid two threads accessing the same piece of memory (with a good chance
 * one is already updating the palette while the other is drawing based on it).
 * @param local_palette The location to copy the palette to.
 * @param force_copy Whether to ignore if there is an update for the palette.
 * @return True iff a copy was done.
 */
bool CopyPalette(Palette &local_palette, bool force_copy)
{
	std::lock_guard<std::recursive_mutex> lock(_palette_mutex);

	if (!force_copy && _cur_palette.count_dirty == 0) return false;

	local_palette = _cur_palette;
	_cur_palette.count_dirty = 0;

	if (force_copy) {
		local_palette.first_dirty = 0;
		local_palette.count_dirty = 256;
	}

	return true;
}

#define EXTR(p, q) (((uint16_t)(palette_animation_counter * (p)) * (q)) >> 16)
#define EXTR2(p, q) (((uint16_t)(~palette_animation_counter * (p)) * (q)) >> 16)

void DoPaletteAnimations()
{
	std::lock_guard<std::recursive_mutex> lock(_palette_mutex);

	/* Animation counter for the palette animation. */
	static int palette_animation_counter = 0;
	palette_animation_counter += 8;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	const Colour *s;
	const ExtraPaletteValues *ev = &_extra_palette_values;
	Colour old_val[PALETTE_ANIM_SIZE];
	const uint old_tc = palette_animation_counter;
	uint j;

	if (blitter != nullptr && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		palette_animation_counter = 0;
	}

	Colour *palette_pos = &_cur_palette.palette[PALETTE_ANIM_START];  // Points to where animations are taking place on the palette
	/* Makes a copy of the current animation palette in old_val,
	 * so the work on the current palette could be compared, see if there has been any changes */
	memcpy(old_val, palette_pos, sizeof(old_val));

	/* Fizzy Drink bubbles animation */
	s = ev->fizzy_drink;
	j = EXTR2(512, EPV_CYCLES_FIZZY_DRINK);
	for (uint i = 0; i != EPV_CYCLES_FIZZY_DRINK; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_FIZZY_DRINK) j = 0;
	}

	/* Oil refinery fire animation */
	s = ev->oil_refinery;
	j = EXTR2(512, EPV_CYCLES_OIL_REFINERY);
	for (uint i = 0; i != EPV_CYCLES_OIL_REFINERY; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_OIL_REFINERY) j = 0;
	}

	/* Radio tower blinking */
	{
		byte i = (palette_animation_counter >> 1) & 0x7F;
		byte v;

		if (i < 0x3f) {
			v = 255;
		} else if (i < 0x4A || i >= 0x75) {
			v = 128;
		} else {
			v = 20;
		}
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;

		i ^= 0x40;
		if (i < 0x3f) {
			v = 255;
		} else if (i < 0x4A || i >= 0x75) {
			v = 128;
		} else {
			v = 20;
		}
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;
	}

	/* Handle lighthouse and stadium animation */
	s = ev->lighthouse;
	j = EXTR(256, EPV_CYCLES_LIGHTHOUSE);
	for (uint i = 0; i != EPV_CYCLES_LIGHTHOUSE; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_LIGHTHOUSE) j = 0;
	}

	/* Dark blue water */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->dark_water_toyland : ev->dark_water;
	j = EXTR(320, EPV_CYCLES_DARK_WATER);
	for (uint i = 0; i != EPV_CYCLES_DARK_WATER; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_DARK_WATER) j = 0;
	}

	/* Glittery water */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->glitter_water_toyland : ev->glitter_water;
	j = EXTR(128, EPV_CYCLES_GLITTER_WATER);
	for (uint i = 0; i != EPV_CYCLES_GLITTER_WATER / 3; i++) {
		*palette_pos++ = s[j];
		j += 3;
		if (j >= EPV_CYCLES_GLITTER_WATER) j -= EPV_CYCLES_GLITTER_WATER;
	}

	if (blitter != nullptr && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		palette_animation_counter = old_tc;
	} else if (_cur_palette.count_dirty == 0 && memcmp(old_val, &_cur_palette.palette[PALETTE_ANIM_START], sizeof(old_val)) != 0) {
		/* Did we changed anything on the palette? Seems so.  Mark it as dirty */
		_cur_palette.first_dirty = PALETTE_ANIM_START;
		_cur_palette.count_dirty = PALETTE_ANIM_SIZE;
	}
}

/**
 * Determine a contrasty text colour for a coloured background.
 * @param background Background colour.
 * @param threshold Background colour brightness threshold below which the background is considered dark and TC_WHITE is returned, range: 0 - 255, default 128.
 * @return TC_BLACK or TC_WHITE depending on what gives a better contrast.
 */
TextColour GetContrastColour(uint8_t background, uint8_t threshold)
{
	Colour c = _cur_palette.palette[background];
	/* Compute brightness according to http://www.w3.org/TR/AERT#color-contrast.
	 * The following formula computes 1000 * brightness^2, with brightness being in range 0 to 255. */
	uint sq1000_brightness = c.r * c.r * 299 + c.g * c.g * 587 + c.b * c.b * 114;
	/* Compare with threshold brightness which defaults to 128 (50%) */
	return sq1000_brightness < ((uint) threshold) * ((uint) threshold) * 1000 ? TC_WHITE : TC_BLACK;
}
