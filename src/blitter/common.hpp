/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file common.hpp Common functionality for all blitter implementations. */

#ifndef BLITTER_COMMON_HPP
#define BLITTER_COMMON_HPP

#include "base.hpp"
#include "../core/math_func.hpp"

#include <utility>

template <typename SetPixelT>
void Blitter::DrawLineGeneric(int x, int y, int x2, int y2, int screen_width, int screen_height, int width, int dash, SetPixelT set_pixel)
{
	int dy;
	int dx;
	int stepx;
	int stepy;

	dy = (y2 - y) * 2;
	if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else {
		stepy = 1;
	}

	dx = (x2 - x) * 2;
	if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else {
		stepx = 1;
	}

	if (dx == 0 && dy == 0) {
		/* The algorithm below cannot handle this special case; make it work at least for line width 1 */
		if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) set_pixel(x, y);
		return;
	}

	int frac_diff = width * max(dx, dy);
	if (width > 1) {
		/* compute frac_diff = width * sqrt(dx*dx + dy*dy)
		 * Start interval:
		 *    max(dx, dy) <= sqrt(dx*dx + dy*dy) <= sqrt(2) * max(dx, dy) <= 3/2 * max(dx, dy) */
		int64 frac_sq = ((int64) width) * ((int64) width) * (((int64) dx) * ((int64) dx) + ((int64) dy) * ((int64) dy));
		int frac_max = 3 * frac_diff / 2;
		while (frac_diff < frac_max) {
			int frac_test = (frac_diff + frac_max) / 2;
			if (((int64) frac_test) * ((int64) frac_test) < frac_sq) {
				frac_diff = frac_test + 1;
			} else {
				frac_max = frac_test - 1;
			}
		}
	}

	int gap = dash;
	if (dash == 0) dash = 1;
	int dash_count = 0;
	if (dx > dy) {
		if (stepx < 0) {
			std::swap(x, x2);
			std::swap(y, y2);
			stepy = -stepy;
		}
		if (x2 < 0 || x >= screen_width) return;

		int y_low     = y;
		int y_high    = y;
		int frac_low  = dy - frac_diff / 2;
		int frac_high = dy + frac_diff / 2;

		while (frac_low < -(dx / 2)) {
			frac_low += dx;
			y_low -= stepy;
		}
		while (frac_high >= dx / 2) {
			frac_high -= dx;
			y_high += stepy;
		}

		if (x < 0) {
			dash_count = (-x) % (dash + gap);
			auto adjust_frac = [&](int64 frac, int &y_bound) -> int {
				frac -= ((int64) dy) * ((int64) x);
				if (frac >= 0) {
					int quotient = frac / dx;
					int remainder = frac % dx;
					y_bound += (1 + quotient) * stepy;
					frac = remainder - dx;
				}
				return frac;
			};
			frac_low = adjust_frac(frac_low, y_low);
			frac_high = adjust_frac(frac_high, y_high);
			x = 0;
		}
		x2++;
		if (x2 > screen_width) {
			x2 = screen_width;
		}

		while (x != x2) {
			if (dash_count < dash) {
				for (int y = y_low; y != y_high; y += stepy) {
					if (y >= 0 && y < screen_height) set_pixel(x, y);
				}
			}
			if (frac_low >= 0) {
				y_low += stepy;
				frac_low -= dx;
			}
			if (frac_high >= 0) {
				y_high += stepy;
				frac_high -= dx;
			}
			x++;
			frac_low += dy;
			frac_high += dy;
			if (++dash_count >= dash + gap) dash_count = 0;
		}
	} else {
		if (stepy < 0) {
			std::swap(x, x2);
			std::swap(y, y2);
			stepx = -stepx;
		}
		if (y2 < 0 || y >= screen_height) return;

		int x_low     = x;
		int x_high    = x;
		int frac_low  = dx - frac_diff / 2;
		int frac_high = dx + frac_diff / 2;

		while (frac_low < -(dy / 2)) {
			frac_low += dy;
			x_low -= stepx;
		}
		while (frac_high >= dy / 2) {
			frac_high -= dy;
			x_high += stepx;
		}

		if (y < 0) {
			dash_count = (-y) % (dash + gap);
			auto adjust_frac = [&](int64 frac, int &x_bound) -> int {
				frac -= ((int64) dx) * ((int64) y);
				if (frac >= 0) {
					int quotient = frac / dy;
					int remainder = frac % dy;
					x_bound += (1 + quotient) * stepx;
					frac = remainder - dy;
				}
				return frac;
			};
			frac_low = adjust_frac(frac_low, x_low);
			frac_high = adjust_frac(frac_high, x_high);
			y = 0;
		}
		y2++;
		if (y2 > screen_height) {
			y2 = screen_height;
		}

		while (y != y2) {
			if (dash_count < dash) {
				for (int x = x_low; x != x_high; x += stepx) {
					if (x >= 0 && x < screen_width) set_pixel(x, y);
				}
			}
			if (frac_low >= 0) {
				x_low += stepx;
				frac_low -= dy;
			}
			if (frac_high >= 0) {
				x_high += stepx;
				frac_high -= dy;
			}
			y++;
			frac_low += dx;
			frac_high += dx;
			if (++dash_count >= dash + gap) dash_count = 0;
		}
	}
}

#endif /* BLITTER_COMMON_HPP */
