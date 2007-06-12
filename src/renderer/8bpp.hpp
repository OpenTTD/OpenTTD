/* $Id$ */

/** @file 8bpp.hpp */

#ifndef RENDERER_8BPP_HPP
#define RENDERER_8BPP_HPP

#include "renderer.hpp"

class Renderer_8bpp : public Renderer {
public:
	/* virtual */ void *MoveTo(const void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 color);
	/* virtual */ void SetPixelIfEmpty(void *video, int x, int y, uint8 color);
	/* virtual */ void SetHorizontalLine(void *video, int width, uint8 color);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void MoveBuffer(void *video_dst, const void *video_src, int width, int height);
	/* virtual */ int BufferSize(int width, int height);
};

class FRenderer_8bpp: public RendererFactory<FRenderer_8bpp> {
public:
	/* virtual */ const char *GetName() { return "8bpp"; }

	/* virtual */ Renderer *CreateInstance() { return new Renderer_8bpp(); }
};

#endif /* RENDERER_8BPP_HPP */
