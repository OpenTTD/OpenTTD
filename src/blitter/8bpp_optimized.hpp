/* $Id$ */

/** @file 8bpp_nice.hpp */

#ifndef BLITTER_8BPP_OPTIMIZED_HPP
#define BLITTER_8BPP_OPTIMIZED_HPP

#include "blitter.hpp"

typedef Pixel Pixel8;

class Blitter_8bppOptimized : public Blitter {
public:
	uint8 GetScreenDepth() { return 8; }

	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

	Sprite *Encode(SpriteLoader::Sprite *sprite);
};

class FBlitter_8bppOptimized: public BlitterFactory<FBlitter_8bppOptimized> {
public:
	/* virtual */ const char *GetName() { return "8bpp-optimzed"; }

	/* virtual */ const char *GetDescription() { return "8bpp Optimized Blitter (compression + all-ZoomLevel cache)"; }

	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppOptimized(); }
};

#endif /* BLITTER_8BPP_OPTIMIZED_HPP */
