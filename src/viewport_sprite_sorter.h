/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file viewport_sprite_sorter.h Types related to sprite sorting. */

#include "stdafx.h"
#include "core/smallvec_type.hpp"
#include "gfx_type.h"

#ifndef VIEWPORT_SPRITE_SORTER_H
#define VIEWPORT_SPRITE_SORTER_H

/** Parent sprite that should be drawn */
struct ParentSpriteToDraw {
	/* Block of 16B loadable in xmm register */
	int32 xmin;                     ///< minimal world X coordinate of bounding box
	int32 ymin;                     ///< minimal world Y coordinate of bounding box
	int32 zmin;                     ///< minimal world Z coordinate of bounding box
	int32 x;                        ///< screen X coordinate of sprite

	/* Second block of 16B loadable in xmm register */
	int32 xmax;                     ///< maximal world X coordinate of bounding box
	int32 ymax;                     ///< maximal world Y coordinate of bounding box
	int32 zmax;                     ///< maximal world Z coordinate of bounding box
	int32 y;                        ///< screen Y coordinate of sprite

	SpriteID image;                 ///< sprite to draw
	PaletteID pal;                  ///< palette to use
	const SubSprite *sub;           ///< only draw a rectangular part of the sprite

	int32 left;                     ///< minimal screen X coordinate of sprite (= x + sprite->x_offs), reference point for child sprites
	int32 top;                      ///< minimal screen Y coordinate of sprite (= y + sprite->y_offs), reference point for child sprites

	int32 first_child;              ///< the first child to draw.
	bool comparison_done;           ///< Used during sprite sorting: true if sprite has been compared with all other sprites
};

typedef SmallVector<ParentSpriteToDraw*, 64> ParentSpriteToSortVector;

/** Type for method for checking whether a viewport sprite sorter exists. */
typedef bool (*VpSorterChecker)();
/** Type for the actual viewport sprite sorter. */
typedef void (*VpSpriteSorter)(ParentSpriteToSortVector *psd);

#ifdef WITH_SSE
bool ViewportSortParentSpritesSSE41Checker();
void ViewportSortParentSpritesSSE41(ParentSpriteToSortVector *psdv);
#endif

void InitializeSpriteSorter();

#endif /* VIEWPORT_SPRITE_SORTER_H */
