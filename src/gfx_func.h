/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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

extern DirectionKeys _dirkeys;
extern bool _fullscreen;
extern Support8bpp _support8bpp;
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
void HandleTextInput(std::string_view str, bool marked = false,
		std::optional<size_t> caret = std::nullopt,
		std::optional<size_t> insert_location = std::nullopt, std::optional<size_t> replacement_end = std::nullopt);
void HandleCtrlChanged();
void HandleMouseEvents();
void UpdateWindows();
void ChangeGameSpeed(bool enable_fast_forward);

void DrawMouseCursor();
void ScreenSizeChanged();
void GameSizeChanged();
void UpdateGUIZoom();
bool AdjustGUIZoom(bool automatic);
void UndrawMouseCursor();

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);

Dimension GetSpriteSize(SpriteID sprid, Point *offset = nullptr, ZoomLevel zoom = _gui_zoom);
Dimension GetScaledSpriteSize(SpriteID sprid); /* widget.cpp */
Dimension GetSquareScaledSpriteSize(SpriteID sprid); /* widget.cpp */
void DrawSpriteViewport(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub = nullptr);
void DrawSprite(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub = nullptr, ZoomLevel zoom = _gui_zoom);
void DrawSpriteIgnorePadding(SpriteID img, PaletteID pal, const Rect &r, StringAlignment align); /* widget.cpp */
std::unique_ptr<uint32_t[]> DrawSpriteToRgbaBuffer(SpriteID spriteId, ZoomLevel zoom = _gui_zoom);

int DrawString(int left, int right, int top, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FontSize::Normal);
int DrawString(int left, int right, int top, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FontSize::Normal);
int DrawStringMultiLine(int left, int right, int top, int bottom, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FontSize::Normal);
int DrawStringMultiLine(int left, int right, int top, int bottom, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FontSize::Normal);
bool DrawStringMultiLineWithClipping(int left, int right, int top, int bottom, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FontSize::Normal);

void DrawCharCentered(char32_t c, const Rect &r, TextColour colour);

void GfxFillRect(int left, int top, int right, int bottom, const std::variant<PixelColour, PaletteID> &colour, FillRectMode mode = FillRectMode::Opaque);
void GfxFillPolygon(std::span<const Point> shape, const std::variant<PixelColour, PaletteID> &colour, FillRectMode mode = FillRectMode::Opaque);
void GfxDrawLine(int left, int top, int right, int bottom, PixelColour colour, int width = 1, int dash = 0);
void DrawBox(int x, int y, int dx1, int dy1, int dx2, int dy2, int dx3, int dy3);
void DrawRectOutline(const Rect &r, PixelColour colour, int width = 1, int dash = 0);

/**
 * Draw string, possibly truncated to make it fit in its allocated space
 *
 * @param r The rectangle to draw the string in.
 * @param str String to draw.
 * @param colour Colour used for drawing the string, for details see _string_colourmap in table/palettes.h or docs/ottd-colourtext-palette.png or the enum TextColour in gfx_type.h
 * @param align The alignment of the string when drawing left-to-right. In the case a right-to-left language is chosen this is inverted so it will be drawn in the right direction.
 * @param underline Whether to underline what has been drawn or not.
 * @param fontsize The size of the initial characters.
 * @return In case of left or center alignment the right most pixel we have drawn to. In case of right alignment the left most pixel we have drawn to.
 */
inline int DrawString(const Rect &r, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FontSize::Normal)
{
	return DrawString(r.left, r.right, r.top, str, colour, align, underline, fontsize);
}

/** @copydoc DrawString(const Rect &r, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FontSize::Normal) */
inline int DrawString(const Rect &r, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FontSize::Normal)
{
	return DrawString(r.left, r.right, r.top, str, colour, align, underline, fontsize);
}

/**
 * Draw string, possibly over multiple lines.
 *
 * @param r The rectangle to draw the string in.
 * @param str String to draw.
 * @param colour Colour used for drawing the string, for details see _string_colourmap in table/palettes.h or docs/ottd-colourtext-palette.png or the enum TextColour in gfx_type.h
 * @param align The horizontal and vertical alignment of the string.
 * @param underline Whether to underline all strings
 * @param fontsize The size of the initial characters.
 *
 * @return If \a align is #SA_BOTTOM, the top to where we have written, else the bottom to where we have written.
 */
inline int DrawStringMultiLine(const Rect &r, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FontSize::Normal)
{
	return DrawStringMultiLine(r.left, r.right, r.top, r.bottom, str, colour, align, underline, fontsize);
}

/** @copydoc DrawStringMultiLine(const Rect &r, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FontSize::Normal) */
inline int DrawStringMultiLine(const Rect &r, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FontSize::Normal)
{
	return DrawStringMultiLine(r.left, r.right, r.top, r.bottom, str, colour, align, underline, fontsize);
}

/**
 * Draw a multiline string, possibly over multiple lines, if the region is within the current display clipping area.
 * @note With clipping, it is not possible to determine how tall the rendered text will be, as it's not layouted.
 *       Regular DrawStringMultiLine must be used if the height needs to be known.
 *
 * @param r The rectangle to draw the string in.
 * @param str String to draw.
 * @param colour Colour used for drawing the string, for details see _string_colourmap in table/palettes.h or docs/ottd-colourtext-palette.png or the enum TextColour in gfx_type.h
 * @param align The horizontal and vertical alignment of the string.
 * @param underline Whether to underline all strings
 * @param fontsize The size of the initial characters.
 *
 * @return \c true iff the string was drawn.
 */
inline bool DrawStringMultiLineWithClipping(const Rect &r, std::string_view str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FontSize::Normal)
{
	return DrawStringMultiLineWithClipping(r.left, r.right, r.top, r.bottom, str, colour, align, underline, fontsize);
}

/**
 * Applies a certain FillRectMode-operation to a rectangle [left, right] x [top, bottom] on the screen.
 *
 * @pre dpi->zoom == ZoomLevel::Min, right >= left, bottom >= top
 * @param r The rectangle to fill.
 * @param colour A 8 bit palette index (FillRectMode::Opaque and FillRectMode::Checker) or a recolour spritenumber (FillRectMode::Recolour)
 * @param mode
 *         FillRectMode::Opaque: Fill the rectangle with the specified colour
 *         FillRectMode::Checker: Like FillRectMode::Opaque, but only draw every second pixel (used to grey out things)
 *         FillRectMode::Recolour: Apply a recolour sprite to every pixel in the rectangle currently on screen
 */
inline void GfxFillRect(const Rect &r, const std::variant<PixelColour, PaletteID> &colour, FillRectMode mode = FillRectMode::Opaque)
{
	GfxFillRect(r.left, r.top, r.right, r.bottom, colour, mode);
}

Dimension GetStringBoundingBox(std::string_view str, FontSize start_fontsize = FontSize::Normal);
Dimension GetStringBoundingBox(StringID strid, FontSize start_fontsize = FontSize::Normal);
uint GetStringListWidth(std::span<const StringID> list, FontSize fontsize = FontSize::Normal);
Dimension GetStringListBoundingBox(std::span<const StringID> list, FontSize fontsize = FontSize::Normal);
int GetStringHeight(std::string_view str, int maxw, FontSize fontsize = FontSize::Normal);
int GetStringHeight(StringID str, int maxw);
int GetStringLineCount(std::string_view str, int maxw);
Dimension GetStringMultiLineBoundingBox(StringID str, const Dimension &suggestion);
Dimension GetStringMultiLineBoundingBox(std::string_view str, const Dimension &suggestion, FontSize fontsize = FontSize::Normal);
void LoadStringWidthTable(FontSizes fontsizes = FONTSIZES_REQUIRED);

void DrawDirtyBlocks();
void AddDirtyBlock(int left, int top, int right, int bottom);
void MarkWholeScreenDirty();

void CheckBlitter();

bool FillDrawPixelInfo(DrawPixelInfo *n, int left, int top, int width, int height);

inline bool FillDrawPixelInfo(DrawPixelInfo *n, const Rect &r)
{
	return FillDrawPixelInfo(n, r.left, r.top, r.Width(), r.Height());
}

/* window.cpp */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom);

void SetMouseCursorBusy(bool busy);
void SetMouseCursor(CursorID cursor, PaletteID pal);
void SetAnimatedMouseCursor(std::span<const AnimCursor> table);
void SetCursor(CursorID cursor, PaletteID pal);
void CursorTick();
void UpdateCursorSize();
bool ChangeResInGame(int w, int h);
void SortResolutions();
bool ToggleFullScreen(bool fs);

/* gfx.cpp */
uint8_t GetCharacterWidth(FontSize size, char32_t key);
uint8_t GetDigitWidth(FontSize size = FontSize::Normal);
std::pair<uint8_t, uint8_t> GetBroadestDigit(FontSize size);

int GetCharacterHeight(FontSize size);

extern DrawPixelInfo *_cur_dpi;

#endif /* GFX_FUNC_H */
