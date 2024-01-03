/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_simple.hpp Simple 32 bpp blitter. */

#ifndef BLITTER_32BPP_SIMPLE_HPP
#define BLITTER_32BPP_SIMPLE_HPP

#include "32bpp_base.hpp"
#include "factory.hpp"

/** The most trivial 32 bpp blitter (without palette animation). */
class Blitter_32bppSimple : public Blitter_32bppBase {
	struct Pixel {
		uint8_t r;  ///< Red-channel
		uint8_t g;  ///< Green-channel
		uint8_t b;  ///< Blue-channel
		uint8_t a;  ///< Alpha-channel
		uint8_t m;  ///< Remap-channel
		uint8_t v;  ///< Brightness-channel
	};
public:
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) override;
	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override;

	const char *GetName() override { return "32bpp-simple"; }
};

/** Factory for the simple 32 bpp blitter. */
class FBlitter_32bppSimple : public BlitterFactory {
public:
	FBlitter_32bppSimple() : BlitterFactory("32bpp-simple", "32bpp Simple Blitter (no palette animation)") {}
	Blitter *CreateInstance() override { return new Blitter_32bppSimple(); }
};

#endif /* BLITTER_32BPP_SIMPLE_HPP */
