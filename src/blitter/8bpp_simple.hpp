/* $Id$ */

/** @file 8bpp_simple.hpp */

#ifndef BLITTER_8BPP_SIMPLE_HPP
#define BLITTER_8BPP_SIMPLE_HPP

#include "8bpp_base.hpp"
#include "factory.hpp"

class Blitter_8bppSimple : public Blitter_8bppBase {
public:
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);
};

class FBlitter_8bppSimple: public BlitterFactory<FBlitter_8bppSimple> {
public:
	/* virtual */ const char *GetName() { return "8bpp-simple"; }
	/* virtual */ const char *GetDescription() { return "8bpp Simple Blitter (relative slow, but never wrong)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppSimple(); }
};

#endif /* BLITTER_8BPP_SIMPLE_HPP */
