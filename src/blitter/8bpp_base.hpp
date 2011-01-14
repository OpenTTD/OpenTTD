/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 8bpp_base.hpp Base for all 8 bpp blitters. */

#ifndef BLITTER_8BPP_BASE_HPP
#define BLITTER_8BPP_BASE_HPP

#include "base.hpp"

class Blitter_8bppBase : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 8; }
//	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal);
//	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, AllocatorProc *allocator);
	/* virtual */ void *MoveTo(const void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 colour);
	/* virtual */ void DrawRect(void *video, int width, int height, uint8 colour);
	/* virtual */ void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 colour);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height);
	/* virtual */ void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y);
	/* virtual */ int BufferSize(int width, int height);
	/* virtual */ void PaletteAnimate(uint start, uint count);
	/* virtual */ Blitter::PaletteAnimation UsePaletteAnimation();
	/* virtual */ int GetBytesPerPixel() { return 1; }
};

#endif /* BLITTER_8BPP_BASE_HPP */
