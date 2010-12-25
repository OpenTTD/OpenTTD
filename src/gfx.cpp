/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx.cpp Handling of drawing text and other gfx related stuff. */

#include "stdafx.h"
#include "gfx_func.h"
#include "fontcache.h"
#include "genworld.h"
#include "zoom_func.h"
#include "blitter/factory.hpp"
#include "video/video_driver.hpp"
#include "strings_func.h"
#include "settings_type.h"
#include "network/network.h"
#include "network/network_func.h"
#include "thread/thread.h"
#include "window_func.h"
#include "newgrf_debug.h"

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
PauseModeByte _pause_mode;
int _pal_first_dirty;
int _pal_count_dirty;

Colour _cur_palette[256];

static int _max_char_height; ///< Cache of the height of the largest font
static int _max_char_width;  ///< Cache of the width of the largest font
static byte _stringwidth_table[FS_END][224]; ///< Cache containing width of often used characters. @see GetCharacterWidth()
DrawPixelInfo *_cur_dpi;
byte _colour_gradient[COLOUR_END][8];

static void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub = NULL, SpriteID sprite_id = SPR_CURSOR_MOUSE);

/**
 * Text drawing parameters, which can change while drawing a line, but are kept between multiple parts
 * of the same text, e.g. on line breaks.
 */
struct DrawStringParams {
	FontSize fontsize;
	TextColour cur_colour, prev_colour;

	DrawStringParams(TextColour colour) : fontsize(FS_NORMAL), cur_colour(colour), prev_colour(colour) {}

	/**
	 * Switch to new colour \a c.
	 * @param c New colour to use.
	 */
	FORCEINLINE void SetColour(TextColour c)
	{
		assert(c >=  TC_BLUE && c <= TC_BLACK);
		this->prev_colour = this->cur_colour;
		this->cur_colour = c;
	}

	/** Switch to previous colour. */
	FORCEINLINE void SetPreviousColour()
	{
		Swap(this->cur_colour, this->prev_colour);
	}

	/**
	 * Switch to using a new font \a f.
	 * @param f New font to use.
	 */
	FORCEINLINE void SetFontSize(FontSize f)
	{
		this->fontsize = f;
	}
};

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
static byte _string_colourremap[3]; ///< Recoloursprite for stringdrawing. The grf loader ensures that #ST_FONT sprites only use colours 0 to 2.

static const uint DIRTY_BLOCK_HEIGHT   = 8;
static const uint DIRTY_BLOCK_WIDTH    = 64;

static uint _dirty_bytes_per_line = 0;
static byte *_dirty_blocks = NULL;

void GfxScroll(int left, int top, int width, int height, int xo, int yo)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();

	if (xo == 0 && yo == 0) return;

	if (_cursor.visible) UndrawMouseCursor();

#ifdef ENABLE_NETWORK
	if (_networking) NetworkUndrawChatMessage();
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

	static const byte colour = 15;

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

	/* Black strings have no shading ever; the shading is black, so it
	 * would be invisible at best, but it actually makes it illegible. */
	bool no_shade   = colour & TC_NO_SHADE || colour == TC_BLACK;
	bool raw_colour = colour & TC_IS_PALETTE_COLOUR;
	colour &= ~(TC_NO_SHADE | TC_IS_PALETTE_COLOUR);

	_string_colourremap[1] = raw_colour ? (byte)colour : _string_colourmap[_use_palette][colour];
	_string_colourremap[2] = no_shade ? 0 : (_use_palette == PAL_DOS ? 1 : 215);
	_colour_remap_ptr = _string_colourremap;
}

#if !defined(WITH_ICU)
typedef WChar UChar;
static UChar *HandleBiDiAndArabicShapes(UChar *text) { return text; }
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
 * @return the buffer to draw from
 */
static UChar *HandleBiDiAndArabicShapes(UChar *buffer)
{
	static UChar input_output[DRAW_STRING_BUFFER];
	UChar intermediate[DRAW_STRING_BUFFER];

	UChar *t = buffer;
	size_t length = 0;
	while (*t != '\0' && length < lengthof(input_output) - 1) {
		input_output[length++] = *t++;
	}
	input_output[length] = 0;

	UErrorCode err = U_ZERO_ERROR;
	UBiDi *para = ubidi_openSized((int32_t)length, 0, &err);
	if (para == NULL) return buffer;

	ubidi_setPara(para, input_output, (int32_t)length, _current_text_dir == TD_RTL ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR, NULL, &err);
	ubidi_writeReordered(para, intermediate, (int32_t)length, UBIDI_REMOVE_BIDI_CONTROLS, &err);
	length = u_shapeArabic(intermediate, (int32_t)length, input_output, lengthof(input_output), U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_LETTERS_SHAPE, &err);
	ubidi_close(para);

	if (U_FAILURE(err)) return buffer;

	input_output[length] = '\0';
	return input_output;
}
#endif /* WITH_ICU */


/**
 * Truncate a given string to a maximum width if neccessary.
 * If the string is truncated, add three dots ('...') to show this.
 * @param *str string that is checked and possibly truncated
 * @param maxw maximum width in pixels of the string
 * @param ignore_setxy whether to ignore SETX(Y) or not
 * @param start_fontsize Fontsize to start the text with
 * @return new width of (truncated) string
 */
static int TruncateString(char *str, int maxw, bool ignore_setxy, FontSize start_fontsize)
{
	int w = 0;
	FontSize size = start_fontsize;
	int ddd, ddd_w;

	WChar c;
	char *ddd_pos;

	ddd_w = ddd = GetCharacterWidth(size, '.') * 3;

	for (ddd_pos = str; (c = Utf8Consume(const_cast<const char **>(&str))) != '\0'; ) {
		if (IsPrintable(c) && !IsTextDirectionChar(c)) {
			w += GetCharacterWidth(size, c);

			if (w > maxw) {
				/* string got too big... insert dotdotdot, but make sure we do not
				 * print anything beyond the string termination character. */
				for (int i = 0; *ddd_pos != '\0' && i < 3; i++, ddd_pos++) *ddd_pos = '.';
				*ddd_pos = '\0';
				return ddd_w;
			}
		} else {
			if (c == SCC_SETX) {
				if (!ignore_setxy) w = *str;
				str++;
			} else if (c == SCC_SETXY) {
				if (!ignore_setxy) w = *str;
				str += 2;
			} else if (c == SCC_TINYFONT) {
				size = FS_SMALL;
				ddd = GetCharacterWidth(size, '.') * 3;
			} else if (c == SCC_BIGFONT) {
				size = FS_LARGE;
				ddd = GetCharacterWidth(size, '.') * 3;
			} else if (c == '\n') {
				DEBUG(misc, 0, "Drawing string using newlines with DrawString instead of DrawStringMultiLine. Please notify the developers of this: [%s]", str);
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

static int ReallyDoDrawString(const UChar *string, int x, int y, DrawStringParams &params, bool parse_string_also_when_clipped = false);

/**
 * Get the real width of the string.
 * @param str the string to draw
 * @param start_fontsize Fontsize to start the text with
 * @return the width.
 */
static int GetStringWidth(const UChar *str, FontSize start_fontsize)
{
	FontSize size = start_fontsize;
	int max_width;
	int width;
	WChar c;

	width = max_width = 0;
	for (;;) {
		c = *str++;
		if (c == 0) break;
		if (IsPrintable(c) && !IsTextDirectionChar(c)) {
			width += GetCharacterWidth(size, c);
		} else {
			switch (c) {
				case SCC_SETX:
				case SCC_SETXY:
					/* At this point there is no SCC_SETX(Y) anymore */
					NOT_REACHED();
					break;
				case SCC_TINYFONT: size = FS_SMALL; break;
				case SCC_BIGFONT:  size = FS_LARGE; break;
				case '\n':
					max_width = max(max_width, width);
					break;
			}
		}
	}

	return max(max_width, width);
}

/**
 * Draw string, possibly truncated to make it fit in its allocated space
 *
 * @param left   The left most position to draw on.
 * @param right  The right most position to draw on.
 * @param top    The top most position to draw on.
 * @param str    String to draw.
 * @param last   The end of the string buffer to draw.
 * @param params Text drawing parameters.
 * @param align  The alignment of the string when drawing left-to-right. In the
 *               case a right-to-left language is chosen this is inverted so it
 *               will be drawn in the right direction.
 * @param underline Whether to underline what has been drawn or not.
 * @param truncate  Whether to truncate the string or not.
 *
 * @return In case of left or center alignment the right most pixel we have drawn to.
 *         In case of right alignment the left most pixel we have drawn to.
 */
static int DrawString(int left, int right, int top, char *str, const char *last, DrawStringParams &params, StringAlignment align, bool underline = false, bool truncate = true)
{
	/* We need the outer limits of both left/right */
	int min_left = INT32_MAX;
	int max_right = INT32_MIN;

	int initial_left = left;
	int initial_right = right;
	int initial_top = top;

	if (truncate) TruncateString(str, right - left + 1, (align & SA_STRIP) == SA_STRIP, params.fontsize);

	/*
	 * To support SETX and SETXY properly with RTL languages we have to
	 * calculate the offsets from the right. To do this we need to split
	 * the string and draw the parts separated by SETX(Y).
	 * So here we split
	 */
	static SmallVector<UChar *, 4> setx_offsets;
	setx_offsets.Clear();

	UChar draw_buffer[DRAW_STRING_BUFFER];
	UChar *p = draw_buffer;

	*setx_offsets.Append() = p;

	char *loc = str;
	for (;;) {
		WChar c;
		/* We cannot use Utf8Consume as we need the location of the SETX(Y) */
		size_t len = Utf8Decode(&c, loc);
		*p++ = c;

		if (c == '\0') break;
		if (p >= lastof(draw_buffer) - 3) {
			/* Make sure we never overflow (even if copying SCC_SETX(Y)). */
			*p = '\0';
			break;
		}
		if (c != SCC_SETX && c != SCC_SETXY) {
			loc += len;
			continue;
		}

		if (align & SA_STRIP) {
			/* We do not want to keep the SETX(Y)!. It was already copied, so
			 * remove it and undo the incrementing of the pointer! */
			*p-- = '\0';
			loc += len + (c == SCC_SETXY ? 2 : 1);
			continue;
		}

		if ((align & SA_HOR_MASK) != SA_LEFT) {
			DEBUG(grf, 1, "Using SETX and/or SETXY when not aligned to the left. Fixing alignment...");

			/* For left alignment and change the left so it will roughly be in the
			 * middle. This will never cause the string to be completely centered,
			 * but once SETX is used you cannot be sure the actual content of the
			 * string is centered, so it doesn't really matter. */
			align = SA_LEFT | SA_FORCE;
			initial_left = left = max(left, (left + right - (int)GetStringBoundingBox(str).width) / 2);
		}

		/* We add the begin of the string, but don't add it twice */
		if (p != draw_buffer) {
			*setx_offsets.Append() = p;
			p[-1] = '\0';
			*p++ = c;
		}

		/* Skip the SCC_SETX(Y) ... */
		loc += len;
		/* ... copy the x coordinate ... */
		*p++ = *loc++;
		/* ... and finally copy the y coordinate if it exists */
		if (c == SCC_SETXY) *p++ = *loc++;
	}

	/* In case we have a RTL language we swap the alignment. */
	if (!(align & SA_FORCE) && _current_text_dir == TD_RTL && !(align & SA_STRIP) && (align & SA_HOR_MASK) != SA_HOR_CENTER) align ^= SA_RIGHT;

	for (UChar **iter = setx_offsets.Begin(); iter != setx_offsets.End(); iter++) {
		UChar *to_draw = *iter;
		int offset = 0;

		/* Skip the SETX(Y) and set the appropriate offsets. */
		if (*to_draw == SCC_SETX || *to_draw == SCC_SETXY) {
			to_draw++;
			offset = *to_draw++;
			if (*to_draw == SCC_SETXY) top = initial_top + *to_draw++;
		}

		to_draw = HandleBiDiAndArabicShapes(to_draw);
		int w = GetStringWidth(to_draw, params.fontsize);

		/* right is the right most position to draw on. In this case we want to do
		 * calculations with the width of the string. In comparison right can be
		 * seen as lastof(todraw) and width as lengthof(todraw). They differ by 1.
		 * So most +1/-1 additions are to move from lengthof to 'indices'.
		 */
		switch (align & SA_HOR_MASK) {
			case SA_LEFT:
				/* right + 1 = left + w */
				left = initial_left + offset;
				right = left + w - 1;
				break;

			case SA_HOR_CENTER:
				left  = RoundDivSU(initial_right + 1 + initial_left - w, 2);
				/* right + 1 = left + w */
				right = left + w - 1;
				break;

			case SA_RIGHT:
				left = initial_right + 1 - w - offset;
				break;

			default:
				NOT_REACHED();
		}

		min_left  = min(left, min_left);
		max_right = max(right, max_right);

		ReallyDoDrawString(to_draw, left, top, params, !truncate);
		if (underline) {
			GfxFillRect(left, top + FONT_HEIGHT_NORMAL, right, top + FONT_HEIGHT_NORMAL, _string_colourremap[1]);
		}
	}

	return (align & SA_HOR_MASK) == SA_RIGHT ? min_left : max_right;
}

/**
 * Draw string, possibly truncated to make it fit in its allocated space
 *
 * @param left   The left most position to draw on.
 * @param right  The right most position to draw on.
 * @param top    The top most position to draw on.
 * @param str    String to draw.
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param align  The alignment of the string when drawing left-to-right. In the
 *               case a right-to-left language is chosen this is inverted so it
 *               will be drawn in the right direction.
 * @param underline Whether to underline what has been drawn or not.
 */
int DrawString(int left, int right, int top, const char *str, TextColour colour, StringAlignment align, bool underline)
{
	char buffer[DRAW_STRING_BUFFER];
	strecpy(buffer, str, lastof(buffer));
	DrawStringParams params(colour);
	return DrawString(left, right, top, buffer, lastof(buffer), params, align, underline);
}

/**
 * Draw string, possibly truncated to make it fit in its allocated space
 *
 * @param left   The left most position to draw on.
 * @param right  The right most position to draw on.
 * @param top    The top most position to draw on.
 * @param str    String to draw.
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param align  The alignment of the string when drawing left-to-right. In the
 *               case a right-to-left language is chosen this is inverted so it
 *               will be drawn in the right direction.
 * @param underline Whether to underline what has been drawn or not.
 */
int DrawString(int left, int right, int top, StringID str, TextColour colour, StringAlignment align, bool underline)
{
	char buffer[DRAW_STRING_BUFFER];
	GetString(buffer, str, lastof(buffer));
	DrawStringParams params(colour);
	return DrawString(left, right, top, buffer, lastof(buffer), params, align, underline);
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
 * @param last the last valid location (for '\0') in the buffer of str
 * @param maxw the maximum width the string can have on one line
 * @param size Fontsize to start the text with
 * @return return a 32bit wide number consisting of 2 packed values:
 *  0 - 15 the number of lines ADDED to the string
 * 16 - 31 the fontsize in which the length calculation was done at
 */
uint32 FormatStringLinebreaks(char *str, const char *last, int maxw, FontSize size)
{
	int num = 0;

	assert(maxw > 0);

	for (;;) {
		/* The character *after* the last space. */
		char *last_space = NULL;
		int w = 0;

		for (;;) {
			WChar c = Utf8Consume(const_cast<const char **>(&str));
			/* whitespace is where we will insert the line-break */
			if (IsWhitespace(c)) last_space = str;

			if (IsPrintable(c) && !IsTextDirectionChar(c)) {
				int char_w = GetCharacterWidth(size, c);
				w += char_w;
				if (w > maxw) {
					/* The string is longer than maximum width so we need to decide
					 * what to do with it. */
					if (w == char_w) {
						/* The character is wider than allowed width; don't know
						 * what to do with this case... bail out! */
						return num + (size << 16);
					}
					if (last_space == NULL) {
						/* No space has been found. Just terminate at our current
						 * location. This usually happens for languages that do not
						 * require spaces in strings, like Chinese, Japanese and
						 * Korean. For other languages terminating mid-word might
						 * not be the best, but terminating the whole string instead
						 * of continuing the word at the next line is worse. */
						str = Utf8PrevChar(str);
						size_t len = strlen(str);
						char *terminator = str + len;

						/* The string location + length of the string + 1 for '\0'
						 * always fits; otherwise there's no trailing '\0' and it
						 * it not a valid string. */
						assert(terminator <= last);
						assert(*terminator == '\0');

						/* If the string is too long we have to terminate it earlier. */
						if (terminator == last) {
							/* Get the 'begin' of the previous character and make that
							 * the terminator of the string; we truncate it 'early'. */
							*Utf8PrevChar(terminator) = '\0';
							len = strlen(str);
						}
						/* Also move the terminator! */
						memmove(str + 1, str, len + 1);
						*str = '\0';
						/* str needs to point to the character *after* the last space */
						str++;
					} else {
						/* A space is found; perfect place to terminate */
						str = last_space;
					}
					break;
				}
			} else {
				switch (c) {
					case '\0': return num + (size << 16);
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


/**
 * Calculates height of string (in pixels). Accepts multiline string with '\0' as separators.
 * @param src string to check
 * @param num number of extra lines (output of FormatStringLinebreaks())
 * @param start_fontsize Fontsize to start the text with
 * @note assumes text won't be truncated. FormatStringLinebreaks() is a good way to ensure that.
 * @return height of pixels of string when it is drawn
 */
static int GetMultilineStringHeight(const char *src, int num, FontSize start_fontsize)
{
	int maxy = 0;
	int y = 0;
	int fh = GetCharacterHeight(start_fontsize);

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


/**
 * Calculates height of string (in pixels). The string is changed to a multiline string if needed.
 * @param str string to check
 * @param maxw maximum string width
 * @return height of pixels of string when it is drawn
 */
int GetStringHeight(StringID str, int maxw)
{
	char buffer[DRAW_STRING_BUFFER];

	GetString(buffer, str, lastof(buffer));

	uint32 tmp = FormatStringLinebreaks(buffer, lastof(buffer), maxw);

	return GetMultilineStringHeight(buffer, GB(tmp, 0, 16), FS_NORMAL);
}

/**
 * Calculate string bounding box for multi-line strings.
 * @param str        String to check.
 * @param suggestion Suggested bounding box.
 * @return Bounding box for the multi-line string, may be bigger than \a suggestion.
 */
Dimension GetStringMultiLineBoundingBox(StringID str, const Dimension &suggestion)
{
	Dimension box = {suggestion.width, GetStringHeight(str, suggestion.width)};
	return box;
}

/**
 * Draw string, possibly over multiple lines.
 *
 * @param left   The left most position to draw on.
 * @param right  The right most position to draw on.
 * @param top    The top most position to draw on.
 * @param bottom The bottom most position to draw on.
 * @param str    String to draw.
 * @param last   The end of the string buffer to draw.
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param align  The horizontal and vertical alignment of the string.
 * @param underline Whether to underline all strings
 *
 * @return If \a align is #SA_BOTTOM, the top to where we have written, else the bottom to where we have written.
 */
static int DrawStringMultiLine(int left, int right, int top, int bottom, char *str, const char *last, TextColour colour, StringAlignment align, bool underline)
{
	int maxw = right - left + 1;
	int maxh = bottom - top + 1;

	/* It makes no sense to even try if it can't be drawn anyway, or
	 * do we really want to support fonts of 0 or less pixels high? */
	if (maxh <= 0) return top;

	uint32 tmp = FormatStringLinebreaks(str, last, maxw);
	int num = GB(tmp, 0, 16) + 1;

	int mt = GetCharacterHeight((FontSize)GB(tmp, 16, 16));
	int total_height = num * mt;

	int skip_lines = 0;
	if (total_height > maxh) {
		if (maxh < mt) return top; //  Not enough room for a single line.
		if ((align & SA_VERT_MASK) == SA_BOTTOM) {
			skip_lines = num;
			num = maxh / mt;
			skip_lines -= num;
		} else {
			num = maxh / mt;
		}
		total_height = num * mt;
	}

	int y;
	switch (align & SA_VERT_MASK) {
		case SA_TOP:
			y = top;
			break;

		case SA_VERT_CENTER:
			y = RoundDivSU(bottom + top - total_height, 2);
			break;

		case SA_BOTTOM:
			y = bottom - total_height;
			break;

		default: NOT_REACHED();
	}

	const char *src = str;
	DrawStringParams params(colour);
	int written_top = bottom; // Uppermost position of rendering a line of text
	for (;;) {
		if (skip_lines == 0) {
			char buf2[DRAW_STRING_BUFFER];
			strecpy(buf2, src, lastof(buf2));
			DrawString(left, right, y, buf2, lastof(buf2), params, align, underline, false);
			if (written_top > y) written_top = y;
			y += mt;
			num--;
		}

		for (;;) {
			WChar c = Utf8Consume(&src);
			if (c == 0) {
				break;
			} else if (c == SCC_SETX) {
				src++;
			} else if (c == SCC_SETXY) {
				src += 2;
			} else if (skip_lines > 0) {
				/* Skipped drawing, so do additional processing to update params. */
				if (c >= SCC_BLUE && c <= SCC_BLACK) {
					params.SetColour((TextColour)(c - SCC_BLUE));
				} else if (c == SCC_PREVIOUS_COLOUR) { // Revert to the previous colour.
					params.SetPreviousColour();
				} else if (c == SCC_TINYFONT) {
					params.SetFontSize(FS_SMALL);
				} else if (c == SCC_BIGFONT) {
					params.SetFontSize(FS_LARGE);
				}

			}
		}
		if (skip_lines > 0) skip_lines--;
		if (num == 0) return ((align & SA_VERT_MASK) == SA_BOTTOM) ? written_top : y;
	}
}

/**
 * Draw string, possibly over multiple lines.
 *
 * @param left   The left most position to draw on.
 * @param right  The right most position to draw on.
 * @param top    The top most position to draw on.
 * @param bottom The bottom most position to draw on.
 * @param str    String to draw.
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param align  The horizontal and vertical alignment of the string.
 * @param underline Whether to underline all strings
 *
 * @return If \a align is #SA_BOTTOM, the top to where we have written, else the bottom to where we have written.
 */
int DrawStringMultiLine(int left, int right, int top, int bottom, const char *str, TextColour colour, StringAlignment align, bool underline)
{
	char buffer[DRAW_STRING_BUFFER];
	strecpy(buffer, str, lastof(buffer));
	return DrawStringMultiLine(left, right, top, bottom, buffer, lastof(buffer), colour, align, underline);
}

/**
 * Draw string, possibly over multiple lines.
 *
 * @param left   The left most position to draw on.
 * @param right  The right most position to draw on.
 * @param top    The top most position to draw on.
 * @param bottom The bottom most position to draw on.
 * @param str    String to draw.
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param align  The horizontal and vertical alignment of the string.
 * @param underline Whether to underline all strings
 *
 * @return If \a align is #SA_BOTTOM, the top to where we have written, else the bottom to where we have written.
 */
int DrawStringMultiLine(int left, int right, int top, int bottom, StringID str, TextColour colour, StringAlignment align, bool underline)
{
	char buffer[DRAW_STRING_BUFFER];
	GetString(buffer, str, lastof(buffer));
	return DrawStringMultiLine(left, right, top, bottom, buffer, lastof(buffer), colour, align, underline);
}

/**
 * Return the string dimension in pixels. The height and width are returned
 * in a single Dimension value. TINYFONT, BIGFONT modifiers are only
 * supported as the first character of the string. The returned dimensions
 * are therefore a rough estimation correct for all the current strings
 * but not every possible combination
 * @param str string to calculate pixel-width
 * @param start_fontsize Fontsize to start the text with
 * @return string width and height in pixels
 */
Dimension GetStringBoundingBox(const char *str, FontSize start_fontsize)
{
	FontSize size = start_fontsize;
	Dimension br;
	uint max_width;
	WChar c;

	br.width = br.height = max_width = 0;
	for (;;) {
		c = Utf8Consume(&str);
		if (c == 0) break;
		if (IsPrintable(c) && !IsTextDirectionChar(c)) {
			br.width += GetCharacterWidth(size, c);
		} else {
			switch (c) {
				case SCC_SETX: br.width = max((uint)*str++, br.width); break;
				case SCC_SETXY:
					br.width  = max((uint)*str++, br.width);
					br.height = max((uint)*str++, br.height);
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
 * Get bounding box of a string. Uses parameters set by #DParam if needed.
 * Has the same restrictions as #GetStringBoundingBox(const char *str).
 * @param strid String to examine.
 * @return Width and height of the bounding box for the string in pixels.
 */
Dimension GetStringBoundingBox(StringID strid)
{
	char buffer[DRAW_STRING_BUFFER];

	GetString(buffer, strid, lastof(buffer));
	return GetStringBoundingBox(buffer);
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

/**
 * Draw a string at the given coordinates with the given colour.
 *  While drawing the string, parse it in case some formatting is specified,
 *  like new colour, new size or even positionning.
 * @param string              The string to draw. This is already bidi reordered.
 * @param x                   Offset from left side of the screen
 * @param y                   Offset from top side of the screen
 * @param params              Text drawing parameters
 * @param parse_string_also_when_clipped
 *                            By default, always test the available space where to draw the string.
 *                            When in multipline drawing, it would already be done,
 *                            so no need to re-perform the same kind (more or less) of verifications.
 *                            It's not only an optimisation, it's also a way to ensures the string will be parsed
 *                            (as there are certain side effects on global variables, which are important for the next line)
 * @return                    the x-coordinates where the drawing has finished.
 *                            If nothing is drawn, the originally passed x-coordinate is returned
 */
static int ReallyDoDrawString(const UChar *string, int x, int y, DrawStringParams &params, bool parse_string_also_when_clipped)
{
	DrawPixelInfo *dpi = _cur_dpi;
	UChar c;
	int xo = x;

	if (!parse_string_also_when_clipped) {
		/* in "mode multiline", the available space have been verified. Not in regular one.
		 * So if the string cannot be drawn, return the original start to say so.*/
		if (x >= dpi->left + dpi->width || y >= dpi->top + dpi->height) return x;
	}

switch_colour:;
	SetColourRemap(params.cur_colour);

check_bounds:
	if (y + _max_char_height <= dpi->top || dpi->top + dpi->height <= y) {
skip_char:;
		for (;;) {
			c = *string++;
			if (!IsPrintable(c)) goto skip_cont;
		}
	}

	for (;;) {
		c = *string++;
skip_cont:;
		if (c == 0) {
			return x;  // Nothing more to draw, get out. And here is the new x position
		}
		if (IsPrintable(c) && !IsTextDirectionChar(c)) {
			if (x >= dpi->left + dpi->width) goto skip_char;
			if (x + _max_char_width >= dpi->left) {
				GfxMainBlitter(GetGlyph(params.fontsize, c), x, y, BM_COLOUR_REMAP);
			}
			x += GetCharacterWidth(params.fontsize, c);
		} else if (c == '\n') { // newline = {}
			x = xo;  // We require a new line, so the x coordinate is reset
			y += GetCharacterHeight(params.fontsize);
			goto check_bounds;
		} else if (c >= SCC_BLUE && c <= SCC_BLACK) { // change colour?
			params.SetColour((TextColour)(c - SCC_BLUE));
			goto switch_colour;
		} else if (c == SCC_PREVIOUS_COLOUR) { // revert to the previous colour
			params.SetPreviousColour();
			goto switch_colour;
		} else if (c == SCC_SETX || c == SCC_SETXY) { // {SETX}/{SETXY}
			/* The characters are handled before calling this. */
			NOT_REACHED();
		} else if (c == SCC_TINYFONT) { // {TINYFONT}
			params.SetFontSize(FS_SMALL);
		} else if (c == SCC_BIGFONT) { // {BIGFONT}
			params.SetFontSize(FS_LARGE);
		} else if (!IsTextDirectionChar(c)) {
			DEBUG(misc, 0, "[utf8] unknown string command character %d", c);
		}
	}
}

/**
 * Get the size of a sprite.
 * @param sprid Sprite to examine.
 * @return Sprite size in pixels.
 * @note The size assumes (0, 0) as top-left coordinate and ignores any part of the sprite drawn at the left or above that position.
 */
Dimension GetSpriteSize(SpriteID sprid)
{
	const Sprite *sprite = GetSprite(sprid, ST_NORMAL);

	Dimension d;
	d.width  = max<int>(0, sprite->x_offs + sprite->width);
	d.height = max<int>(0, sprite->y_offs + sprite->height);
	return d;
}

/**
 * Draw a sprite.
 * @param img  Image number to draw
 * @param pal  Palette to use.
 * @param x    Left coordinate of image
 * @param y    Top coordinate of image
 * @param sub  If available, draw only specified part of the sprite
 */
void DrawSprite(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub)
{
	SpriteID real_sprite = GB(img, 0, SPRITE_WIDTH);
	if (HasBit(img, PALETTE_MODIFIER_TRANSPARENT)) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitter(GetSprite(real_sprite, ST_NORMAL), x, y, BM_TRANSPARENT, sub, real_sprite);
	} else if (pal != PAL_NONE) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitter(GetSprite(real_sprite, ST_NORMAL), x, y, BM_COLOUR_REMAP, sub, real_sprite);
	} else {
		GfxMainBlitter(GetSprite(real_sprite, ST_NORMAL), x, y, BM_NORMAL, sub, real_sprite);
	}
}

static void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub, SpriteID sprite_id)
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

	/* We do not want to catch the mouse. However we also use that spritenumber for unknown (text) sprites. */
	if (_newgrf_debug_sprite_picker.mode == SPM_REDRAW && sprite_id != SPR_CURSOR_MOUSE) {
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
		void *topleft = blitter->MoveTo(bp.dst, bp.left, bp.top);
		void *bottomright = blitter->MoveTo(topleft, bp.width - 1, bp.height - 1);

		void *clicked = _newgrf_debug_sprite_picker.clicked_pixel;

		if (topleft <= clicked && clicked <= bottomright) {
			uint offset = (((size_t)clicked - (size_t)topleft) / (blitter->GetScreenDepth() / 8)) % bp.pitch;
			if (offset < (uint)bp.width) {
				_newgrf_debug_sprite_picker.sprites.Include(sprite_id);
			}
		}
	}

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

#define EXTR(p, q) (((uint16)(palette_animation_counter * (p)) * (q)) >> 16)
#define EXTR2(p, q) (((uint16)(~palette_animation_counter * (p)) * (q)) >> 16)

void DoPaletteAnimations()
{
	/* Animation counter for the palette animation. */
	static int palette_animation_counter = 0;
	palette_animation_counter += 8;

	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	const Colour *s;
	const ExtraPaletteValues *ev = &_extra_palette_values;
	/* Amount of colours to be rotated.
	 * A few more for the DOS palette, because the water colours are
	 * 245-254 for DOS and 217-226 for Windows.  */
	const int colour_rotation_amount = (_use_palette == PAL_DOS) ? PALETTE_ANIM_SIZE_DOS : PALETTE_ANIM_SIZE_WIN;
	Colour old_val[PALETTE_ANIM_SIZE_DOS];
	const int oldval_size = colour_rotation_amount * sizeof(*old_val);
	const uint old_tc = palette_animation_counter;
	uint i;
	uint j;

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		palette_animation_counter = 0;
	}

	Colour *palette_pos = &_cur_palette[PALETTE_ANIM_SIZE_START];  // Points to where animations are taking place on the palette
	/* Makes a copy of the current anmation palette in old_val,
	 * so the work on the current palette could be compared, see if there has been any changes */
	memcpy(old_val, palette_pos, oldval_size);

	/* Dark blue water */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->dark_water_toyland : ev->dark_water;
	j = EXTR(320, EPV_CYCLES_DARK_WATER);
	for (i = 0; i != EPV_CYCLES_DARK_WATER; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_DARK_WATER) j = 0;
	}

	/* Glittery water; jumps over 3 colours each cycle! */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->glitter_water_toyland : ev->glitter_water;
	j = EXTR(128, EPV_CYCLES_GLITTER_WATER);
	for (i = 0; i != EPV_CYCLES_GLITTER_WATER / 3; i++) {
		*palette_pos++ = s[j];
		j += 3;
		if (j >= EPV_CYCLES_GLITTER_WATER) j -= EPV_CYCLES_GLITTER_WATER;
	}

	/* Fizzy Drink bubbles animation */
	s = ev->fizzy_drink;
	j = EXTR2(512, EPV_CYCLES_FIZZY_DRINK);
	for (i = 0; i != EPV_CYCLES_FIZZY_DRINK; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_FIZZY_DRINK) j = 0;
	}

	/* Oil refinery fire animation */
	s = ev->oil_refinery;
	j = EXTR2(512, EPV_CYCLES_OIL_REFINERY);
	for (i = 0; i != EPV_CYCLES_OIL_REFINERY; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_OIL_REFINERY) j = 0;
	}

	/* Radio tower blinking */
	{
		byte i = (palette_animation_counter >> 1) & 0x7F;
		byte v;

		if (i < 0x3f) {
			v = 255;
		} else if (i < 0x4A || i >= 0x75) {
			v = 128;
		} else {
			v = 20;
		}
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;

		i ^= 0x40;
		if (i < 0x3f) {
			v = 255;
		} else if (i < 0x4A || i >= 0x75) {
			v = 128;
		} else {
			v = 20;
		}
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;
	}

	/* Handle lighthouse and stadium animation */
	s = ev->lighthouse;
	j = EXTR(256, EPV_CYCLES_LIGHTHOUSE);
	for (i = 0; i != EPV_CYCLES_LIGHTHOUSE; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_LIGHTHOUSE) j = 0;
	}

	/* Animate water for old DOS graphics */
	if (_use_palette == PAL_DOS) {
		/* Dark blue water DOS */
		s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->dark_water_toyland : ev->dark_water;
		j = EXTR(320, EPV_CYCLES_DARK_WATER);
		for (i = 0; i != EPV_CYCLES_DARK_WATER; i++) {
			*palette_pos++ = s[j];
			j++;
			if (j == EPV_CYCLES_DARK_WATER) j = 0;
		}

		/* Glittery water DOS */
		s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->glitter_water_toyland : ev->glitter_water;
		j = EXTR(128, EPV_CYCLES_GLITTER_WATER);
		for (i = 0; i != EPV_CYCLES_GLITTER_WATER / 3; i++) {
			*palette_pos++ = s[j];
			j += 3;
			if (j >= EPV_CYCLES_GLITTER_WATER) j -= EPV_CYCLES_GLITTER_WATER;
		}
	}

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		palette_animation_counter = old_tc;
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
	_max_char_height = 0;
	_max_char_width  = 0;

	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		_max_char_height = max<int>(_max_char_height, GetCharacterHeight(fs));
		for (uint i = 0; i != 224; i++) {
			_stringwidth_table[fs][i] = GetGlyphWidth(fs, i + 32);
			_max_char_width = max<int>(_max_char_width, _stringwidth_table[fs][i]);
		}
	}

	/* Needed because they need to be 1 more than the widest. */
	_max_char_height++;
	_max_char_width++;

	ReInitAllWindows();
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

/**
 * Return the maximum width of single digit.
 * @param size  Font of the digit
 * @return Width of the digit.
 */
byte GetDigitWidth(FontSize size)
{
	byte width = 0;
	for (char c = '0'; c <= '9'; c++) {
		width = max(GetCharacterWidth(size, c), width);
	}
	return width;
}


void ScreenSizeChanged()
{
	_dirty_bytes_per_line = CeilDiv(_screen.width, DIRTY_BLOCK_WIDTH);
	_dirty_blocks = ReallocT<byte>(_dirty_blocks, _dirty_bytes_per_line * CeilDiv(_screen.height, DIRTY_BLOCK_HEIGHT));

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
	if (_networking) NetworkUndrawChatMessage();
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

	if (IsGeneratingWorld()) {
		/* We are generating the world, so release our rights to the map and
		 * painting while we are waiting a bit. */
		_genworld_paint_mutex->EndCritical();
		_genworld_mapgen_mutex->EndCritical();

		/* Wait a while and update _realtime_tick so we are given the rights */
		CSleep(GENWORLD_REDRAW_TIMEOUT);
		_realtime_tick += GENWORLD_REDRAW_TIMEOUT;
		_genworld_paint_mutex->BeginCritical();
		_genworld_mapgen_mutex->BeginCritical();
	}

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
	} while (b += -(int)(w / DIRTY_BLOCK_WIDTH) + _dirty_bytes_per_line, (y += DIRTY_BLOCK_HEIGHT) != h);

	_invalid_rect.left = w;
	_invalid_rect.top = h;
	_invalid_rect.right = 0;
	_invalid_rect.bottom = 0;
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

/**
 * Set up a clipping area for only drawing into a certain area. To do this,
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
 * get some nasty results
 */
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

/**
 * Update cursor dimension.
 * Called when changing cursor sprite resp. reloading grfs.
 */
void UpdateCursorSize()
{
	CursorVars *cv = &_cursor;
	const Sprite *p = GetSprite(GB(cv->sprite, 0, SPRITE_WIDTH), ST_NORMAL);

	cv->size.y = p->height;
	cv->size.x = p->width;
	cv->offs.x = p->x_offs;
	cv->offs.y = p->y_offs;

	cv->dirty = true;
}

/**
 * Switch cursor to different sprite.
 * @param cursor Sprite to draw for the cursor.
 * @param pal Palette to use for recolouring.
 */
static void SetCursorSprite(CursorID cursor, PaletteID pal)
{
	CursorVars *cv = &_cursor;
	if (cv->sprite == cursor) return;

	cv->sprite = cursor;
	cv->pal    = pal;
	UpdateCursorSize();

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
	if (_cursor.animate_timeout != 0 && --_cursor.animate_timeout == 0) {
		SwitchAnimatedCursor();
	}
}

/**
 * Assign a single non-animated sprite to the cursor.
 * @param sprite Sprite to draw for the cursor.
 * @param pal Palette to use for recolouring.
 * @see SetAnimatedMouseCursor
 */
void SetMouseCursor(CursorID sprite, PaletteID pal)
{
	/* Turn off animation */
	_cursor.animate_timeout = 0;
	/* Set cursor */
	SetCursorSprite(sprite, pal);
}

/**
 * Assign an animation to the cursor.
 * @param table Array of animation states.
 * @see SetMouseCursor
 */
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
