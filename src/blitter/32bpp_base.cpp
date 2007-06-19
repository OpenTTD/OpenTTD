#include "../stdafx.h"
#include "../gfx.h"
#include "32bpp_base.hpp"

void *Blitter_32bppBase::MoveTo(const void *video, int x, int y)
{
	return (uint32 *)video + x + y * _screen.pitch;
}

void Blitter_32bppBase::SetPixel(void *video, int x, int y, uint8 color)
{
	*((uint32 *)video + x + y * _screen.pitch) = LookupColourInPalette(color);
}

void Blitter_32bppBase::SetPixelIfEmpty(void *video, int x, int y, uint8 color)
{
	uint32 *dst = (uint32 *)video + x + y * _screen.pitch;
	if (*dst == 0) *dst = LookupColourInPalette(color);
}

void Blitter_32bppBase::DrawRect(void *video, int width, int height, uint8 color)
{
	uint32 color32 = LookupColourInPalette(color);

	do {
		uint32 *dst = (uint32 *)video;
		for (int i = width; i > 0; i--) {
			*dst = color32;
			dst++;
		}
		video = (uint32 *)video + _screen.pitch;
	} while (--height);
}

void Blitter_32bppBase::DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 color)
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

	this->SetPixel(video, x, y, color);
	if (dx > dy) {
		frac = dy - (dx / 2);
		while (x != x2) {
			if (frac >= 0) {
				y += stepy;
				frac -= dx;
			}
			x += stepx;
			frac += dy;
			if (x > 0 && y > 0 && x < screen_width && y < screen_height) this->SetPixel(video, x, y, color);
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
			if (x > 0 && y > 0 && x < screen_width && y < screen_height) this->SetPixel(video, x, y, color);
		}
	}
}
void Blitter_32bppBase::CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch)
{
	int direction = (height < 0) ? -1 : 1;
	uint32 *dst = (uint32 *)video;
	uint32 *usrc = (uint32 *)src;

	height = abs(height);
	for (; height > 0; height--) {
		memcpy(dst, usrc, width * sizeof(uint32));
		usrc += src_pitch * direction;
		dst += _screen.pitch * direction;
	}
}

void Blitter_32bppBase::CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch)
{
	int direction = (height < 0) ? -1 : 1;
	uint32 *udst = (uint32 *)dst;
	uint32 *src = (uint32 *)video;

	height = abs(height);
	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint32));
		src += _screen.pitch * direction;
		udst += dst_pitch * direction;
	}
}

void Blitter_32bppBase::MoveBuffer(void *video_dst, const void *video_src, int width, int height)
{
	uint32 *dst = (uint32 *)video_dst;
	uint32 *src = (uint32 *)video_src;

	for (; height > 0; height--) {
		memmove(dst, src, width * sizeof(uint32));
		src += _screen.pitch;
		dst += _screen.pitch;
	}
}

void Blitter_32bppBase::ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y)
{
	const uint32 *src;
	uint32 *dst;

	if (scroll_y > 0) {
		/*Calculate pointers */
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

		/* Negative height as we want to copy from bottom to top */
		this->CopyFromBuffer(dst, src, width, -height, _screen.pitch);
	} else {
		/* Calculate pointers */
		dst = (uint32 *)video + left + top * _screen.pitch;
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
		this->MoveBuffer(dst, src, width, height);
	}
}

int Blitter_32bppBase::BufferSize(int width, int height)
{
	return width * height * sizeof(uint32);
}

void Blitter_32bppBase::PaletteAnimate(uint start, uint count)
{
	/* By default, 32bpp doesn't have palette animation */
}

Blitter::PaletteAnimation Blitter_32bppBase::UsePaletteAnimation()
{
	return Blitter::PALETTE_ANIMATION_NONE;
}
