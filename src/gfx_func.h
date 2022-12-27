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
extern uint16 _game_speed;

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
void HandleKeypress(uint keycode, WChar key);
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

/** Size of the buffer used for drawing strings. */
static const int DRAW_STRING_BUFFER = 2048;

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);

Dimension GetSpriteSize(SpriteID sprid, Point *offset = nullptr, ZoomLevel zoom = ZOOM_LVL_GUI);
Dimension GetScaledSpriteSize(SpriteID sprid); /* widget.cpp */
void DrawSpriteViewport(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub = nullptr);
void DrawSprite(SpriteID img, PaletteID pal, int x, int y, const SubSprite *sub = nullptr, ZoomLevel zoom = ZOOM_LVL_GUI);
void DrawSpriteIgnorePadding(SpriteID img, PaletteID pal, const Rect &r, bool clicked, StringAlignment align); /* widget.cpp */
std::unique_ptr<uint32[]> DrawSpriteToRgbaBuffer(SpriteID spriteId, ZoomLevel zoom = ZOOM_LVL_GUI);

int DrawString(int left, int right, int top, const char *str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawString(int left, int right, int top, const std::string &str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawString(int left, int right, int top, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawStringMultiLine(int left, int right, int top, int bottom, const char *str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawStringMultiLine(int left, int right, int top, int bottom, const std::string &str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL);
int DrawStringMultiLine(int left, int right, int top, int bottom, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL);

void DrawCharCentered(WChar c, const Rect &r, TextColour colour);

void GfxFillRect(int left, int top, int right, int bottom, int colour, FillRectMode mode = FILLRECT_OPAQUE);
void GfxFillPolygon(const std::vector<Point> &shape, int colour, FillRectMode mode = FILLRECT_OPAQUE);
void GfxDrawLine(int left, int top, int right, int bottom, int colour, int width = 1, int dash = 0);
void DrawBox(int x, int y, int dx1, int dy1, int dx2, int dy2, int dx3, int dy3);

/* Versions of DrawString/DrawStringMultiLine that accept a Rect instead of separate left, right, top and bottom parameters. */
static inline int DrawString(const Rect &r, const char *str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawString(r.left, r.right, r.top, str, colour, align, underline, fontsize);
}

static inline int DrawString(const Rect &r, const std::string &str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawString(r.left, r.right, r.top, str, colour, align, underline, fontsize);
}

static inline int DrawString(const Rect &r, StringID str, TextColour colour = TC_FROMSTRING, StringAlignment align = SA_LEFT, bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawString(r.left, r.right, r.top, str, colour, align, underline, fontsize);
}

static inline int DrawStringMultiLine(const Rect &r, const char *str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL)
{
	return DrawStringMultiLine(r.left, r.right, r.top, r.bottom, str, colour, align, underline, fontsize);
}

static inline int DrawStringMultiLine(const Rect &r, const std::string &str, TextColour colour = TC_FROMSTRING, StringAlignment align = (SA_TOP | SA_LEFT), bool underline = false, FontSize fontsize = FS_NORMAL)
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

Dimension GetStringBoundingBox(const char *str, FontSize start_fontsize = FS_NORMAL);
Dimension GetStringBoundingBox(const std::string &str, FontSize start_fontsize = FS_NORMAL);
Dimension GetStringBoundingBox(StringID strid, FontSize start_fontsize = FS_NORMAL);
uint GetStringListWidth(const StringID *list, FontSize fontsize = FS_NORMAL);
int GetStringHeight(const char *str, int maxw, FontSize fontsize = FS_NORMAL);
int GetStringHeight(StringID str, int maxw);
int GetStringLineCount(StringID str, int maxw);
Dimension GetStringMultiLineBoundingBox(StringID str, const Dimension &suggestion);
Dimension GetStringMultiLineBoundingBox(const char *str, const Dimension &suggestion);
void LoadStringWidthTable(bool monospace = false);
Point GetCharPosInString(const char *str, const char *ch, FontSize start_fontsize = FS_NORMAL);
const char *GetCharAtPosition(const char *str, int x, FontSize start_fontsize = FS_NORMAL);

void DrawDirtyBlocks();
void AddDirtyBlock(int left, int top, int right, int bottom);
void MarkWholeScreenDirty();

bool CopyPalette(Palette &local_palette, bool force_copy = false);
void GfxInitPalettes();
void CheckBlitter();

bool FillDrawPixelInfo(DrawPixelInfo *n, int left, int top, int width, int height);

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
byte GetCharacterWidth(FontSize size, WChar key);
byte GetDigitWidth(FontSize size = FS_NORMAL);
void GetBroadestDigit(uint *front, uint *next, FontSize size = FS_NORMAL);

int GetCharacterHeight(FontSize size);

/** Height of characters in the small (#FS_SMALL) font. @note Some characters may be oversized. */
#define FONT_HEIGHT_SMALL  (GetCharacterHeight(FS_SMALL))

/** Height of characters in the normal (#FS_NORMAL) font. @note Some characters may be oversized. */
#define FONT_HEIGHT_NORMAL (GetCharacterHeight(FS_NORMAL))

/** Height of characters in the large (#FS_LARGE) font. @note Some characters may be oversized. */
#define FONT_HEIGHT_LARGE  (GetCharacterHeight(FS_LARGE))

/** Height of characters in the large (#FS_MONO) font. @note Some characters may be oversized. */
#define FONT_HEIGHT_MONO  (GetCharacterHeight(FS_MONO))

extern DrawPixelInfo *_cur_dpi;

/**
 * Checks if a Colours value is valid.
 *
 * @param colours The value to check
 * @return true if the given value is a valid Colours.
 */
static inline bool IsValidColours(Colours colours)
{
	return colours < COLOUR_END;
}

TextColour GetContrastColour(uint8 background, uint8 threshold = 128);

/**
 * All 16 colour gradients
 * 8 colours per gradient from darkest (0) to lightest (7)
 */
extern byte _colour_gradient[COLOUR_END][8];

/**
 * Return the colour for a particular greyscale level.
 * @param level Intensity, 0 = black, 15 = white
 * @return colour
 */
#define GREY_SCALE(level) (level)

static const uint8 PC_BLACK              = GREY_SCALE(1);  ///< Black palette colour.
static const uint8 PC_DARK_GREY          = GREY_SCALE(6);  ///< Dark grey palette colour.
static const uint8 PC_GREY               = GREY_SCALE(10); ///< Grey palette colour.
static const uint8 PC_WHITE              = GREY_SCALE(15); ///< White palette colour.

static const uint8 PC_VERY_DARK_RED      = 0xB2;           ///< Almost-black red palette colour.
static const uint8 PC_DARK_RED           = 0xB4;           ///< Dark red palette colour.
static const uint8 PC_RED                = 0xB8;           ///< Red palette colour.

static const uint8 PC_VERY_DARK_BROWN    = 0x56;           ///< Almost-black brown palette colour.

static const uint8 PC_ORANGE             = 0xC2;           ///< Orange palette colour.

static const uint8 PC_YELLOW             = 0xBF;           ///< Yellow palette colour.
static const uint8 PC_LIGHT_YELLOW       = 0x44;           ///< Light yellow palette colour.
static const uint8 PC_VERY_LIGHT_YELLOW  = 0x45;           ///< Almost-white yellow palette colour.

static const uint8 PC_GREEN              = 0xD0;           ///< Green palette colour.

static const uint8 PC_VERY_DARK_BLUE     = 0x9A;           ///< Almost-black blue palette colour.
static const uint8 PC_DARK_BLUE          = 0x9D;           ///< Dark blue palette colour.
static const uint8 PC_LIGHT_BLUE         = 0x98;           ///< Light blue palette colour.

static const uint8 PC_ROUGH_LAND         = 0x52;           ///< Dark green palette colour for rough land.
static const uint8 PC_GRASS_LAND         = 0x54;           ///< Dark green palette colour for grass land.
static const uint8 PC_BARE_LAND          = 0x37;           ///< Brown palette colour for bare land.
static const uint8 PC_RAINFOREST         = 0x5C;           ///< Pale green palette colour for rainforest.
static const uint8 PC_FIELDS             = 0x25;           ///< Light brown palette colour for fields.
static const uint8 PC_TREES              = 0x57;           ///< Green palette colour for trees.
static const uint8 PC_WATER              = 0xC9;           ///< Dark blue palette colour for water.
#endif /* GFX_FUNC_H */
