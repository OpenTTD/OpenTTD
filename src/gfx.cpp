/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx.cpp Handling of drawing text and other gfx related stuff. */

#include "stdafx.h"
#include "gfx_layout.h"
#include "progress.h"
#include "zoom_func.h"
#include "blitter/factory.hpp"
#include "video/video_driver.hpp"
#include "strings_func.h"
#include "settings_type.h"
#include "network/network.h"
#include "network/network_func.h"
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
Palette _cur_palette;

static byte _stringwidth_table[FS_END][224]; ///< Cache containing width of often used characters. @see GetCharacterWidth()
DrawPixelInfo *_cur_dpi;
byte _colour_gradient[COLOUR_END][8];

static void GfxMainBlitterViewport(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub = NULL, SpriteID sprite_id = SPR_CURSOR_MOUSE);
static void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub = NULL, SpriteID sprite_id = SPR_CURSOR_MOUSE, ZoomLevel zoom = ZOOM_LVL_NORMAL);

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
extern uint _dirty_block_colour;

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

void GfxDrawLine(int x, int y, int x2, int y2, int colour, int width)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	DrawPixelInfo *dpi = _cur_dpi;

	assert(width > 0);

	x -= dpi->left;
	x2 -= dpi->left;
	y -= dpi->top;
	y2 -= dpi->top;

	/* Check clipping */
	if (x + width / 2 < 0           && x2 + width / 2 < 0          ) return;
	if (y + width / 2 < 0           && y2 + width / 2 < 0          ) return;
	if (x - width / 2 > dpi->width  && x2 - width / 2 > dpi->width ) return;
	if (y - width / 2 > dpi->height && y2 - width / 2 > dpi->height) return;

	blitter->DrawLine(dpi->dst_ptr, x, y, x2, y2, dpi->width, dpi->height, colour, width);
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
			UnScaleByZoom(dpi->width, dpi->zoom), UnScaleByZoom(dpi->height, dpi->zoom), colour, 1);
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

	static const byte colour = PC_WHITE;

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
	bool no_shade   = (colour & TC_NO_SHADE) != 0 || colour == TC_BLACK;
	bool raw_colour = (colour & TC_IS_PALETTE_COLOUR) != 0;
	colour &= ~(TC_NO_SHADE | TC_IS_PALETTE_COLOUR);

	_string_colourremap[1] = raw_colour ? (byte)colour : _string_colourmap[colour];
	_string_colourremap[2] = no_shade ? 0 : 1;
	_colour_remap_ptr = _string_colourremap;
}

/**
 * Drawing routine for drawing a laid out line of text.
 * @param line      String to draw.
 * @param y         The top most position to draw on.
 * @param left      The left most position to draw on.
 * @param right     The right most position to draw on.
 * @param align     The alignment of the string when drawing left-to-right. In the
 *                  case a right-to-left language is chosen this is inverted so it
 *                  will be drawn in the right direction.
 * @param underline Whether to underline what has been drawn or not.
 * @param truncation Whether to perform string truncation or not.
 *
 * @return In case of left or center alignment the right most pixel we have drawn to.
 *         In case of right alignment the left most pixel we have drawn to.
 */
static int DrawLayoutLine(ParagraphLayout::Line *line, int y, int left, int right, StringAlignment align, bool underline, bool truncation)
{
	if (line->countRuns() == 0) return 0;

	int w = line->getWidth();
	int h = line->getLeading();

	/*
	 * The following is needed for truncation.
	 * Depending on the text direction, we either remove bits at the rear
	 * or the front. For this we shift the entire area to draw so it fits
	 * within the left/right bounds and the side we do not truncate it on.
	 * Then we determine the truncation location, i.e. glyphs that fall
	 * outside of the range min_x - max_x will not be drawn; they are thus
	 * the truncated glyphs.
	 *
	 * At a later step we insert the dots.
	 */

	int max_w = right - left + 1; // The maximum width.

	int offset_x = 0;  // The offset we need for positioning the glyphs
	int min_x = left;  // The minimum x position to draw normal glyphs on.
	int max_x = right; // The maximum x position to draw normal glyphs on.

	truncation &= max_w < w;         // Whether we need to do truncation.
	int dot_width = 0;               // Cache for the width of the dot.
	const Sprite *dot_sprite = NULL; // Cache for the sprite of the dot.

	if (truncation) {
		/*
		 * Assumption may be made that all fonts of a run are of the same size.
		 * In any case, we'll use these dots for the abbreviation, so even if
		 * another size would be chosen it won't have truncated too little for
		 * the truncation dots.
		 */
		FontCache *fc = ((const Font*)line->getVisualRun(0)->getFont())->fc;
		GlyphID dot_glyph = fc->MapCharToGlyph('.');
		dot_width = fc->GetGlyphWidth(dot_glyph);
		dot_sprite = fc->GetGlyph(dot_glyph);

		if (_current_text_dir == TD_RTL) {
			min_x += 3 * dot_width;
			offset_x = w - 3 * dot_width - max_w;
		} else {
			max_x -= 3 * dot_width;
		}

		w = max_w;
	}

	/* In case we have a RTL language we swap the alignment. */
	if (!(align & SA_FORCE) && _current_text_dir == TD_RTL && (align & SA_HOR_MASK) != SA_HOR_CENTER) align ^= SA_RIGHT;

	/* right is the right most position to draw on. In this case we want to do
	 * calculations with the width of the string. In comparison right can be
	 * seen as lastof(todraw) and width as lengthof(todraw). They differ by 1.
	 * So most +1/-1 additions are to move from lengthof to 'indices'.
	 */
	switch (align & SA_HOR_MASK) {
		case SA_LEFT:
			/* right + 1 = left + w */
			right = left + w - 1;
			break;

		case SA_HOR_CENTER:
			left  = RoundDivSU(right + 1 + left - w, 2);
			/* right + 1 = left + w */
			right = left + w - 1;
			break;

		case SA_RIGHT:
			left = right + 1 - w;
			break;

		default:
			NOT_REACHED();
	}

	for (int run_index = 0; run_index < line->countRuns(); run_index++) {
		const ParagraphLayout::VisualRun *run = line->getVisualRun(run_index);
		const Font *f = (const Font*)run->getFont();

		FontCache *fc = f->fc;
		TextColour colour = f->colour;
		SetColourRemap(colour);

		DrawPixelInfo *dpi = _cur_dpi;
		int dpi_left  = dpi->left;
		int dpi_right = dpi->left + dpi->width - 1;

		bool draw_shadow = fc->GetDrawGlyphShadow() && colour != TC_BLACK;

		for (int i = 0; i < run->getGlyphCount(); i++) {
			GlyphID glyph = run->getGlyphs()[i];

			/* Not a valid glyph (empty) */
			if (glyph == 0xFFFF) continue;

			int begin_x = run->getPositions()[i * 2]     + left - offset_x;
			int end_x   = run->getPositions()[i * 2 + 2] + left - offset_x  - 1;
			int top     = run->getPositions()[i * 2 + 1] + y;

			/* Truncated away. */
			if (truncation && (begin_x < min_x || end_x > max_x)) continue;

			const Sprite *sprite = fc->GetGlyph(glyph);
			/* Check clipping (the "+ 1" is for the shadow). */
			if (begin_x + sprite->x_offs > dpi_right || begin_x + sprite->x_offs + sprite->width /* - 1 + 1 */ < dpi_left) continue;

			if (draw_shadow && (glyph & SPRITE_GLYPH) == 0) {
				SetColourRemap(TC_BLACK);
				GfxMainBlitter(sprite, begin_x + 1, top + 1, BM_COLOUR_REMAP);
				SetColourRemap(colour);
			}
			GfxMainBlitter(sprite, begin_x, top, BM_COLOUR_REMAP);
		}
	}

	if (truncation) {
		int x = (_current_text_dir == TD_RTL) ? left : (right - 3 * dot_width);
		for (int i = 0; i < 3; i++, x += dot_width) {
			GfxMainBlitter(dot_sprite, x, y, BM_COLOUR_REMAP);
		}
	}

	if (underline) {
		GfxFillRect(left, y + h, right, y + h, _string_colourremap[1]);
	}

	return (align & SA_HOR_MASK) == SA_RIGHT ? left : right;
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
 * @param fontsize The size of the initial characters.
 * @return In case of left or center alignment the right most pixel we have drawn to.
 *         In case of right alignment the left most pixel we have drawn to.
 */
int DrawString(int left, int right, int top, const char *str, TextColour colour, StringAlignment align, bool underline, FontSize fontsize)
{
	/* The string may contain control chars to change the font, just use the biggest font for clipping. */
	int max_height = max(max(FONT_HEIGHT_SMALL, FONT_HEIGHT_NORMAL), max(FONT_HEIGHT_LARGE, FONT_HEIGHT_MONO));

	/* Funny glyphs may extent outside the usual bounds, so relax the clipping somewhat. */
	int extra = max_height / 2;

	if (_cur_dpi->top + _cur_dpi->height + extra < top || _cur_dpi->top > top + max_height + extra ||
			_cur_dpi->left + _cur_dpi->width + extra < left || _cur_dpi->left > right + extra) {
		return 0;
	}

	Layouter layout(str, INT32_MAX, colour, fontsize);
	if (layout.Length() == 0) return 0;

	return DrawLayoutLine(*layout.Begin(), top, left, right, align, underline, true);
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
 * @param fontsize The size of the initial characters.
 * @return In case of left or center alignment the right most pixel we have drawn to.
 *         In case of right alignment the left most pixel we have drawn to.
 */
int DrawString(int left, int right, int top, StringID str, TextColour colour, StringAlignment align, bool underline, FontSize fontsize)
{
	char buffer[DRAW_STRING_BUFFER];
	GetString(buffer, str, lastof(buffer));
	return DrawString(left, right, top, buffer, colour, align, underline, fontsize);
}

/**
 * Calculates height of string (in pixels). The string is changed to a multiline string if needed.
 * @param str string to check
 * @param maxw maximum string width
 * @return height of pixels of string when it is drawn
 */
static int GetStringHeight(const char *str, int maxw)
{
	Layouter layout(str, maxw);
	return layout.GetBounds().height;
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
	return GetStringHeight(buffer, maxw);
}

/**
 * Calculates number of lines of string. The string is changed to a multiline string if needed.
 * @param str string to check
 * @param maxw maximum string width
 * @return number of lines of string when it is drawn
 */
int GetStringLineCount(StringID str, int maxw)
{
	char buffer[DRAW_STRING_BUFFER];
	GetString(buffer, str, lastof(buffer));

	Layouter layout(buffer, maxw);
	return layout.Length();
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
 * Calculate string bounding box for multi-line strings.
 * @param str        String to check.
 * @param suggestion Suggested bounding box.
 * @return Bounding box for the multi-line string, may be bigger than \a suggestion.
 */
Dimension GetStringMultiLineBoundingBox(const char *str, const Dimension &suggestion)
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
 * @param colour Colour used for drawing the string, see DoDrawString() for details
 * @param align  The horizontal and vertical alignment of the string.
 * @param underline Whether to underline all strings
 * @param fontsize The size of the initial characters.
 *
 * @return If \a align is #SA_BOTTOM, the top to where we have written, else the bottom to where we have written.
 */
int DrawStringMultiLine(int left, int right, int top, int bottom, const char *str, TextColour colour, StringAlignment align, bool underline, FontSize fontsize)
{
	int maxw = right - left + 1;
	int maxh = bottom - top + 1;

	/* It makes no sense to even try if it can't be drawn anyway, or
	 * do we really want to support fonts of 0 or less pixels high? */
	if (maxh <= 0) return top;

	Layouter layout(str, maxw, colour, fontsize);
	int total_height = layout.GetBounds().height;
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

	int last_line = top;
	int first_line = bottom;

	for (ParagraphLayout::Line **iter = layout.Begin(); iter != layout.End(); iter++) {
		ParagraphLayout::Line *line = *iter;

		int line_height = line->getLeading();
		if (y >= top && y < bottom) {
			last_line = y + line_height;
			if (first_line > y) first_line = y;

			DrawLayoutLine(line, y, left, right, align, underline, false);
		}
		y += line_height;
	}

	return ((align & SA_VERT_MASK) == SA_BOTTOM) ? first_line : last_line;
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
 * @param fontsize The size of the initial characters.
 *
 * @return If \a align is #SA_BOTTOM, the top to where we have written, else the bottom to where we have written.
 */
int DrawStringMultiLine(int left, int right, int top, int bottom, StringID str, TextColour colour, StringAlignment align, bool underline, FontSize fontsize)
{
	char buffer[DRAW_STRING_BUFFER];
	GetString(buffer, str, lastof(buffer));
	return DrawStringMultiLine(left, right, top, bottom, buffer, colour, align, underline, fontsize);
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
	Layouter layout(str, INT32_MAX, TC_FROMSTRING, start_fontsize);
	return layout.GetBounds();
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
 * Get the size of a sprite.
 * @param sprid Sprite to examine.
 * @param [out] offset Optionally returns the sprite position offset.
 * @return Sprite size in pixels.
 * @note The size assumes (0, 0) as top-left coordinate and ignores any part of the sprite drawn at the left or above that position.
 */
Dimension GetSpriteSize(SpriteID sprid, Point *offset, ZoomLevel zoom)
{
	const Sprite *sprite = GetSprite(sprid, ST_NORMAL);

	if (offset != NULL) {
		offset->x = UnScaleByZoom(sprite->x_offs, zoom);
		offset->y = UnScaleByZoom(sprite->y_offs, zoom);
	}

	Dimension d;
	d.width  = max<int>(0, UnScaleByZoom(sprite->x_offs + sprite->width, zoom));
	d.height = max<int>(0, UnScaleByZoom(sprite->y_offs + sprite->height, zoom));
	return d;
}

/**
 * Draw a sprite in a viewport.
 * @param img  Image number to draw
 * @param pal  Palette to use.
 * @param x    Left coordinate of image in viewport, scaled by zoom
 * @param y    Top coordinate of image in viewport, scaled by zoom
 * @param sub  If available, draw only specified part of the sprite
 */
void DrawSpriteViewport(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub)
{
	SpriteID real_sprite = GB(img, 0, SPRITE_WIDTH);
	if (HasBit(img, PALETTE_MODIFIER_TRANSPARENT)) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitterViewport(GetSprite(real_sprite, ST_NORMAL), x, y, BM_TRANSPARENT, sub, real_sprite);
	} else if (pal != PAL_NONE) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitterViewport(GetSprite(real_sprite, ST_NORMAL), x, y, BM_COLOUR_REMAP, sub, real_sprite);
	} else {
		GfxMainBlitterViewport(GetSprite(real_sprite, ST_NORMAL), x, y, BM_NORMAL, sub, real_sprite);
	}
}

/**
 * Draw a sprite, not in a viewport
 * @param img  Image number to draw
 * @param pal  Palette to use.
 * @param x    Left coordinate of image in pixels
 * @param y    Top coordinate of image in pixels
 * @param sub  If available, draw only specified part of the sprite
 * @param zoom Zoom level of sprite
 */
void DrawSprite(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub, ZoomLevel zoom)
{
	SpriteID real_sprite = GB(img, 0, SPRITE_WIDTH);
	if (HasBit(img, PALETTE_MODIFIER_TRANSPARENT)) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitter(GetSprite(real_sprite, ST_NORMAL), x, y, BM_TRANSPARENT, sub, real_sprite, zoom);
	} else if (pal != PAL_NONE) {
		_colour_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH), ST_RECOLOUR) + 1;
		GfxMainBlitter(GetSprite(real_sprite, ST_NORMAL), x, y, BM_COLOUR_REMAP, sub, real_sprite, zoom);
	} else {
		GfxMainBlitter(GetSprite(real_sprite, ST_NORMAL), x, y, BM_NORMAL, sub, real_sprite, zoom);
	}
}

static void GfxMainBlitterViewport(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub, SpriteID sprite_id)
{
	const DrawPixelInfo *dpi = _cur_dpi;
	Blitter::BlitterParams bp;

	/* Amount of pixels to clip from the source sprite */
	int clip_left   = (sub != NULL ? max(0,                   -sprite->x_offs + sub->left * ZOOM_LVL_BASE       ) : 0);
	int clip_top    = (sub != NULL ? max(0,                   -sprite->y_offs + sub->top  * ZOOM_LVL_BASE       ) : 0);
	int clip_right  = (sub != NULL ? max(0, sprite->width  - (-sprite->x_offs + (sub->right + 1)  * ZOOM_LVL_BASE)) : 0);
	int clip_bottom = (sub != NULL ? max(0, sprite->height - (-sprite->y_offs + (sub->bottom + 1) * ZOOM_LVL_BASE)) : 0);

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

static void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub, SpriteID sprite_id, ZoomLevel zoom)
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

	/* Scale it */
	x = ScaleByZoom(x, zoom);
	y = ScaleByZoom(y, zoom);

	/* Move to the correct offset */
	x += sprite->x_offs;
	y += sprite->y_offs;

	/* Copy the main data directly from the sprite */
	bp.sprite = sprite->data;
	bp.sprite_width = sprite->width;
	bp.sprite_height = sprite->height;
	bp.width = UnScaleByZoom(sprite->width - clip_left - clip_right, zoom);
	bp.height = UnScaleByZoom(sprite->height - clip_top - clip_bottom, zoom);
	bp.top = 0;
	bp.left = 0;
	bp.skip_left = UnScaleByZoomLower(clip_left, zoom);
	bp.skip_top = UnScaleByZoomLower(clip_top, zoom);

	x += ScaleByZoom(bp.skip_left, zoom);
	y += ScaleByZoom(bp.skip_top, zoom);

	bp.dst = dpi->dst_ptr;
	bp.pitch = dpi->pitch;
	bp.remap = _colour_remap_ptr;

	assert(sprite->width > 0);
	assert(sprite->height > 0);

	if (bp.width <= 0) return;
	if (bp.height <= 0) return;

	y -= ScaleByZoom(dpi->top, zoom);
	/* Check for top overflow */
	if (y < 0) {
		bp.height -= -UnScaleByZoom(y, zoom);
		if (bp.height <= 0) return;
		bp.skip_top += -UnScaleByZoom(y, zoom);
		y = 0;
	} else {
		bp.top = UnScaleByZoom(y, zoom);
	}

	/* Check for bottom overflow */
	y += ScaleByZoom(bp.height - dpi->height, zoom);
	if (y > 0) {
		bp.height -= UnScaleByZoom(y, zoom);
		if (bp.height <= 0) return;
	}

	x -= ScaleByZoom(dpi->left, zoom);
	/* Check for left overflow */
	if (x < 0) {
		bp.width -= -UnScaleByZoom(x, zoom);
		if (bp.width <= 0) return;
		bp.skip_left += -UnScaleByZoom(x, zoom);
		x = 0;
	} else {
		bp.left = UnScaleByZoom(x, zoom);
	}

	/* Check for right overflow */
	x += ScaleByZoom(bp.width - dpi->width, zoom);
	if (x > 0) {
		bp.width -= UnScaleByZoom(x, zoom);
		if (bp.width <= 0) return;
	}

	assert(bp.skip_left + bp.width <= UnScaleByZoom(sprite->width, zoom));
	assert(bp.skip_top + bp.height <= UnScaleByZoom(sprite->height, zoom));

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

	BlitterFactoryBase::GetCurrentBlitter()->Draw(&bp, mode, zoom);
}

void DoPaletteAnimations();

void GfxInitPalettes()
{
	memcpy(&_cur_palette, &_palette, sizeof(_cur_palette));
	DoPaletteAnimations();
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
	Colour old_val[PALETTE_ANIM_SIZE];
	const uint old_tc = palette_animation_counter;
	uint i;
	uint j;

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		palette_animation_counter = 0;
	}

	Colour *palette_pos = &_cur_palette.palette[PALETTE_ANIM_START];  // Points to where animations are taking place on the palette
	/* Makes a copy of the current animation palette in old_val,
	 * so the work on the current palette could be compared, see if there has been any changes */
	memcpy(old_val, palette_pos, sizeof(old_val));

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

	/* Dark blue water */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->dark_water_toyland : ev->dark_water;
	j = EXTR(320, EPV_CYCLES_DARK_WATER);
	for (i = 0; i != EPV_CYCLES_DARK_WATER; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_DARK_WATER) j = 0;
	}

	/* Glittery water */
	s = (_settings_game.game_creation.landscape == LT_TOYLAND) ? ev->glitter_water_toyland : ev->glitter_water;
	j = EXTR(128, EPV_CYCLES_GLITTER_WATER);
	for (i = 0; i != EPV_CYCLES_GLITTER_WATER / 3; i++) {
		*palette_pos++ = s[j];
		j += 3;
		if (j >= EPV_CYCLES_GLITTER_WATER) j -= EPV_CYCLES_GLITTER_WATER;
	}

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		palette_animation_counter = old_tc;
	} else {
		if (memcmp(old_val, &_cur_palette.palette[PALETTE_ANIM_START], sizeof(old_val)) != 0 && _cur_palette.count_dirty == 0) {
			/* Did we changed anything on the palette? Seems so.  Mark it as dirty */
			_cur_palette.first_dirty = PALETTE_ANIM_START;
			_cur_palette.count_dirty = PALETTE_ANIM_SIZE;
		}
	}
}

/**
 * Determine a contrasty text colour for a coloured background.
 * @param background Background colour.
 * @return TC_BLACK or TC_WHITE depending on what gives a better contrast.
 */
TextColour GetContrastColour(uint8 background)
{
	Colour c = _cur_palette.palette[background];
	/* Compute brightness according to http://www.w3.org/TR/AERT#color-contrast.
	 * The following formula computes 1000 * brightness^2, with brightness being in range 0 to 255. */
	uint sq1000_brightness = c.r * c.r * 299 + c.g * c.g * 587 + c.b * c.b * 114;
	/* Compare with threshold brightness 128 (50%) */
	return sq1000_brightness < 128 * 128 * 1000 ? TC_WHITE : TC_BLACK;
}

/**
 * Initialize _stringwidth_table cache
 * @param monospace Whether to load the monospace cache or the normal fonts.
 */
void LoadStringWidthTable(bool monospace)
{
	for (FontSize fs = monospace ? FS_MONO : FS_BEGIN; fs < (monospace ? FS_END : FS_MONO); fs++) {
		for (uint i = 0; i != 224; i++) {
			_stringwidth_table[fs][i] = GetGlyphWidth(fs, i + 32);
		}
	}

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

/**
 * Determine the broadest digits for guessing the maximum width of a n-digit number.
 * @param [out] front Broadest digit, which is not 0. (Use this digit as first digit for numbers with more than one digit.)
 * @param [out] next Broadest digit, including 0. (Use this digit for all digits, except the first one; or for numbers with only one digit.)
 * @param size  Font of the digit
 */
void GetBroadestDigit(uint *front, uint *next, FontSize size)
{
	int width = -1;
	for (char c = '9'; c >= '0'; c--) {
		int w = GetCharacterWidth(size, c);
		if (w > width) {
			width = w;
			*next = c - '0';
			if (c != '0') *front = c - '0';
		}
	}
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

/**
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

	if (HasModalProgress()) {
		/* We are generating the world, so release our rights to the map and
		 * painting while we are waiting a bit. */
		_modal_progress_paint_mutex->EndCritical();
		_modal_progress_work_mutex->EndCritical();

		/* Wait a while and update _realtime_tick so we are given the rights */
		if (!IsFirstModalProgressLoop()) CSleep(MODAL_PROGRESS_REDRAW_TIMEOUT);
		_realtime_tick += MODAL_PROGRESS_REDRAW_TIMEOUT;
		_modal_progress_paint_mutex->BeginCritical();
		_modal_progress_work_mutex->BeginCritical();

		/* When we ended with the modal progress, do not draw the blocks.
		 * Simply let the next run do so, otherwise we would be loading
		 * the new state (and possibly change the blitter) when we hold
		 * the drawing lock, which we must not do. */
		if (_switch_mode != SM_NONE && !HasModalProgress()) return;
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

	++_dirty_block_colour;
	_invalid_rect.left = w;
	_invalid_rect.top = h;
	_invalid_rect.right = 0;
	_invalid_rect.bottom = 0;
}

/**
 * This function extends the internal _invalid_rect rectangle as it
 * now contains the rectangle defined by the given parameters. Note
 * the point (0,0) is top left.
 *
 * @param left The left edge of the rectangle
 * @param top The top edge of the rectangle
 * @param right The right edge of the rectangle
 * @param bottom The bottom edge of the rectangle
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

		do b[--i] = 0xFF; while (i != 0);

		b += _dirty_bytes_per_line;
	} while (--height != 0);
}

/**
 * This function mark the whole screen as dirty. This results in repainting
 * the whole screen. Use this with care as this function will break the
 * idea about marking only parts of the screen as 'dirty'.
 * @ingroup dirty
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

	cv->size.y = UnScaleByZoom(p->height, ZOOM_LVL_GUI);
	cv->size.x = UnScaleByZoom(p->width, ZOOM_LVL_GUI);
	cv->offs.x = UnScaleByZoom(p->x_offs, ZOOM_LVL_GUI);
	cv->offs.y = UnScaleByZoom(p->y_offs, ZOOM_LVL_GUI);

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
