#include "../stdafx.h"
#include "../gfx.h"
#include "null.hpp"

static FRenderer_Null iFRenderer_Null;

void *Renderer_Null::MoveTo(const void *video, int x, int y)
{
	return NULL;
}

void Renderer_Null::SetPixel(void *video, int x, int y, uint8 color)
{
}

void Renderer_Null::SetPixelIfEmpty(void *video, int x, int y, uint8 color)
{
}

void Renderer_Null::SetHorizontalLine(void *video, int width, uint8 color)
{
}

void Renderer_Null::CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch)
{
}

void Renderer_Null::CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch)
{
}

void Renderer_Null::MoveBuffer(void *video_dst, const void *video_src, int width, int height)
{
}

int Renderer_Null::BufferSize(int width, int height)
{
	return 0;
}
