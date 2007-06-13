/* $Id$ */

/** @file 8bpp_simple.hpp */

#ifndef BLITTER_8BPP_SIMPLE_HPP
#define BLITTER_8BPP_SIMPLE_HPP

#include "blitter.hpp"

class Blitter_8bppSimple : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 8; }

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);

	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);

	/* virtual */ const char *GetRenderer() { return "8bpp"; }
};

class FBlitter_8bppSimple: public BlitterFactory<FBlitter_8bppSimple> {
public:
	/* virtual */ const char *GetName() { return "8bpp-simple"; }

	/* virtual */ const char *GetDescription() { return "8bpp Simple Blitter (relative slow, but never wrong)"; }

	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppSimple(); }
};

#endif /* BLITTER_8BPP_SIMPLE_HPP */
