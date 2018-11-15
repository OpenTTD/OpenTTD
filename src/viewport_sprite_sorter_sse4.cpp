/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file viewport_sprite_sorter_sse4.cpp Sprite sorter that uses SSE4.1. */

#ifdef WITH_SSE

#include "stdafx.h"
#include "cpu.h"
#include "smmintrin.h"
#include "viewport_sprite_sorter.h"

#include "safeguards.h"

#ifdef _SQ64
	assert_compile((sizeof(ParentSpriteToDraw) % 16) == 0);
	#define LOAD_128 _mm_load_si128
#else
	#define LOAD_128 _mm_loadu_si128
#endif

/** Sort parent sprites pointer array using SSE4.1 optimizations. */
void ViewportSortParentSpritesSSE41(ParentSpriteToSortVector *psdv)
{
	const __m128i mask_ptest = _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0,  0,  0);
	ParentSpriteToDraw ** const psdvend = psdv->End();
	ParentSpriteToDraw **psd = psdv->Begin();
	while (psd != psdvend) {
		ParentSpriteToDraw * const ps = *psd;

		if (ps->comparison_done) {
			psd++;
			continue;
		}

		ps->comparison_done = true;

		for (ParentSpriteToDraw **psd2 = psd + 1; psd2 != psdvend; psd2++) {
			ParentSpriteToDraw * const ps2 = *psd2;

			if (ps2->comparison_done) continue;

			/*
			 * Decide which comparator to use, based on whether the bounding boxes overlap
			 *
			 * Original code:
			 * if (ps->xmax >= ps2->xmin && ps->xmin <= ps2->xmax && // overlap in X?
			 *     ps->ymax >= ps2->ymin && ps->ymin <= ps2->ymax && // overlap in Y?
			 *     ps->zmax >= ps2->zmin && ps->zmin <= ps2->zmax) { // overlap in Z?
			 *
			 * Above conditions are equivalent to:
			 * 1/    !( (ps->xmax >= ps2->xmin) && (ps->ymax >= ps2->ymin) && (ps->zmax >= ps2->zmin)   &&    (ps->xmin <= ps2->xmax) && (ps->ymin <= ps2->ymax) && (ps->zmin <= ps2->zmax) )
			 * 2/    !( (ps->xmax >= ps2->xmin) && (ps->ymax >= ps2->ymin) && (ps->zmax >= ps2->zmin)   &&    (ps2->xmax >= ps->xmin) && (ps2->ymax >= ps->ymin) && (ps2->zmax >= ps->zmin) )
			 * 3/  !( ( (ps->xmax >= ps2->xmin) && (ps->ymax >= ps2->ymin) && (ps->zmax >= ps2->zmin) ) &&  ( (ps2->xmax >= ps->xmin) && (ps2->ymax >= ps->ymin) && (ps2->zmax >= ps->zmin) ) )
			 * 4/ !( !( (ps->xmax <  ps2->xmin) || (ps->ymax <  ps2->ymin) || (ps->zmax <  ps2->zmin) ) && !( (ps2->xmax <  ps->xmin) || (ps2->ymax <  ps->ymin) || (ps2->zmax <  ps->zmin) ) )
			 * 5/ PTEST <---------------------------------- rslt1 ---------------------------------->         <------------------------------ rslt2 -------------------------------------->
			 */
			__m128i ps1_max = LOAD_128((__m128i*) &ps->xmax);
			__m128i ps2_min = LOAD_128((__m128i*) &ps2->xmin);
			__m128i rslt1 = _mm_cmplt_epi32(ps1_max, ps2_min);
			if (!_mm_testz_si128(mask_ptest, rslt1))
				continue;

			__m128i ps1_min = LOAD_128((__m128i*) &ps->xmin);
			__m128i ps2_max = LOAD_128((__m128i*) &ps2->xmax);
			__m128i rslt2 = _mm_cmplt_epi32(ps2_max, ps1_min);
			if (_mm_testz_si128(mask_ptest, rslt2)) {
				/* Use X+Y+Z as the sorting order, so sprites closer to the bottom of
				 * the screen and with higher Z elevation, are drawn in front.
				 * Here X,Y,Z are the coordinates of the "center of mass" of the sprite,
				 * i.e. X=(left+right)/2, etc.
				 * However, since we only care about order, don't actually divide / 2
				 */
				if (ps->xmin + ps->xmax + ps->ymin + ps->ymax + ps->zmin + ps->zmax <=
						ps2->xmin + ps2->xmax + ps2->ymin + ps2->ymax + ps2->zmin + ps2->zmax) {
					continue;
				}
			}

			/* Move ps2 in front of ps */
			ParentSpriteToDraw * const temp = ps2;
			for (ParentSpriteToDraw **psd3 = psd2; psd3 > psd; psd3--) {
				*psd3 = *(psd3 - 1);
			}
			*psd = temp;
		}
	}
}

/**
 * Check whether the current CPU supports SSE 4.1.
 * @return True iff the CPU supports SSE 4.1.
 */
bool ViewportSortParentSpritesSSE41Checker()
{
	return HasCPUIDFlag(1, 2, 19);
}

#endif /* WITH_SSE */
