/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_ssse3.h SSSE3 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSSE3_HPP
#define BLITTER_32BPP_SSSE3_HPP

#ifdef WITH_SSE

#ifndef SSE_VERSION
#define SSE_VERSION 3
#endif

#ifndef SSE_TARGET
#define SSE_TARGET "ssse3"
#endif

#ifndef FULL_ANIMATION
#define FULL_ANIMATION 0
#endif

#include "32bpp_sse2.h"

/** The SSSE3 32 bpp blitter (without palette animation). */
class Blitter_32bppSSSE3 : public Blitter_32bppSSE2 {
public:
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	template <BlitterMode mode, Blitter_32bppSSE_Base::ReadMode read_mode, Blitter_32bppSSE_Base::BlockType bt_last, bool translucent>
	void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
	const char *GetName() override { return "32bpp-ssse3"; }
};

/** Factory for the SSSE3 32 bpp blitter (without palette animation). */
class FBlitter_32bppSSSE3: public BlitterFactory {
public:
	FBlitter_32bppSSSE3() : BlitterFactory("32bpp-ssse3", "32bpp SSSE3 Blitter (no palette animation)", HasCPUIDFlag(1, 2, 9)) {}
	Blitter *CreateInstance() override { return new Blitter_32bppSSSE3(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSSE3_HPP */
