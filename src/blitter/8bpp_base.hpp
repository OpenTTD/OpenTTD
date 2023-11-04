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

/** Base for all 8bpp blitters. */
class Blitter_8bppBase : public Blitter {
public:
	uint8_t GetScreenDepth() override { return 8; }
	void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) override;
	void *MoveTo(void *video, int x, int y) override;
	void SetPixel(void *video, int x, int y, uint8_t colour) override;
	void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8_t colour, int width, int dash) override;
	void DrawRect(void *video, int width, int height, uint8_t colour) override;
	void CopyFromBuffer(void *video, const void *src, int width, int height) override;
	void CopyToBuffer(const void *video, void *dst, int width, int height) override;
	void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) override;
	void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) override;
	size_t BufferSize(uint width, uint height) override;
	void PaletteAnimate(const Palette &palette) override;
	Blitter::PaletteAnimation UsePaletteAnimation() override;
};

#endif /* BLITTER_8BPP_BASE_HPP */
