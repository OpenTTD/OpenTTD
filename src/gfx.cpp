/* $Id$ */

/** @file gfx.cpp Handling of drawing text and other gfx related stuff. */

#include "stdafx.h"
#include "openttd.h"
#include "gfx_func.h"
#include "variables.h"
#include "spritecache.h"
#include "fontcache.h"
#include "genworld.h"
#include "zoom_func.h"
#include "blitter/factory.hpp"
#include "video/video_driver.hpp"
#include "strings_func.h"
#include "settings_type.h"
#include "core/alloc_func.hpp"
#include "core/sort_func.hpp"
#include "landscape_type.h"
#include "network/network_func.h"

#include "table/palettes.h"
#include "table/sprites.h"
#include "table/control_codes.h"

byte _dirkeys;        ///< 1 = left, 2 = up, 4 = right, 8 = down
bool _fullscreen;
CursorVars _cursor;
bool _ctrl_pressed;   ///< Is Ctrl pressed?
bool _shift_pressed;  ///< Is Shift pressed?
byte _fast_forward;
bool _left_button_down;     ///< Is left mouse button pressed?
bool _left_button_clicked;  ///< Is left mouse button clicked?
bool _right_button_down;    ///< Is right mouse button pressed?
bool _right_button_clicked; ///< Is right mouse button clicked?
DrawPixelInfo _screen;
bool _screen_disable_anim = false;   ///< Disable palette animation (important for 32bpp-anim blitter during giant screenshot)
bool _exit_game;
GameMode _game_mode;
SwitchMode _switch_mode;  ///< The next mainloop command.
int8 _pause_game;
int _pal_first_dirty;
int _pal_count_dirty;

Colour _cur_palette[256];
byte _stringwidth_table[FS_END][224]; ///< Cache containing width of often used characters. @see GetCharacterWidth()
DrawPixelInfo *_cur_dpi;
byte _colour_gradient[COLOUR_END][8];

static void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub = NULL);
static int ReallyDoDrawString(const char *string, int x, int y, TextColour colour, bool parse_string_also_when_clipped = false);

FontSize _cur_fontsize;
static FontSize _last_fontsize;
static ReusableBuffer<uint8> _cursor_backup;

/**
 * The rect for repaint.
 *
 * This rectangle defines the area which should be repaint by the video driver.
 *
 * @ingroup dirty
 */
static Rect _invalid_rect;
static const byte *_colour_remap_ptr;
static byte _string_colourremap[3];

enum {
	DIRTY_BLOCK_HEIGHT   = 8,
	DIRTY_BLOCK_WIDTH    = 64,
};
static uint _dirty_bytes_per_line = 0;
static byte *_dirty_blocks = NULL;

void GfxScroll(int left, int top, int width, int height, int xo, int yo)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();

	if (xo == 0 && yo == 0) return;

	if (_cursor.visible) UndrawMouseCursor();

#ifdef ENABLE_NETWORK
	NetworkUndrawChatMessage();
#endif /* ENABLE_NETWORK */

	blitter->ScrollBuffer(_screen.dst_ptr, left, top, width, height, xo, yo);
	/* This part of the screen is now dirty. */
	_video_driver->MakeDirty(left, top, width, height);
}


/**
 * Applies a certain FillRectMode-operation to a rectangle [left, right] x [top, bottom] on the screen.
 *
 * @pre dpi->zoom == ZOOM_LVL_NORMAL, right >= left, bottom >= top
 * @param left Minimum X (inclusive)
 * @param top Minimum Y (inclusive)
 * @param right Maximum X (inclusive)
 * @param bottom Maximum Y (inclusive)
 * @param colour A 8 bit palette index (FILLRECT_OPAQUE and FILLRECT_CHECKER) or a recolour spritenumber (FILLRECT_RECOLOUR)
 * @param mode
 *         FILLRECT_OPAQUE:   Fill the rectangle with the specified colour
 *         FILLRECT_CHECKER:  Like FILLRECT_OPAQUE, but only draw every second pixel (used to grey out things)
 *         FILLRECT_RECOLOUR:  Apply a recolour sprite to every pixel in the rectangle currently on screen
 */
void GfxFillRect(int left, int top, int right, int bottom, int colour, FillRectMode mode)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	const DrawPixelInfo *dpi = _cur_dpi;
	void *dst;
	const int otop = top;
	const int oleft = left;

	if (dpi->zoom != ZOOM_LVL_NORMAL) return;
	if (left > right || top > bottom) return;
	if (right < dpi->left || left >= dpi->left + dpi->width) return;
	if (bottom < dpi->top || top >= dpi->top + dpi->height) return;

	if ( (left -= dpi->left) < 0) left = 0;
	right = right - dpi->left + 1;
	if (right > dpi->width) right = dpi->width;
	right -= left;
	assert(right > 0);

	if ( (top -= dpi->top) < 0) top = 0;
	bottom = bottom - dpi->top + 1;
	if (bottom > dpi->height) bottom = dpi->height;
	bottom -= top;
	assert(bottom > 0);

	dst = blitter->MoveTo(dpi->dst_ptr, left, top);

	switch (mode) {
		default: // FILLRECT_OPAQUE
			blitter->DrawRect(dst, right, bottom, (uint8)colour);
			break;

		case FILLRECT_RECOLOUR:
			blitter->DrawColourMappingRect(dst, right, bottom, GB(colour, 0, PALETTE_WIDTH));
			break;

		case FILLRECT_CHECKER: {
			byte bo = (oleft - left + dpi->left + otop - top + dpi->top) & 1;
			do {
				for (int i = (bo ^= 1); i < right; i += 2) blitter->SetPixel(dst, i, 0, (uint8)colour);
				dst = blitter->MoveTo(dst, 0, 1);
			} while (--bottom > 0);
			break;
		}
	}
}

void GfxDrawLine(int x, int y, int x2, int y2, int colour)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	DrawPixelInfo *dpi = _cur_dpi;

	x -= dpi->left;
	x2 -= dpi->left;
	y -= dpi->top;
	y2 -= dpi->top;

	/* Check clipping */
	if (x < 0 && x2 < 0) return;
	if (y < 0 && y2 < 0) return;
	if (x > dpi->width  && x2 > dpi->width)  return;
	if (y > dpi->height && y2 > dpi->height) return;

	blitter->DrawLine(dpi->dst_ptr, x, y, x2, y2, dpi->width, dpi->height, colour);
}

void GfxDrawLineUnscaled(int x, int y, int x2, int y2, int colour)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	DrawPixelInfo *dpi = _cur_dpi;

	x -= dpi->left;
	x2 -= dpi->left;
	y -= dpi->top;
	y2 -= dpi->top;

	/* Check clipping */
	if (x < 0 && x2 < 0) return;
	if (y < 0 && y2 < 0) return;
	if (x > dpi->width  && x2 > dpi->width)  return;
	if (y > dpi->height && y2 > dpi->height) return;

	blitter->DrawLine(dpi->dst_ptr, UnScaleByZoom(x, dpi->zoom), UnScaleByZoom(y, dpi->zoom),
			UnScaleByZoom(x2, dpi->zoom), UnScaleByZoom(y2, dpi->zoom),
			UnScaleByZoom(dpi->width, dpi->zoom), UnScaleByZoom(dpi->height, dpi->zoom), colour);
}

/**
 * Draws the projection of a parallelepiped.
 * This can be used to draw boxes in world coordinates.
 *
 * @param x   Screen X-coordinate of top front corner.
 * @param y   Screen Y-coordinate of top front corner.
 * @param dx1 Screen X-length of first edge.
 * @param dy1 Screen Y-length of first edge.
 * @param dx2 Screen X-length of second edge.
 * @param dy2 Screen Y-length of second edge.
 * @param dx3 Screen X-length of third edge.
 * @param dy3 Screen Y-length of third edge.
 */
void DrawBox(int x, int y, int dx1, int dy1, int dx2, int dy2, int dx3, int dy3)
{
	/*           ....
	 *         ..    ....
	 *       ..          ....
	 *     ..                ^
	 *   <--__(dx1,dy1)    /(dx2,dy2)
	 *   :    --__       /   :
	 *   :        --__ /     :
	 *   :            *(x,y) :
	 *   :            |      :
	 *   :            |     ..
	 *    ....        |(dx3,dy3)
	 *        ....    | ..
	 *            ....V.
	 */

	static const byte colour = 255;

	GfxDrawLineUnscaled(x, y, x + dx1, y + dy1, colour);
	GfxDrawLineUnscaled(x, y, x + dx2, y + dy2, colour);
	GfxDrawLineUnscaled(x, y, x + dx3, y + dy3, colour);

	GfxDrawLineUnscaled(x + dx1, y + dy1, x + dx1 + dx2, y + dy1 + dy2, colour);
	GfxDrawLineUnscaled(x + dx1, y + dy1, x + dx1 + dx3, y + dy1 + dy3, colour);
	GfxDrawLineUnscaled(x + dx2, y + dy2, x + dx2 + dx1, y + dy2 + dy1, colour);
	GfxDrawLineUnscaled(x + dx2, y + dy2, x + dx2 + dx3, y + dy2 + dy3, colour);
	GfxDrawLineUnscaled(x + dx3, y + dy3, x + dx3 + dx1, y + dy3 + dy1, colour);
	GfxDrawLineUnscaled(x + dx3, y + dy3, x + dx3 + dx2, y + dy3 + dy2, colour);
}

/**
 * Set the colour remap to be for the given colour.
 * @param colour the new colour of the remap.
 */
static void SetColourRemap(TextColour colour)
{
	if (colour == TC_INVALID) return;

	if (colour & IS_PALETTE_COLOUR) {
		_string_colourremap[1] = colour & ~IS_PALETTE_COLOUR;
		_string_colourremap[2] = (_use_palette == PAL_DOS) ? 1 : 215;
	} else {
		_string_colourremap[1] = _string_colourmap[_use_palette][colour].text;
		_string_colourremap[2] = _string_colourmap[_use_palette][colour].shadow;
	}
	_colour_remap_ptr = _string_colourremap;
}

#if !defined(WITH_ICU)
static void HandleBiDiAndArabicShapes(char *text, const char *lastof) {}
#else
#include <unicode/ubidi.h>
#include <unicode/ushape.h>

/**
 * Function to be able to handle right-to-left text and Arabic chars properly.
 *
 * First: right-to-left (RTL) is stored 'logically' in almost all applications
 *        and so do we. This means that their text is stored from right to the
 *        left in memory and any non-RTL text (like numbers or English) are
 *        then stored from left-to-right. When we want to actually draw the
 *        text we need to reverse the RTL text in memory, which is what
 *        happens in ubidi_writeReordered.
 * Second: Arabic characters "differ" based on their context. To draw the
 *        correct variant we pass it through u_shapeArabic. This function can
 *        add or remove some characters. This is the reason for the lastof
 *        so we know till where we can fill the output.
 *
 * Sadly enough these functions work with a custom character format, UChar,
 * which isn't the same size as WChar. Because of that we need to transform
 * our text first to UChars and then back to something we can use.
 *
 * To be able to truncate strings properly you must truncate before passing to
 * this function. This way the logical begin of the string remains and the end
 * gets chopped of instead of the other way around.
 *
 * The reshaping of Arabic characters might increase or decrease the width of
 * the characters/string. So it might still overflow after truncation, though
 * the chance is fairly slim as most characters get shorter instead of longer.
 * @param buffer the buffer to read from/to
 * @param lastof the end of the buffer
 */
static void HandleBiDiAndArabicShapes(char *buffer, const char *lastof)
{
	UChar input_output[DRAW_STRING_BUFFER];
	UChar intermediate[DRAW_STRING_BUFFER];

	char *t = buffer;
	size_t length = 0;
	while (*t != '\0' && length < lengthof(input_output) - 1) {
		WChar tmp;
		t += Utf8Decode(&tmp, t);
		input_output[length++] = tmp;
	}
	input_output[length] = 0;

	UErrorCode err = U_ZERO_ERROR;
	UBiDi *para = ubidi_openSized((int32_t)length, 0, &err);
	if (para == NULL) return;

	ubidi_setPara(para, input_output, (int32_t)length, _dynlang.text_dir == TD_RTL ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR, NULL, &err);
	ubidi_writeReordered(para, intermediate, (int32_t)length, 0, &err);
	length = u_shapeArabic(intermediate, (int32_t)length, input_output, lengthof(input_output), U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_LETTERS_SHAPE, &err);
	ubidi_close(para);

	if (U_FAILURE(err)) return;

	t = buffer;
	for (size_t i = 0; i < length && t < (lastof - 4); i++) {
		t += Utf8Encode(t, input_output[i]);
	}
	*t = '\0';
}
#endif /* WITH_ICU */


/** Truncate a given string to a maximum width if neccessary.
 * If the string is truncated, add three dots ('...') to show this.
 * @param *str string that is checked and possibly truncated
 * @param maxw maximum width in pixels of the string
 * @return new width of (truncated) string */
static int TruncateString(char *str, int maxw)
{
	int w = 0;
	FontSize size = _cur_fontsize;
	int ddd, ddd_w;

	WChar c;
	char *ddd_pos;

	ddd_w = ddd = GetCharacterWidth(size, '.') * 3;

	for (ddd_pos = str; (c = Utf8Consume((const char **)&str)) != '\0'; ) {
		if (IsPrintable(c)) {
			w += GetCharacterWidth(size, c);

			if (w >= maxw) {
				/* string got too big... insert dotdotdot, but make sure we do not
				 * print anything beyond the string termination character. */
				for (int i = 0; *ddd_pos != '\0' && i < 3; i++, ddd_pos++) *ddd_pos = '.';
				*ddd_pos = '\0';
				return ddd_w;
			}
		} else {
			if (c == SCC_SETX) {
				w = *str;
				str++;
			} else if (c == SCC_SETXY) {
				w = *str;
				str += 2;
			} else if (c == SCC_TINYFONT) {
				size = FS_SMALL;
				ddd = GetCharacterWidth(size, '.') * 3;
			} else if (c == SCC_BIGFONT) {
				size = FS_LARGE;
				ddd = GetCharacterWidth(size, '.') * 3;
			}
		}

		/* Remember the last position where three dots fit. */
		if (w + ddd < maxw) {
			ddd_w = w + ddd;
			ddd_pos = str;
		}
	}

	return w;
}

/**
 * Write string to output buffer, truncating it to specified maximal width in pixels if it is too long.
 *
 * @param src   String to truncate
 * @param dest  Start of character output buffer where truncated string is stored
 * @param maxw  Maximal allowed length of the string in pixels
 * @param last  Address of last character in output buffer
 *
 * @return Actual width of the (possibly) truncated string in pixels
 */
static inline int TruncateStringID(StringID src, char *dest, int maxw, const char *last)
{
	GetString(dest, src, last);
	return TruncateString(dest, maxw);
}

/**
 * Draw string starting at position (x,y).
 *
 * @param x      X position to start drawing
 * @param y      Y position to start drawing
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 *
 * @return Horizontal coordinate after drawing the string
 */
int DrawString(int x, int y, StringID str, TextColour colour)
{
	char buffer[DRAW_STRING_BUFFER];

	GetString(buffer, str, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));
	return ReallyDoDrawString(buffer, x, y, colour);
}

/**
 * Draw string, possibly truncated to make it fit in its allocated space
 *
 * @param x      X position to start drawing
 * @param y      Y position to start drawing
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param maxw   Maximal width of the string
 *
 * @return Horizontal coordinate after drawing the (possibly truncated) string
 */
int DrawStringTruncated(int x, int y, StringID str, TextColour colour, uint maxw)
{
	char buffer[DRAW_STRING_BUFFER];
	TruncateStringID(str, buffer, maxw, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));
	return ReallyDoDrawString(buffer, x, y, colour);
}

/**
 * Draw string right-aligned.
 *
 * @param x      Right-most x position of the string
 * @param y      Y position of the string
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 *
 * @return Width of drawn string in pixels
 */
int DrawStringRightAligned(int x, int y, StringID str, TextColour colour)
{
	char buffer[DRAW_STRING_BUFFER];
	int w;

	GetString(buffer, str, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));

	w = GetStringBoundingBox(buffer).width;
	ReallyDoDrawString(buffer, x - w, y, colour);

	return w;
}

/**
 * Draw string right-aligned, possibly truncated to make it fit in its allocated space
 *
 * @param x      Right-most x position to start drawing
 * @param y      Y position to start drawing
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param maxw   Maximal width of the string
 */
void DrawStringRightAlignedTruncated(int x, int y, StringID str, TextColour colour, uint maxw)
{
	char buffer[DRAW_STRING_BUFFER];

	TruncateStringID(str, buffer, maxw, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));
	ReallyDoDrawString(buffer, x - GetStringBoundingBox(buffer).width, y, colour);
}

/**
 * Draw string right-aligned with a line underneath it.
 *
 * @param x      Right-most x position of the string
 * @param y      Y position of the string
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 */
void DrawStringRightAlignedUnderline(int x, int y, StringID str, TextColour colour)
{
	int w = DrawStringRightAligned(x, y, str, colour);
	GfxFillRect(x - w, y + 10, x, y + 10, _string_colourremap[1]);
}

/**
 * Draw string centered.
 *
 * @param x      X position of center of the string
 * @param y      Y position of center of the string
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 *
 * @return Width of the drawn string in pixels
 */
int DrawStringCentered(int x, int y, StringID str, TextColour colour)
{
	char buffer[DRAW_STRING_BUFFER];
	int w;

	GetString(buffer, str, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));

	w = GetStringBoundingBox(buffer).width;
	ReallyDoDrawString(buffer, x - w / 2, y, colour);

	return w;
}

/**
 * Draw string centered, possibly truncated to fit in the assigned space.
 *
 * @param xl     Left-most x position
 * @param xr     Right-most x position
 * @param y      Y position of the string
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 *
 * @return Right-most coordinate of the (possibly truncated) drawn string
 */
int DrawStringCenteredTruncated(int xl, int xr, int y, StringID str, TextColour colour)
{
	char buffer[DRAW_STRING_BUFFER];
	TruncateStringID(str, buffer, xr - xl, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));

	int w = GetStringBoundingBox(buffer).width;
	return ReallyDoDrawString(buffer, (xl + xr - w) / 2, y, colour);
}

/**
 * Draw string centered.
 *
 * @param x      X position of center of the string
 * @param y      Y position of center of the string
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 *
 * @return Width of the drawn string in pixels
 */
int DoDrawStringCentered(int x, int y, const char *str, TextColour colour)
{
	char buffer[DRAW_STRING_BUFFER];
	strecpy(buffer, str, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));

	int w = GetStringBoundingBox(buffer).width;
	ReallyDoDrawString(buffer, x - w / 2, y, colour);
	return w;
}

/**
 * Draw string centered, with additional line underneath it
 *
 * @param x      X position of center of the string
 * @param y      Y position of center of the string
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 */
void DrawStringCenterUnderline(int x, int y, StringID str, TextColour colour)
{
	int w = DrawStringCentered(x, y, str, colour);
	GfxFillRect(x - (w >> 1), y + 10, x - (w >> 1) + w, y + 10, _string_colourremap[1]);
}

/**
 * Draw string centered possibly truncated, with additional line underneath it
 *
 * @param xl     Left x position of the string
 * @param xr     Right x position of the string
 * @param y      Y position of center of the string
 * @param str    String to draw
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 */
void DrawStringCenterUnderlineTruncated(int xl, int xr, int y, StringID str, TextColour colour)
{
	int w = DrawStringCenteredTruncated(xl, xr, y, str, colour);
	GfxFillRect((xl + xr - w) / 2, y + 10, (xl + xr + w) / 2, y + 10, _string_colourremap[1]);
}

/**
 * 'Correct' a string to a maximum length. Longer strings will be cut into
 * additional lines at whitespace characters if possible. The string parameter
 * is modified with terminating characters mid-string which are the
 * placeholders for the newlines.
 * The string WILL be truncated if there was no whitespace for the current
 * line's maximum width.
 *
 * @note To know if the terminating '\0' is the string end or just a
 * newline, the returned 'num' value should be consulted. The num'th '\0',
 * starting with index 0 is the real string end.
 *
 * @param str string to check and correct for length restrictions
 * @param maxw the maximum width the string can have on one line
 * @return return a 32bit wide number consisting of 2 packed values:
 *  0 - 15 the number of lines ADDED to the string
 * 16 - 31 the fontsize in which the length calculation was done at
 */
uint32 FormatStringLinebreaks(char *str, int maxw)
{
	FontSize size = _cur_fontsize;
	int num = 0;

	assert(maxw > 0);

	for (;;) {
		char *last_space = NULL;
		int w = 0;

		for (;;) {
			WChar c = Utf8Consume((const char **)&str);
			/* whitespace is where we will insert the line-break */
			if (IsWhitespace(c)) last_space = str;

			if (IsPrintable(c)) {
				w += GetCharacterWidth(size, c);
				/* string is longer than maximum width so we need to decide what to
				 * do. We can do two things:
				 * 1. If no whitespace was found at all up until now (on this line) then
				 *    we will truncate the string and bail out.
				 * 2. In all other cases force a linebreak at the last seen whitespace */
				if (w > maxw) {
					if (last_space == NULL) {
						*Utf8PrevChar(str) = '\0';
						return num + (size << 16);
					}
					str = last_space;
					break;
				}
			} else {
				switch (c) {
					case '\0': return num + (size << 16); break;
					case SCC_SETX:  str++; break;
					case SCC_SETXY: str += 2; break;
					case SCC_TINYFONT: size = FS_SMALL; break;
					case SCC_BIGFONT:  size = FS_LARGE; break;
					case '\n': goto end_of_inner_loop;
				}
			}
		}
end_of_inner_loop:
		/* String didn't fit on line (or a '\n' was encountered), so 'dummy' terminate
		 * and increase linecount. We use Utf8PrevChar() as also non 1 char long
		 * whitespace seperators are supported */
		num++;
		char *s = Utf8PrevChar(str);
		*s++ = '\0';

		/* In which case (see above) we will shift remainder to left and close the gap */
		if (str - s >= 1) {
			for (; str[-1] != '\0';) *s++ = *str++;
		}
	}
}


/** Calculates height of string (in pixels). Accepts multiline string with '\0' as separators.
 * @param src string to check
 * @param num number of extra lines (output of FormatStringLinebreaks())
 * @note assumes text won't be truncated. FormatStringLinebreaks() is a good way to ensure that.
 * @return height of pixels of string when it is drawn
 */
static int GetMultilineStringHeight(const char *src, int num)
{
	int maxy = 0;
	int y = 0;
	int fh = GetCharacterHeight(_cur_fontsize);

	for (;;) {
		WChar c = Utf8Consume(&src);

		switch (c) {
			case 0:            y += fh; if (--num < 0) return maxy; break;
			case '\n':         y += fh;                             break;
			case SCC_SETX:     src++;                               break;
			case SCC_SETXY:    src++; y = (int)*src++;              break;
			case SCC_TINYFONT: fh = GetCharacterHeight(FS_SMALL);   break;
			case SCC_BIGFONT:  fh = GetCharacterHeight(FS_LARGE);   break;
			default:           maxy = max<int>(maxy, y + fh);       break;
		}
	}
}


/** Calculates height of string (in pixels). The string is changed to a multiline string if needed.
 * @param str string to check
 * @param maxw maximum string width
 * @return height of pixels of string when it is drawn
 */
int GetStringHeight(StringID str, int maxw)
{
	char buffer[DRAW_STRING_BUFFER];

	GetString(buffer, str, lastof(buffer));

	uint32 tmp = FormatStringLinebreaks(buffer, maxw);

	return GetMultilineStringHeight(buffer, GB(tmp, 0, 16));
}


/** Draw a given string with the centre around the given (x,y) coordinates
 * @param x Centre the string around this pixel width
 * @param y Centre the string around this pixel height
 * @param str String to draw
 * @param maxw Maximum width the string can have before it is wrapped */
void DrawStringMultiCenter(int x, int y, StringID str, int maxw)
{
	char buffer[DRAW_STRING_BUFFER];
	uint32 tmp;
	int num, mt;
	const char *src;
	WChar c;

	GetString(buffer, str, lastof(buffer));

	tmp = FormatStringLinebreaks(buffer, maxw);
	num = GB(tmp, 0, 16);

	mt = GetCharacterHeight((FontSize)GB(tmp, 16, 16));

	y -= (mt >> 1) * num;

	src = buffer;

	for (;;) {
		char buf2[DRAW_STRING_BUFFER];
		strecpy(buf2, src, lastof(buf2));
		HandleBiDiAndArabicShapes(buf2, lastof(buf2));
		int w = GetStringBoundingBox(buf2).width;
		ReallyDoDrawString(buf2, x - (w >> 1), y, TC_FROMSTRING, true);
		_cur_fontsize = _last_fontsize;

		for (;;) {
			c = Utf8Consume(&src);
			if (c == 0) {
				y += mt;
				if (--num < 0) {
					_cur_fontsize = FS_NORMAL;
					return;
				}
				break;
			} else if (c == SCC_SETX) {
				src++;
			} else if (c == SCC_SETXY) {
				src += 2;
			}
		}
	}
}


uint DrawStringMultiLine(int x, int y, StringID str, int maxw, int maxh)
{
	char buffer[DRAW_STRING_BUFFER];
	uint32 tmp;
	int num, mt;
	uint total_height;
	const char *src;
	WChar c;

	GetString(buffer, str, lastof(buffer));

	tmp = FormatStringLinebreaks(buffer, maxw);
	num = GB(tmp, 0, 16);

	mt = GetCharacterHeight((FontSize)GB(tmp, 16, 16));
	total_height = (num + 1) * mt;

	if (maxh != -1 && (int)total_height > maxh) {
		/* Check there's room enough for at least one line. */
		if (maxh < mt) return 0;

		num = maxh / mt - 1;
		total_height = (num + 1) * mt;
	}

	src = buffer;

	for (;;) {
		char buf2[DRAW_STRING_BUFFER];
		strecpy(buf2, src, lastof(buf2));
		HandleBiDiAndArabicShapes(buf2, lastof(buf2));
		ReallyDoDrawString(buf2, x, y, TC_FROMSTRING, true);
		_cur_fontsize = _last_fontsize;

		for (;;) {
			c = Utf8Consume(&src);
			if (c == 0) {
				y += mt;
				if (--num < 0) {
					_cur_fontsize = FS_NORMAL;
					return total_height;
				}
				break;
			} else if (c == SCC_SETX) {
				src++;
			} else if (c == SCC_SETXY) {
				src += 2;
			}
		}
	}
}

/** Return the string dimension in pixels. The height and width are returned
 * in a single Dimension value. TINYFONT, BIGFONT modifiers are only
 * supported as the first character of the string. The returned dimensions
 * are therefore a rough estimation correct for all the current strings
 * but not every possible combination
 * @param str string to calculate pixel-width
 * @return string width and height in pixels */
Dimension GetStringBoundingBox(const char *str)
{
	FontSize size = _cur_fontsize;
	Dimension br;
	int max_width;
	WChar c;

	br.width = br.height = max_width = 0;
	for (;;) {
		c = Utf8Consume(&str);
		if (c == 0) break;
		if (IsPrintable(c)) {
			br.width += GetCharacterWidth(size, c);
		} else {
			switch (c) {
				case SCC_SETX: br.width += (byte)*str++; break;
				case SCC_SETXY:
					br.width += (byte)*str++;
					br.height += (byte)*str++;
					break;
				case SCC_TINYFONT: size = FS_SMALL; break;
				case SCC_BIGFONT:  size = FS_LARGE; break;
				case '\n':
					br.height += GetCharacterHeight(size);
					if (br.width > max_width) max_width = br.width;
					br.width = 0;
					break;
			}
		}
	}
	br.height += GetCharacterHeight(size);

	br.width  = max(br.width, max_width);
	return br;
}

/**
 * Draw single character horizontally centered around (x,y)
 * @param c           Character (glyph) to draw
 * @param x           X position to draw character
 * @param y           Y position to draw character
 * @param colour      Colour to use, see DoDrawString() for details
 */
void DrawCharCentered(WChar c, int x, int y, TextColour colour)
{
	SetColourRemap(colour);
	GfxMainBlitter(GetGlyph(FS_NORMAL, c), x - GetCharacterWidth(FS_NORMAL, c) / 2, y, BM_COLOUR_REMAP);
}

/** Draw a string at the given coordinates with the given colour.
 *  While drawing the string, parse it in case some formatting is specified,
 *  like new colour, new size or even positionning.
 * @param string              The string to draw. This is not yet bidi reordered.
 * @param x                   Offset from left side of the screen
 * @param y                   Offset from top side of the screen
 * @param colour              Colour of the string, see _string_colourmap in
 *                            table/palettes.h or docs/ottd-colourtext-palette.png or the enum TextColour in gfx_type.h
 * @param parse_string_also_when_clipped
 *                            By default, always test the available space where to draw the string.
 *                            When in multipline drawing, it would already be done,
 *                            so no need to re-perform the same kind (more or less) of verifications.
 *                            It's not only an optimisation, it's also a way to ensures the string will be parsed
 *                            (as there are certain side effects on global variables, which are important for the next line)
 * @return                    the x-coordinates where the drawing has finished.
 *                            If nothing is drawn, the originally passed x-coordinate is returned
 */
int DoDrawString(const char *string, int x, int y, TextColour colour, bool parse_string_also_when_clipped)
{
	char buffer[DRAW_STRING_BUFFER];
	strecpy(buffer, string, lastof(buffer));
	HandleBiDiAndArabicShapes(buffer, lastof(buffer));

	return ReallyDoDrawString(buffer, x, y, colour, parse_string_also_when_clipped);
}

/** Draw a string at the given coordinates with the given colour.
 *  While drawing the string, parse it in case some formatting is specified,
 *  like new colour, new size or even positionning.
 * @param string              The string to draw. This is already bidi reordered.
 * @param x                   Offset from left side of the screen
 * @param y                   Offset from top side of the screen
 * @param colour              Colour of the string, see _string_colourmap in
 *                            table/palettes.h or docs/ottd-colourtext-palette.png or the enum TextColour in gfx_type.h
 * @param parse_string_also_when_clipped
 *                            By default, always test the available space where to draw the string.
 *                            When in multipline drawing, it would already be done,
 *                            so no need to re-perform the same kind (more or less) of verifications.
 *                            It's not only an optimisation, it's also a way to ensures the string will be parsed
 *                            (as there are certain side effects on global variables, which are important for the next line)
 * @return                    the x-coordinates where the drawing has finished.
 *                            If nothing is drawn, the originally passed x-coordinate is returned
 */
static int ReallyDoDrawString(const char *string, int x, int y, TextColour colour, bool parse_string_also_when_clipped)
{
	DrawPixelInfo *dpi = _cur_dpi;
	FontSize size = _cur_fontsize;
	WChar c;
	int xo = x, yo = y;

	TextColour previous_colour = colour;

	if (!parse_string_also_when_clipped) {
		/* in "mode multiline", the available space have been verified. Not in regular one.
		 * So if the string cannot be drawn, return the original start to say so.*/
		if (x >= dpi->left + dpi->width || y >= dpi->top + dpi->height) return x;

		if (colour != TC_INVALID) { // the invalid colour flag test should not  really occur.  But better be safe
switch_colour:;
			SetColourRemap(colour);
		}
	}

check_bounds:
	if (y + 19 <= dpi->top || dpi->top + dpi->height <= y) {
skip_char:;
		for (;;) {
			c = Utf8Consume(&string);
			if (!IsPrintable(c)) goto skip_cont;
		}
	}

	for (;;) {
		c = Utf8Consume(&string);
skip_cont:;
		if (c == 0) {
			_last_fontsize = size;
			return x;  // Nothing more to draw, get out. And here is the new x position
		}
		if (IsPrintable(c)) {
			if (x >= dpi->left + dpi->width) goto skip_char;
			if (x + 26 >= dpi->left) {
				GfxMainBlitter(GetGlyph(size, c), x, y, BM_COLOUR_REMAP);
			}
			x += GetCharacterWidth(size, c);
		} else if (c == '\n') { // newline = {}
			x = xo;  // We require a new line, so the x coordinate is reset
			y += GetCharacterHeight(size);
			goto check_bounds;
		} else if (c >= SCC_BLUE && c <= SCC_BLACK) { // change colour?
			previous_colour = colour;
			colour = (TextColour)(c - SCC_BLUE);
			goto switch_colour;
		} else if (c == SCC_PREVIOUS_COLOUR) { // revert to the previous colour
			Swap(colour, previous_colour);
			goto switch_colour;
		} else if (c == SCC_SETX) { // {SETX}
			x = xo + (byte)*string++;
		} else if (c == SCC_SETXY) {// {SETXY}
			x = xo + (byte)*string++;
			y = yo + (byte)*string++;
		} else if (c == SCC_TINYFONT) { // {TINYFONT}
			size = FS_SMALL;
		} else if (c == SCC_BIGFONT) { // {BIGFONT}
			size = FS_LARGE;
		} else {
			DEBUG(misc, 0, "[utf8] unknown string command character %d", c);
		}
	}
}

/**
 * Draw the string of the character buffer, starting at position (x,y) with a given maximal width.
 * String is truncated if it is too long.
 *
 * @param str  Character buffer containing the string
 * @param x    Left-most x coordinate to start drawing
 * @param y    Y coordinate to draw the string
 * @param colour Colour to use, see DoDrawString() for details.
 * @param maxw  Maximal width in pixels that may be used for drawing
 *
 * @return Right-most x position after drawing the (possibly truncated) string
 */
int DoDrawStringTruncated(const char *str, int x, int y, TextColour colour, uint maxw)
{
	char buffer[DRAW_STRING_BUFFER];
	strecpy(buffer, str, lastof(buffer));
	TruncateString(buffer, maxw);
	return DoDrawString(buffer, x, y, colour);
}

/**
 * Draw a sprite.
 * @param img  Image number to draw
 * @param pal  Palette to use.
 * @param x    Left coordinate of image
 * @param y    Top coordinate of image
 * @param sub  If available, draw only specified part of the sprite
 */
void DrawSprite(SpriteID img, SpriteID pal, int x, int y, const SubSprite *sub)
{
	if (HasBit(img, PALETTE_MODIFIER_TRANSPARENT)) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitter(GetSprite(GB(img, 0, SPRITE_WIDTH), ST_NORMAL), x, y, BM_TRANSPARENT, sub);
	} else if (pal != PAL_NONE) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitter(GetSprite(GB(img, 0, SPRITE_WIDTH), ST_NORMAL), x, y, BM_COLOUR_REMAP, sub);
	} else {
		GfxMainBlitter(GetSprite(GB(img, 0, SPRITE_WIDTH), ST_NORMAL), x, y, BM_NORMAL, sub);
	}
}

static void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub)
{
	const DrawPixelInfo *dpi = _cur_dpi;
	Blitter::BlitterParams bp;

	/* Amount of pixels to clip from the source sprite */
	int clip_left   = (sub != NULL ? max(0,                   -sprite->x_offs + sub->left       ) : 0);
	int clip_top    = (sub != NULL ? max(0,                   -sprite->y_offs + sub->top        ) : 0);
	int clip_right  = (sub != NULL ? max(0, sprite->width  - (-sprite->x_offs + sub->right  + 1)) : 0);
	int clip_bottom = (sub != NULL ? max(0, sprite->height - (-sprite->y_offs + sub->bottom + 1)) : 0);

	if (clip_left + clip_right >= sprite->width) return;
	if (clip_top + clip_bottom >= sprite->height) return;

	/* Move to the correct offset */
	x += sprite->x_offs;
	y += sprite->y_offs;

	/* Copy the main data directly from the sprite */
	bp.sprite = sprite->data;
	bp.sprite_width = sprite->width;
	bp.sprite_height = sprite->height;
	bp.width = UnScaleByZoom(sprite->width - clip_left - clip_right, dpi->zoom);
	bp.height = UnScaleByZoom(sprite->height - clip_top - clip_bottom, dpi->zoom);
	bp.top = 0;
	bp.left = 0;
	bp.skip_left = UnScaleByZoomLower(clip_left, dpi->zoom);
	bp.skip_top = UnScaleByZoomLower(clip_top, dpi->zoom);

	x += ScaleByZoom(bp.skip_left, dpi->zoom);
	y += ScaleByZoom(bp.skip_top, dpi->zoom);

	bp.dst = dpi->dst_ptr;
	bp.pitch = dpi->pitch;
	bp.remap = _colour_remap_ptr;

	assert(sprite->width > 0);
	assert(sprite->height > 0);

	if (bp.width <= 0) return;
	if (bp.height <= 0) return;

	y -= dpi->top;
	/* Check for top overflow */
	if (y < 0) {
		bp.height -= -UnScaleByZoom(y, dpi->zoom);
		if (bp.height <= 0) return;
		bp.skip_top += -UnScaleByZoom(y, dpi->zoom);
		y = 0;
	} else {
		bp.top = UnScaleByZoom(y, dpi->zoom);
	}

	/* Check for bottom overflow */
	y += ScaleByZoom(bp.height, dpi->zoom) - dpi->height;
	if (y > 0) {
		bp.height -= UnScaleByZoom(y, dpi->zoom);
		if (bp.height <= 0) return;
	}

	x -= dpi->left;
	/* Check for left overflow */
	if (x < 0) {
		bp.width -= -UnScaleByZoom(x, dpi->zoom);
		if (bp.width <= 0) return;
		bp.skip_left += -UnScaleByZoom(x, dpi->zoom);
		x = 0;
	} else {
		bp.left = UnScaleByZoom(x, dpi->zoom);
	}

	/* Check for right overflow */
	x += ScaleByZoom(bp.width, dpi->zoom) - dpi->width;
	if (x > 0) {
		bp.width -= UnScaleByZoom(x, dpi->zoom);
		if (bp.width <= 0) return;
	}

	assert(bp.skip_left + bp.width <= UnScaleByZoom(sprite->width, dpi->zoom));
	assert(bp.skip_top + bp.height <= UnScaleByZoom(sprite->height, dpi->zoom));

	BlitterFactoryBase::GetCurrentBlitter()->Draw(&bp, mode, dpi->zoom);
}

void DoPaletteAnimations();

void GfxInitPalettes()
{
	memcpy(_cur_palette, _palettes[_use_palette], sizeof(_cur_palette));

	DoPaletteAnimations();
	_pal_first_dirty = 0;
	_pal_count_dirty = 256;
}

#define EXTR(p, q) (((uint16)(_palette_animation_counter * (p)) * (q)) >> 16)
#define EXTR2(p, q) (((uint16)(~_palette_animation_counter * (p)) * (q)) >> 16)

void DoPaletteAnimations()
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	const Colour *s;
	const ExtraPaletteValues *ev = &_extra_palette_values;
	/* Amount of colours to be rotated.
	 * A few more for the DOS palette, because the water colours are
	 * 245-254 for DOS and 217-226 for Windows.  */
	const int colour_rotation_amount = (_use_palette == PAL_DOS) ? PALETTE_ANIM_SIZE_DOS : PALETTE_ANIM_SIZE_WIN;
	Colour old_val[PALETTE_ANIM_SIZE_DOS];
	const int oldval_size = colour_rotation_amount * sizeof(*old_val);
	const uint old_tc = _palette_animation_counter;
	uint i;
	uint j;

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		_palette_animation_counter = 0;
	}

	Colour *palette_pos = &_cur_palette[PALETTE_ANIM_SIZE_START];  // Points to where animations are taking place on the palette
	/* Makes a copy of the current anmation palette in old_val,
	 * so the work on the current palette could be compared, see if there has been any changes */
	memcpy(old_val, palette_pos, oldval_size);

	/* Dark blue water */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->dark_water_TOY : ev->dark_water;
	j = EXTR(320, 5);
	for (i = 0; i != 5; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == 5) j = 0;
	}

	/* Glittery water */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->glitter_water_TOY : ev->glitter_water;
	j = EXTR(128, 15);
	for (i = 0; i != 5; i++) {
		*palette_pos++ = s[j];
		j += 3;
		if (j >= 15) j -= 15;
	}

	/* Fizzy Drink bubbles animation */
	s = ev->fizzy_drink;
	j = EXTR2(512, 5);
	for (i = 0; i != 5; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == 5) j = 0;
	}

	/* Oil refinery fire animation */
	s = ev->oil_ref;
	j = EXTR2(512, 7);
	for (i = 0; i != 7; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == 7) j = 0;
	}

	/* Radio tower blinking */
	{
		byte i = (_palette_animation_counter >> 1) & 0x7F;
		byte v;

		(v = 255, i < 0x3f) ||
		(v = 128, i < 0x4A || i >= 0x75) ||
		(v = 20);
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;

		i ^= 0x40;
		(v = 255, i < 0x3f) ||
		(v = 128, i < 0x4A || i >= 0x75) ||
		(v = 20);
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;
	}

	/* Handle lighthouse and stadium animation */
	s = ev->lighthouse;
	j = EXTR(256, 4);
	for (i = 0; i != 4; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == 4) j = 0;
	}

	/* Animate water for old DOS graphics */
	if (_use_palette == PAL_DOS) {
		/* Dark blue water DOS */
		s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->dark_water_TOY : ev->dark_water;
		j = EXTR(320, 5);
		for (i = 0; i != 5; i++) {
			*palette_pos++ = s[j];
			j++;
			if (j == 5) j = 0;
		}

		/* Glittery water DOS */
		s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->glitter_water_TOY : ev->glitter_water;
		j = EXTR(128, 15);
		for (i = 0; i != 5; i++) {
			*palette_pos++ = s[j];
			j += 3;
			if (j >= 15) j -= 15;
		}
	}

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		_palette_animation_counter = old_tc;
	} else {
		if (memcmp(old_val, &_cur_palette[PALETTE_ANIM_SIZE_START], oldval_size) != 0) {
			/* Did we changed anything on the palette? Seems so.  Mark it as dirty */
			_pal_first_dirty = PALETTE_ANIM_SIZE_START;
			_pal_count_dirty = colour_rotation_amount;
		}
	}
}


/** Initialize _stringwidth_table cache */
void LoadStringWidthTable()
{
	uint i;

	/* Normal font */
	for (i = 0; i != 224; i++) {
		_stringwidth_table[FS_NORMAL][i] = GetGlyphWidth(FS_NORMAL, i + 32);
	}

	/* Small font */
	for (i = 0; i != 224; i++) {
		_stringwidth_table[FS_SMALL][i] = GetGlyphWidth(FS_SMALL, i + 32);
	}

	/* Large font */
	for (i = 0; i != 224; i++) {
		_stringwidth_table[FS_LARGE][i] = GetGlyphWidth(FS_LARGE, i + 32);
	}
}

/**
 * Return width of character glyph.
 * @param size  Font of the character
 * @param key   Character code glyph
 * @return Width of the character glyph
 */
byte GetCharacterWidth(FontSize size, WChar key)
{
	/* Use _stringwidth_table cache if possible */
	if (key >= 32 && key < 256) return _stringwidth_table[size][key - 32];

	return GetGlyphWidth(size, key);
}


void ScreenSizeChanged()
{
	_dirty_bytes_per_line = (_screen.width + DIRTY_BLOCK_WIDTH - 1) / DIRTY_BLOCK_WIDTH;
	_dirty_blocks = ReallocT<byte>(_dirty_blocks, _dirty_bytes_per_line * ((_screen.height + DIRTY_BLOCK_HEIGHT - 1) / DIRTY_BLOCK_HEIGHT));

	/* check the dirty rect */
	if (_invalid_rect.right >= _screen.width) _invalid_rect.right = _screen.width;
	if (_invalid_rect.bottom >= _screen.height) _invalid_rect.bottom = _screen.height;

	/* screen size changed and the old bitmap is invalid now, so we don't want to undraw it */
	_cursor.visible = false;
}

void UndrawMouseCursor()
{
	/* Don't undraw the mouse cursor if the screen is not ready */
	if (_screen.dst_ptr == NULL) return;

	if (_cursor.visible) {
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
		_cursor.visible = false;
		blitter->CopyFromBuffer(blitter->MoveTo(_screen.dst_ptr, _cursor.draw_pos.x, _cursor.draw_pos.y), _cursor_backup.GetBuffer(), _cursor.draw_size.x, _cursor.draw_size.y);
		_video_driver->MakeDirty(_cursor.draw_pos.x, _cursor.draw_pos.y, _cursor.draw_size.x, _cursor.draw_size.y);
	}
}

void DrawMouseCursor()
{
#if defined(WINCE)
	/* Don't ever draw the mouse for WinCE, as we work with a stylus */
	return;
#endif

	/* Don't draw the mouse cursor if the screen is not ready */
	if (_screen.dst_ptr == NULL) return;

	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	int x;
	int y;
	int w;
	int h;

	/* Redraw mouse cursor but only when it's inside the window */
	if (!_cursor.in_window) return;

	/* Don't draw the mouse cursor if it's already drawn */
	if (_cursor.visible) {
		if (!_cursor.dirty) return;
		UndrawMouseCursor();
	}

	w = _cursor.size.x;
	x = _cursor.pos.x + _cursor.offs.x + _cursor.short_vehicle_offset;
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (w > _screen.width - x) w = _screen.width - x;
	if (w <= 0) return;
	_cursor.draw_pos.x = x;
	_cursor.draw_size.x = w;

	h = _cursor.size.y;
	y = _cursor.pos.y + _cursor.offs.y;
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (h > _screen.height - y) h = _screen.height - y;
	if (h <= 0) return;
	_cursor.draw_pos.y = y;
	_cursor.draw_size.y = h;

	uint8 *buffer = _cursor_backup.Allocate(blitter->BufferSize(w, h));

	/* Make backup of stuff below cursor */
	blitter->CopyToBuffer(blitter->MoveTo(_screen.dst_ptr, _cursor.draw_pos.x, _cursor.draw_pos.y), buffer, _cursor.draw_size.x, _cursor.draw_size.y);

	/* Draw cursor on screen */
	_cur_dpi = &_screen;
	DrawSprite(_cursor.sprite, _cursor.pal, _cursor.pos.x + _cursor.short_vehicle_offset, _cursor.pos.y);

	_video_driver->MakeDirty(_cursor.draw_pos.x, _cursor.draw_pos.y, _cursor.draw_size.x, _cursor.draw_size.y);

	_cursor.visible = true;
	_cursor.dirty = false;
}

void RedrawScreenRect(int left, int top, int right, int bottom)
{
	assert(right <= _screen.width && bottom <= _screen.height);
	if (_cursor.visible) {
		if (right > _cursor.draw_pos.x &&
				left < _cursor.draw_pos.x + _cursor.draw_size.x &&
				bottom > _cursor.draw_pos.y &&
				top < _cursor.draw_pos.y + _cursor.draw_size.y) {
			UndrawMouseCursor();
		}
	}

#ifdef ENABLE_NETWORK
	NetworkUndrawChatMessage();
#endif /* ENABLE_NETWORK */

	DrawOverlappedWindowForAll(left, top, right, bottom);

	_video_driver->MakeDirty(left, top, right - left, bottom - top);
}

/*!
 * Repaints the rectangle blocks which are marked as 'dirty'.
 *
 * @see SetDirtyBlocks
 */
void DrawDirtyBlocks()
{
	byte *b = _dirty_blocks;
	const int w = Align(_screen.width,  DIRTY_BLOCK_WIDTH);
	const int h = Align(_screen.height, DIRTY_BLOCK_HEIGHT);
	int x;
	int y;

	if (IsGeneratingWorld() && !IsGeneratingWorldReadyForPaint()) return;

	y = 0;
	do {
		x = 0;
		do {
			if (*b != 0) {
				int left;
				int top;
				int right = x + DIRTY_BLOCK_WIDTH;
				int bottom = y;
				byte *p = b;
				int h2;

				/* First try coalescing downwards */
				do {
					*p = 0;
					p += _dirty_bytes_per_line;
					bottom += DIRTY_BLOCK_HEIGHT;
				} while (bottom != h && *p != 0);

				/* Try coalescing to the right too. */
				h2 = (bottom - y) / DIRTY_BLOCK_HEIGHT;
				assert(h2 > 0);
				p = b;

				while (right != w) {
					byte *p2 = ++p;
					int h = h2;
					/* Check if a full line of dirty flags is set. */
					do {
						if (!*p2) goto no_more_coalesc;
						p2 += _dirty_bytes_per_line;
					} while (--h != 0);

					/* Wohoo, can combine it one step to the right!
					 * Do that, and clear the bits. */
					right += DIRTY_BLOCK_WIDTH;

					h = h2;
					p2 = p;
					do {
						*p2 = 0;
						p2 += _dirty_bytes_per_line;
					} while (--h != 0);
				}
				no_more_coalesc:

				left = x;
				top = y;

				if (left   < _invalid_rect.left  ) left   = _invalid_rect.left;
				if (top    < _invalid_rect.top   ) top    = _invalid_rect.top;
				if (right  > _invalid_rect.right ) right  = _invalid_rect.right;
				if (bottom > _invalid_rect.bottom) bottom = _invalid_rect.bottom;

				if (left < right && top < bottom) {
					RedrawScreenRect(left, top, right, bottom);
				}

			}
		} while (b++, (x += DIRTY_BLOCK_WIDTH) != w);
	} while (b += -(w / DIRTY_BLOCK_WIDTH) + _dirty_bytes_per_line, (y += DIRTY_BLOCK_HEIGHT) != h);

	_invalid_rect.left = w;
	_invalid_rect.top = h;
	_invalid_rect.right = 0;
	_invalid_rect.bottom = 0;

	/* If we are generating a world, and waiting for a paint run, mark it here
	 *  as done painting, so we can continue generating. */
	if (IsGeneratingWorld() && IsGeneratingWorldReadyForPaint()) {
		SetGeneratingWorldPaintStatus(false);
	}
}

/*!
 * This function extends the internal _invalid_rect rectangle as it
 * now contains the rectangle defined by the given parameters. Note
 * the point (0,0) is top left.
 *
 * @param left The left edge of the rectangle
 * @param top The top edge of the rectangle
 * @param right The right edge of the rectangle
 * @param bottom The bottm edge of the rectangle
 * @see DrawDirtyBlocks
 *
 * @todo The name of the function should be called like @c AddDirtyBlock as
 *       it neither set a dirty rect nor add several dirty rects although
 *       the function name is in plural. (Progman)
 */
void SetDirtyBlocks(int left, int top, int right, int bottom)
{
	byte *b;
	int width;
	int height;

	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > _screen.width) right = _screen.width;
	if (bottom > _screen.height) bottom = _screen.height;

	if (left >= right || top >= bottom) return;

	if (left   < _invalid_rect.left  ) _invalid_rect.left   = left;
	if (top    < _invalid_rect.top   ) _invalid_rect.top    = top;
	if (right  > _invalid_rect.right ) _invalid_rect.right  = right;
	if (bottom > _invalid_rect.bottom) _invalid_rect.bottom = bottom;

	left /= DIRTY_BLOCK_WIDTH;
	top  /= DIRTY_BLOCK_HEIGHT;

	b = _dirty_blocks + top * _dirty_bytes_per_line + left;

	width  = ((right  - 1) / DIRTY_BLOCK_WIDTH)  - left + 1;
	height = ((bottom - 1) / DIRTY_BLOCK_HEIGHT) - top  + 1;

	assert(width > 0 && height > 0);

	do {
		int i = width;

		do b[--i] = 0xFF; while (i);

		b += _dirty_bytes_per_line;
	} while (--height != 0);
}

/*!
 * This function mark the whole screen as dirty. This results in repainting
 * the whole screen. Use this with care as this function will break the
 * idea about marking only parts of the screen as 'dirty'.
 */
void MarkWholeScreenDirty()
{
	SetDirtyBlocks(0, 0, _screen.width, _screen.height);
}

/** Set up a clipping area for only drawing into a certain area. To do this,
 * Fill a DrawPixelInfo object with the supplied relative rectangle, backup
 * the original (calling) _cur_dpi and assign the just returned DrawPixelInfo
 * _cur_dpi. When you are done, give restore _cur_dpi's original value
 * @param *n the DrawPixelInfo that will be the clipping rectangle box allowed
 * for drawing
 * @param left,top,width,height the relative coordinates of the clipping
 * rectangle relative to the current _cur_dpi. This will most likely be the
 * offset from the calling window coordinates
 * @return return false if the requested rectangle is not possible with the
 * current dpi pointer. Only continue of the return value is true, or you'll
 * get some nasty results */
bool FillDrawPixelInfo(DrawPixelInfo *n, int left, int top, int width, int height)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	const DrawPixelInfo *o = _cur_dpi;

	n->zoom = ZOOM_LVL_NORMAL;

	assert(width > 0);
	assert(height > 0);

	if ((left -= o->left) < 0) {
		width += left;
		if (width <= 0) return false;
		n->left = -left;
		left = 0;
	} else {
		n->left = 0;
	}

	if (width > o->width - left) {
		width = o->width - left;
		if (width <= 0) return false;
	}
	n->width = width;

	if ((top -= o->top) < 0) {
		height += top;
		if (height <= 0) return false;
		n->top = -top;
		top = 0;
	} else {
		n->top = 0;
	}

	n->dst_ptr = blitter->MoveTo(o->dst_ptr, left, top);
	n->pitch = o->pitch;

	if (height > o->height - top) {
		height = o->height - top;
		if (height <= 0) return false;
	}
	n->height = height;

	return true;
}

static void SetCursorSprite(SpriteID cursor, SpriteID pal)
{
	CursorVars *cv = &_cursor;
	const Sprite *p;

	if (cv->sprite == cursor) return;

	p = GetSprite(GB(cursor, 0, SPRITE_WIDTH), ST_NORMAL);
	cv->sprite = cursor;
	cv->pal    = pal;
	cv->size.y = p->height;
	cv->size.x = p->width;
	cv->offs.x = p->x_offs;
	cv->offs.y = p->y_offs;

	cv->dirty = true;
	cv->short_vehicle_offset = 0;
}

static void SwitchAnimatedCursor()
{
	const AnimCursor *cur = _cursor.animate_cur;

	if (cur == NULL || cur->sprite == AnimCursor::LAST) cur = _cursor.animate_list;

	SetCursorSprite(cur->sprite, _cursor.pal);

	_cursor.animate_timeout = cur->display_time;
	_cursor.animate_cur     = cur + 1;
}

void CursorTick()
{
	if (_cursor.animate_timeout != 0 && --_cursor.animate_timeout == 0)
		SwitchAnimatedCursor();
}

void SetMouseCursor(SpriteID sprite, SpriteID pal)
{
	/* Turn off animation */
	_cursor.animate_timeout = 0;
	/* Set cursor */
	SetCursorSprite(sprite, pal);
}

void SetAnimatedMouseCursor(const AnimCursor *table)
{
	_cursor.animate_list = table;
	_cursor.animate_cur = NULL;
	_cursor.pal = PAL_NONE;
	SwitchAnimatedCursor();
}

bool ChangeResInGame(int width, int height)
{
	return (_screen.width == width && _screen.height == height) || _video_driver->ChangeResolution(width, height);
}

bool ToggleFullScreen(bool fs)
{
	bool result = _video_driver->ToggleFullscreen(fs);
	if (_fullscreen != fs && _num_resolutions == 0) {
		DEBUG(driver, 0, "Could not find a suitable fullscreen resolution");
	}
	return result;
}

static int CDECL compare_res(const Dimension *pa, const Dimension *pb)
{
	int x = pa->width - pb->width;
	if (x != 0) return x;
	return pa->height - pb->height;
}

void SortResolutions(int count)
{
	QSortT(_resolutions, count, &compare_res);
}
