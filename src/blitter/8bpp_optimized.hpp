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

/** 8bpp blitter optimised for speed. */
class Blitter_8bppOptimized FINAL : public Blitter_8bppBase {
public:
	/** Data stored about a (single) sprite. */
	struct SpriteData {
		uint32_t offset[ZOOM_LVL_END]; ///< Offsets (from .data) to streams for different zoom levels.
		byte data[];                   ///< Data, all zoomlevels.
	};

	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override;

	const char *GetName() override { return "8bpp-optimized"; }
};

/** Factory for the 8bpp blitter optimised for speed. */
class FBlitter_8bppOptimized : public BlitterFactory {
public:
	FBlitter_8bppOptimized() : BlitterFactory("8bpp-optimized", "8bpp Optimized Blitter (compression + all-ZoomLevel cache)") {}
	Blitter *CreateInstance() override { return new Blitter_8bppOptimized(); }
};

#endif /* BLITTER_8BPP_OPTIMIZED_HPP */
