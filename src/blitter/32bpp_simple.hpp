/* $Id$ */

/** @file 32bpp_simple.hpp */

#ifndef BLITTER_32BPP_SIMPLE_HPP
#define BLITTER_32BPP_SIMPLE_HPP

#include "blitter.hpp"

class Blitter_32bppSimple : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 32; }

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);

	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);

	/* virtual */ const char *GetRenderer() { return "32bpp"; }
};

class FBlitter_32bppSimple: public BlitterFactory<FBlitter_32bppSimple> {
public:
	/* virtual */ const char *GetName() { return "32bpp-simple"; }

	/* virtual */ const char *GetDescription() { return "32bpp Simple Blitter (no palette animation)"; }

	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSimple(); }
};

#endif /* BLITTER_32BPP_SIMPLE_HPP */
