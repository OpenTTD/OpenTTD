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

/** The optimised 32 bpp blitter (without palette animation). */
class Blitter_32bppOptimized : public Blitter_32bppSimple {
public:
	/** Data stored about a (single) sprite. */
	struct SpriteData {
		uint32_t offset[ZOOM_LVL_END][2]; ///< Offsets (from .data) to streams for different zoom levels, and the normal and remap image information.
		byte data[];                      ///< Data, all zoomlevels.
	};

	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override;

	const char *GetName() override { return "32bpp-optimized"; }

	template <BlitterMode mode, bool Tpal_to_rgb = false> void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);

protected:
	template <bool Tpal_to_rgb> void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	template <bool Tpal_to_rgb> Sprite *EncodeInternal(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator);
};

/** Factory for the optimised 32 bpp blitter (without palette animation). */
class FBlitter_32bppOptimized : public BlitterFactory {
public:
	FBlitter_32bppOptimized() : BlitterFactory("32bpp-optimized", "32bpp Optimized Blitter (no palette animation)") {}
	Blitter *CreateInstance() override { return new Blitter_32bppOptimized(); }
};

#endif /* BLITTER_32BPP_OPTIMIZED_HPP */
