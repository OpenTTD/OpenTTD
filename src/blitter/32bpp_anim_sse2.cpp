/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_anim.cpp Implementation of a partially SSSE2 32bpp blitter with animation support. */

#ifdef WITH_SSE

#include "../stdafx.h"
#include "../video/video_driver.hpp"
#include "32bpp_anim_sse2.hpp"
#include "32bpp_sse_func.hpp"

#include "../safeguards.h"

/** Instantiation of the partially SSSE2 32bpp with animation blitter factory. */
static FBlitter_32bppSSE2_Anim iFBlitter_32bppSSE2_Anim;

GNU_TARGET("sse2")
void Blitter_32bppSSE2_Anim::PaletteAnimate(const Palette &palette)
{
	assert(!_screen_disable_anim);

	this->palette = palette;
	/* If first_dirty is 0, it is for 8bpp indication to send the new
	 *  palette. However, only the animation colours might possibly change.
	 *  Especially when going between toyland and non-toyland. */
	assert(this->palette.first_dirty == PALETTE_ANIM_START || this->palette.first_dirty == 0);

	const uint16_t *anim = this->anim_buf;
	Colour *dst = (Colour *)_screen.dst_ptr;

	bool screen_dirty = false;

	/* Let's walk the anim buffer and try to find the pixels */
	const int width = this->anim_buf_width;
	const int screen_pitch = _screen.pitch;
	const int anim_pitch = this->anim_buf_pitch;
	__m128i anim_cmp = _mm_set1_epi16(PALETTE_ANIM_START - 1);
	__m128i brightness_cmp = _mm_set1_epi16(Blitter_32bppBase::DEFAULT_BRIGHTNESS);
	__m128i colour_mask = _mm_set1_epi16(0xFF);
	for (int y = this->anim_buf_height; y != 0 ; y--) {
		Colour *next_dst_ln = dst + screen_pitch;
		const uint16_t *next_anim_ln = anim + anim_pitch;
		int x = width;
		while (x > 0) {
			__m128i data = _mm_load_si128((const __m128i *) anim);

			/* low bytes only, shifted into high positions */
			__m128i colour_data = _mm_and_si128(data, colour_mask);

			/* test if any colour >= PALETTE_ANIM_START */
			int colour_cmp_result = _mm_movemask_epi8(_mm_cmpgt_epi16(colour_data, anim_cmp));
			if (colour_cmp_result) {
				/* test if any brightness is unexpected */
				if (x < 8 || colour_cmp_result != 0xFFFF ||
						_mm_movemask_epi8(_mm_cmpeq_epi16(_mm_srli_epi16(data, 8), brightness_cmp)) != 0xFFFF) {
					/* slow path: < 8 pixels left or unexpected brightnesses */
					for (int z = std::min<int>(x, 8); z != 0 ; z--) {
						int value = _mm_extract_epi16(data, 0);
						uint8_t colour = GB(value, 0, 8);
						if (colour >= PALETTE_ANIM_START) {
							/* Update this pixel */
							*dst = AdjustBrightneSSE(LookupColourInPalette(colour), GB(value, 8, 8));
							screen_dirty = true;
						}
						data = _mm_srli_si128(data, 2);
						dst++;
					}
				} else {
					/* medium path: 8 pixels to animate all of expected brightnesses */
					for (int z = 0; z < 8; z++) {
						*dst = LookupColourInPalette(_mm_extract_epi16(colour_data, 0));
						colour_data = _mm_srli_si128(colour_data, 2);
						dst++;
					}
					screen_dirty = true;
				}
			} else {
				/* fast path, no animation */
				dst += 8;
			}
			anim += 8;
			x -= 8;
		}
		dst = next_dst_ln;
		anim = next_anim_ln;
	}

	if (screen_dirty) {
		/* Make sure the backend redraws the whole screen */
		VideoDriver::GetInstance()->MakeDirty(0, 0, _screen.width, _screen.height);
	}
}

#endif /* WITH_SSE */
