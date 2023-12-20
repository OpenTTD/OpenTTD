/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse2.hpp SSE2 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSE2_HPP
#define BLITTER_32BPP_SSE2_HPP

#ifdef WITH_SSE

#ifndef SSE_VERSION
#define SSE_VERSION 2
#endif

#ifndef SSE_TARGET
#define SSE_TARGET "sse2"
#endif

#ifndef FULL_ANIMATION
#define FULL_ANIMATION 0
#endif

#include "32bpp_sse_type.h"

/** Base methods for 32bpp SSE blitters. */
class Blitter_32bppSSE_Base {
public:
	virtual ~Blitter_32bppSSE_Base() = default;

	struct MapValue {
		uint8_t m;
		uint8_t v;
	};
	static_assert(sizeof(MapValue) == 2);

	/** Helper for creating specialised functions for specific optimisations. */
	enum ReadMode {
		RM_WITH_SKIP,   ///< Use normal code for skipping empty pixels.
		RM_WITH_MARGIN, ///< Use cached number of empty pixels at begin and end of line to reduce work.
		RM_NONE,        ///< No specialisation.
	};

	/** Helper for creating specialised functions for the case where the sprite width is odd or even. */
	enum BlockType {
		BT_EVEN, ///< An even number of pixels in the width; no need for a special case for the last pixel.
		BT_ODD,  ///< An odd number of pixels in the width; special case for the last pixel.
		BT_NONE, ///< No specialisation for either case.
	};

	/** Helper for using specialised functions designed to prevent whenever it's possible things like:
	 *  - IO (reading video buffer),
	 *  - calculations (alpha blending),
	 *  - heavy branching (remap lookups and animation buffer handling).
	 */
	enum SpriteFlags {
		SF_NONE        = 0,
		SF_TRANSLUCENT = 1 << 1, ///< The sprite has at least 1 translucent pixel.
		SF_NO_REMAP    = 1 << 2, ///< The sprite has no remappable colour pixel.
		SF_NO_ANIM     = 1 << 3, ///< The sprite has no palette animated pixel.
	};

	/** Data stored about a (single) sprite. */
	struct SpriteInfo {
		uint32_t sprite_offset;    ///< The offset to the sprite data.
		uint32_t mv_offset;        ///< The offset to the map value data.
		uint16_t sprite_line_size; ///< The size of a single line (pitch).
		uint16_t sprite_width;     ///< The width of the sprite.
	};
	struct SpriteData {
		SpriteFlags flags;
		SpriteInfo infos[ZOOM_LVL_END];
		byte data[]; ///< Data, all zoomlevels.
	};

	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator);
};

DECLARE_ENUM_AS_BIT_SET(Blitter_32bppSSE_Base::SpriteFlags);

/** The SSE2 32 bpp blitter (without palette animation). */
class Blitter_32bppSSE2 : public Blitter_32bppSimple, public Blitter_32bppSSE_Base {
public:
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	template <BlitterMode mode, Blitter_32bppSSE_Base::ReadMode read_mode, Blitter_32bppSSE_Base::BlockType bt_last, bool translucent>
	void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);

	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override {
		return Blitter_32bppSSE_Base::Encode(sprite, allocator);
	}

	const char *GetName() override { return "32bpp-sse2"; }
};

/** Factory for the SSE2 32 bpp blitter (without palette animation). */
class FBlitter_32bppSSE2 : public BlitterFactory {
public:
	FBlitter_32bppSSE2() : BlitterFactory("32bpp-sse2", "32bpp SSE2 Blitter (no palette animation)", HasCPUIDFlag(1, 3, 26)) {}
	Blitter *CreateInstance() override { return new Blitter_32bppSSE2(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE2_HPP */
