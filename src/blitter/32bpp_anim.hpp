/* $Id$ */

/** @file 32bpp_anim.hpp */

#ifndef BLITTER_32BPP_ANIM_HPP
#define BLITTER_32BPP_ANIM_HPP

#include "32bpp_simple.hpp"
#include "factory.hpp"

class Blitter_32bppAnim : public Blitter_32bppSimple {
private:
	uint8 *anim_buf; ///< In this buffer we keep track of the 8bpp indexes so we can do palette animation
	int anim_buf_width;
	int anim_buf_height;

public:
	Blitter_32bppAnim() :
		anim_buf(NULL),
		anim_buf_width(0),
		anim_buf_height(0)
	{}

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 color);
	/* virtual */ void SetPixelIfEmpty(void *video, int x, int y, uint8 color);
	/* virtual */ void DrawRect(void *video, int width, int height, uint8 color);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height);
	/* virtual */ void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y);
	/* virtual */ void PaletteAnimate(uint start, uint count);
	/* virtual */ Blitter::PaletteAnimation UsePaletteAnimation();
};

class FBlitter_32bppAnim: public BlitterFactory<FBlitter_32bppAnim> {
public:
	/* virtual */ const char *GetName() { return "32bpp-anim"; }
	/* virtual */ const char *GetDescription() { return "32bpp Animation Blitter (palette animation)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppAnim(); }
};

#endif /* BLITTER_32BPP_ANIM_HPP */
