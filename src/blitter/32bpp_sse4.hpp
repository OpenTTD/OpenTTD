/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse4.hpp SSE4 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSE4_HPP
#define BLITTER_32BPP_SSE4_HPP

#ifdef WITH_SSE

#ifndef SSE_VERSION
#define SSE_VERSION 4
#endif

#ifndef SSE_TARGET
#define SSE_TARGET "sse4.1"
#endif

#ifndef FULL_ANIMATION
#define FULL_ANIMATION 0
#endif

#include "32bpp_ssse3.hpp"

/** The SSE4 32 bpp blitter (without palette animation). */
class Blitter_32bppSSE4 : public Blitter_32bppSSSE3 {
public:
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	template <BlitterMode mode, Blitter_32bppSSE_Base::ReadMode read_mode, Blitter_32bppSSE_Base::BlockType bt_last, bool translucent>
	void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
	std::string_view GetName() override { return "32bpp-sse4"; }
};

/** Factory for the SSE4 32 bpp blitter (without palette animation). */
class FBlitter_32bppSSE4: public BlitterFactory {
public:
	FBlitter_32bppSSE4() : BlitterFactory("32bpp-sse4", "32bpp SSE4 Blitter (no palette animation)", HasCPUIDFlag(1, 2, 19)) {}
	Blitter *CreateInstance() override { return new Blitter_32bppSSE4(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE4_HPP */
