/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_optimized.hpp Optimized 32 bpp blitter. */

#ifndef BLITTER_32BPP_OPTIMIZED_HPP
#define BLITTER_32BPP_OPTIMIZED_HPP

#include "32bpp_simple.hpp"

class Blitter_32bppOptimized : public Blitter_32bppSimple {
public:
	struct SpriteData {
		uint32 offset[ZOOM_LVL_COUNT][2];
		byte data[];
	};

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, AllocatorProc *allocator);

	/* virtual */ const char *GetName() { return "32bpp-optimized"; }

	template <BlitterMode mode> void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
};

class FBlitter_32bppOptimized: public BlitterFactory<FBlitter_32bppOptimized> {
public:
	/* virtual */ const char *GetName() { return "32bpp-optimized"; }
	/* virtual */ const char *GetDescription() { return "32bpp Optimized Blitter (no palette animation)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppOptimized(); }
};

#endif /* BLITTER_32BPP_OPTIMIZED_HPP */
