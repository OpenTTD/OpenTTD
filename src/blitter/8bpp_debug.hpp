/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 8bpp_debug.hpp A 8 bpp blitter that uses random colours to show the drawn sprites. */

#ifndef BLITTER_8BPP_DEBUG_HPP
#define BLITTER_8BPP_DEBUG_HPP

#include "8bpp_base.hpp"
#include "factory.hpp"

class Blitter_8bppDebug : public Blitter_8bppBase {
public:
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, AllocatorProc *allocator);

	/* virtual */ const char *GetName() { return "8bpp-debug"; }
};

class FBlitter_8bppDebug: public BlitterFactory<FBlitter_8bppDebug> {
public:
	/* virtual */ const char *GetName() { return "8bpp-debug"; }
	/* virtual */ const char *GetDescription() { return "8bpp Debug Blitter (testing only)"; }
	/* virtual */ Blitter *CreateInstance() { return new Blitter_8bppDebug(); }
};

#endif /* BLITTER_8BPP_DEBUG_HPP */
