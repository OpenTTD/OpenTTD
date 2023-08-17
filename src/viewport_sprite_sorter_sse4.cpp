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
#include <forward_list>
#include <stack>

#include "safeguards.h"

#ifdef POINTER_IS_64BIT
	static_assert((sizeof(ParentSpriteToDraw) % 16) == 0);
#	define LOAD_128 _mm_load_si128
#else
#	define LOAD_128 _mm_loadu_si128
#endif

GNU_TARGET("sse4.1")
void ViewportSortParentSpritesSSE41(ParentSpriteToSortVector *psdv)
{
	if (psdv->size() < 2) return;

	const __m128i mask_ptest = _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0,  0,  0);

	/* We rely on sprites being, for the most part, already ordered.
	 * So we don't need to move many of them and can keep track of their
	 * order efficiently by using stack. We always move sprites to the front
	 * of the current position, i.e. to the top of the stack.
	 * Also use special constants to indicate sorting state without
	 * adding extra fields to ParentSpriteToDraw structure.
	 */
	const uint32_t ORDER_COMPARED = UINT32_MAX; // Sprite was compared but we still need to compare the ones preceding it
	const uint32_t ORDER_RETURNED = UINT32_MAX - 1; // Mark sorted sprite in case there are other occurrences of it in the stack
	std::stack<ParentSpriteToDraw *> sprite_order;
	uint32_t next_order = 0;

	std::forward_list<std::pair<int64_t, ParentSpriteToDraw *>> sprite_list;  // We store sprites in a list sorted by xmin+ymin

	/* Initialize sprite list and order. */
	for (auto p = psdv->rbegin(); p != psdv->rend(); p++) {
		sprite_list.push_front(std::make_pair((*p)->xmin + (*p)->ymin, *p));
		sprite_order.push(*p);
		(*p)->order = next_order++;
	}

	sprite_list.sort();

	std::vector<ParentSpriteToDraw *> preceding;  // Temporarily stores sprites that precede current and their position in the list
	auto preceding_prev = sprite_list.begin(); // Store iterator in case we need to delete a single preciding sprite
	auto out = psdv->begin();  // Iterator to output sorted sprites

	while (!sprite_order.empty()) {

		auto s = sprite_order.top();
		sprite_order.pop();

		/* Sprite is already sorted, ignore it. */
		if (s->order == ORDER_RETURNED) continue;

		/* Sprite was already compared, just need to output it. */
		if (s->order == ORDER_COMPARED) {
			*(out++) = s;
			s->order = ORDER_RETURNED;
			continue;
		}

		preceding.clear();

		/* We only need sprites with xmin <= s->xmax && ymin <= s->ymax && zmin <= s->zmax
		 * So by iterating sprites with xmin + ymin <= s->xmax + s->ymax
		 * we get all we need and some more that we filter out later.
		 * We don't include zmin into the sum as there are usually more neighbors on x and y than z
		 * so including it will actually increase the amount of false positives.
		 * Also min coordinates can be > max so using max(xmin, xmax) + max(ymin, ymax)
		 * to ensure that we iterate the current sprite as we need to remove it from the list.
		 */
		auto ssum = std::max(s->xmax, s->xmin) + std::max(s->ymax, s->ymin);
		auto prev = sprite_list.before_begin();
		auto x = sprite_list.begin();
		while (x != sprite_list.end() && ((*x).first <= ssum)) {
			auto p = (*x).second;
			if (p == s) {
				/* We found the current sprite, remove it and move on. */
				x = sprite_list.erase_after(prev);
				continue;
			}

			auto p_prev = prev;
			prev = x++;

			/* Check that p->xmin <= s->xmax && p->ymin <= s->ymax && p->zmin <= s->zmax */
			__m128i s_max = LOAD_128((__m128i*) &s->xmax);
			__m128i p_min = LOAD_128((__m128i*) &p->xmin);
			__m128i r1 = _mm_cmplt_epi32(s_max, p_min);
			if (!_mm_testz_si128(mask_ptest, r1))
				continue;

			/* Check if sprites overlap, i.e.
			 * s->xmin <= p->xmax && s->ymin <= p->ymax && s->zmin <= p->zmax
			 */
			__m128i s_min = LOAD_128((__m128i*) &s->xmin);
			__m128i p_max = LOAD_128((__m128i*) &p->xmax);
			__m128i r2 = _mm_cmplt_epi32(p_max, s_min);
			if (_mm_testz_si128(mask_ptest, r2)) {
				/* Use X+Y+Z as the sorting order, so sprites closer to the bottom of
				 * the screen and with higher Z elevation, are drawn in front.
				 * Here X,Y,Z are the coordinates of the "center of mass" of the sprite,
				 * i.e. X=(left+right)/2, etc.
				 * However, since we only care about order, don't actually divide / 2
				 */
				if (s->xmin + s->xmax + s->ymin + s->ymax + s->zmin + s->zmax <=
						p->xmin + p->xmax + p->ymin + p->ymax + p->zmin + p->zmax) {
					continue;
				}
			}

			preceding.push_back(p);
			preceding_prev = p_prev;
		}

		if (preceding.empty()) {
			/* No preceding sprites, add current one to the output */
			*(out++) = s;
			s->order = ORDER_RETURNED;
			continue;
		}

		/* Optimization for the case when we only have 1 sprite to move. */
		if (preceding.size() == 1) {
			auto p = preceding[0];
			/* We can only output the preceding sprite if there can't be any other sprites preceding it. */
			if (p->xmax <= s->xmax && p->ymax <= s->ymax && p->zmax <= s->zmax) {
				p->order = ORDER_RETURNED;
				s->order = ORDER_RETURNED;
				sprite_list.erase_after(preceding_prev);
				*(out++) = p;
				*(out++) = s;
				continue;
			}
		}

		/* Sort all preceding sprites by order and assign new orders in reverse (as original sorter did). */
		std::sort(preceding.begin(), preceding.end(), [](const ParentSpriteToDraw *a, const ParentSpriteToDraw *b) {
			return a->order > b->order;
		});

		s->order = ORDER_COMPARED;
		sprite_order.push(s);  // Still need to output so push it back for now

		for (auto p: preceding) {
			p->order = next_order++;
			sprite_order.push(p);
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
