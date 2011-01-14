/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 8bpp_optimized.hpp An optimized 8 bpp blitter. */

#ifndef BLITTER_8BPP_OPTIMIZED_HPP
#define BLITTER_8BPP_OPTIMIZED_HPP

#include "8bpp_base.hpp"
#include "factory.hpp"

class Blitter_8bppOptimized : public Blitter_8bppBase {
public:
	struct SpriteData {
		uint32 offset[ZOOM_LVL_COUNT]; ///< offsets (from .data) to streams for different zoom levels
		byte data[];                   ///< data, all zoomlevels
	};

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, AllocatorProc *allocator);

	/* virtual */ const char *GetName() { return "8bpp-optimized"; }
};

class FBlitter_8bppOptimized: public BlitterFactory<FBlitter_8bppOptimized> {
public:
	/* virtual */ const char *GetName() { return "8bpp-optimized"; }
	/* virtual */ const char *GetDescription() { return "8bpp Optimized Blitter (compression + all-ZoomLevel cache)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppOptimized(); }
};

#endif /* BLITTER_8BPP_OPTIMIZED_HPP */
