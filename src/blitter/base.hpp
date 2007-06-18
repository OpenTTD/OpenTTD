/* $Id$ */

#ifndef BLITTER_BASE_HPP
#define BLITTER_BASE_HPP

#include "../spritecache.h"
#include "../spriteloader/spriteloader.hpp"

enum BlitterMode {
	BM_NORMAL,
	BM_COLOUR_REMAP,
	BM_TRANSPARENT,
};

/**
 * How all blitters should look like. Extend this class to make your own.
 */
class Blitter {
public:
	struct BlitterParams {
		const void *sprite;      ///< Pointer to the sprite how ever the encoder stored it
		const byte *remap;       ///< XXX -- Temporary storage for remap array

		int skip_left, skip_top; ///< How much pixels of the source to skip on the left and top (based on zoom of dst)
		int width, height;       ///< The width and height in pixels that needs to be drawn to dst
		int sprite_width;        ///< Real width of the sprite
		int sprite_height;       ///< Real height of the sprite
		int left, top;           ///< The offset in the 'dst' in pixels to start drawing

		void *dst;               ///< Destination buffer
		int pitch;               ///< The pitch of the destination buffer
	};

	typedef void *AllocatorProc(size_t size);

	/**
	 * Get the screen depth this blitter works for.
	 *  This is either: 8, 16, 24 or 32.
	 */
	virtual uint8 GetScreenDepth() = 0;

	/**
	 * Draw an image to the screen, given an amount of params defined above.
	 */
	virtual void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) = 0;

	/**
	 * Draw a colortable to the screen. This is: the color of the screen is read
	 *  and is looked-up in the palette to match a new color, which then is put
	 *  on the screen again.
	 * @param dst the destination pointer (video-buffer).
	 * @param width the width of the buffer.
	 * @param height the height of the buffer.
	 * @param pal the palette to use.
	 */
	virtual void DrawColorMappingRect(void *dst, int width, int height, int pal) = 0;

	/**
	 * Convert a sprite from the loader to our own format.
	 */
	virtual Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator) = 0;

	/**
	 * Move the destination pointer the requested amount x and y, keeping in mind
	 *  any pitch and bpp of the renderer.
	 * @param video The destination pointer (video-buffer) to scroll.
	 * @param x How much you want to scroll to the right.
	 * @param y How much you want to scroll to the bottom.
	 * @return A new destination pointer moved the the requested place.
	 */
	virtual void *MoveTo(const void *video, int x, int y) = 0;

	/**
	 * Draw a pixel with a given color on the video-buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param x The x position within video-buffer.
	 * @param y The y position within video-buffer.
	 * @param color A 8bpp mapping color.
	 */
	virtual void SetPixel(void *video, int x, int y, uint8 color) = 0;

	/**
	 * Draw a pixel with a given color on the video-buffer if there is currently a black pixel.
	 * @param video The destination pointer (video-buffer).
	 * @param x The x position within video-buffer.
	 * @param y The y position within video-buffer.
	 * @param color A 8bpp mapping color.
	 */
	virtual void SetPixelIfEmpty(void *video, int x, int y, uint8 color) = 0;

	/**
	 * Make a single horizontal line in a single color on the video-buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param width The lenght of the line.
	 * @param color A 8bpp mapping color.
	 */
	virtual void DrawRect(void *video, int width, int height, uint8 color) = 0;

	/**
	 * Copy from a buffer to the screen.
	 * @param video The destionation pointer (video-buffer).
	 * @param src The buffer from which the data will be read.
	 * @param width The width of the buffer.
	 * @param height The height of the buffer.
	 * @param src_pitch The pitch (byte per line) of the source buffer.
	 */
	virtual void CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch) = 0;

	/**
	 * Copy from the screen to a buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param dst The buffer in which the data will be stored.
	 * @param width The width of the buffer.
	 * @param height The height of the buffer.
	 * @param dst_pitch The pitch (byte per line) of the destination buffer.
	 */
	virtual void CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) = 0;

	/**
	 * Move the videobuffer some places (via memmove).
	 * @param video_dst The destination pointer (video-buffer).
	 * @param video_src The source pointer (video-buffer).
	 * @param width The width of the buffer to move.
	 * @param height The height of the buffer to move.
	 */
	virtual void MoveBuffer(void *video_dst, const void *video_src, int width, int height) = 0;

	/**
	 * Calculate how much memory there is needed for an image of this size in the video-buffer.
	 * @param width The width of the buffer-to-be.
	 * @param height The height of the buffer-to-be.
	 * @return The size needed for the buffer.
	 */
	virtual int BufferSize(int width, int height) = 0;

	virtual ~Blitter() { }
};

#endif /* BLITTER_BASE_HPP */
