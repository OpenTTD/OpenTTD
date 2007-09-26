#include "../stdafx.h"
#include "../gfx.h"
#include "8bpp_base.hpp"

void Blitter_8bppBase::DrawColorMappingRect(void *dst, int width, int height, int pal)
{
	const uint8 *ctab = GetNonSprite(pal) + 1;

	do {
		for (int i = 0; i != width; i++) *((uint8 *)dst + i) = ctab[((uint8 *)dst)[i]];
		dst = (uint8 *)dst + _screen.pitch;
	} while (--height);
}

void *Blitter_8bppBase::MoveTo(const void *video, int x, int y)
{
	return (uint8 *)video + x + y * _screen.pitch;
}

void Blitter_8bppBase::SetPixel(void *video, int x, int y, uint8 color)
{
	*((uint8 *)video + x + y * _screen.pitch) = color;
}

void Blitter_8bppBase::SetPixelIfEmpty(void *video, int x, int y, uint8 color)
{
	uint8 *dst = (uint8 *)video + x + y * _screen.pitch;
	if (*dst == 0) *dst = color;
}

void Blitter_8bppBase::DrawRect(void *video, int width, int height, uint8 color)
{
	do {
		memset(video, color, width);
		video = (uint8 *)video + _screen.pitch;
	} while (--height);
}

void Blitter_8bppBase::DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 color)
{
	int dy;
	int dx;
	int stepx;
	int stepy;
	int frac;

	dy = (y2 - y) * 2;
	if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else {
		stepy = 1;
	}

	dx = (x2 - x) * 2;
	if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else {
		stepx = 1;
	}

	if (x >= 0 && y >= 0 && x < screen_width && y < screen_height) this->SetPixel(video, x, y, color);
	if (dx > dy) {
		frac = dy - (dx / 2);
		while (x != x2) {
			if (frac >= 0) {
				y += stepy;
				frac -= dx;
			}
			x += stepx;
			frac += dy;
			if (x >= 0 && y >= 0 && x < screen_width && y < screen_height) this->SetPixel(video, x, y, color);
		}
	} else {
		frac = dx - (dy / 2);
		while (y != y2) {
			if (frac >= 0) {
				x += stepx;
				frac -= dy;
			}
			y += stepy;
			frac += dx;
			if (x >= 0 && y >= 0 && x < screen_width && y < screen_height) this->SetPixel(video, x, y, color);
		}
	}
}

void Blitter_8bppBase::CopyFromBuffer(void *video, const void *src, int width, int height)
{
	uint8 *dst = (uint8 *)video;
	uint8 *usrc = (uint8 *)src;

	for (; height > 0; height--) {
		memcpy(dst, usrc, width * sizeof(uint8));
		usrc += width;
		dst += _screen.pitch;
	}
}

void Blitter_8bppBase::CopyToBuffer(const void *video, void *dst, int width, int height)
{
	uint8 *udst = (uint8 *)dst;
	uint8 *src = (uint8 *)video;

	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint8));
		src += _screen.pitch;
		udst += width;
	}
}

void Blitter_8bppBase::CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch)
{
	uint8 *udst = (uint8 *)dst;
	uint8 *src = (uint8 *)video;

	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint8));
		src += _screen.pitch;
		udst += dst_pitch;
	}
}

void Blitter_8bppBase::ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y)
{
	const uint8 *src;
	uint8 *dst;

	if (scroll_y > 0) {
		/*Calculate pointers */
		dst = (uint8 *)video + left + (top + height - 1) * _screen.pitch;
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
			memcpy(dst, src, width * sizeof(uint8));
			src -= _screen.pitch;
			dst -= _screen.pitch;
		}
	} else {
		/* Calculate pointers */
		dst = (uint8 *)video + left + top * _screen.pitch;
		src = dst - scroll_y * _screen.pitch;

		/* Decrese height. (scroll_y is <=0). */
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
			memmove(dst, src, width * sizeof(uint8));
			src += _screen.pitch;
			dst += _screen.pitch;
		}
	}
}

int Blitter_8bppBase::BufferSize(int width, int height)
{
	return width * height;
}

void Blitter_8bppBase::PaletteAnimate(uint start, uint count)
{
	/* Video backend takes care of the palette animation */
}

Blitter::PaletteAnimation Blitter_8bppBase::UsePaletteAnimation()
{
	return Blitter::PALETTE_ANIMATION_VIDEO_BACKEND;
}
