/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base.hpp Base for all blitters. */

#ifndef BLITTER_BASE_HPP
#define BLITTER_BASE_HPP

#include "../spritecache.h"
#include "../spriteloader/spriteloader.hpp"

/** The modes of blitting we can do. */
enum BlitterMode {
	BM_NORMAL,       ///< Perform the simple blitting.
	BM_COLOUR_REMAP, ///< Perform a colour remapping.
	BM_TRANSPARENT,  ///< Perform transparency darkening remapping.
	BM_TRANSPARENT_REMAP, ///< Perform transparency colour remapping.
	BM_CRASH_REMAP,  ///< Perform a crash remapping.
	BM_BLACK_REMAP,  ///< Perform remapping to a completely blackened sprite
};

/**
 * How all blitters should look like. Extend this class to make your own.
 */
class Blitter : public SpriteEncoder {
public:
	/** Parameters related to blitting. */
	struct BlitterParams {
		const void *sprite; ///< Pointer to the sprite how ever the encoder stored it
		const byte *remap;  ///< XXX -- Temporary storage for remap array

		int skip_left;      ///< How much pixels of the source to skip on the left (based on zoom of dst)
		int skip_top;       ///< How much pixels of the source to skip on the top (based on zoom of dst)
		int width;          ///< The width in pixels that needs to be drawn to dst
		int height;         ///< The height in pixels that needs to be drawn to dst
		int sprite_width;   ///< Real width of the sprite
		int sprite_height;  ///< Real height of the sprite
		int left;           ///< The left offset in the 'dst' in pixels to start drawing
		int top;            ///< The top offset in the 'dst' in pixels to start drawing

		void *dst;          ///< Destination buffer
		int pitch;          ///< The pitch of the destination buffer
	};

	/** Types of palette animation. */
	enum PaletteAnimation {
		PALETTE_ANIMATION_NONE,           ///< No palette animation
		PALETTE_ANIMATION_VIDEO_BACKEND,  ///< Palette animation should be done by video backend (8bpp only!)
		PALETTE_ANIMATION_BLITTER,        ///< The blitter takes care of the palette animation
	};

	/**
	 * Get the screen depth this blitter works for.
	 *  This is either: 8, 16, 24 or 32.
	 */
	virtual uint8_t GetScreenDepth() = 0;

	bool Is32BppSupported() override
	{
		return this->GetScreenDepth() > 8;
	}

	/**
	 * Draw an image to the screen, given an amount of params defined above.
	 */
	virtual void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) = 0;

	/**
	 * Draw a colourtable to the screen. This is: the colour of the screen is read
	 *  and is looked-up in the palette to match a new colour, which then is put
	 *  on the screen again.
	 * @param dst the destination pointer (video-buffer).
	 * @param width the width of the buffer.
	 * @param height the height of the buffer.
	 * @param pal the palette to use.
	 */
	virtual void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) = 0;

	/**
	 * Move the destination pointer the requested amount x and y, keeping in mind
	 *  any pitch and bpp of the renderer.
	 * @param video The destination pointer (video-buffer) to scroll.
	 * @param x How much you want to scroll to the right.
	 * @param y How much you want to scroll to the bottom.
	 * @return A new destination pointer moved the the requested place.
	 */
	virtual void *MoveTo(void *video, int x, int y) = 0;

	/**
	 * Draw a pixel with a given colour on the video-buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param x The x position within video-buffer.
	 * @param y The y position within video-buffer.
	 * @param colour A 8bpp mapping colour.
	 */
	virtual void SetPixel(void *video, int x, int y, uint8_t colour) = 0;

	/**
	 * Make a single horizontal line in a single colour on the video-buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param width The length of the line.
	 * @param height The height of the line.
	 * @param colour A 8bpp mapping colour.
	 */
	virtual void DrawRect(void *video, int width, int height, uint8_t colour) = 0;

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
	 * @param width Line width.
	 * @param dash Length of dashes for dashed lines. 0 means solid line.
	 */
	virtual void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8_t colour, int width, int dash = 0) = 0;

	/**
	 * Copy from a buffer to the screen.
	 * @param video The destination pointer (video-buffer).
	 * @param src The buffer from which the data will be read.
	 * @param width The width of the buffer.
	 * @param height The height of the buffer.
	 * @note You can not do anything with the content of the buffer, as the blitter can store non-pixel data in it too!
	 */
	virtual void CopyFromBuffer(void *video, const void *src, int width, int height) = 0;

	/**
	 * Copy from the screen to a buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param dst The buffer in which the data will be stored.
	 * @param width The width of the buffer.
	 * @param height The height of the buffer.
	 * @note You can not do anything with the content of the buffer, as the blitter can store non-pixel data in it too!
	 */
	virtual void CopyToBuffer(const void *video, void *dst, int width, int height) = 0;

	/**
	 * Copy from the screen to a buffer in a palette format for 8bpp and RGBA format for 32bpp.
	 * @param video The destination pointer (video-buffer).
	 * @param dst The buffer in which the data will be stored.
	 * @param width The width of the buffer.
	 * @param height The height of the buffer.
	 * @param dst_pitch The pitch (byte per line) of the destination buffer.
	 */
	virtual void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) = 0;

	/**
	 * Scroll the videobuffer some 'x' and 'y' value.
	 * @param video The buffer to scroll into.
	 * @param left The left value of the screen to scroll.
	 * @param top The top value of the screen to scroll.
	 * @param width The width of the screen to scroll.
	 * @param height The height of the screen to scroll.
	 * @param scroll_x How much to scroll in X.
	 * @param scroll_y How much to scroll in Y.
	 */
	virtual void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) = 0;

	/**
	 * Calculate how much memory there is needed for an image of this size in the video-buffer.
	 * @param width The width of the buffer-to-be.
	 * @param height The height of the buffer-to-be.
	 * @return The size needed for the buffer.
	 */
	virtual size_t BufferSize(uint width, uint height) = 0;

	/**
	 * Called when the 8bpp palette is changed; you should redraw all pixels on the screen that
	 *  are equal to the 8bpp palette indexes 'first_dirty' to 'first_dirty + count_dirty'.
	 * @param palette The new palette.
	 */
	virtual void PaletteAnimate(const Palette &palette) = 0;

	/**
	 * Check if the blitter uses palette animation at all.
	 * @return True if it uses palette animation.
	 */
	virtual Blitter::PaletteAnimation UsePaletteAnimation() = 0;

	/**
	 * Does this blitter require a separate animation buffer from the video backend?
	 */
	virtual bool NeedsAnimationBuffer()
	{
		return false;
	}

	/**
	 * Get the name of the blitter, the same as the Factory-instance returns.
	 */
	virtual const char *GetName() = 0;

	/**
	 * Post resize event
	 */
	virtual void PostResize() { };

	virtual ~Blitter() = default;

	template <typename SetPixelT> void DrawLineGeneric(int x, int y, int x2, int y2, int screen_width, int screen_height, int width, int dash, SetPixelT set_pixel);
};

#endif /* BLITTER_BASE_HPP */
