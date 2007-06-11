/* $Id$ */

/** @file debug.hpp */

#ifndef BLITTER_8BPP_DEBUG_HPP
#define BLITTER_8BPP_DEBUG_HPP

#include "blitter.hpp"

typedef Pixel Pixel8;

class Blitter_8bppDebug : public Blitter {
public:
	uint8 GetScreenDepth() { return 8; }

	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

	Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);
};

class FBlitter_8bppDebug: public BlitterFactory<FBlitter_8bppDebug> {
public:
	/* virtual */ const char *GetName() { return "8bpp-debug"; }

	/* virtual */ const char *GetDescription() { return "8bpp Debug Blitter (testing only)"; }

	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppDebug(); }
};

#endif /* BLITTER_8BPP_DEBUG_HPP */
