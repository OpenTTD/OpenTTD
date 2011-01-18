/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_func.h Functions related to the gfx engine. */

/**
 * @defgroup dirty Dirty
 *
 * Handles the repaint of some part of the screen.
 *
 * Some places in the code are called functions which makes something "dirty".
 * This has nothing to do with making a Tile or Window darker or less visible.
 * This term comes from memory caching and is used to define an object must
 * be repaint. If some data of an object (like a Tile, Window, Vehicle, whatever)
 * are changed which are so extensive the object must be repaint its marked
 * as "dirty". The video driver repaint this object instead of the whole screen
 * (this is btw. also possible if needed). This is used to avoid a
 * flickering of the screen by the video driver constantly repainting it.
 *
 * This whole mechanism is controlled by an rectangle defined in #_invalid_rect. This
 * rectangle defines the area on the screen which must be repaint. If a new object
 * needs to be repainted this rectangle is extended to 'catch' the object on the
 * screen. At some point (which is normaly uninteressted for patch writers) this
 * rectangle is send to the video drivers method
 * VideoDriver::MakeDirty and it is truncated back to an empty rectangle. At some
 * later point (which is uninteressted, too) the video driver
 * repaints all these saved rectangle instead of the whole screen and drop the
 * rectangle informations. Then a new round begins by marking objects "dirty".
 *
 * @see VideoDriver::MakeDirty
 * @see _invalid_rect
 * @see _screen
 */


#ifndef GFX_FUNC_H
#define GFX_FUNC_H

#include "gfx_type.h"
#include "strings_type.h"

void GameLoop();

void CreateConsole();

extern byte _dirkeys;        ///< 1 = left, 2 = up, 4 = right, 8 = down
extern bool _fullscreen;
extern CursorVars _cursor;
extern bool _ctrl_pressed;   ///< Is Ctrl pressed?
extern bool _shift_pressed;  ///< Is Shift pressed?
extern byte _fast_forward;

extern bool _left_button_down;
extern bool _left_button_clicked;
extern bool _right_button_down;
extern bool _right_button_clicked;

extern DrawPixelInfo _screen;
extern bool _screen_disable_anim;   ///< Disable palette animation (important for 32bpp-anim blitter during giant screenshot)

extern int _pal_first_dirty;
extern int _pal_count_dirty;
extern int _num_resolutions;
extern Dimension _resolutions[32];
extern Dimension _cur_resolution;
extern Colour _cur_palette[256]; ///< Current palette. Entry 0 has to be always fully transparent!

void HandleKeypress(uint32 key);
void HandleCtrlChanged();
void HandleMouseEvents();
void CSleep(int milliseconds);
void UpdateWindows();

void DrawMouseCursor();
void ScreenSizeChanged();
void GameSizeChanged();
void UndrawMouseCursor();

/** Size of the buffer used for drawing strings. */
static const int DRAW_STRING_BUFFER = 2048;

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);

Dimension GetSpriteSize(SpriteID sprid);
void DrawSprite(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub = NULL);

/** How to align the to-be drawn text. */
enum StringAlignment {
	SA_LEFT        = 0 << 0, ///< Left align the text.
	SA_HOR_CENTER  = 1 << 0, ///< Horizontally center the text.
	SA_RIGHT       = 2 << 0, ///< Right align the text (must be a single bit).
	SA_HOR_MASK    = 3 << 0, ///< Mask for horizontal alignment.

	SA_TOP         = 0 << 2, ///< Top align the text.
	SA_VERT_CENTER = 1 << 2, ///< Vertically center the text.
	SA_BOTTOM      = 2 << 2, ///< Bottom align the text.
	SA_VERT_MASK   = 3 << 2, ///< Mask for vertical alignment.

	SA_CENTER      = SA_HOR_CENTER | SA_VERT_CENTER, ///< Center both horizontally and vertically.

	SA_FORCE       = 1 << 4, ///< Force the alignment, i.e. don't swap for RTL languages.
	SA_STRIP       = 1 << 5, ///< Strip the SETX/SETXY commands from the string
};
DECLARE_ENUM_AS_BIT_SET(StringAlignment)

int DrawString(int left, int right, int top, const char *str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false);
int DrawString(int left, int right, int top, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false);
int DrawStringMultiLine(int left, int right, int top, int bottom, const char *str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false);
int DrawStringMultiLine(int left, int right, int top, int bottom, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false);

void DrawCharCentered(uint32 c, int x, int y, TextColour colour);

void GfxFillRect(int left, int top, int right, int bottom, int colour, FillRectMode mode = FILLRECT_OPAQUE);
void GfxDrawLine(int left, int top, int right, int bottom, int colour);
void DrawBox(int x, int y, int dx1, int dy1, int dx2, int dy2, int dx3, int dy3);

Dimension GetStringBoundingBox(const char *str, FontSize start_fontsize = FS_NORMAL);
Dimension GetStringBoundingBox(StringID strid);
uint32 FormatStringLinebreaks(char *str, const char *last, int maxw, FontSize start_fontsize = FS_NORMAL);
int GetStringHeight(StringID str, int maxw);
Dimension GetStringMultiLineBoundingBox(StringID str, const Dimension &suggestion);
void LoadStringWidthTable();

void DrawDirtyBlocks();
void SetDirtyBlocks(int left, int top, int right, int bottom);
void MarkWholeScreenDirty();

void GfxInitPalettes();

bool FillDrawPixelInfo(DrawPixelInfo *n, int left, int top, int width, int height);

/* window.cpp */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom);

void SetMouseCursor(CursorID cursor, PaletteID pal);
void SetAnimatedMouseCursor(const AnimCursor *table);
void CursorTick();
void UpdateCursorSize();
bool ChangeResInGame(int w, int h);
void SortResolutions(int count);
bool ToggleFullScreen(bool fs);

/* gfx.cpp */
byte GetCharacterWidth(FontSize size, uint32 key);
byte GetDigitWidth(FontSize size = FS_NORMAL);

/**
 * Get height of a character for a given font size.
 * @param size Font size to get height of
 * @return     Height of characters in the given font (pixels)
 */
static inline byte GetCharacterHeight(FontSize size)
{
	assert(size < FS_END);
	extern int _font_height[FS_END];
	return _font_height[size];
}

/** Height of characters in the small (#FS_SMALL) font. */
#define FONT_HEIGHT_SMALL  (GetCharacterHeight(FS_SMALL))

/** Height of characters in the normal (#FS_NORMAL) font. */
#define FONT_HEIGHT_NORMAL (GetCharacterHeight(FS_NORMAL))

/** Height of characters in the large (#FS_LARGE) font. */
#define FONT_HEIGHT_LARGE  (GetCharacterHeight(FS_LARGE))

extern DrawPixelInfo *_cur_dpi;

/**
 * All 16 colour gradients
 * 8 colours per gradient from darkest (0) to lightest (7)
 */
extern byte _colour_gradient[COLOUR_END][8];

extern PaletteType _use_palette;
extern bool _palette_remap_grf[];
extern const byte *_palette_remap;
extern const byte *_palette_reverse_remap;

#endif /* GFX_FUNC_H */
