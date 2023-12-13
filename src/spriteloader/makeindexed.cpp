/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file makeindexed.cpp Implementation for converting sprites from another source from 32bpp RGBA to indexed 8bpp. */

#include "../stdafx.h"
#include "../core/bitmath_func.hpp"
#include "../core/math_func.hpp"
#include "../gfx_func.h"
#include "../palette_func.h"
#include "makeindexed.h"

#include "../safeguards.h"

/**
 * Adjust brightness of colour.
 * This is a copy of Blitter_32bppBase::ReallyAdjustBrightness.
 * @param colour Colour to adjust.
 * @param brightness Brightess to apply to colour.
 * @returns Adjusted coloured.
 */
static Colour AdjustBrightness(Colour colour, int brightness)
{
	uint64_t combined = (((uint64_t) colour.r) << 32) | (((uint64_t) colour.g) << 16) | ((uint64_t) colour.b);
	combined *= brightness;

	uint16_t r = GB(combined, 39, 9);
	uint16_t g = GB(combined, 23, 9);
	uint16_t b = GB(combined, 7, 9);

	if ((combined & 0x800080008000L) == 0L) {
		return Colour(r, g, b, colour.a);
	}

	uint16_t ob = 0;
	/* Sum overbright */
	if (r > 255) ob += r - 255;
	if (g > 255) ob += g - 255;
	if (b > 255) ob += b - 255;

	/* Reduce overbright strength */
	ob /= 2;
	return Colour(
		r >= 255 ? 255 : std::min(r + ob * (255 - r) / 256, 255),
		g >= 255 ? 255 : std::min(g + ob * (255 - g) / 256, 255),
		b >= 255 ? 255 : std::min(b + ob * (255 - b) / 256, 255),
		colour.a);
}

/**
 * Convert in place a 32bpp sprite to 8bpp.
 * @param sprite Sprite to convert.
 */
static void Convert32bppTo8bpp(SpriteLoader::Sprite &sprite)
{
	const auto *pixel_end = sprite.data + sprite.width * sprite.height;
	for (auto *pixel = sprite.data; pixel != pixel_end; ++pixel) {
		if (pixel->m != 0) {
			/* Pixel has 8bpp mask, test if should be reshaded. */
			uint8_t brightness = std::max({pixel->r, pixel->g, pixel->b});
			if (brightness == 0 || brightness == 128) continue;

			/* Update RGB component with reshaded palette colour, and enabled reshade. */
			Colour c = AdjustBrightness(_cur_palette.palette[pixel->m], brightness);

			if (IsInsideMM(pixel->m, 0xC6, 0xCE)) {
				/* Dumb but simple brightness conversion. */
				pixel->m = GetNearestColourReshadeIndex((c.r + c.g + c.b) / 3);
			} else {
				pixel->m = GetNearestColourIndex(c.r, c.g, c.b);
			}
		} else if (pixel->a < 128) {
			/* Transparent pixel. */
			pixel->m = 0;
		} else {
			/* Find nearest match from palette. */
			pixel->m = GetNearestColourIndex(pixel->r, pixel->g, pixel->b);
		}
	}
}

uint8_t SpriteLoaderMakeIndexed::LoadSprite(SpriteLoader::SpriteCollection &sprite, SpriteFile &file, size_t file_pos, SpriteType sprite_type, bool, uint8_t control_flags, uint8_t &avail_8bpp, uint8_t &avail_32bpp)
{
	uint8_t avail = this->baseloader.LoadSprite(sprite, file, file_pos, sprite_type, true, control_flags, avail_8bpp, avail_32bpp);

	for (ZoomLevel zoom = ZOOM_LVL_NORMAL; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(avail, zoom)) Convert32bppTo8bpp(sprite[zoom]);
	}

	return avail;
}
