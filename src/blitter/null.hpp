/* $Id$ */

/** @file null.hpp */

#ifndef BLITTER_NULL_HPP
#define BLITTER_NULL_HPP

#include "blitter.hpp"

class Blitter_Null : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 0; }

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);

	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);

	/* virtual */ const char *GetRenderer() { return "null"; }
};

class FBlitter_Null: public BlitterFactory<FBlitter_Null> {
public:
	/* virtual */ const char *GetName() { return "null"; }

	/* virtual */ const char *GetDescription() { return "Null Blitter (does nothing)"; }

	/* virtual */ Blitter *CreateInstance() { return new Blitter_Null(); }
};

#endif /* BLITTER_NULL_HPP */
