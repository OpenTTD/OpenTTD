/* $Id$ */

/** @file 8bpp_optimized.hpp */

#ifndef BLITTER_8BPP_OPTIMIZED_HPP
#define BLITTER_8BPP_OPTIMIZED_HPP

#include "blitter.hpp"

class Blitter_8bppOptimized : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 8; }

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);

	/* virtual */ const char *GetRenderer() { return "8bpp"; }
};

class FBlitter_8bppOptimized: public BlitterFactory<FBlitter_8bppOptimized> {
public:
	/* virtual */ const char *GetName() { return "8bpp-optimized"; }

	/* virtual */ const char *GetDescription() { return "8bpp Optimized Blitter (compression + all-ZoomLevel cache)"; }

	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppOptimized(); }
};

#endif /* BLITTER_8BPP_OPTIMIZED_HPP */
