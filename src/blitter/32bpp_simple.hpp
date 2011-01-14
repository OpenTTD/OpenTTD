/* $Id$ */

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

class Blitter_32bppSimple : public Blitter_32bppBase {
public:
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal);
	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, AllocatorProc *allocator);

	/* virtual */ const char *GetName() { return "32bpp-simple"; }
};

class FBlitter_32bppSimple: public BlitterFactory<FBlitter_32bppSimple> {
public:
	/* virtual */ const char *GetName() { return "32bpp-simple"; }
	/* virtual */ const char *GetDescription() { return "32bpp Simple Blitter (no palette animation)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSimple(); }
};

#endif /* BLITTER_32BPP_SIMPLE_HPP */
