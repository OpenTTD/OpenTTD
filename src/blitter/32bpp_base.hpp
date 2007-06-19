/* $Id$ */

/** @file 32bpp_base.hpp */

#ifndef BLITTER_32BPP_BASE_HPP
#define BLITTER_32BPP_BASE_HPP

#include "base.hpp"

class Blitter_32bppBase : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 32; }
//	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
//	/* virtual */ void DrawColorMappingRect(void *dst, int width, int height, int pal);
//	/* virtual */ Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator);
	/* virtual */ void *MoveTo(const void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 color);
	/* virtual */ void SetPixelIfEmpty(void *video, int x, int y, uint8 color);
	/* virtual */ void DrawRect(void *video, int width, int height, uint8 color);
	/* virtual */ void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 color);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void MoveBuffer(void *video_dst, const void *video_src, int width, int height);
	/* virtual */ void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y);
	/* virtual */ int BufferSize(int width, int height);
	/* virtual */ void PaletteAnimate(uint start, uint count);
	/* virtual */ Blitter::PaletteAnimation UsePaletteAnimation();

	/**
	 * Compose a colour based on RGB values.
	 */
	static inline uint ComposeColour(uint a, uint r, uint g, uint b)
	{
		return (((a) << 24) & 0xFF000000) | (((r) << 16) & 0x00FF0000) | (((g) << 8) & 0x0000FF00) | ((b) & 0x000000FF);
	}

	/**
	 * Look up the colour in the current palette.
	 **/
	static inline uint32 LookupColourInPalette(uint8 index)
	{
		if (index == 0) return 0x00000000; // Full transparent pixel */
		return ComposeColour(0xFF, _cur_palette[index].r, _cur_palette[index].g, _cur_palette[index].b);
	}

	/**
	 * Compose a colour based on RGBA values and the current pixel value.
	 */
	static inline uint ComposeColourRGBA(uint r, uint g, uint b, uint a, uint current)
	{
		uint cr, cg, cb;
		cr = GB(current, 16, 8);
		cg = GB(current, 8,  8);
		cb = GB(current, 0,  8);

		return ComposeColour(0xFF,
												(r * a + cr * (255 - a)) / 255,
												(g * a + cg * (255 - a)) / 255,
												(b * a + cb * (255 - a)) / 255);
	}

	/**
	* Compose a colour based on Pixel value, alpha value, and the current pixel value.
	*/
	static inline uint ComposeColourPA(uint colour, uint a, uint current)
	{
		uint r, g, b, cr, cg, cb;
		r  = GB(colour,   16, 8);
		g  = GB(colour,   8,  8);
		b  = GB(colour,   0,  8);
		cr = GB(current, 16, 8);
		cg = GB(current, 8,  8);
		cb = GB(current, 0,  8);

		return ComposeColour(0xFF,
												(r * a + cr * (255 - a)) / 255,
												(g * a + cg * (255 - a)) / 255,
												(b * a + cb * (255 - a)) / 255);
	}

	/**
	* Make a pixel looks like it is transparent.
	* @param colour the colour already on the screen.
	* @param amount the amount of transparency, times 100.
	* @return the new colour for the screen.
	*/
	static inline uint MakeTransparent(uint colour, uint amount)
	{
		uint r, g, b;
		r = GB(colour, 16, 8);
		g = GB(colour, 8,  8);
		b = GB(colour, 0,  8);

		return ComposeColour(0xFF, r * amount / 100, g * amount / 100, b * amount / 100);
	}

	/**
	* Make a colour grey-based.
	* @param colour the colour to make grey.
	* @return the new colour, now grey.
	*/
	static inline uint MakeGrey(uint colour)
	{
		uint r, g, b;
		r = GB(colour, 16, 8);
		g = GB(colour, 8,  8);
		b = GB(colour, 0,  8);

		/* To avoid doubles and stuff, multiple it with a total of 65536 (16bits), then
		*  divide by it to normalize the value to a byte again. See heightmap.cpp for
		*  information about the formula. */
		colour = ((r * 19595) + (g * 38470) + (b * 7471)) / 65536;

		return ComposeColour(0xFF, colour, colour, colour);
	}
};

#endif /* BLITTER_32BPP_BASE_HPP */
