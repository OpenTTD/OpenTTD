#include "../stdafx.h"
#include "../gfx.h"
#include "32bpp.hpp"

static FRenderer_32bpp iFRenderer_32bpp;

void *Renderer_32bpp::MoveTo(const void *video, int x, int y)
{
	return (uint32 *)video + x + y * _screen.pitch;
}

void Renderer_32bpp::SetPixel(void *video, int x, int y, uint8 color)
{
	*((uint32 *)video + x + y * _screen.pitch) = LookupColourInPalette(color);
}

void Renderer_32bpp::SetPixelIfEmpty(void *video, int x, int y, uint8 color)
{
	uint32 *dst = (uint32 *)video + x + y * _screen.pitch;
	if (*dst == 0) *dst = LookupColourInPalette(color);
}

void Renderer_32bpp::SetHorizontalLine(void *video, int width, uint8 color)
{
	uint32 *dst = (uint32 *)video;
	uint32 color32 = LookupColourInPalette(color);

	for (; width > 0; width--) {
		*dst = color32;
		dst++;
	}
}

void Renderer_32bpp::CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch)
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

void Renderer_32bpp::CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch)
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

void Renderer_32bpp::MoveBuffer(void *video_dst, const void *video_src, int width, int height)
{
	uint32 *dst = (uint32 *)video_dst;
	uint32 *src = (uint32 *)video_src;

	for (; height > 0; height--) {
		memmove(dst, src, width * sizeof(uint32));
		src += _screen.pitch;
		dst += _screen.pitch;
	}
}

int Renderer_32bpp::BufferSize(int width, int height)
{
	return width * height * sizeof(uint32);
}
