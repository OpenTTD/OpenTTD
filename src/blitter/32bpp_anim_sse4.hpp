/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse4_anim.hpp A SSE4 32 bpp blitter with animation support. */

#ifndef BLITTER_32BPP_SSE4_ANIM_HPP
#define BLITTER_32BPP_SSE4_ANIM_HPP

#ifdef WITH_SSE

#ifndef SSE_VERSION
#define SSE_VERSION 4
#endif

#ifndef FULL_ANIMATION
#define FULL_ANIMATION 1
#endif

#include "32bpp_anim.hpp"
#include "32bpp_anim_sse2.hpp"
#include "32bpp_sse4.hpp"

#undef MARGIN_NORMAL_THRESHOLD
#define MARGIN_NORMAL_THRESHOLD 4

/** The SSE4 32 bpp blitter with palette animation. */
class Blitter_32bppSSE4_Anim FINAL : public Blitter_32bppSSE2_Anim, public Blitter_32bppSSE_Base {
private:

public:
	template <BlitterMode mode, Blitter_32bppSSE_Base::ReadMode read_mode, Blitter_32bppSSE_Base::BlockType bt_last, bool translucent, bool animated>
	/* virtual */ void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ Sprite *Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator) {
		return Blitter_32bppSSE_Base::Encode(sprite, allocator);
	}
	/* virtual */ const char *GetName() { return "32bpp-sse4-anim"; }
};

/** Factory for the SSE4 32 bpp blitter (with palette animation). */
class FBlitter_32bppSSE4_Anim: public BlitterFactory {
public:
	FBlitter_32bppSSE4_Anim() : BlitterFactory("32bpp-sse4-anim", "SSE4 Blitter (palette animation)", HasCPUIDFlag(1, 2, 19)) {}
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSSE4_Anim(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE4_ANIM_HPP */
