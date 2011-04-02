/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base.cpp Implementation of the base for all blitters. */

#include "../stdafx.h"
#include "base.hpp"

/**
 * Draw a line with a given colour.
 * @param video The destination pointer (video-buffer).
 * @param x The x coordinate from where the line starts.
 * @param y The y coordinate from where the line starts.
 * @param x2 The x coordinate to where the line goes.
 * @param y2 The y coordinate to where the lines goes.
 * @param screen_width The width of the screen you are drawing in (to avoid buffer-overflows).
 * @param screen_height The height of the screen you are drawing in (to avoid buffer-overflows).
 * @param colour A 8bpp mapping colour.
 */
void Blitter::DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 colour)
{
	int dy;
	int dx;
	int stepx;
	int stepy;
	int frac;

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

	if (dx > dy) {
		frac = dy - (dx / 2);
		x2 += stepx; // Make x2 the first column to not draw to
		while (x != x2) {
			if (x >= 0 && y >= 0 && x < screen_width && y < screen_height) this->SetPixel(video, x, y, colour);
			if (frac >= 0) {
				y += stepy;
				frac -= dx;
			}
			x += stepx;
			frac += dy;
		}
	} else {
		frac = dx - (dy / 2);
		y2 += stepy; // Make y2 the first row to not draw to
		while (y != y2) {
			if (x >= 0 && y >= 0 && x < screen_width && y < screen_height) this->SetPixel(video, x, y, colour);
			if (frac >= 0) {
				x += stepx;
				frac -= dy;
			}
			y += stepy;
			frac += dx;
		}
	}
}
