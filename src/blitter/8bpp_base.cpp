#include "../stdafx.h"
#include "../gfx.h"
#include "8bpp_base.hpp"

void Blitter_8bppBase::DrawColorMappingRect(void *dst, int width, int height, int pal)
{
	const uint8 *ctab = GetNonSprite(pal) + 1;

	do {
		for (int i = 0; i != width; i++) *((uint8 *)dst + i) = ctab[((uint8 *)dst)[i]];
		dst = (uint8 *)dst + _screen.pitch;
	} while (height--);
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

void Blitter_8bppBase::SetHorizontalLine(void *video, int width, uint8 color)
{
	memset(video, color, width);
}

void Blitter_8bppBase::CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch)
{
	int direction = (height < 0) ? -1 : 1;
	uint8 *dst = (uint8 *)video;
	uint8 *usrc = (uint8 *)src;

	height = abs(height);
	for (; height > 0; height--) {
		memcpy(dst, usrc, width);
		usrc += src_pitch * direction;
		dst += _screen.pitch * direction;
	}
}

void Blitter_8bppBase::CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch)
{
	int direction = (height < 0) ? -1 : 1;
	uint8 *udst = (uint8 *)dst;
	uint8 *src = (uint8 *)video;

	height = abs(height);
	for (; height > 0; height--) {
		memcpy(udst, src, width);
		src += _screen.pitch * direction;
		udst += dst_pitch * direction;
	}
}

void Blitter_8bppBase::MoveBuffer(void *video_dst, const void *video_src, int width, int height)
{
	uint8 *dst = (uint8 *)video_dst;
	uint8 *src = (uint8 *)video_src;

	for (; height > 0; height--) {
		memmove(dst, src, width);
		src += _screen.pitch;
		dst += _screen.pitch;
	}
}

int Blitter_8bppBase::BufferSize(int width, int height)
{
	return width * height;
}
