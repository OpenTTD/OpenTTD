/* $Id$ */

/** @file 32bpp_base.hpp */

#ifndef BLITTER_32BPP_BASE_HPP
#define BLITTER_32BPP_BASE_HPP

#include "base.hpp"

class Blitter_32bppBase : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 32; }
//	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
//	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);
//	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);
	/* virtual */ void *MoveTo(const void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 color);
	/* virtual */ void SetPixelIfEmpty(void *video, int x, int y, uint8 color);
	/* virtual */ void SetHorizontalLine(void *video, int width, uint8 color);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void MoveBuffer(void *video_dst, const void *video_src, int width, int height);
	/* virtual */ int BufferSize(int width, int height);

	static inline uint32 LookupColourInPalette(uint8 index) {
		#define ARGB(a, r, g, b) ((((a) << 24) & 0xFF000000) | (((r) << 16) & 0x00FF0000) | (((g) << 8) & 0x0000FF00) | ((b) & 0x000000FF))
		if (index == 0) return 0x00000000;
		return ARGB(0xFF, _cur_palette[index].r, _cur_palette[index].g, _cur_palette[index].b);
	}
};

#endif /* BLITTER_32BPP_BASE_HPP */
