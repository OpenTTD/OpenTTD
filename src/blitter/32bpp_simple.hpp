/* $Id$ */

/** @file 32bpp_simple.hpp */

#ifndef BLITTER_32BPP_SIMPLE_HPP
#define BLITTER_32BPP_SIMPLE_HPP

#include "32bpp_base.hpp"
#include "factory.hpp"

class Blitter_32bppSimple : public Blitter_32bppBase {
public:
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);
	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);
};

class FBlitter_32bppSimple: public BlitterFactory<FBlitter_32bppSimple> {
public:
	/* virtual */ const char *GetName() { return "32bpp-simple"; }
	/* virtual */ const char *GetDescription() { return "32bpp Simple Blitter (no palette animation)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSimple(); }
};

#endif /* BLITTER_32BPP_SIMPLE_HPP */
