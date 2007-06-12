/* $Id$ */

/** @file null.hpp */

#ifndef RENDERER_NULL_HPP
#define RENDERER_NULL_HPP

#include "renderer.hpp"

class Renderer_Null : public Renderer {
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

class FRenderer_Null: public RendererFactory<FRenderer_Null> {
public:
	/* virtual */ const char *GetName() { return "null"; }

	/* virtual */ Renderer *CreateInstance() { return new Renderer_Null(); }
};

#endif /* RENDERER_NULL_HPP */
