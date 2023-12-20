/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 8bpp_simple.hpp Simple (and slow) 8 bpp blitter. */

#ifndef BLITTER_8BPP_SIMPLE_HPP
#define BLITTER_8BPP_SIMPLE_HPP

#include "8bpp_base.hpp"
#include "factory.hpp"

/** Most trivial 8bpp blitter. */
class Blitter_8bppSimple FINAL : public Blitter_8bppBase {
public:
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override;

	const char *GetName() override { return "8bpp-simple"; }
};

/** Factory for the most trivial 8bpp blitter. */
class FBlitter_8bppSimple : public BlitterFactory {
public:
	FBlitter_8bppSimple() : BlitterFactory("8bpp-simple", "8bpp Simple Blitter (relative slow, but never wrong)") {}
	Blitter *CreateInstance() override { return new Blitter_8bppSimple(); }
};

#endif /* BLITTER_8BPP_SIMPLE_HPP */
