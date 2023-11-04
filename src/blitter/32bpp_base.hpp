/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_base.hpp Base for all 32 bits blitters. */

#ifndef BLITTER_32BPP_BASE_HPP
#define BLITTER_32BPP_BASE_HPP

#include "base.hpp"
#include "../core/bitmath_func.hpp"
#include "../core/math_func.hpp"
#include "../gfx_func.h"

/** Base for all 32bpp blitters. */
class Blitter_32bppBase : public Blitter {
public:
	uint8_t GetScreenDepth() override { return 32; }
	void *MoveTo(void *video, int x, int y) override;
	void SetPixel(void *video, int x, int y, uint8_t colour) override;
	void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8_t colour, int width, int dash) override;
	void DrawRect(void *video, int width, int height, uint8_t colour) override;
	void CopyFromBuffer(void *video, const void *src, int width, int height) override;
	void CopyToBuffer(const void *video, void *dst, int width, int height) override;
	void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) override;
	void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) override;
	size_t BufferSize(uint width, uint height) override;
	void PaletteAnimate(const Palette &palette) override;
	Blitter::PaletteAnimation UsePaletteAnimation() override;

	/**
	 * Look up the colour in the current palette.
	 */
	static inline Colour LookupColourInPalette(uint index)
	{
		return _cur_palette.palette[index];
	}

	/**
	 * Compose a colour based on RGBA values and the current pixel value.
	 */
	static inline Colour ComposeColourRGBANoCheck(uint r, uint g, uint b, uint a, Colour current)
	{
		uint cr = current.r;
		uint cg = current.g;
		uint cb = current.b;

		/* The 256 is wrong, it should be 255, but 256 is much faster... */
		return Colour(
							((int)(r - cr) * a) / 256 + cr,
							((int)(g - cg) * a) / 256 + cg,
							((int)(b - cb) * a) / 256 + cb);
	}

	/**
	 * Compose a colour based on RGBA values and the current pixel value.
	 * Handles fully transparent and solid pixels in a special (faster) way.
	 */
	static inline Colour ComposeColourRGBA(uint r, uint g, uint b, uint a, Colour current)
	{
		if (a == 0) return current;
		if (a >= 255) return Colour(r, g, b);

		return ComposeColourRGBANoCheck(r, g, b, a, current);
	}

	/**
	 * Compose a colour based on Pixel value, alpha value, and the current pixel value.
	 */
	static inline Colour ComposeColourPANoCheck(Colour colour, uint a, Colour current)
	{
		uint r  = colour.r;
		uint g  = colour.g;
		uint b  = colour.b;

		return ComposeColourRGBANoCheck(r, g, b, a, current);
	}

	/**
	 * Compose a colour based on Pixel value, alpha value, and the current pixel value.
	 * Handles fully transparent and solid pixels in a special (faster) way.
	 */
	static inline Colour ComposeColourPA(Colour colour, uint a, Colour current)
	{
		if (a == 0) return current;
		if (a >= 255) {
			colour.a = 255;
			return colour;
		}

		return ComposeColourPANoCheck(colour, a, current);
	}

	/**
	 * Make a pixel looks like it is transparent.
	 * @param colour the colour already on the screen.
	 * @param nom the amount of transparency, nominator, makes colour lighter.
	 * @param denom denominator, makes colour darker.
	 * @return the new colour for the screen.
	 */
	static inline Colour MakeTransparent(Colour colour, uint nom, uint denom = 256)
	{
		uint r = colour.r;
		uint g = colour.g;
		uint b = colour.b;

		return Colour(r * nom / denom, g * nom / denom, b * nom / denom);
	}

	/**
	 * Make a colour dark grey, for specialized 32bpp remapping.
	 * @param r red component
	 * @param g green component
	 * @param b blue component
	 * @return the brightness value of the new colour, now dark grey.
	 */
	static inline uint8_t MakeDark(uint8_t r, uint8_t g, uint8_t b)
	{
		/* Magic-numbers are ~66% of those used in MakeGrey() */
		return ((r * 13063) + (g * 25647) + (b * 4981)) / 65536;
	}

	/**
	 * Make a colour dark grey, for specialized 32bpp remapping.
	 * @param colour the colour to make dark.
	 * @return the new colour, now darker.
	 */
	static inline Colour MakeDark(Colour colour)
	{
		uint8_t d = MakeDark(colour.r, colour.g, colour.b);

		return Colour(d, d, d);
	}

	/**
	 * Make a colour grey - based.
	 * @param colour the colour to make grey.
	 * @return the new colour, now grey.
	 */
	static inline Colour MakeGrey(Colour colour)
	{
		uint r = colour.r;
		uint g = colour.g;
		uint b = colour.b;

		/* To avoid doubles and stuff, multiple it with a total of 65536 (16bits), then
		 *  divide by it to normalize the value to a byte again. See heightmap.cpp for
		 *  information about the formula. */
		uint grey = ((r * 19595) + (g * 38470) + (b * 7471)) / 65536;

		return Colour(grey, grey, grey);
	}

	static const int DEFAULT_BRIGHTNESS = 128;

	static Colour ReallyAdjustBrightness(Colour colour, uint8_t brightness);

	static inline Colour AdjustBrightness(Colour colour, uint8_t brightness)
	{
		/* Shortcut for normal brightness */
		if (brightness == DEFAULT_BRIGHTNESS) return colour;

		return ReallyAdjustBrightness(colour, brightness);
	}

	static inline uint8_t GetColourBrightness(Colour colour)
	{
		uint8_t rgb_max = std::max(colour.r, std::max(colour.g, colour.b));

		/* Black pixel (8bpp or old 32bpp image), so use default value */
		if (rgb_max == 0) rgb_max = DEFAULT_BRIGHTNESS;

		return rgb_max;
	}
};

#endif /* BLITTER_32BPP_BASE_HPP */
