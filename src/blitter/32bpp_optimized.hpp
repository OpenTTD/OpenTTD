/* $Id$ */

/** @file 32bpp_optimized.hpp */

#ifndef BLITTER_32BPP_OPTIMIZED_HPP
#define BLITTER_32BPP_OPTIMIZED_HPP

#include "32bpp_simple.hpp"
#include "factory.hpp"

class Blitter_32bppOptimized : public Blitter_32bppSimple {
public:
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);

	/* virtual */ const char *GetName() { return "32bpp-optimized"; }
};

class FBlitter_32bppOptimized: public BlitterFactory<FBlitter_32bppOptimized> {
public:
	/* virtual */ const char *GetName() { return "32bpp-optimized"; }
	/* virtual */ const char *GetDescription() { return "32bpp Optimized Blitter (no palette animation)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppOptimized(); }
};

#endif /* BLITTER_32BPP_OPTIMIZED_HPP */
