/* $Id$ */

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
#include "../gfx_func.h"

class Blitter_32bppBase : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 32; }
//	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
//	/* virtual */ void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal);
//	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);
	/* virtual */ void *MoveTo(const void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 colour);
	/* virtual */ void DrawRect(void *video, int width, int height, uint8 colour);
	/* virtual */ void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 colour);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height);
	/* virtual */ void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y);
	/* virtual */ int BufferSize(int width, int height);
	/* virtual */ void PaletteAnimate(uint start, uint count);
	/* virtual */ Blitter::PaletteAnimation UsePaletteAnimation();
	/* virtual */ int GetBytesPerPixel() { return 4; }

	/**
	 * Compose a colour based on RGB values.
	 */
	static inline uint32 ComposeColour(uint a, uint r, uint g, uint b)
	{
		return (((a) << 24) & 0xFF000000) | (((r) << 16) & 0x00FF0000) | (((g) << 8) & 0x0000FF00) | ((b) & 0x000000FF);
	}

	/**
	 * Look up the colour in the current palette.
	 */
	static inline uint32 LookupColourInPalette(uint index)
	{
		return _cur_palette[index].data;
	}

	/**
	 * Compose a colour based on RGBA values and the current pixel value.
	 */
	static inline uint32 ComposeColourRGBANoCheck(uint r, uint g, uint b, uint a, uint32 current)
	{
		uint cr = GB(current, 16, 8);
		uint cg = GB(current, 8,  8);
		uint cb = GB(current, 0,  8);

		/* The 256 is wrong, it should be 255, but 256 is much faster... */
		return ComposeColour(0xFF,
							((int)(r - cr) * a) / 256 + cr,
							((int)(g - cg) * a) / 256 + cg,
							((int)(b - cb) * a) / 256 + cb);
	}

	/**
	 * Compose a colour based on RGBA values and the current pixel value.
	 * Handles fully transparent and solid pixels in a special (faster) way.
	 */
	static inline uint32 ComposeColourRGBA(uint r, uint g, uint b, uint a, uint32 current)
	{
		if (a == 0) return current;
		if (a >= 255) return ComposeColour(0xFF, r, g, b);

		return ComposeColourRGBANoCheck(r, g, b, a, current);
	}

	/**
	 * Compose a colour based on Pixel value, alpha value, and the current pixel value.
	 */
	static inline uint32 ComposeColourPANoCheck(uint32 colour, uint a, uint32 current)
	{
		uint r  = GB(colour,  16, 8);
		uint g  = GB(colour,  8,  8);
		uint b  = GB(colour,  0,  8);

		return ComposeColourRGBANoCheck(r, g, b, a, current);
	}

	/**
	 * Compose a colour based on Pixel value, alpha value, and the current pixel value.
	 * Handles fully transparent and solid pixels in a special (faster) way.
	 */
	static inline uint32 ComposeColourPA(uint32 colour, uint a, uint32 current)
	{
		if (a == 0) return current;
		if (a >= 255) return (colour | 0xFF000000);

		return ComposeColourPANoCheck(colour, a, current);
	}

	/**
	 * Make a pixel looks like it is transparent.
	 * @param colour the colour already on the screen.
	 * @param nom the amount of transparency, nominator, makes colour lighter.
	 * @param denom denominator, makes colour darker.
	 * @return the new colour for the screen.
	 */
	static inline uint32 MakeTransparent(uint32 colour, uint nom, uint denom = 256)
	{
		uint r = GB(colour, 16, 8);
		uint g = GB(colour, 8,  8);
		uint b = GB(colour, 0,  8);

		return ComposeColour(0xFF, r * nom / denom, g * nom / denom, b * nom / denom);
	}

	/**
	 * Make a colour grey - based.
	 * @param colour the colour to make grey.
	 * @return the new colour, now grey.
	 */
	static inline uint32 MakeGrey(uint32 colour)
	{
		uint r = GB(colour, 16, 8);
		uint g = GB(colour, 8,  8);
		uint b = GB(colour, 0,  8);

		/* To avoid doubles and stuff, multiple it with a total of 65536 (16bits), then
		 *  divide by it to normalize the value to a byte again. See heightmap.cpp for
		 *  information about the formula. */
		colour = ((r * 19595) + (g * 38470) + (b * 7471)) / 65536;

		return ComposeColour(0xFF, colour, colour, colour);
	}
};

#endif /* BLITTER_32BPP_BASE_HPP */
