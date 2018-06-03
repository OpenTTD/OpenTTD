/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_anim.hpp A partially SSE2 32 bpp blitter with animation support. */

#ifndef BLITTER_32BPP_SSE2_ANIM_HPP
#define BLITTER_32BPP_SSE2_ANIM_HPP

#ifdef WITH_SSE

#ifndef SSE_VERSION
#define SSE_VERSION 2
#endif

#ifndef FULL_ANIMATION
#define FULL_ANIMATION 1
#endif

#include "32bpp_anim.hpp"
#include "32bpp_sse2.hpp"

/** A partially 32 bpp blitter with palette animation. */
class Blitter_32bppSSE2_Anim : public Blitter_32bppAnim {
public:
	/* virtual */ void PaletteAnimate(const Palette &palette);
	/* virtual */ const char *GetName() { return "32bpp-sse2-anim"; }
};

/** Factory for the partially 32bpp blitter with animation. */
class FBlitter_32bppSSE2_Anim : public BlitterFactory {
public:
	FBlitter_32bppSSE2_Anim() : BlitterFactory("32bpp-sse2-anim", "32bpp partially SSE2 Animation Blitter (palette animation)", HasCPUIDFlag(1, 3, 26)) {}
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSSE2_Anim(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_ANIM_HPP */
