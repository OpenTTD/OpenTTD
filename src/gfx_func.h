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
 * screen. At some point (which is normally uninteresting for patch writers) this
 * rectangle is send to the video drivers method
 * VideoDriver::MakeDirty and it is truncated back to an empty rectangle. At some
 * later point (which is uninteresting, too) the video driver
 * repaints all these saved rectangle instead of the whole screen and drop the
 * rectangle information. Then a new round begins by marking objects "dirty".
 *
 * @see VideoDriver::MakeDirty
 * @see _invalid_rect
 * @see _screen
 */


#ifndef GFX_FUNC_H
#define GFX_FUNC_H

#include "gfx_type.h"
#include "strings_type.h"
#include "string_type.h"

void GameLoop();

void CreateConsole();

extern byte _dirkeys;        ///< 1 = left, 2 = up, 4 = right, 8 = down
extern bool _fullscreen;
extern byte _support8bpp;
extern CursorVars _cursor;
extern bool _ctrl_pressed;   ///< Is Ctrl pressed?
extern bool _shift_pressed;  ///< Is Shift pressed?
extern uint16_t _game_speed;

extern bool _left_button_down;
extern bool _left_button_clicked;
extern bool _right_button_down;
extern bool _right_button_clicked;

extern DrawPixelInfo _screen;
extern bool _screen_disable_anim;   ///< Disable palette animation (important for 32bpp-anim blitter during giant screenshot)

extern std::vector<Dimension> _resolutions;
extern Dimension _cur_resolution;
extern Palette _cur_palette; ///< Current palette

void HandleToolbarHotkey(int hotkey);
void HandleKeypress(uint keycode, char32_t key);
void HandleTextInput(const char *str, bool marked = false, const char *caret = nullptr, const char *insert_location = nullptr, const char *replacement_end = nullptr);
void HandleCtrlChanged();
void HandleMouseEvents();
void UpdateWindows();
void ChangeGameSpeed(bool enable_fast_forward);

void DrawMouseCursor();
void ScreenSizeChanged();
void GameSizeChanged();
bool AdjustGUIZoom(bool automatic);
void UndrawMouseCursor();

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);

Dimension GetSpriteSize(SpriteID sprid, Point *offset = nullptr, ZoomLevel zoom = ZOOM_LVL_GUI);
Dimension GetScaledSpriteSize(SpriteID sprid); /* widget.cpp */
void DrawSpriteViewport(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub = nullptr);
void DrawSprite(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub = nullptr, ZoomLevel zoom = ZOOM_LVL_GUI);
void DrawSpriteIgnorePadding(SpriteID img, PaletteID pal, const Rect &r, StringAlignment align); /* widget.cpp */
std::unique_ptr<uint32_t[]> DrawSpriteToRgbaBuffer(SpriteID spriteId, ZoomLevel zoom = ZOOM_LVL_GUI);

int DrawString(int left, int right, int top, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawString(int left, int right, int top, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawStringMultiLine(int left, int right, int top, int bottom, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawStringMultiLine(int left, int right, int top, int bottom, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL);

void DrawCharCentered(char32_t c, const Rect &r, TextColour colour);

void GfxFillRect(int left, int top, int right, int bottom, int colour, FillRectMode mode = FILLRECT_OPAQUE);
void GfxFillPolygon(const std::vector<Point> &shape, int colour, FillRectMode mode = FILLRECT_OPAQUE);
void GfxDrawLine(int left, int top, int right, int bottom, int colour, int width = 1, int dash = 0);
void DrawBox(int x, int y, int dx1, int dy1, int dx2, int dy2, int dx3, int dy3);
void DrawRectOutline(const Rect &r, int colour, int width = 1, int dash = 0);

/* Versions of DrawString/DrawStringMultiLine that accept a Rect instead of separate left, right, top and bottom parameters. */
static inline int DrawString(const Rect &r, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawString(r.left, r.right, r.top, str, colour, align, underline, fontsize);
}

static inline int DrawString(const Rect &r, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawString(r.left, r.right, r.top, str, colour, align, underline, fontsize);
}

static inline int DrawStringMultiLine(const Rect &r, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawStringMultiLine(r.left, r.right, r.top, r.bottom, str, colour, align, underline, fontsize);
}

static inline int DrawStringMultiLine(const Rect &r, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawStringMultiLine(r.left, r.right, r.top, r.bottom, str, colour, align, underline, fontsize);
}

static inline void GfxFillRect(const Rect &r, int colour, FillRectMode mode = FILLRECT_OPAQUE)
{
	GfxFillRect(r.left, r.top, r.right, r.bottom, colour, mode);
}

Dimension GetStringBoundingBox(std::string_view str, FontSize start_fontsize = FS_NORMAL);
Dimension GetStringBoundingBox(StringID strid, FontSize start_fontsize = FS_NORMAL);
uint GetStringListWidth(const StringID *list, FontSize fontsize = FS_NORMAL);
int GetStringHeight(std::string_view str, int maxw, FontSize fontsize = FS_NORMAL);
int GetStringHeight(StringID str, int maxw);
int GetStringLineCount(StringID str, int maxw);
Dimension GetStringMultiLineBoundingBox(StringID str, const Dimension &suggestion);
Dimension GetStringMultiLineBoundingBox(std::string_view str, const Dimension &suggestion);
void LoadStringWidthTable(bool monospace = false);
Point GetCharPosInString(std::string_view str, const char *ch, FontSize start_fontsize = FS_NORMAL);
ptrdiff_t GetCharAtPosition(std::string_view str, int x, FontSize start_fontsize = FS_NORMAL);

void DrawDirtyBlocks();
void AddDirtyBlock(int left, int top, int right, int bottom);
void MarkWholeScreenDirty();

void CheckBlitter();

bool FillDrawPixelInfo(DrawPixelInfo *n, int left, int top, int width, int height);

static inline bool FillDrawPixelInfo(DrawPixelInfo *n, const Rect &r)
{
	return FillDrawPixelInfo(n, r.left, r.top, r.Width(), r.Height());
}

/**
 * Determine where to draw a centred object inside a widget.
 * @param min The top or left coordinate.
 * @param max The bottom or right coordinate.
 * @param size The height or width of the object to draw.
 * @return Offset of where to start drawing the object.
 */
static inline int CenterBounds(int min, int max, int size)
{
	return (min + max - size + 1) / 2;
}

/* window.cpp */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom);

void SetMouseCursorBusy(bool busy);
void SetMouseCursor(CursorID cursor, PaletteID pal);
void SetAnimatedMouseCursor(const AnimCursor *table);
void CursorTick();
void UpdateCursorSize();
bool ChangeResInGame(int w, int h);
void SortResolutions();
bool ToggleFullScreen(bool fs);

/* gfx.cpp */
byte GetCharacterWidth(FontSize size, char32_t key);
byte GetDigitWidth(FontSize size = FS_NORMAL);
void GetBroadestDigit(uint *front, uint *next, FontSize size = FS_NORMAL);

int GetCharacterHeight(FontSize size);

extern DrawPixelInfo *_cur_dpi;

#endif /* GFX_FUNC_H */
