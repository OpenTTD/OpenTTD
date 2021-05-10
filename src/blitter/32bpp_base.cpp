/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_base.cpp Implementation of base for 32 bpp blitters. */

#include "../stdafx.h"
#include "32bpp_base.hpp"
#include "common.hpp"

#include "../safeguards.h"

void *Blitter_32bppBase::MoveTo(void *video, int x, int y)
{
	return (uint32 *)video + x + y * _screen.pitch;
}

void Blitter_32bppBase::SetPixel(void *video, int x, int y, uint8 colour)
{
	*((Colour *)video + x + y * _screen.pitch) = LookupColourInPalette(colour);
}

void Blitter_32bppBase::DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 colour, int width, int dash)
{
	const Colour c = LookupColourInPalette(colour);
	this->DrawLineGeneric(x, y, x2, y2, screen_width, screen_height, width, dash, [=](int x, int y) {
		*((Colour *)video + x + y * _screen.pitch) = c;
	});
}

void Blitter_32bppBase::DrawRect(void *video, int width, int height, uint8 colour)
{
	Colour colour32 = LookupColourInPalette(colour);

	do {
		Colour *dst = (Colour *)video;
		for (int i = width; i > 0; i--) {
			*dst = colour32;
			dst++;
		}
		video = (uint32 *)video + _screen.pitch;
	} while (--height);
}

void Blitter_32bppBase::CopyFromBuffer(void *video, const void *src, int width, int height)
{
	uint32 *dst = (uint32 *)video;
	const uint32 *usrc = (const uint32 *)src;

	for (; height > 0; height--) {
		memcpy(dst, usrc, width * sizeof(uint32));
		usrc += width;
		dst += _screen.pitch;
	}
}

void Blitter_32bppBase::CopyToBuffer(const void *video, void *dst, int width, int height)
{
	uint32 *udst = (uint32 *)dst;
	const uint32 *src = (const uint32 *)video;

	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint32));
		src += _screen.pitch;
		udst += width;
	}
}

void Blitter_32bppBase::CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch)
{
	uint32 *udst = (uint32 *)dst;
	const uint32 *src = (const uint32 *)video;

	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint32));
		src += _screen.pitch;
		udst += dst_pitch;
	}
}

void Blitter_32bppBase::ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y)
{
	const uint32 *src;
	uint32 *dst;

	if (scroll_y > 0) {
		/* Calculate pointers */
		dst = (uint32 *)video + left + (top + height - 1) * _screen.pitch;
		src = dst - scroll_y * _screen.pitch;

		/* Decrease height and increase top */
		top += scroll_y;
		height -= scroll_y;
		assert(height > 0);

		/* Adjust left & width */
		if (scroll_x >= 0) {
			dst += scroll_x;
			left += scroll_x;
			width -= scroll_x;
		} else {
			src -= scroll_x;
			width += scroll_x;
		}

		for (int h = height; h > 0; h--) {
			memcpy(dst, src, width * sizeof(uint32));
			src -= _screen.pitch;
			dst -= _screen.pitch;
		}
	} else {
		/* Calculate pointers */
		dst = (uint32 *)video + left + top * _screen.pitch;
		src = dst - scroll_y * _screen.pitch;

		/* Decrease height. (scroll_y is <=0). */
		height += scroll_y;
		assert(height > 0);

		/* Adjust left & width */
		if (scroll_x >= 0) {
			dst += scroll_x;
			left += scroll_x;
			width -= scroll_x;
		} else {
			src -= scroll_x;
			width += scroll_x;
		}

		/* the y-displacement may be 0 therefore we have to use memmove,
		 * because source and destination may overlap */
		for (int h = height; h > 0; h--) {
			memmove(dst, src, width * sizeof(uint32));
			src += _screen.pitch;
			dst += _screen.pitch;
		}
	}
}

int Blitter_32bppBase::BufferSize(int width, int height)
{
	return width * height * sizeof(uint32);
}

void Blitter_32bppBase::PaletteAnimate(const Palette &palette)
{
	/* By default, 32bpp doesn't have palette animation */
}

Colour Blitter_32bppBase::ReallyAdjustBrightness(Colour colour, uint8 brightness)
{
	assert(DEFAULT_BRIGHTNESS == 1 << 7);

	uint64 combined = (((uint64) colour.r) << 32) | (((uint64) colour.g) << 16) | ((uint64) colour.b);
	combined *= brightness;

	uint16 r = GB(combined, 39, 9);
	uint16 g = GB(combined, 23, 9);
	uint16 b = GB(combined, 7, 9);

	if ((combined & 0x800080008000L) == 0L) {
		return Colour(r, g, b, colour.a);
	}

	uint16 ob = 0;
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

Blitter::PaletteAnimation Blitter_32bppBase::UsePaletteAnimation()
{
	return Blitter::PALETTE_ANIMATION_NONE;
}
