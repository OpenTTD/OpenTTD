/* $Id$ */

/** @file 8bpp_base.hpp */

#ifndef BLITTER_8BPP_BASE_HPP
#define BLITTER_8BPP_BASE_HPP

#include "base.hpp"

class Blitter_8bppBase : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 8; }
//	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);
//	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);
	/* virtual */ void *MoveTo(const void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 color);
	/* virtual */ void SetPixelIfEmpty(void *video, int x, int y, uint8 color);
	/* virtual */ void SetHorizontalLine(void *video, int width, uint8 color);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void MoveBuffer(void *video_dst, const void *video_src, int width, int height);
	/* virtual */ int BufferSize(int width, int height);
};

#endif /* BLITTER_8BPP_BASE_HPP */
