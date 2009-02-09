/* $Id$ */

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

enum {
	/* Size of the buffer used for drawing strings. */
	DRAW_STRING_BUFFER = 2048,
};

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);

void DrawSprite(SpriteID img, SpriteID pal, int x, int y, const SubSprite *sub = NULL);

int DrawStringCentered(int x, int y, StringID str, TextColour colour);
int DrawStringCenteredTruncated(int xl, int xr, int y, StringID str, TextColour colour);
int DoDrawStringCentered(int x, int y, const char *str, TextColour colour);

int DrawString(int x, int y, StringID str, TextColour colour);
int DrawStringTruncated(int x, int y, StringID str, TextColour colour, uint maxw);

int DoDrawString(const char *string, int x, int y, TextColour colour, bool parse_string_also_when_clipped = false);
int DoDrawStringTruncated(const char *str, int x, int y, TextColour colour, uint maxw);

void DrawStringCenterUnderline(int x, int y, StringID str, TextColour colour);
void DrawStringCenterUnderlineTruncated(int xl, int xr, int y, StringID str, TextColour colour);

int DrawStringRightAligned(int x, int y, StringID str, TextColour colour);
void DrawStringRightAlignedTruncated(int x, int y, StringID str, TextColour colour, uint maxw);
void DrawStringRightAlignedUnderline(int x, int y, StringID str, TextColour colour);

void DrawCharCentered(uint32 c, int x, int y, TextColour colour);

void GfxFillRect(int left, int top, int right, int bottom, int colour, FillRectMode mode = FILLRECT_OPAQUE);
void GfxDrawLine(int left, int top, int right, int bottom, int colour);
void DrawBox(int x, int y, int dx1, int dy1, int dx2, int dy2, int dx3, int dy3);

Dimension GetStringBoundingBox(const char *str);
uint32 FormatStringLinebreaks(char *str, int maxw);
int GetStringHeight(StringID str, int maxw);
void LoadStringWidthTable();
void DrawStringMultiCenter(int x, int y, StringID str, int maxw);
uint DrawStringMultiLine(int x, int y, StringID str, int maxw, int maxh = -1);

/**
 * Let the dirty blocks repainting by the video driver.
 *
 * @ingroup dirty
 */
void DrawDirtyBlocks();

/**
 * Set a new dirty block.
 *
 * @ingroup dirty
 */
void SetDirtyBlocks(int left, int top, int right, int bottom);

/**
 * Marks the whole screen as dirty.
 *
 * @ingroup dirty
 */
void MarkWholeScreenDirty();

void GfxInitPalettes();

bool FillDrawPixelInfo(DrawPixelInfo *n, int left, int top, int width, int height);

/* window.cpp */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom);

void SetMouseCursor(SpriteID sprite, SpriteID pal);
void SetAnimatedMouseCursor(const AnimCursor *table);
void CursorTick();
bool ChangeResInGame(int w, int h);
void SortResolutions(int count);
bool ToggleFullScreen(bool fs);

/* gfx.cpp */
#define ASCII_LETTERSTART 32
extern FontSize _cur_fontsize;

byte GetCharacterWidth(FontSize size, uint32 key);

/**
 * Get height of a character for a given font size.
 * @param size Font size to get height of
 * @return     Height of characters in the given font (pixels)
 */
static inline byte GetCharacterHeight(FontSize size)
{
	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return 10;
		case FS_SMALL:  return 6;
		case FS_LARGE:  return 18;
	}
}

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
