/* $Id$ */

/** @file 8bpp.hpp */

#ifndef BLITTER_8BPP_SIMPLE_HPP
#define BLITTER_8BPP_SIMPLE_HPP

#include "blitter.hpp"

typedef Pixel Pixel8;

class Blitter_8bppSimple : public Blitter {
public:
	uint8 GetScreenDepth() { return 8; }

	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

	Sprite *Encode(SpriteLoader::Sprite *sprite);
};

class FBlitter_8bppSimple: public BlitterFactory<FBlitter_8bppSimple> {
public:
	/* virtual */ const char *GetName() { return "8bpp-simple"; }

	/* virtual */ const char *GetDescription() { return "8bpp Simple Blitter (relative slow, but never wrong)"; }

	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppSimple(); }
};

#endif /* BLITTER_8BPP_SIMPLE_HPP */
