#include "../stdafx.h"
#include "../gfx.h"
#include "8bpp.hpp"

static FRenderer_8bpp iFRenderer_8bpp;

void *Renderer_8bpp::MoveTo(const void *video, int x, int y)
{
	return (uint8 *)video + x + y * _screen.pitch;
}

void Renderer_8bpp::SetPixel(void *video, int x, int y, uint8 color)
{
	*((uint8 *)video + x + y * _screen.pitch) = color;
}

void Renderer_8bpp::SetPixelIfEmpty(void *video, int x, int y, uint8 color)
{
	uint8 *dst = (uint8 *)video + x + y * _screen.pitch;
	if (*dst == 0) *dst = color;
}

void Renderer_8bpp::SetHorizontalLine(void *video, int width, uint8 color)
{
	memset(video, color, width);
}

void Renderer_8bpp::CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch)
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

void Renderer_8bpp::CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch)
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

void Renderer_8bpp::MoveBuffer(void *video_dst, const void *video_src, int width, int height)
{
	uint8 *dst = (uint8 *)video_dst;
	uint8 *src = (uint8 *)video_src;

	for (; height > 0; height--) {
		memmove(dst, src, width);
		src += _screen.pitch;
		dst += _screen.pitch;
	}
}

int Renderer_8bpp::BufferSize(int width, int height)
{
	return width * height;
}
